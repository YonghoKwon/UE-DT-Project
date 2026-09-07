#include "VirtualSensorPcdReceiveContract.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualSensorStreamReceiverTypes.h"

namespace
{
bool IsUnsignedInteger(const FString& S)
{
	if(S.IsEmpty()) return false;
	for(TCHAR C:S) if(C<TEXT('0')||C>TEXT('9')) return false;
	return true;
}
bool Canonical(FString& S,int32 Type)
{
	if(S.IsEmpty()) return false;
	if(Type==1) { int64 N=0; if(!IsUnsignedInteger(S)||!LexTryParseString(N,*S)||N<0) return false; S=LexToString(N); }
	if(Type==2)
	{
		FDateTime D; int64 Ms=0;
		if(IsUnsignedInteger(S))
		{
			if(!LexTryParseString(Ms,*S)||Ms<=0||Ms>253402300799999LL) return false;
			D=FDateTime::FromUnixTimestamp(Ms/1000)+FTimespan((Ms%1000)*ETimespan::TicksPerMillisecond);
		}
		else if(!FDateTime::ParseIso8601(*S,D)) return false;
		S=D.ToIso8601();
	}
	if(Type==3) { if(S.Len()!=40) return false; for(TCHAR C:S) if(!FChar::IsHexDigit(C)) return false; S=S.ToLower(); }
	return true;
}
}
bool FVirtualSensorPcdReceiveContract::Normalize(const TMap<FName,FString>& Input,TMap<FName,FString>& Output,FVirtualSensorTopicReceivedDataBase& D)
{
	Output=Input;
	D.SensorId=Input.FindRef(TEXT("x-sensor-id")); if(D.SensorId.IsEmpty()) D.SensorId=Input.FindRef(TEXT("sensor-id"));
	FString Frame=Input.FindRef(TEXT("x-frame-id")); if(Frame.IsEmpty()) Frame=Input.FindRef(TEXT("frame-id")); LexTryParseString(D.FrameId,*Frame);
	D.SchemaVersion=Input.FindRef(TEXT("schema")); D.Topic=Input.FindRef(TEXT("destination"));
	D.RequestId=Input.FindRef(TEXT("x-request-id")); if(D.RequestId.IsEmpty()) D.RequestId=Input.FindRef(TEXT("request-id"));
	struct FAlias { const TCHAR* A; const TCHAR* B; int32 Type; };
	const FAlias Fields[]={{TEXT("schema"),TEXT("schema"),0},{TEXT("x-sensor-id"),TEXT("sensor-id"),0},
		{TEXT("x-frame-id"),TEXT("frame-id"),1},{TEXT("x-point-count"),TEXT("point-count"),1},
		{TEXT("x-source-point-count"),TEXT("source-point-count"),1},{TEXT("x-filter-revision"),TEXT("filter-revision"),1},
		{TEXT("x-checksum-sha1"),TEXT("checksum"),3},{TEXT("x-acquisition-profile"),TEXT("acquisition-profile"),0},
		{TEXT("x-utc"),TEXT("timestamp-utc"),2},{TEXT("content-type"),TEXT("content-type"),0}};
	for(const auto& F:Fields)
	{
		const FString *A=Input.Find(F.A),*B=Input.Find(F.B);
		if(!A&&!B) { D.ErrorCode=TEXT("header-missing"); D.Message=FString::Printf(TEXT("필수 헤더 누락: %s 또는 %s"),F.A,F.B); return false; }
		FString VA=A?*A:*B, VB=B?*B:VA;
		if(!Canonical(VA,F.Type)||!Canonical(VB,F.Type)) { D.ErrorCode=TEXT("header-invalid"); D.Message=FString::Printf(TEXT("헤더 값/타입 오류: %s 또는 %s"),F.A,F.B); return false; }
		if(VA!=VB) { D.ErrorCode=TEXT("header-conflict"); D.Message=FString::Printf(TEXT("헤더 별칭 충돌: %s / %s"),F.A,F.B); return false; }
		Output.Add(F.A,VA);
	}
	D.RunId=Input.FindRef(TEXT("x-run-uuid")); D.MtlNo=Input.FindRef(TEXT("x-mtl-no"));
	if(!D.RunId.IsEmpty()||!D.MtlNo.IsEmpty()||Input.Contains(TEXT("x-slab-frame-no")))
	{
		FGuid Id; FString N=Input.FindRef(TEXT("x-slab-frame-no")), T=Input.FindRef(TEXT("x-slab-elapsed-sec"));
		if(!FGuid::Parse(D.RunId,Id)||!Id.IsValid()||D.MtlNo.IsEmpty()||!LexTryParseString(D.SlabFrameNo,*N)||D.SlabFrameNo<0||
			!LexTryParseString(D.SlabElapsedSec,*T)||!FMath::IsFinite(D.SlabElapsedSec)||D.SlabElapsedSec<0)
		{ D.ErrorCode=TEXT("slab-context-invalid"); D.Message=TEXT("Slab 연계 헤더 오류: UUID, mtl_no, frame_no, elapsed_sec를 확인하십시오."); return false; }
	}
	if(const FString* S=Input.Find(TEXT("x-session-segment"))) if(!LexTryParseString(D.Segment,**S)||D.Segment<0)
	{ D.ErrorCode=TEXT("slab-context-invalid"); D.Message=TEXT("Slab 구간 번호 오류"); return false; }
	return true;
}
