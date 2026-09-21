#include "VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarExportComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraPayloadCodec.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"
#include <atomic>

struct FSensorFileCancellation { std::atomic<bool> Cancelled{false}; };
struct FVirtualSensorFileSaveRequest
{
	FVirtualSensorFileSaveStatus Status;
	TWeakObjectPtr<AVirtualSensorActorBase> Actor;
	uint64 Revision=0;
	double StartedSeconds=0;
	int64 BaselineFrame=-1;
	int64 RequestedUnixNs=0;
	FString CaptureDirectory;
	FVirtualSensorFrameEnvelope Lidar;
	FVirtualSensorStreamConfig ExportConfig;
	TFunction<FString()> LidarJsonEncoder;
	TSharedPtr<const FVirtualCameraLocalSaveFrame,ESPMode::ThreadSafe> Camera;
	TSharedPtr<const FString,ESPMode::ThreadSafe> CameraJson;
	TSharedPtr<FSensorFileCancellation,ESPMode::ThreadSafe> Cancellation=MakeShared<FSensorFileCancellation,ESPMode::ThreadSafe>();
	bool ReadyToWrite=false;
	bool Writing=false;
};
namespace
{
bool PointFormat(EVirtualSensorExportKind Kind,EVirtualPointCloudStreamFormat& Format)
{
	switch(Kind)
	{
	case EVirtualSensorExportKind::PointCloudCsv:Format=EVirtualPointCloudStreamFormat::CSV;return true;
	case EVirtualSensorExportKind::PointCloudJsonLines:Format=EVirtualPointCloudStreamFormat::JSONL;return true;
	case EVirtualSensorExportKind::PointCloudPcd:Format=EVirtualPointCloudStreamFormat::PCD;return true;
	case EVirtualSensorExportKind::PointCloudLas:Format=EVirtualPointCloudStreamFormat::LAS;return true;
	case EVirtualSensorExportKind::PointCloudLaz:Format=EVirtualPointCloudStreamFormat::LAZ;return true;
	default:return false;
	}
}
FString FullPath(FString Path) { Path=FPaths::ConvertRelativePathToFull(Path);FPaths::NormalizeDirectoryName(Path);FPaths::CollapseRelativeDirectories(Path);return Path; }
FString SafeId(const FString& Id) { FString Value=FPaths::MakeValidFileName(Id,TEXT('_'));return Value.IsEmpty()||Value==TEXT(".")||Value==TEXT("..")?TEXT("Sensor"):Value; }
FDateTime UnixNsDate(int64 Ns) {return Ns>0?FDateTime(1970,1,1)+FTimespan(Ns/100):FDateTime::UtcNow();}
void RemoveFiles(TArray<FVirtualSensorExportResult> Results)
{
	Async(EAsyncExecution::ThreadPool,[Results=MoveTemp(Results)](){for(const auto& R:Results)if(!R.AbsolutePath.IsEmpty())IFileManager::Get().Delete(*R.AbsolutePath,false,true);});
}
struct FFileWriteInput
{
	FString RequestId,SensorId,CaptureDirectory;
	FVirtualSensorCaptureSelection Selection;
	EVirtualSensorKind Kind=EVirtualSensorKind::Lidar;
	FVirtualSensorFrameEnvelope Lidar;
	FVirtualSensorStreamConfig ExportConfig;
	TFunction<FString()> JsonEncoder;
	TSharedPtr<const FVirtualCameraLocalSaveFrame,ESPMode::ThreadSafe> Camera;
	TSharedPtr<const FString,ESPMode::ThreadSafe> CameraJson;
	TSharedPtr<FSensorFileCancellation,ESPMode::ThreadSafe> Cancellation;
};
TArray<FVirtualSensorExportResult> WriteFiles(const FFileWriteInput& Input,FString& Error)
{
	TArray<FVirtualSensorExportResult> Results;
	const FString Root=Input.CaptureDirectory.IsEmpty()?FullPath(FPaths::ProjectSavedDir()/TEXT("SensorCaptures")/SafeId(Input.SensorId)):Input.CaptureDirectory;
	const bool Timed=!Input.CaptureDirectory.IsEmpty();
	const FString Prefix=FString::Printf(TEXT("%s_%s_%s"),*SafeId(Input.SensorId),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),*Input.RequestId);
	auto Save=[&](const uint8* Data,int64 Num,const FString& Extension,const FString& Subdir,EVirtualSensorExportKind Kind)->bool
	{
		if(Input.Cancellation->Cancelled){Error=TEXT("저장 요청이 취소되었습니다.");return false;}
		const FString Directory=Root/Subdir;const FString Final=Directory/(Prefix+TEXT(".")+Extension);const FString Temp=Final+TEXT(".pending");
		if(!IFileManager::Get().MakeDirectory(*Directory,true)) {Error=TEXT("저장 폴더를 만들 수 없습니다.");return false;}
		if(!Data||Num<=0||Num>MAX_int32||!FFileHelper::SaveArrayToFile(TArrayView<const uint8>(Data,static_cast<int32>(Num)),*Temp)) {IFileManager::Get().Delete(*Temp,false,true);Error=TEXT("파일 쓰기에 실패했거나 단일 파일 2GiB 한도를 초과했습니다.");return false;}
		if(Input.Cancellation->Cancelled||!IFileManager::Get().Move(*Final,*Temp,false,true,false,true)) {IFileManager::Get().Delete(*Temp,false,true);Error=TEXT("저장 취소 또는 파일 확정에 실패했습니다.");return false;}
		FVirtualSensorExportResult R;R.Kind=Kind;R.SensorId=Input.SensorId;R.bSucceeded=true;R.AbsolutePath=Final;R.FileSizeBytes=IFileManager::Get().FileSize(*Final);R.TimestampUtc=FDateTime::UtcNow();R.Message=Extension.ToUpper()+TEXT(" 파일 저장 완료");Results.Add(MoveTemp(R));return true;
	};
	auto SaveText=[&](const FString& Text,const FString& Extension,const FString& Subdir,EVirtualSensorExportKind Kind)->bool
	{FTCHARToUTF8 Utf8(*Text);return Save(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length(),Extension,Subdir,Kind);};
	if(Input.Kind==EVirtualSensorKind::Camera)
	{
		const auto& Camera=Input.Camera;
		if(Input.CameraJson&&Input.Selection.bCameraPayload&&!Input.Selection.bCameraImage)
			SaveText(*Input.CameraJson,TEXT("json"),Timed?TEXT("Camera"):TEXT("ServerPayload"),EVirtualSensorExportKind::ServerPayload);
		else if(!Camera||!Camera->Jpeg||Camera->Jpeg->IsEmpty())Error=TEXT("고정된 Camera JPEG 프레임이 없습니다.");
		else
		{
			if(Input.Selection.bCameraImage&&!Save(Camera->Jpeg->GetData(),Camera->Jpeg->Num(),TEXT("jpg"),TEXT("Camera"),EVirtualSensorExportKind::CameraJpeg)) {}
			if(Error.IsEmpty()&&Input.Selection.bCameraPayload)
			{
				const FString Json=FVirtualCameraPayloadCodec::EncodeJpegBase64(Camera->Payload,FBase64::Encode(Camera->Jpeg->GetData(),Camera->Jpeg->Num()),Camera->Jpeg->Num());
				SaveText(Json,TEXT("json"),Timed?TEXT("Camera"):TEXT("ServerPayload"),EVirtualSensorExportKind::ServerPayload);
			}
		}
	}
	else
	{
		if(Input.Selection.bLidarPayload) SaveText(Input.JsonEncoder?Input.JsonEncoder():FString(),TEXT("json"),Timed?TEXT("Lidar"):TEXT("ServerPayload"),EVirtualSensorExportKind::ServerPayload);
		if(Error.IsEmpty()&&Input.Selection.bPointCloud)
		{
			FString Extension;TArray<uint8> Bytes;int32 Count=0;
			if(UVirtualLidarExportComponent::SerializeImmutableFrame(Input.Lidar,Input.ExportConfig,Extension,Bytes,Count,Error))
				Save(Bytes.GetData(),Bytes.Num(),Extension,Timed?TEXT("Lidar"):TEXT("PointCloud"),Input.Selection.PointCloudFormat);
		}
	}
	if(Input.Cancellation->Cancelled&&Error.IsEmpty())Error=TEXT("저장 요청이 취소되었습니다.");
	if(!Error.IsEmpty()) {for(const auto& R:Results)IFileManager::Get().Delete(*R.AbsolutePath,false,true);Results.Reset();}
	return Results;
}
}

uint64 UVirtualSensorFileSaveSubsystem::GetSensorSaveRevision(AVirtualSensorActorBase* Sensor)
{
	if(!IsValid(Sensor)) return 0;
	FVirtualSensorEditableState State;if(!Sensor->ReadEditableState(State))return 0;
	// Natural mounting/Crane motion and display/output policies do not invalidate an immutable acquisition.
	State.ActorTransform=FTransform::Identity;State.PersistentActorTag=NAME_None;
	State.PreviewPointStride=2;State.MaxPreviewPoints=3000;State.bPreviewHitOnly=true;
	State.ServerPayloadStride=1;State.MaxServerPayloadPoints=0;State.bIncludeMissPointsInServerPayload=false;
	State.bExportCsvOnScan=false;State.bExportJsonLinesOnScan=false;State.bExportPcdOnScan=false;
	State.CameraCaptureMode=EVirtualCameraCaptureMode::PreviewOnly;
	TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FVirtualSensorEditableState::StaticStruct()->SerializeItem(Writer,&State,nullptr);
	uint32 Generation=0;
	if(const auto* Camera=Cast<AVirtualCameraSensorActor>(Sensor)) {if(Camera->CaptureComponent)Generation=Camera->CaptureComponent->GetFileCaptureRevision();}
	if(const auto* Lidar=Cast<AVirtualLidarSensorActor>(Sensor)) {if(Lidar->ScanComponent)Generation=Lidar->ScanComponent->GetFileCaptureRevision();}
	return(uint64(Generation)<<32)|FCrc::MemCrc32(Bytes.GetData(),Bytes.Num());
}
int64 UVirtualSensorFileSaveSubsystem::GetAvailableFrameId(AVirtualSensorActorBase* Sensor)
{
	if(const auto* Camera=Cast<AVirtualCameraSensorActor>(Sensor))
	{if(!Camera->CaptureComponent)return-1;const int64 Local=Camera->CaptureComponent->GetAvailableLocalFrameId();const auto External=Camera->CaptureComponent->GetExternalFileFrame();return Local>=0?Local:External&&External->HasJsonPayload()?External->FrameId:-1;}
	if(const auto* Lidar=Cast<AVirtualLidarSensorActor>(Sensor)) {const auto Frame=Lidar->ScanComponent?Lidar->ScanComponent->GetLastFrameSnapshot():nullptr;return Frame&&Frame->Points?Frame->FrameId:-1;}
	return -1;
}
bool UVirtualSensorFileSaveSubsystem::CanSaveCurrent(AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection)
{
	if(!IsValid(Sensor))return false;
	if(const auto* Camera=Cast<AVirtualCameraSensorActor>(Sensor))
	{
		if(!Camera->CaptureComponent||(!Selection.bCameraImage&&!Selection.bCameraPayload))return false;
		if(Camera->CaptureComponent->GetAvailableLocalFrameId()>=0)return true;
		const auto External=Camera->CaptureComponent->GetExternalFileFrame();return!Selection.bCameraImage&&Selection.bCameraPayload&&External&&External->HasJsonPayload();
	}
	return (Selection.bLidarPayload||Selection.bPointCloud)&&GetAvailableFrameId(Sensor)>=0;
}
bool UVirtualSensorFileSaveSubsystem::HasPendingSave(AVirtualSensorActorBase* Sensor) const
{
	if(!IsValid(Sensor))return false;
	for(const auto& P:Requests)if(!P.Value->Status.IsTerminal()&&(P.Value->Actor==Sensor||P.Value->Status.SensorId==Sensor->GetSensorId()))return true;
	return false;
}
bool UVirtualSensorFileSaveSubsystem::CanAcceptSave(AVirtualSensorActorBase* Sensor) const
{
	if(bShuttingDown||!IsValid(Sensor)||Sensor->GetWorld()!=GetWorld()||HasPendingSave(Sensor))return false;
	int32 Pending=0;for(const auto& R:Requests)Pending+=!R.Value->Status.IsTerminal();return Pending<8;
}
FString UVirtualSensorFileSaveSubsystem::RequestSave(EVirtualSensorFileSaveMode Mode,AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection)
{return RequestSaveInternal(Mode,Sensor,Selection,FString());}
FString UVirtualSensorFileSaveSubsystem::RequestSaveInCaptureSession(AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection,const FString& SessionDirectory)
{return RequestSaveInternal(EVirtualSensorFileSaveMode::CurrentFrame,Sensor,Selection,SessionDirectory);}
FString UVirtualSensorFileSaveSubsystem::RequestSaveInternal(EVirtualSensorFileSaveMode Mode,AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection,const FString& SessionDirectory)
{
	check(IsInGameThread());
	const bool Duplicate=HasPendingSave(Sensor);
	auto R=MakeShared<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>();R->Status.RequestId=FGuid::NewGuid().ToString(EGuidFormats::Digits);R->Status.Mode=Mode;R->Status.Selection=Selection;R->Status.RequestedUtc=FDateTime::UtcNow();R->Actor=Sensor;
	R->StartedSeconds=FPlatformTime::Seconds();R->RequestedUnixNs=(R->Status.RequestedUtc-FDateTime(1970,1,1)).GetTicks()*100;
	const FString Id=R->Status.RequestId;Requests.Add(Id,R);Order.Insert(Id,0);TrimHistory();
	auto Reject=[&](const FString& Why){SetState(R,EVirtualSensorFileSaveState::Failed,Why);return Id;};
	if(bShuttingDown||!IsValid(Sensor)||Sensor->GetWorld()!=GetWorld()||Sensor->IsActorBeingDestroyed())return Reject(TEXT("같은 실행 월드의 유효한 센서를 선택하세요."));
	R->Status.SensorId=Sensor->GetSensorId();R->Status.SensorKind=Sensor->GetSensorKind();R->Revision=GetSensorSaveRevision(Sensor);R->BaselineFrame=GetAvailableFrameId(Sensor);
	if(Duplicate)return Reject(TEXT("이 센서의 파일 저장 요청이 이미 진행 중입니다."));
	int32 Pending=0;for(const auto& P:Requests)Pending+=!P.Value->Status.IsTerminal();if(Pending>=8)return Reject(TEXT("진행 중인 저장 요청이 많습니다. 완료 후 다시 요청하세요."));
	if(R->Status.SensorId.IsEmpty())return Reject(TEXT("SensorId가 비어 있습니다."));
	const bool Camera=R->Status.SensorKind==EVirtualSensorKind::Camera;
	if(Camera?(!Selection.bCameraImage&&!Selection.bCameraPayload):(!Selection.bLidarPayload&&!Selection.bPointCloud))return Reject(TEXT("선택 센서에서 저장할 출력 항목을 선택하세요."));
	if(!Camera&&Selection.bPointCloud&&!PointFormat(Selection.PointCloudFormat,R->ExportConfig.PointCloudFormat))return Reject(TEXT("지원하는 Point Cloud 저장 형식을 선택하세요."));
	if(!SessionDirectory.IsEmpty())
	{
		const FString Allowed=FullPath(FPaths::ProjectSavedDir()/TEXT("SensorCaptures/LocalTimedCapture"))+TEXT("/");R->CaptureDirectory=FullPath(SessionDirectory);
		if(!R->CaptureDirectory.StartsWith(Allowed,ESearchCase::IgnoreCase))return Reject(TEXT("주기 캡처 경로는 LocalTimedCapture 하위여야 합니다."));
	}
	if(Mode==EVirtualSensorFileSaveMode::NewFrame&&!Sensor->IsSensorRunning())return Reject(TEXT("센서 측정을 시작한 뒤 새 프레임 저장을 요청하세요."));
	if(Mode==EVirtualSensorFileSaveMode::CurrentFrame&&!CanSaveCurrent(Sensor,Selection))return Reject(TEXT("선택 출력으로 저장할 현재 프레임이 없습니다. 외부 Camera JSON만 있는 경우 JPEG를 해제하세요."));
	SetState(R,EVirtualSensorFileSaveState::Accepted,TEXT("파일 저장 요청 접수"));
	if(R->Status.IsTerminal()||bShuttingDown)return Id;
	FString ChangedDuringAdmission;
	if(!IsRequestCurrent(R,ChangedDuringAdmission)){SetState(R,EVirtualSensorFileSaveState::Cancelled,ChangedDuringAdmission);return Id;}
	SetState(R,EVirtualSensorFileSaveState::Waiting,Mode==EVirtualSensorFileSaveMode::NewFrame?TEXT("새 측정 프레임 대기 중 (최대 2초)"):TEXT("현재 프레임을 고정하는 중"));
	if(R->Status.IsTerminal()||bShuttingDown)return Id;
	if(!IsRequestCurrent(R,ChangedDuringAdmission)){SetState(R,EVirtualSensorFileSaveState::Cancelled,ChangedDuringAdmission);return Id;}
	if(Camera)
	{
		auto* Actor=Cast<AVirtualCameraSensorActor>(Sensor);FString Error;TWeakObjectPtr<UVirtualSensorFileSaveSubsystem> Weak(this);
		const auto External=Actor&&Actor->CaptureComponent?Actor->CaptureComponent->GetExternalFileFrame():nullptr;
		if(Mode==EVirtualSensorFileSaveMode::CurrentFrame&&!Selection.bCameraImage&&Selection.bCameraPayload&&External&&External->HasJsonPayload())
		{
			R->CameraJson=External->JsonPayload;R->Status.FrameId=External->FrameId;R->Status.AcquisitionUtc=External->TimestampUtc;R->ReadyToWrite=true;
			SetState(R,EVirtualSensorFileSaveState::Saving,TEXT("검증된 외부 Camera JSON 원문을 저장 중"));QueueWrite(R);return Id;
		}
		if(!Actor||!Actor->CaptureComponent||!Actor->CaptureComponent->RequestLocalFileFrame(Mode==EVirtualSensorFileSaveMode::NewFrame,
			[Weak,Id](auto Frame,const FString& Failure){if(Weak.IsValid())if(const auto* Entry=Weak->Requests.Find(Id))Weak->StartCameraSave(*Entry,Frame,Failure);},Error))
			SetState(R,EVirtualSensorFileSaveState::Failed,Error.IsEmpty()?TEXT("Camera 파일 프레임을 요청할 수 없습니다."):Error);
	}
	else if(Mode==EVirtualSensorFileSaveMode::CurrentFrame)StartLidarSave(R);
	else if(auto* Lidar=Cast<AVirtualLidarSensorActor>(Sensor))Lidar->ScanComponent->RequestImmediateScheduledScan();
	return Id;
}
bool UVirtualSensorFileSaveSubsystem::IsRequestCurrent(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& R,FString& Why) const
{
	if(bShuttingDown||!R->Actor.IsValid()||R->Actor->IsActorBeingDestroyed()||R->Actor->GetWorld()!=GetWorld()) {Why=TEXT("센서 삭제 또는 실행 종료로 저장을 취소했습니다.");return false;}
	if(const auto* Camera=Cast<AVirtualCameraSensorActor>(R->Actor.Get())) if(!IsValid(Camera->CaptureComponent)||!Camera->CaptureComponent->IsRegistered()) {Why=TEXT("Camera 캡처 컴포넌트가 종료되었습니다.");return false;}
	if(const auto* Lidar=Cast<AVirtualLidarSensorActor>(R->Actor.Get())) if(!IsValid(Lidar->ScanComponent)||!Lidar->ScanComponent->IsRegistered()) {Why=TEXT("LiDAR 측정 컴포넌트가 종료되었습니다.");return false;}
	if(R->Actor->GetSensorId()!=R->Status.SensorId||GetSensorSaveRevision(R->Actor.Get())!=R->Revision) {Why=TEXT("센서 측정 설정 변경으로 이전 저장 요청을 취소했습니다.");return false;}
	return true;
}
void UVirtualSensorFileSaveSubsystem::SetState(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& R,EVirtualSensorFileSaveState State,const FString& Message)
{
	R->Status.State=State;R->Status.Message=Message;
	if(R->Status.IsTerminal())
	{
		R->Status.CompletedUtc=FDateTime::UtcNow();if(State!=EVirtualSensorFileSaveState::Succeeded)R->Cancellation->Cancelled=true;
		R->Camera.Reset();R->CameraJson.Reset();R->Lidar=FVirtualSensorFrameEnvelope();R->LidarJsonEncoder=nullptr;R->ReadyToWrite=false;
	}
	PendingNotifications.Add(R->Status);
	if(bPublishingNotifications)return;
	TGuardValue<bool> Publishing(bPublishingNotifications,true);
	while(!PendingNotifications.IsEmpty())
	{
		const FVirtualSensorFileSaveStatus Update=MoveTemp(PendingNotifications[0]);PendingNotifications.RemoveAt(0,1,false);
		OnRequestUpdatedNative.Broadcast(Update);OnRequestUpdated.Broadcast(Update);
	}
}
void UVirtualSensorFileSaveSubsystem::StartLidarSave(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& R)
{
	auto* Lidar=Cast<AVirtualLidarSensorActor>(R->Actor.Get());if(!Lidar||!Lidar->ScanComponent)return;
	const auto Frame=Lidar->ScanComponent->GetLastFrameSnapshot();if(!Frame||!Frame->Points)return;
	if(R->Status.Mode==EVirtualSensorFileSaveMode::NewFrame&&(Frame->FrameId<=R->BaselineFrame||Frame->AcquisitionStartUnixNanoseconds<R->RequestedUnixNs))return;
	R->Lidar.SensorId=R->Status.SensorId;R->Lidar.SensorKind=EVirtualSensorKind::Lidar;R->Lidar.FrameId=Frame->FrameId;R->Lidar.TimestampUtc=UnixNsDate(Frame->AcquisitionStartUnixNanoseconds);R->Lidar.PointSnapshot=Frame->Points;R->Lidar.LidarFrameSnapshot=Frame;R->Lidar.SlabContext=Frame->SlabContext;
	R->LidarJsonEncoder=Lidar->ScanComponent->CreateLocalFilePayloadEncoder(Frame);
	R->ExportConfig.StreamKind=EVirtualSensorStreamKind::PointCloud;R->ExportConfig.PcdDataMode=EVirtualPcdDataMode::Binary;
	R->ExportConfig.LazCompressorPath=Lidar->ScanComponent->bUseExternalLazCompressor?Lidar->ScanComponent->ExternalLazCompressorPath:FString();R->ExportConfig.LazCompressorArguments=Lidar->ScanComponent->ExternalLazCompressorArguments;
	R->Status.FrameId=Frame->FrameId;R->Status.AcquisitionUtc=R->Lidar.TimestampUtc;R->ReadyToWrite=true;SetState(R,EVirtualSensorFileSaveState::Saving,TEXT("고정된 LiDAR 프레임을 파일로 저장 중"));QueueWrite(R);
}
void UVirtualSensorFileSaveSubsystem::StartCameraSave(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& R,TSharedPtr<const FVirtualCameraLocalSaveFrame,ESPMode::ThreadSafe> Frame,const FString& Error)
{
	if(R->Status.IsTerminal())return;FString Why;
	if(!IsRequestCurrent(R,Why)){SetState(R,EVirtualSensorFileSaveState::Cancelled,Why);return;}
	if(!Error.IsEmpty()||!Frame||!Frame->Jpeg||Frame->Jpeg->IsEmpty()){SetState(R,EVirtualSensorFileSaveState::Failed,Error.IsEmpty()?TEXT("Camera 프레임 생성에 실패했습니다."):Error);return;}
	if(Frame->Payload.SensorId!=R->Status.SensorId){SetState(R,EVirtualSensorFileSaveState::Failed,TEXT("Camera 프레임의 SensorId가 요청한 센서와 다릅니다."));return;}
	if(R->Status.Mode==EVirtualSensorFileSaveMode::NewFrame&&(Frame->Payload.FrameId<=R->BaselineFrame||Frame->AcquisitionSeconds<R->StartedSeconds)) {SetState(R,EVirtualSensorFileSaveState::Failed,TEXT("요청 이후의 새 Camera 프레임이 아닙니다."));return;}
	R->Camera=Frame;R->Status.FrameId=Frame->Payload.FrameId;R->Status.AcquisitionUtc=Frame->Payload.TimestampUtc;R->ReadyToWrite=true;SetState(R,EVirtualSensorFileSaveState::Saving,TEXT("고정된 Camera 프레임을 파일로 저장 중"));QueueWrite(R);
}
void UVirtualSensorFileSaveSubsystem::QueueWrite(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& R)
{
	if(R->Writing||!R->ReadyToWrite||R->Status.IsTerminal()||ActiveWrites>=2)return;
	R->Writing=true;++ActiveWrites;
	FFileWriteInput Input;Input.RequestId=R->Status.RequestId;Input.SensorId=R->Status.SensorId;Input.Kind=R->Status.SensorKind;Input.Selection=R->Status.Selection;Input.CaptureDirectory=R->CaptureDirectory;Input.Lidar=R->Lidar;Input.ExportConfig=R->ExportConfig;Input.JsonEncoder=R->LidarJsonEncoder;Input.Camera=R->Camera;Input.CameraJson=R->CameraJson;Input.Cancellation=R->Cancellation;
	TWeakObjectPtr<UVirtualSensorFileSaveSubsystem> Weak(this);
	Async(EAsyncExecution::ThreadPool,[Weak,Input=MoveTemp(Input)]() mutable
	{
		FString Error;auto Results=WriteFiles(Input,Error);
		if(Input.Cancellation->Cancelled) {for(const auto& F:Results)IFileManager::Get().Delete(*F.AbsolutePath,false,true);Results.Reset();}
		AsyncTask(ENamedThreads::GameThread,[Weak,Id=Input.RequestId,Results=MoveTemp(Results),Error=MoveTemp(Error)]()mutable
		{if(Weak.IsValid())Weak->FinishWrite(Id,MoveTemp(Results),Error);else RemoveFiles(MoveTemp(Results));});
	});
}
void UVirtualSensorFileSaveSubsystem::FinishWrite(const FString& Id,TArray<FVirtualSensorExportResult> Results,const FString& Error)
{
	ActiveWrites=FMath::Max(0,ActiveWrites-1);const auto* Entry=Requests.Find(Id);if(!Entry){RemoveFiles(MoveTemp(Results));return;}const auto R=*Entry;R->Writing=false;FString Why;
	if(R->Status.IsTerminal()||!IsRequestCurrent(R,Why)) {RemoveFiles(MoveTemp(Results));if(!R->Status.IsTerminal())SetState(R,EVirtualSensorFileSaveState::Cancelled,Why);return;}
	if(!Error.IsEmpty()||Results.IsEmpty()) {SetState(R,EVirtualSensorFileSaveState::Failed,Error.IsEmpty()?TEXT("저장된 파일이 없습니다."):Error);return;}
	R->Status.FileResults=MoveTemp(Results);SetState(R,EVirtualSensorFileSaveState::Succeeded,TEXT("선택한 파일 저장 완료"));
}
void UVirtualSensorFileSaveSubsystem::Tick(float)
{
	if(bShuttingDown)return;const auto Snapshot=Order;
	for(const auto& Id:Snapshot)
	{
		const auto* Entry=Requests.Find(Id);if(!Entry)continue;const auto R=*Entry;if(R->Status.IsTerminal())continue;FString Why;
		if(!IsRequestCurrent(R,Why))
		{if(auto* C=Cast<AVirtualCameraSensorActor>(R->Actor.Get()))if(C->CaptureComponent)C->CaptureComponent->CancelLocalFileFrame();SetState(R,EVirtualSensorFileSaveState::Cancelled,Why);continue;}
		if(R->Status.State==EVirtualSensorFileSaveState::Waiting)
		{
			const bool Fresh=R->Status.Mode==EVirtualSensorFileSaveMode::NewFrame;
			if(FPlatformTime::Seconds()-R->StartedSeconds>=(Fresh?2.0:10.0)){if(auto* C=Cast<AVirtualCameraSensorActor>(R->Actor.Get()))if(C->CaptureComponent)C->CaptureComponent->CancelLocalFileFrame();SetState(R,EVirtualSensorFileSaveState::Failed,Fresh?TEXT("2초 안에 저장 가능한 새 프레임을 얻지 못했습니다. 이전 프레임은 저장하지 않았습니다."):TEXT("현재 프레임 읽기가 10초 안에 완료되지 않았습니다."));continue;}
			if(R->Status.SensorKind==EVirtualSensorKind::Camera){if(auto* C=Cast<AVirtualCameraSensorActor>(R->Actor.Get()))C->CaptureComponent->PollLocalFileFrame();}
			else StartLidarSave(R);
		}
		QueueWrite(R);
	}
}
bool UVirtualSensorFileSaveSubsystem::CancelRequest(const FString& Id)
{
	const auto* Entry=Requests.Find(Id);if(!Entry||(*Entry)->Status.IsTerminal())return false;const auto R=*Entry;
	if(auto* C=Cast<AVirtualCameraSensorActor>(R->Actor.Get()))if(C->CaptureComponent)C->CaptureComponent->CancelLocalFileFrame();
	SetState(R,EVirtualSensorFileSaveState::Cancelled,TEXT("파일 저장 요청이 취소되었습니다."));return true;
}
FVirtualSensorFileSaveStatus UVirtualSensorFileSaveSubsystem::GetRequestStatus(const FString& Id) const
{if(const auto* Entry=Requests.Find(Id))return(*Entry)->Status;FVirtualSensorFileSaveStatus S;S.RequestId=Id;S.Message=TEXT("저장 요청을 찾을 수 없습니다.");return S;}
TArray<FVirtualSensorFileSaveStatus> UVirtualSensorFileSaveSubsystem::GetRecentRequests() const
{TArray<FVirtualSensorFileSaveStatus> Result;for(const auto& Id:Order)if(const auto* R=Requests.Find(Id))Result.Add((*R)->Status);return Result;}
void UVirtualSensorFileSaveSubsystem::TrimHistory()
{for(int32 I=Order.Num()-1;I>=0&&Order.Num()>32;--I){const auto* R=Requests.Find(Order[I]);if(!R||((*R)->Status.IsTerminal()&&!(*R)->Writing)){Requests.Remove(Order[I]);Order.RemoveAt(I);}}}
TStatId UVirtualSensorFileSaveSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(UVirtualSensorFileSaveSubsystem,STATGROUP_Tickables);}
void UVirtualSensorFileSaveSubsystem::Deinitialize()
{if(bShuttingDown)return;bShuttingDown=true;const auto Ids=Order;for(const auto& Id:Ids)CancelRequest(Id);OnRequestUpdatedNative.Clear();OnRequestUpdated.Clear();Super::Deinitialize();}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorFileSaveStateTest,"MA0T10.SensorFiles.RequestIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorFileSaveStateTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* A=World->SpawnActor<AVirtualLidarSensorActor>();auto* B=World->SpawnActor<AVirtualLidarSensorActor>();
	A->ScanComponent->SensorId=TEXT("File-A");B->ScanComponent->SensorId=TEXT("File-B");auto* Service=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>();
	FVirtualSensorCaptureSelection Selection;Selection.bLidarPayload=false;Selection.PointCloudFormat=EVirtualSensorExportKind::PointCloudPcd;
	const FString Stopped=Service->RequestSave(EVirtualSensorFileSaveMode::NewFrame,A,Selection);
	TestFalse(TEXT("rejected request still has identity"),Stopped.IsEmpty());TestEqual(TEXT("stopped NewFrame fails explicitly"),Service->GetRequestStatus(Stopped).State,EVirtualSensorFileSaveState::Failed);TestFalse(TEXT("request does not start sensor"),A->IsSensorRunning());
	const FString Empty=Service->RequestSave(EVirtualSensorFileSaveMode::CurrentFrame,A,Selection);TestEqual(TEXT("missing current frame does not trigger sync acquisition"),Service->GetRequestStatus(Empty).State,EVirtualSensorFileSaveState::Failed);
	TestEqual(TEXT("missing frame sentinel"),UVirtualSensorFileSaveSubsystem::GetAvailableFrameId(A),int64(-1));
	auto MakeWaiting=[&](AVirtualSensorActorBase* Actor)
	{
		auto R=MakeShared<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>();R->Actor=Actor;R->Revision=UVirtualSensorFileSaveSubsystem::GetSensorSaveRevision(Actor);R->StartedSeconds=FPlatformTime::Seconds();R->Status.RequestId=FGuid::NewGuid().ToString(EGuidFormats::Digits);R->Status.SensorId=Actor->GetSensorId();R->Status.SensorKind=EVirtualSensorKind::Lidar;R->Status.Mode=EVirtualSensorFileSaveMode::NewFrame;R->Status.Selection=Selection;R->Status.State=EVirtualSensorFileSaveState::Waiting;Service->Requests.Add(R->Status.RequestId,R);Service->Order.Add(R->Status.RequestId);return R;
	};
	const uint64 Rev=UVirtualSensorFileSaveSubsystem::GetSensorSaveRevision(A);TestEqual(TEXT("revision hash is stable without edits"),UVirtualSensorFileSaveSubsystem::GetSensorSaveRevision(A),Rev);
	auto Waiting=MakeWaiting(A);AVirtualSensorActorBase* UiSelection=A;UiSelection=B;
	Selection.PointCloudFormat=EVirtualSensorExportKind::PointCloudCsv;Selection.bPointCloud=false;
	TestTrue(TEXT("changing selected UI sensor cannot retarget request"),Waiting->Actor.Get()==A&&UiSelection==B);
	TestEqual(TEXT("per-request format frozen"),Waiting->Status.Selection.PointCloudFormat,EVirtualSensorExportKind::PointCloudPcd);TestTrue(TEXT("per-request output flags frozen"),Waiting->Status.Selection.bPointCloud);
	const FString Duplicate=Service->RequestSave(EVirtualSensorFileSaveMode::CurrentFrame,A,Waiting->Status.Selection);TestEqual(TEXT("same sensor duplicate rejected"),Service->GetRequestStatus(Duplicate).State,EVirtualSensorFileSaveState::Failed);
	Waiting->StartedSeconds-=2.01;Service->Tick(0);TestEqual(TEXT("2s timeout is failure, never stale fallback"),Waiting->Status.State,EVirtualSensorFileSaveState::Failed);TestTrue(TEXT("timeout produces no files"),Waiting->Status.FileResults.IsEmpty());
	Selection.bPointCloud=true;auto Changed=MakeWaiting(A);A->SetActorLocation(FVector(10,0,0));Service->Tick(0);
	TestEqual(TEXT("natural sensor motion does not cancel immutable save"),Changed->Status.State,EVirtualSensorFileSaveState::Waiting);
	A->ScanComponent->MaxDistance+=10;Service->Tick(0);
	TestEqual(TEXT("revision change cancels old request"),Changed->Status.State,EVirtualSensorFileSaveState::Cancelled);TestTrue(TEXT("worker cancellation is visible"),Changed->Cancellation->Cancelled.load());
	auto Deleted=MakeWaiting(B);B->Destroy();Service->Tick(0);TestEqual(TEXT("actor deletion cancels"),Deleted->Status.State,EVirtualSensorFileSaveState::Cancelled);
	auto Hidden=MakeWaiting(A);A->SetActorHiddenInGame(true);Service->Tick(0);TestEqual(TEXT("visibility is not request ownership"),Hidden->Status.State,EVirtualSensorFileSaveState::Waiting);
	Service->Deinitialize();TestEqual(TEXT("world shutdown cancels pending request"),Hidden->Status.State,EVirtualSensorFileSaveState::Cancelled);A->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorFileSaveWorkerTest,"MA0T10.SensorFiles.ImmutableWriters",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorFileSaveWorkerTest::RunTest(const FString&)
{
	FFileWriteInput Input;Input.RequestId=FGuid::NewGuid().ToString(EGuidFormats::Digits);Input.SensorId=TEXT("Automation-Snapshot");Input.Cancellation=MakeShared<FSensorFileCancellation,ESPMode::ThreadSafe>();
	Input.CaptureDirectory=FullPath(FPaths::ProjectSavedDir()/TEXT("SensorCaptures/LocalTimedCapture/Automation")/Input.RequestId);
	Input.Selection.bLidarPayload=false;Input.Selection.bPointCloud=true;Input.Selection.PointCloudFormat=EVirtualSensorExportKind::PointCloudPcd;Input.ExportConfig.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;Input.ExportConfig.PcdDataMode=EVirtualPcdDataMode::Binary;
	auto Points=MakeShared<TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>();FVirtualLidarPoint P;P.bHit=true;P.WorldLocation=FVector(100,20,30);P.SensorLocalPositionMeters=FVector(1,-.2,.3);P.Distance=106;P.RawIntensity=100;Points->Add(P);
	auto Frame=MakeShared<FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe>();Frame->Points=Points;Frame->FrameId=17;Frame->HorizontalSamples=1;Frame->VerticalChannels=1;
	Frame->SlabContext.bEligible=true;Frame->SlabContext.RunId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);Frame->SlabContext.MtlNo=TEXT("TEST-MTL");Frame->SlabContext.SlabFrameNo=580;Frame->SlabContext.ElapsedSec=29;
	Input.Lidar.SensorId=Input.SensorId;Input.Lidar.SensorKind=EVirtualSensorKind::Lidar;Input.Lidar.FrameId=17;Input.Lidar.TimestampUtc=FDateTime::UtcNow();Input.Lidar.PointSnapshot=Points;Input.Lidar.LidarFrameSnapshot=Frame;Input.Lidar.SlabContext=Frame->SlabContext;
	// The exact production writer is run on a worker; only immutable values cross the boundary.
	auto Future=Async(EAsyncExecution::ThreadPool,[Input]()mutable{FString Error;auto Files=WriteFiles(Input,Error);return TPair<TArray<FVirtualSensorExportResult>,FString>(MoveTemp(Files),Error);});
	const auto Result=Future.Get();TestTrue(TEXT("immutable PCD write succeeds"),Result.Value.IsEmpty()&&Result.Key.Num()==1);
	if(Result.Key.Num()==1)
	{
		TArray<uint8> Bytes;TestTrue(TEXT("written file available"),FFileHelper::LoadFileToArray(Bytes,*Result.Key[0].AbsolutePath));
		const ANSICHAR Marker[]="DATA binary\n";int32 HeaderLength=0;
		for(int32 I=0;I+int32(sizeof(Marker))-1<=Bytes.Num();++I)if(FMemory::Memcmp(Bytes.GetData()+I,Marker,sizeof(Marker)-1)==0){HeaderLength=I+sizeof(Marker)-1;break;}
		const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),HeaderLength);const FString Header(Text.Length(),Text.Get());
		TestTrue(TEXT("PCD remains binary"),Header.Contains(TEXT("DATA binary")));TestTrue(TEXT("Slab material metadata survives snapshot writer"),Header.Contains(TEXT("TEST-MTL")));TestTrue(TEXT("sensor frame and Slab frame retained"),Header.Contains(TEXT("580")));
		TestTrue(TEXT("timed LiDAR folder retained"),Result.Key[0].AbsolutePath.Contains(TEXT("/Lidar/")));TestEqual(TEXT("file size is actual bytes"),Result.Key[0].FileSizeBytes,int64(Bytes.Num()));
		IFileManager::Get().Delete(*Result.Key[0].AbsolutePath,false,true);
	}
	Input.Cancellation->Cancelled=true;
	auto Cancelled=Async(EAsyncExecution::ThreadPool,[Input]()mutable{FString Error;auto Files=WriteFiles(Input,Error);return Files.Num();});TestEqual(TEXT("cancelled request cannot write output"),Cancelled.Get(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorExternalCameraFileTest,"MA0T10.SensorFiles.ExternalCameraJson",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorExternalCameraFileTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();
	FVirtualCameraPayloadSnapshot Descriptor;Descriptor.SensorId=TEXT("EXTERNAL-CAM");Descriptor.Manufacturer=TEXT("Fixture");Descriptor.Model=TEXT("JSONOnly");Descriptor.SimulationQuality=TEXT("Debug");Descriptor.FrameId=301;Descriptor.TimestampUtc=FDateTime(2026,9,21,1,2,3);Descriptor.Width=2;Descriptor.Height=2;Descriptor.HorizontalFov=87;Descriptor.VerticalFov=58;
	const TArray<uint8> Signature={0xff,0xd8,0xff,0xd9};
	const FString Original=FVirtualCameraPayloadCodec::EncodeJpegBase64(Descriptor,FBase64::Encode(Signature),Signature.Num());
	TestTrue(TEXT("existing external JSON validation accepts fixture"),Camera->CaptureComponent->InjectExternalJsonPayload(Original,false));
	FVirtualSensorCaptureSelection Selection;Selection.bCameraImage=false;Selection.bCameraPayload=true;
	TestTrue(TEXT("JSON-only current file is available without a render target"),UVirtualSensorFileSaveSubsystem::CanSaveCurrent(Camera,Selection));
	TestEqual(TEXT("external source frame identity preserved"),UVirtualSensorFileSaveSubsystem::GetAvailableFrameId(Camera),int64(301));
	Selection.bCameraImage=true;TestFalse(TEXT("external JSON cannot masquerade as an RT JPEG"),UVirtualSensorFileSaveSubsystem::CanSaveCurrent(Camera,Selection));Selection.bCameraImage=false;
	const auto External=Camera->CaptureComponent->GetExternalFileFrame();TestTrue(TEXT("validated immutable original held"),External&&External->JsonPayload&&*External->JsonPayload==Original);
	if(External)
	{
		TestEqual(TEXT("source acquisition UTC not replaced by local receive time"),External->TimestampUtc,Descriptor.TimestampUtc);
		FFileWriteInput Input;Input.RequestId=FGuid::NewGuid().ToString(EGuidFormats::Digits);Input.SensorId=Camera->GetSensorId();Input.Kind=EVirtualSensorKind::Camera;Input.Selection=Selection;Input.CameraJson=External->JsonPayload;Input.Cancellation=MakeShared<FSensorFileCancellation,ESPMode::ThreadSafe>();Input.CaptureDirectory=FullPath(FPaths::ProjectSavedDir()/TEXT("SensorCaptures/LocalTimedCapture/Automation")/Input.RequestId);
		auto Work=Async(EAsyncExecution::ThreadPool,[Input](){FString Error;return WriteFiles(Input,Error);});const auto Files=Work.Get();TestEqual(TEXT("external JSON writes one file"),Files.Num(),1);
		if(Files.Num()==1) {FString Saved;FFileHelper::LoadFileToString(Saved,*Files[0].AbsolutePath);TestEqual(TEXT("external JSON source is saved without regeneration"),Saved,Original);IFileManager::Get().Delete(*Files[0].AbsolutePath,false,true);}
	}
	Camera->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorFileSaveReentrantCancelTest,"MA0T10.SensorFiles.CancelDuringAdmission",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorFileSaveReentrantCancelTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Actor=World->SpawnActor<AVirtualLidarSensorActor>();Actor->StartSensor();
	auto* Files=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>();FVirtualSensorCaptureSelection Selection;Selection.bLidarPayload=false;
	for(auto CancelAt:{EVirtualSensorFileSaveState::Accepted,EVirtualSensorFileSaveState::Waiting})
	{
		TArray<EVirtualSensorFileSaveState> Observed;
		const auto Cancel=Files->OnRequestUpdatedNative.AddLambda([Files,CancelAt](const FVirtualSensorFileSaveStatus& S){if(S.State==CancelAt)Files->CancelRequest(S.RequestId);});
		const auto Observe=Files->OnRequestUpdatedNative.AddLambda([&Observed](const FVirtualSensorFileSaveStatus& S){Observed.Add(S.State);});
		const FString Id=Files->RequestSave(EVirtualSensorFileSaveMode::NewFrame,Actor,Selection);
		TestEqual(TEXT("admission callback cancellation remains terminal"),Files->GetRequestStatus(Id).State,EVirtualSensorFileSaveState::Cancelled);
		TestTrue(TEXT("native events preserve Accepted before Cancelled ordering"),!Observed.IsEmpty()&&Observed[0]==EVirtualSensorFileSaveState::Accepted&&Observed.Last()==EVirtualSensorFileSaveState::Cancelled);
		TestFalse(TEXT("cancelled admission does not remain busy"),Files->HasPendingSave(Actor));TestTrue(TEXT("cancelled admission has no file results"),Files->GetRequestStatus(Id).FileResults.IsEmpty());
		Files->OnRequestUpdatedNative.Remove(Cancel);Files->OnRequestUpdatedNative.Remove(Observe);
	}
	Actor->StopSensor();Actor->Destroy();return true;
}
#endif
