#include "SlabScenarioCodec.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/SecureHash.h"

namespace
{
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double& Value,FString& Error)
{
	if(!O->TryGetNumberField(Key,Value)||!FMath::IsFinite(Value))
	{ Error=FString::Printf(TEXT("유한한 숫자가 필요합니다: %s"),Key); return false; }
	return true;
}
}

bool FSlabScenarioCodec::Parse(const FString& Json,FSlabScenarioDataPtr& Out,FString& Error,bool bAllowMissingUuid)
{
	Out.Reset(); Error.Reset();
	FTCHARToUTF8 Utf8(*Json);
	if(Utf8.Length()>8*1024*1024) { Error=TEXT("시나리오 JSON은 UTF-8 기준 8MiB 이하여야 합니다."); return false; }
	TSharedPtr<FJsonObject> Root;
	if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root.IsValid())
	{ Error=TEXT("시나리오 JSON 구문이 올바르지 않습니다."); return false; }
	FString MessageId;
	if(!Root->TryGetStringField(TEXT("MESSAGE_ID"),MessageId)||MessageId!=TEXT("IFactory-agent"))
	{ Error=TEXT("MESSAGE_ID는 IFactory-agent여야 합니다."); return false; }
	auto Data=MakeShared<FSlabScenarioData,ESPMode::ThreadSafe>();
	Data->OriginalJson=Json;
	if(!Root->TryGetStringField(TEXT("CREATE_TIMESTAMP"),Data->CreateTimestamp)||Data->CreateTimestamp.IsEmpty())
	{ Error=TEXT("CREATE_TIMESTAMP 문자열이 필요합니다."); return false; }
	const TSharedPtr<FJsonObject>* Meta=nullptr;
	double ExplicitDuration=0;
	bool bHasExplicitDuration=false;
	if(Root->TryGetObjectField(TEXT("_meta"),Meta)&&Meta&&Meta->IsValid())
	{
		(*Meta)->TryGetStringField(TEXT("scenario"),Data->Name);
		if((*Meta)->HasField(TEXT("UUID"))&&!(*Meta)->TryGetStringField(TEXT("UUID"),Data->SourceUUID))
		{ Error=TEXT("_meta.UUID는 문자열이어야 합니다."); return false; }
		if((*Meta)->HasField(TEXT("sample_period_sec")))
		{
			if(!Number(*Meta,TEXT("sample_period_sec"),Data->SamplePeriodSec,Error)||Data->SamplePeriodSec<=0||Data->SamplePeriodSec>60)
			{ Error=TEXT("sample_period_sec는 0보다 크고 60초 이하여야 합니다."); return false; }
		}
		bHasExplicitDuration=(*Meta)->HasField(TEXT("duration_sec"));
		if(bHasExplicitDuration&&!Number(*Meta,TEXT("duration_sec"),ExplicitDuration,Error)) return false;
	}
	if(Data->SourceUUID.IsEmpty())
	{
		if(!bAllowMissingUuid) { Error=TEXT("_meta.UUID가 필요합니다."); return false; }
		uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Utf8.Get(),Utf8.Length(),Hash);
		FGuid GeneratedId;
		FGuid::ParseExact(BytesToHex(Hash,16),EGuidFormats::Digits,GeneratedId);
		Data->ScenarioUUID=GeneratedId.ToString(EGuidFormats::DigitsWithHyphensLower);
		Data->bGeneratedArchiveId=true;
	}
	else
	{
		FGuid Id;
		if(!FGuid::Parse(Data->SourceUUID,Id)||!Id.IsValid()) { Error=TEXT("_meta.UUID가 유효한 UUID가 아닙니다."); return false; }
		Data->ScenarioUUID=Id.ToString(EGuidFormats::DigitsWithHyphensLower);
	}
	if(Data->Name.IsEmpty()) Data->Name=TEXT("Slab 시나리오");
	const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
	if(!Root->TryGetArrayField(TEXT("DATA_MAP"),Rows)||!Rows||Rows->IsEmpty()||Rows->Num()>100000)
	{ Error=TEXT("DATA_MAP에는 1~100000개의 행이 필요합니다."); return false; }
	Data->Rows.Reserve(Rows->Num());
	for(const auto& Item:*Rows)
	{
		const TSharedPtr<FJsonObject>* Obj=nullptr;
		if(!Item.IsValid()||!Item->TryGetObject(Obj)||!Obj||!Obj->IsValid()) { Error=TEXT("DATA_MAP 행은 객체여야 합니다."); return false; }
		const auto& O=*Obj; FSlabScenarioRow R; double Frame=0;
		if(!Number(O,TEXT("frame_no"),Frame,Error)||Frame<0||Frame>9007199254740991.0||FMath::FloorToDouble(Frame)!=Frame)
		{ Error=TEXT("frame_no는 음수가 아닌 정확한 정수여야 합니다."); return false; }
		R.FrameNo=static_cast<int64>(Frame);
		if(!O->TryGetStringField(TEXT("mtl_no"),R.MtlNo)||R.MtlNo.TrimStartAndEnd().IsEmpty()||R.MtlNo.Len()>256||R.MtlNo.Contains(TEXT("\r"))||R.MtlNo.Contains(TEXT("\n"))) { Error=TEXT("mtl_no는 줄바꿈 없는 1~256자 문자열이어야 합니다."); return false; }
		if(!Number(O,TEXT("slab_thk"),R.Thickness,Error)||!Number(O,TEXT("slab_wth"),R.Width,Error)||
			!Number(O,TEXT("slab_len"),R.Length,Error)||!Number(O,TEXT("slab_wt"),R.Weight,Error)||
			!Number(O,TEXT("left_skew_angle"),R.LeftAngle,Error)||!Number(O,TEXT("right_skew_angle"),R.RightAngle,Error)||
			!Number(O,TEXT("center_x"),R.CenterX,Error)||!Number(O,TEXT("elapsed_sec"),R.ElapsedSec,Error)) return false;
		if(O->HasField(TEXT("center_y"))&&!Number(O,TEXT("center_y"),R.CenterY,Error)) return false;
		if(O->HasField(TEXT("mov_pos"))&&!Number(O,TEXT("mov_pos"),R.MovePosition,Error)) return false;
		if(O->HasField(TEXT("is_meandering"))&&!O->TryGetBoolField(TEXT("is_meandering"),R.bMeandering)) { Error=TEXT("is_meandering은 bool이어야 합니다."); return false; }
		if(R.Thickness<=0||R.Width<=0||R.Length<=0||R.Weight<0||R.ElapsedSec<0||R.ElapsedSec>86400||FMath::Abs(R.LeftAngle)>180||FMath::Abs(R.RightAngle)>180)
		{ Error=TEXT("치수·무게·시각·각도의 허용 범위를 확인하세요."); return false; }
		if(!Data->Rows.IsEmpty())
		{
			const auto& First=Data->Rows[0]; const auto& Previous=Data->Rows.Last();
			if(R.MtlNo!=First.MtlNo) { Error=TEXT("한 시나리오에는 하나의 mtl_no만 사용할 수 있습니다."); return false; }
			if(R.FrameNo<=Previous.FrameNo||R.ElapsedSec<=Previous.ElapsedSec) { Error=TEXT("프레임과 시각은 엄격한 증가 순서여야 합니다."); return false; }
			if(R.Thickness!=First.Thickness||R.Width!=First.Width||R.Length!=First.Length)
			{ Error=TEXT("동일 소재의 두께·폭·길이는 시나리오 전체에서 같아야 합니다."); return false; }
		}
		Data->Rows.Add(MoveTemp(R));
	}
	Data->DurationSec=bHasExplicitDuration?ExplicitDuration:Data->Rows.Last().ElapsedSec+Data->SamplePeriodSec;
	if(Data->DurationSec<=Data->Rows.Last().ElapsedSec||Data->DurationSec>86460)
	{ Error=TEXT("duration_sec는 마지막 데이터 시각보다 크고 86460초 이하여야 합니다."); return false; }
	Out=Data; return true;
}

bool FSlabScenarioCodec::Sample(const FSlabScenarioData& Data,double Time,FSlabScenarioRow& Out,int32& LowerIndex)
{
	if(Data.Rows.IsEmpty()||!FMath::IsFinite(Time)) return false;
	int32 Low=0,High=Data.Rows.Num()-1;
	while(Low<High) { const int32 Mid=(Low+High+1)/2; if(Data.Rows[Mid].ElapsedSec<=Time) Low=Mid; else High=Mid-1; }
	LowerIndex=Low; Out=Data.Rows[Low];
	if(Low+1<Data.Rows.Num()&&Time>Out.ElapsedSec)
	{
		const auto& B=Data.Rows[Low+1]; const double Alpha=FMath::Clamp((Time-Out.ElapsedSec)/(B.ElapsedSec-Out.ElapsedSec),0.0,1.0);
		Out.CenterX=FMath::Lerp(Out.CenterX,B.CenterX,Alpha);
		Out.LeftAngle+=FMath::FindDeltaAngleDegrees(Out.LeftAngle,B.LeftAngle)*Alpha;
		Out.RightAngle+=FMath::FindDeltaAngleDegrees(Out.RightAngle,B.RightAngle)*Alpha;
	}
	// Keep source FrameNo/ElapsedSec paired. Continuous playback time is carried by status/metrics.
	return true;
}

double FSlabScenarioCodec::ToCm(double Value,ESlabInputUnit Unit)
{
	return Value*(Unit==ESlabInputUnit::Millimeters?0.1:Unit==ESlabInputUnit::Meters?100.0:1.0);
}

FString FSlabScenarioCodec::MakeSyntheticJson(const FString& UUID)
{
	TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>(); Root->SetStringField(TEXT("CREATE_TIMESTAMP"),TEXT("20260727174504137"));
	Root->SetStringField(TEXT("MESSAGE_ID"),TEXT("IFactory-agent"));
	auto Meta=MakeShared<FJsonObject>(); Meta->SetStringField(TEXT("scenario"),TEXT("Continuous Casting Right Meandering - 30s"));
	Meta->SetNumberField(TEXT("duration_sec"),30); Meta->SetNumberField(TEXT("sample_period_sec"),0.05);
	if(!UUID.IsEmpty()) Meta->SetStringField(TEXT("UUID"),UUID); Root->SetObjectField(TEXT("_meta"),Meta);
	TArray<TSharedPtr<FJsonValue>> Rows; Rows.Reserve(600);
	for(int32 I=0;I<600;++I)
	{
		const double A=FMath::Clamp((I-1)/579.0,0.0,1.0);
		auto R=MakeShared<FJsonObject>(); R->SetNumberField(TEXT("frame_no"),I); R->SetStringField(TEXT("mtl_no"),TEXT("SQ83521 047"));
		R->SetNumberField(TEXT("slab_thk"),250); R->SetNumberField(TEXT("slab_wth"),1100); R->SetNumberField(TEXT("slab_len"),10830); R->SetNumberField(TEXT("slab_wt"),23290);
		const double Left=I==0?-0.2:FMath::Lerp(-0.185,-5.0,A);
		R->SetNumberField(TEXT("left_skew_angle"),Left); R->SetNumberField(TEXT("right_skew_angle"),-Left);
		R->SetNumberField(TEXT("center_x"),I==0?0:FMath::Lerp(1.55,909.0,A)); R->SetNumberField(TEXT("center_y"),I==0?0.1:FMath::Lerp(0.085,9.9,A));
		R->SetNumberField(TEXT("mov_pos"),I==0?1210:FMath::Lerp(1212.0,2120.0,A)); R->SetNumberField(TEXT("elapsed_sec"),I*0.05); R->SetBoolField(TEXT("is_meandering"),I>=400);
		Rows.Add(MakeShared<FJsonValueObject>(R));
	}
	Root->SetArrayField(TEXT("DATA_MAP"),Rows); FString Json; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json)); return Json;
}
