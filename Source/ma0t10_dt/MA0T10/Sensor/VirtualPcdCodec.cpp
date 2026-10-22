#include "VirtualPcdCodec.h"

namespace
{
constexpr int32 MaxBytes = 128 * 1024 * 1024;
const TCHAR* Names[] = {TEXT("x"),TEXT("y"),TEXT("z"),TEXT("intensity"),TEXT("ring"),TEXT("horizontal_index"),TEXT("return_index"),TEXT("return_count"),TEXT("time_offset_ns"),TEXT("validity"),TEXT("confidence")};
constexpr int32 Sizes[] = {4,4,4,2,2,2,1,1,8,1,4};
const TCHAR* Types[] = {TEXT("F"),TEXT("F"),TEXT("F"),TEXT("U"),TEXT("U"),TEXT("U"),TEXT("U"),TEXT("U"),TEXT("I"),TEXT("U"),TEXT("F")};
void Append(TArray<uint8>& Out, uint64 V, int32 N) { for(int32 I=0;I<N;++I) Out.Add(uint8(V>>(I*8))); }
uint64 Read(const uint8* P,int32 N) { uint64 V=0; for(int32 I=0;I<N;++I) V|=uint64(P[I])<<(8*I); return V; }
uint32 FloatBits(float F) { uint32 V; FMemory::Memcpy(&V,&F,4); return V; }
float ReadFloat(const uint8* P) { uint32 V=uint32(Read(P,4)); float F; FMemory::Memcpy(&F,&V,4); return F; }
uint64 Value(const FVirtualLidarPoint& P,int32 I)
{
	switch(I)
	{
	case 0:return FloatBits(float(P.SensorLocalPositionMeters.X));
	case 1:return FloatBits(float(P.SensorLocalPositionMeters.Y));
	case 2:return FloatBits(float(P.SensorLocalPositionMeters.Z));
	case 3:return FMath::Clamp(P.RawIntensity,0,65535);
	case 4:return FMath::Clamp(P.Ring,0,65535);
	case 5:return FMath::Clamp(P.HorizontalIndex,0,65535);
	case 6:return FMath::Clamp(P.EchoIndex,0,255);
	case 7:return FMath::Clamp(P.EchoCount,0,255);
	case 8:return uint64(P.PointTimeOffsetNanoseconds);
	case 9:return uint8(P.Validity);
	default:return FloatBits(P.Confidence);
	}
}
}

int32 FVirtualPcdSendOptions::FieldMask() const
{
	switch(Profile)
	{
	case EVirtualPcdFieldProfile::Compact:return 7|(1<<3)|(1<<4)|(1<<8);
	case EVirtualPcdFieldProfile::Minimal:return 7;
	case EVirtualPcdFieldProfile::Custom:return (CustomFieldMask&2047)|7;
	default:return 2047;
	}
}
const TCHAR* FVirtualPcdCodec::FieldName(int32 I) { return I>=0&&I<11?Names[I]:TEXT(""); }
int32 FVirtualPcdCodec::BytesPerPoint(const FVirtualPcdSendOptions& O)
{
	int32 N=0; for(int32 I=0;I<11;++I) if(O.FieldMask()&(1<<I)) N+=Sizes[I]; return N;
}
double FVirtualPcdCodec::EstimateBytesPerSecond(const FVirtualPcdSendOptions& O,int32 Points,double Hz)
{
	// Compression is data dependent. Show the uncompressed upper estimate explicitly in UI.
	return double(FMath::Max(0,Points))*BytesPerPoint(O)*FMath::Max(0.,Hz)/O.Divisor();
}

void FVirtualPcdCodec::CompressLzf(const TArray<uint8>& Input,TArray<uint8>& Out)
{
	// Original implementation of the public LZF stream format (32-byte literals,
	// 13-bit backward distance, 3..264-byte matches). No third-party source copied.
	Out.Reset(); Out.Reserve(Input.Num()+Input.Num()/32+1);
	TArray<int32> Hash; Hash.Init(-1,65536);
	int32 I=0,Start=0;
	auto Flush=[&](int32 End) { while(Start<End) { const int32 N=FMath::Min(32,End-Start);Out.Add(uint8(N-1));Out.Append(Input.GetData()+Start,N);Start+=N; } };
	while(I+2<Input.Num())
	{
		const uint32 H=((uint32(Input[I])*57321u)^(uint32(Input[I+1])*173u)^Input[I+2])&65535;
		const int32 Ref=Hash[H];Hash[H]=I;
		int32 N=0;
		if(Ref>=0&&I-Ref<=8192&&Input[Ref]==Input[I]&&Input[Ref+1]==Input[I+1]&&Input[Ref+2]==Input[I+2])
		{
			N=3;while(N<264&&I+N<Input.Num()&&Input[Ref+N]==Input[I+N])++N;
		}
		if(N>=3)
		{
			Flush(I);const int32 Distance=I-Ref-1,Len=N-2;
			Out.Add(uint8((FMath::Min(Len,7)<<5)|(Distance>>8)));
			if(Len>=7)Out.Add(uint8(Len-7));Out.Add(uint8(Distance));I+=N;Start=I;
		}
		else { ++I;if(I-Start==32)Flush(I); }
	}
	Flush(Input.Num());
}
bool FVirtualPcdCodec::DecompressLzf(const uint8* Input,int32 InSize,int32 OutSize,TArray<uint8>& Out)
{
	Out.Reset();if(InSize<0||OutSize<0||OutSize>MaxBytes)return false;Out.SetNumUninitialized(OutSize);
	int32 I=0,O=0;
	while(I<InSize)
	{
		const int32 C=Input[I++];
		if(C<32)
		{
			const int32 N=C+1;if(N>InSize-I||N>OutSize-O)return false;
			FMemory::Memcpy(Out.GetData()+O,Input+I,N);I+=N;O+=N;
		}
		else
		{
			int32 N=C>>5;if(N==7){if(I>=InSize)return false;N+=Input[I++];}
			if(I>=InSize)return false;const int32 Distance=((C&31)<<8)+Input[I++]+1;N+=2;
			if(Distance>O||N>OutSize-O)return false;
			for(int32 J=0;J<N;++J){Out[O]=Out[O-Distance];++O;}
		}
	}
	return O==OutSize;
}

bool FVirtualPcdCodec::Encode(const TArray<const FVirtualLidarPoint*>& Points,const FString& Meta,const FVirtualPcdSendOptions& Options,TArray<uint8>& Out)
{
	Out.Reset();const int32 Step=BytesPerPoint(Options),Mask=Options.FieldMask();
	if(int64(Points.Num())*Step>MaxBytes)return false;
	FString Fields,Size,Type,Count;
	for(int32 I=0;I<11;++I)if(Mask&(1<<I)){Fields+=FString(TEXT(" "))+Names[I];Size+=TEXT(" ")+LexToString(Sizes[I]);Type+=FString(TEXT(" "))+Types[I];Count+=TEXT(" 1");}
	const FString Comment=Meta.IsEmpty()?FString():TEXT("# MA0T10_META ")+Meta+TEXT("\n");
	const FString Header=FString::Printf(TEXT("# .PCD v0.7\n%sVERSION 0.7\nFIELDS%s\nSIZE%s\nTYPE%s\nCOUNT%s\nWIDTH %d\nHEIGHT 1\nVIEWPOINT 0 0 0 1 0 0 0\nPOINTS %d\nDATA %s\n"),*Comment,*Fields,*Size,*Type,*Count,Points.Num(),Points.Num(),Options.bCompressed?TEXT("binary_compressed"):TEXT("binary"));
	FTCHARToUTF8 Utf8(*Header);Out.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());
	if(!Options.bCompressed)
	{
		Out.Reserve(Out.Num()+Points.Num()*Step);
		for(const auto* P:Points)for(int32 I=0;I<11;++I)if(Mask&(1<<I))Append(Out,Value(*P,I),Sizes[I]);
	}
	else
	{
		TArray<uint8> Soa,Compressed;Soa.Reserve(Points.Num()*Step);
		for(int32 I=0;I<11;++I)if(Mask&(1<<I))for(const auto* P:Points)Append(Soa,Value(*P,I),Sizes[I]);
		CompressLzf(Soa,Compressed);Append(Out,Compressed.Num(),4);Append(Out,Soa.Num(),4);Out.Append(Compressed);
	}
	return true;
}

bool FVirtualPcdCodec::Decode(const TArray<uint8>& Bytes,FVirtualPcdDocument& Out,FString& Error)
{
	Out=FVirtualPcdDocument();Error=TEXT("Invalid PCD header/body");
	TMap<FString,TArray<FString>> Header;int32 Start=0;
	for(int32 I=0;I<FMath::Min(Bytes.Num(),65536);++I)
	{
		if(Bytes[I]!='\n')continue;
		FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()+Start),I-Start);
		FString Line(Utf8.Length(),Utf8.Get());Start=I+1;Line.TrimStartAndEndInline();
		if(Line.StartsWith(TEXT("#")))continue;
		TArray<FString> Tokens;Line.ParseIntoArrayWS(Tokens);if(Tokens.IsEmpty())continue;
		const FString Key=Tokens[0];Tokens.RemoveAt(0);if(Header.Contains(Key))return false;Header.Add(Key,Tokens);
		if(Key==TEXT("DATA")){Out.PayloadOffset=Start;break;}
	}
	if(!Out.PayloadOffset)return false;
	auto Single=[&](const TCHAR* Key,FString& V){auto* T=Header.Find(Key);if(!T||T->Num()!=1)return false;V=(*T)[0];return true;};
	FString Version,N,W,H;
	if(!Single(TEXT("VERSION"),Version)||Version!=TEXT("0.7")||!Single(TEXT("POINTS"),N)||!LexTryParseString(Out.PointCount,*N)||Out.PointCount<0||
		!Single(TEXT("DATA"),Out.Encoding)||!Single(TEXT("WIDTH"),W)||!Single(TEXT("HEIGHT"),H))return false;
	int32 Width=0,Height=0;if(!LexTryParseString(Width,*W)||!LexTryParseString(Height,*H)||Width<0||Height<1||int64(Width)*Height!=Out.PointCount)return false;
	auto* Fields=Header.Find(TEXT("FIELDS"));auto* Size=Header.Find(TEXT("SIZE"));auto* Type=Header.Find(TEXT("TYPE"));auto* Count=Header.Find(TEXT("COUNT"));
	if(!Fields||!Size||!Type||!Count||Fields->Num()<3||Fields->Num()>64||Size->Num()!=Fields->Num()||Type->Num()!=Fields->Num()||Count->Num()!=Fields->Num())return false;
	TSet<FString> Seen;
	for(int32 I=0;I<Fields->Num();++I)
	{
		int32 S=0,C=0;if(!LexTryParseString(S,*(*Size)[I])||!LexTryParseString(C,*(*Count)[I])||S<1||S>8||C<1||C>1024||Seen.Contains((*Fields)[I]))return false;
		Seen.Add((*Fields)[I]);const FString& T=(*Type)[I];if(T!=TEXT("F")&&T!=TEXT("U")&&T!=TEXT("I"))return false;
		for(int32 J=0;J<11;++J)if((*Fields)[I]==Names[J]&&(S!=Sizes[J]||C!=1||T!=Types[J]))return false;
		Out.Offsets.Add(Out.PointBytes);Out.Sizes.Add(S*C);Out.PointBytes+=S*C;
	}
	if(!Seen.Contains(TEXT("x"))||!Seen.Contains(TEXT("y"))||!Seen.Contains(TEXT("z"))||int64(Out.PointCount)*Out.PointBytes>MaxBytes)return false;
	Out.Names=*Fields;const int32 Expected=Out.PointCount*Out.PointBytes;
	FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),Out.PayloadOffset);Out.Header=FString(Utf8.Length(),Utf8.Get());
	if(Out.Encoding==TEXT("binary"))
	{
		if(int64(Out.PayloadOffset)+Expected!=Bytes.Num())return false;Out.Records.Append(Bytes.GetData()+Out.PayloadOffset,Expected);
	}
	else if(Out.Encoding==TEXT("binary_compressed"))
	{
		if(Bytes.Num()-Out.PayloadOffset<8)return false;
		const uint8* B=Bytes.GetData()+Out.PayloadOffset;const uint64 Packed=Read(B,4),Unpacked=Read(B+4,4);
		if(Unpacked!=uint64(Expected)||Packed!=uint64(Bytes.Num()-Out.PayloadOffset-8))return false;
		TArray<uint8> Soa;if(!DecompressLzf(B+8,int32(Packed),Expected,Soa))return false;
		Out.Records.SetNumUninitialized(Expected);int32 Plane=0;
		for(int32 F=0;F<Out.Names.Num();++F){for(int32 P=0;P<Out.PointCount;++P)FMemory::Memcpy(Out.Records.GetData()+P*Out.PointBytes+Out.Offsets[F],Soa.GetData()+Plane+P*Out.Sizes[F],Out.Sizes[F]);Plane+=Out.PointCount*Out.Sizes[F];}
	}
	else return false;
	Error.Reset();return true;
}

bool FVirtualPcdDocument::ReadPoints(TArray<FVirtualLidarPoint>& Out) const
{
	Out.Reset();Out.Reserve(PointCount);int32 Map[11];for(int32 J=0;J<11;++J)Map[J]=Names.IndexOfByKey(::Names[J]);
	for(int32 I=0;I<PointCount;++I)
	{
		const uint8* B=Records.GetData()+I*PointBytes;
		auto U=[&](int32 J,uint64 Default=0)->uint64 {return Map[J]<0?Default:Read(B+Offsets[Map[J]],Sizes[Map[J]]);};
		auto F=[&](int32 J,float Default=0.f){return Map[J]<0?Default:ReadFloat(B+Offsets[Map[J]]);};
		FVirtualLidarPoint P;P.SensorLocalPositionMeters=FVector(F(0),F(1),F(2));P.Confidence=F(10,1.f);
		if(P.SensorLocalPositionMeters.ContainsNaN()||!FMath::IsFinite(P.Confidence)||P.Confidence<0||P.Confidence>1||U(9)>uint8(EVirtualLidarPointValidity::InvalidCalibration))return false;
		P.RawIntensity=int32(U(3));P.NormalizedIntensity=P.RawIntensity/65535.f;P.Ring=P.Row=int32(U(4));P.HorizontalIndex=P.Col=int32(U(5));P.EchoIndex=P.ReturnIndex=int32(U(6));P.EchoCount=int32(U(7,1));
		const uint64 Time=U(8);FMemory::Memcpy(&P.PointTimeOffsetNanoseconds,&Time,8);P.Validity=EVirtualLidarPointValidity(U(9));P.bHit=P.Validity==EVirtualLidarPointValidity::Valid;P.bHasGridCoord=Map[4]>=0&&Map[5]>=0;
		Out.Add(MoveTemp(P));
	}
	return true;
}
