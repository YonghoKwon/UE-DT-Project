#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Editor/EditorPerformanceSettings.h"
#include "DynamicRHI.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"

namespace
{
bool ReadJpegDimensions(const TArray<uint8>& Bytes,FIntPoint& Size)
{
	if(Bytes.Num()<4||Bytes[0]!=0xff||Bytes[1]!=0xd8||Bytes[Bytes.Num()-2]!=0xff||Bytes.Last()!=0xd9)return false;
	int32 I=2;
	while(I+3<Bytes.Num())
	{
		if(Bytes[I++]!=0xff)return false;
		while(I<Bytes.Num()&&Bytes[I]==0xff)++I;
		if(I>=Bytes.Num())return false;
		const uint8 Marker=Bytes[I++];
		if(Marker==0xda||Marker==0xd9)return false;
		if(Marker==0x01||(Marker>=0xd0&&Marker<=0xd7))continue;
		if(I+1>=Bytes.Num())return false;
		const int32 Length=(Bytes[I]<<8)|Bytes[I+1];
		if(Length<2||I+Length>Bytes.Num())return false;
		const bool Sof=Marker>=0xc0&&Marker<=0xcf&&Marker!=0xc4&&Marker!=0xc8&&Marker!=0xcc;
		if(Sof){if(Length<8)return false;Size=FIntPoint((Bytes[I+5]<<8)|Bytes[I+6],(Bytes[I+3]<<8)|Bytes[I+4]);return Size.X>0&&Size.Y>0;}
		I+=Length;
	}
	return false;
}
TSharedPtr<FJsonObject> ParseJsonFile(const FString& Path)
{
	FString Text;TSharedPtr<FJsonObject> Object;
	if(FFileHelper::LoadFileToString(Text,*Path))FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object);
	return Object;
}
TSharedPtr<FJsonObject> ReadPcdMetadata(const TArray<uint8>& Bytes)
{
	const ANSICHAR Marker[]="DATA binary\n";int32 HeaderBytes=0;
	for(int32 I=0;I+int32(sizeof(Marker))-1<=FMath::Min(Bytes.Num(),65536);++I)
		if(FMemory::Memcmp(Bytes.GetData()+I,Marker,sizeof(Marker)-1)==0){HeaderBytes=I+sizeof(Marker)-1;break;}
	if(!HeaderBytes)return nullptr;
	const FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),HeaderBytes);
	const FString Header(Utf8.Length(),Utf8.Get());TArray<FString> Lines;Header.ParseIntoArrayLines(Lines);
	for(const auto& Line:Lines)if(Line.StartsWith(TEXT("# MA0T10_META ")))
	{TSharedPtr<FJsonObject> Meta;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line.Mid(14)),Meta);return Meta;}
	return nullptr;
}

class FSensorFileSaveRhiCommand final:public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Started=FPlatformTime::Seconds(),PeriodicStarted=0,StopStarted=0,CancelledAt=0;
	int32 Stage=0,MaxPendingForCamera=0;
	int64 BaselineCameraFrame=-1,PinnedLidarFrame=-1,PeriodicCameraFrame=-1,PeriodicEndFrame=-1;
	double PeriodicWorldStart=0,PeriodicWorldEnd=0;
	bool OldThrottle=false,OldMonitor=false,bReportWritten=false,bCreatedPanel=false;
	ESlateVisibility OldVisibility=ESlateVisibility::Collapsed;
	EVirtualSensorTransportMode OriginalTransportMode=EVirtualSensorTransportMode::LogOnly;
	TWeakObjectPtr<UWorld> TestWorld;
	TWeakObjectPtr<AVirtualCameraSensorActor> Camera;
	TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;
	TWeakObjectPtr<AVirtualSensorCoordinator> Coordinator;
	TWeakObjectPtr<UVirtualSensorCaptureExportPanelWidget> Panel;
	FString CameraId,LidarId,CameraRequest,LidarRequest,PeriodicId,CancelledRequest;
	FVirtualSensorFileSaveStatus CameraResult,LidarResult;
	FVirtualSensorPeriodicFileSaveStatus PeriodicResult;
	TSet<FString> SeenPeriodicRequests;
	TSet<int64> SavedPeriodicFrames;
	TArray<TSharedPtr<FJsonValue>> Evidence;
public:
	explicit FSensorFileSaveRhiCommand(FAutomationTestBase* InTest):Test(InTest)
	{
		auto* Settings=GetMutableDefault<UEditorPerformanceSettings>();OldThrottle=Settings->bThrottleCPUWhenNotForeground;OldMonitor=Settings->bMonitorEditorPerformance;
		Settings->bThrottleCPUWhenNotForeground=false;Settings->bMonitorEditorPerformance=false;
	}
	~FSensorFileSaveRhiCommand()
	{
		if(TestWorld.IsValid())
		{
			if(auto* Periodic=TestWorld->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>())if(!PeriodicId.IsEmpty())Periodic->StopPeriodicSave(PeriodicId);
			if(auto* Files=TestWorld->GetSubsystem<UVirtualSensorFileSaveSubsystem>())for(const auto& Id:{CameraRequest,LidarRequest,CancelledRequest})if(!Id.IsEmpty())Files->CancelRequest(Id);
		}
		if(Panel.IsValid()){if(bCreatedPanel)Panel->RemoveFromParent();else Panel->SetVisibility(OldVisibility);}
		auto* Settings=GetMutableDefault<UEditorPerformanceSettings>();Settings->bThrottleCPUWhenNotForeground=OldThrottle;Settings->bMonitorEditorPerformance=OldMonitor;
	}
	bool Finish(const FString& Reason)
	{
		if(bReportWritten)return true;bReportWritten=true;
		auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("reason"),Reason);Report->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());
		Report->SetStringField(TEXT("rhi"),GDynamicRHI?GDynamicRHI->GetName():TEXT("none"));Report->SetArrayField(TEXT("requests"),Evidence);
		Report->SetStringField(TEXT("camera_id"),CameraId);Report->SetStringField(TEXT("lidar_id"),LidarId);
		Report->SetNumberField(TEXT("periodic_interval_sec"),PeriodicResult.IntervalSeconds);Report->SetNumberField(TEXT("periodic_completed"),PeriodicResult.CompletedCount);
		Report->SetNumberField(TEXT("periodic_skipped"),PeriodicResult.SkippedCount);Report->SetNumberField(TEXT("periodic_failed"),PeriodicResult.FailedCount);
		Report->SetNumberField(TEXT("max_pending_per_camera"),MaxPendingForCamera);Report->SetNumberField(TEXT("observed_unique_periodic_frames"),SavedPeriodicFrames.Num());
		Report->SetNumberField(TEXT("periodic_camera_acquisitions"),FMath::Max<int64>(0,PeriodicEndFrame-PeriodicCameraFrame));
		Report->SetNumberField(TEXT("periodic_world_seconds"),FMath::Max(0.0,PeriodicWorldEnd-PeriodicWorldStart));
		Report->SetBoolField(TEXT("ui_hidden_during_periodic"),Panel.IsValid()&&Panel->GetVisibility()==ESlateVisibility::Collapsed);
		Report->SetStringField(TEXT("periodic_folder"),PeriodicResult.Folder);Report->SetBoolField(TEXT("camera_revision_cancel_test"),Stage>=6);
		Report->SetBoolField(TEXT("world_shutdown_generation_test"),false);
		if(TestWorld.IsValid())if(auto* Viewport=TestWorld->GetGameViewport())if(Viewport->Viewport)
		{const auto Size=Viewport->Viewport->GetSizeXY();Report->SetNumberField(TEXT("viewport_width"),Size.X);Report->SetNumberField(TEXT("viewport_height"),Size.Y);}
		const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/UiSimplification");IFileManager::Get().MakeDirectory(*Dir,true);
		FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));
		Test->TestTrue(TEXT("actual file workflow report written"),FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("file_save_rhi.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));return true;
	}
	void RecordRequest(const FVirtualSensorFileSaveStatus& S)
	{
		auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("request_id"),S.RequestId);Row->SetStringField(TEXT("sensor_id"),S.SensorId);Row->SetNumberField(TEXT("frame_id"),S.FrameId);
		Row->SetNumberField(TEXT("state"),int32(S.State));Row->SetStringField(TEXT("message"),S.Message);TArray<TSharedPtr<FJsonValue>> Paths;
		for(const auto& F:S.FileResults)Paths.Add(MakeShared<FJsonValueString>(F.AbsolutePath));Row->SetArrayField(TEXT("files"),Paths);Evidence.Add(MakeShared<FJsonValueObject>(Row));
	}
	bool CheckFiles(const FVirtualSensorFileSaveStatus& S)
	{
		bool Ok=Test->TestEqual(TEXT("file service reached success"),S.State,EVirtualSensorFileSaveState::Succeeded);
		Ok&=Test->TestEqual(TEXT("two selected outputs saved"),S.FileResults.Num(),2);
		for(const auto& F:S.FileResults)
		{
			Ok&=Test->TestTrue(TEXT("reported file is an absolute existing nonempty file"),!FPaths::IsRelative(F.AbsolutePath)&&IFileManager::Get().FileSize(*F.AbsolutePath)>0);
			Ok&=Test->TestEqual(TEXT("file result preserves sensor identity"),F.SensorId,S.SensorId);
			Ok&=Test->TestEqual(TEXT("actual file length matches report"),IFileManager::Get().FileSize(*F.AbsolutePath),F.FileSizeBytes);
		}
		return Ok;
	}
	bool Update() override
	{
		if(FPlatformTime::Seconds()-Started>40){Test->AddError(TEXT("Sensor file RHI workflow timed out."));return Finish(TEXT("timeout"));}
		UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){World=C.World();break;}
		if(!World||!World->GetGameInstance())return false;TestWorld=World;
		auto* Files=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>();auto* Periodic=World->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>();
		if(Stage==0)
		{
			if(FPlatformTime::Seconds()-Started<5)return false; // Existing validation rig has finished its one-time camera Stop/configuration.
			for(TActorIterator<AVirtualCameraSensorActor> It(World);It;++It){Camera=*It;break;}
			for(TActorIterator<AVirtualLidarSensorActor> It(World);It;++It){Lidar=*It;break;}
			for(TActorIterator<AVirtualSensorCoordinator> It(World);It;++It){Coordinator=*It;break;}
			if(!Camera.IsValid()||!Lidar.IsValid()||!Coordinator.IsValid()){Test->AddError(TEXT("Validation map requires Camera, LiDAR and Coordinator."));return Finish(TEXT("map actors missing"));}
			FVirtualSensorEditableState State;FString Error;Camera->ReadEditableState(State);
			State.SimulationQuality=EVirtualSensorSimulationQuality::Custom;State.CameraResolution=FIntPoint(320,180);State.CameraCaptureInterval=.1f;State.CameraJpegQuality=70;State.CameraCaptureMode=EVirtualCameraCaptureMode::PreviewOnly;
			if(!Test->TestTrue(TEXT("camera configured through Actor editable-state API"),Camera->ApplyEditableState(State,Error))){Test->AddError(Error);return Finish(TEXT("camera configuration failed"));}
			Camera->StartSensor();CameraId=Camera->GetSensorId();LidarId=Lidar->GetSensorId();OriginalTransportMode=Coordinator->SharedTransportComponent->TransportMode;
			Coordinator->SelectCameraByIndex(Coordinator->FindCameraIndexBySensorId(CameraId));Coordinator->SetViewMode(EVirtualSensorViewMode::Camera);
			if(auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>())Panel=Cast<UVirtualSensorCaptureExportPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Data));
			if(!Panel.IsValid()){Panel=CreateWidget<UVirtualSensorCaptureExportPanelWidget>(World,UVirtualSensorCaptureExportPanelWidget::StaticClass());Panel->BindSensorManager(Coordinator.Get());Panel->AddToViewport(90);bCreatedPanel=true;}
			OldVisibility=Panel->GetVisibility();Stage=1;return false;
		}
		if(!Camera.IsValid()||!Lidar.IsValid()||!Coordinator.IsValid()){Test->AddError(TEXT("A pinned test sensor disappeared."));return Finish(TEXT("sensor disappeared"));}
		if(Stage==1)
		{
			if(Files->GetAvailableFrameId(Camera.Get())<1||Files->GetAvailableFrameId(Lidar.Get())<1)return false;
			FVirtualSensorCaptureSelection S;S.bCameraImage=true;S.bCameraPayload=true;S.bPointCloud=false;S.bLidarPayload=false;
			BaselineCameraFrame=Files->GetAvailableFrameId(Camera.Get());const int64 Before=Camera->GetSensorRuntimeStatus().FrameId;
			CameraRequest=Files->RequestSave(EVirtualSensorFileSaveMode::NewFrame,Camera.Get(),S);
			Test->TestEqual(TEXT("NewFrame request does not synchronously increment acquisition"),Camera->GetSensorRuntimeStatus().FrameId,Before);
			const FString Duplicate=Files->RequestSave(EVirtualSensorFileSaveMode::NewFrame,Camera.Get(),S);
			Test->TestEqual(TEXT("same-camera duplicate is explicitly rejected"),Files->GetRequestStatus(Duplicate).State,EVirtualSensorFileSaveState::Failed);
			Coordinator->SelectLidarByIndex(0);Coordinator->SetViewMode(EVirtualSensorViewMode::Lidar);
			Panel->SetVisibility(ESlateVisibility::Collapsed);Stage=2;return false;
		}
		if(Stage==2)
		{
			CameraResult=Files->GetRequestStatus(CameraRequest);if(!CameraResult.IsTerminal())return false;RecordRequest(CameraResult);
			if(!CheckFiles(CameraResult))return Finish(TEXT("camera save failed"));
			Test->TestEqual(TEXT("selection change cannot retarget Camera request"),CameraResult.SensorId,CameraId);
			Test->TestTrue(TEXT("NewFrame is newer than accepted baseline"),CameraResult.FrameId>BaselineCameraFrame);
			TArray<uint8> Jpeg;TSharedPtr<FJsonObject> Json;
			for(const auto& F:CameraResult.FileResults){if(F.Kind==EVirtualSensorExportKind::CameraJpeg)FFileHelper::LoadFileToArray(Jpeg,*F.AbsolutePath);else if(F.Kind==EVirtualSensorExportKind::ServerPayload)Json=ParseJsonFile(F.AbsolutePath);}
			FIntPoint Size;Test->TestTrue(TEXT("JPEG has SOI/EOI and valid SOF dimensions"),ReadJpegDimensions(Jpeg,Size));Test->TestEqual(TEXT("JPEG requested dimensions"),Size,FIntPoint(320,180));
			Test->TestTrue(TEXT("camera Payload JSON parses"),Json.IsValid());
			if(Json)
			{
				Test->TestEqual(TEXT("camera JSON identity matches pinned sensor"),Json->GetStringField(TEXT("sensorId")),CameraId);
				Test->TestEqual(TEXT("camera JPEG/JSON share frame"),int64(Json->GetNumberField(TEXT("frameId"))),CameraResult.FrameId);
				Test->TestEqual(TEXT("camera JSON width"),int32(Json->GetNumberField(TEXT("width"))),320);Test->TestEqual(TEXT("camera JSON height"),int32(Json->GetNumberField(TEXT("height"))),180);
				TArray<uint8> Embedded;Test->TestTrue(TEXT("camera JSON image Base64 decodes"),FBase64::Decode(Json->GetStringField(TEXT("image")),Embedded));Test->TestTrue(TEXT("saved JPEG is exactly the JSON image"),Embedded==Jpeg);
			}
			FVirtualSensorCaptureSelection S;S.bCameraImage=false;S.bCameraPayload=false;S.bLidarPayload=true;S.bPointCloud=true;S.PointCloudFormat=EVirtualSensorExportKind::PointCloudPcd;
			PinnedLidarFrame=Files->GetAvailableFrameId(Lidar.Get());const int64 Before=Lidar->GetSensorRuntimeStatus().FrameId;
			LidarRequest=Files->RequestSave(EVirtualSensorFileSaveMode::CurrentFrame,Lidar.Get(),S);
			Test->TestEqual(TEXT("CurrentFrame cannot perform a synchronous LiDAR scan"),Lidar->GetSensorRuntimeStatus().FrameId,Before);
			Coordinator->SetViewMode(EVirtualSensorViewMode::Camera);Stage=3;return false;
		}
		if(Stage==3)
		{
			LidarResult=Files->GetRequestStatus(LidarRequest);if(!LidarResult.IsTerminal())return false;RecordRequest(LidarResult);
			if(!CheckFiles(LidarResult))return Finish(TEXT("LiDAR save failed"));
			Test->TestEqual(TEXT("LiDAR request pins exact current frame"),LidarResult.FrameId,PinnedLidarFrame);
			Test->TestEqual(TEXT("LiDAR request preserves sensor selection"),LidarResult.SensorId,LidarId);
			for(const auto& F:LidarResult.FileResults)
			{
				if(F.Kind==EVirtualSensorExportKind::ServerPayload){auto Json=ParseJsonFile(F.AbsolutePath);Test->TestTrue(TEXT("LiDAR JSON parses"),Json.IsValid());if(Json){Test->TestEqual(TEXT("LiDAR JSON sensor"),Json->GetStringField(TEXT("sensorId")),LidarId);Test->TestEqual(TEXT("LiDAR JSON pinned frame"),int64(Json->GetNumberField(TEXT("frameId"))),PinnedLidarFrame);}}
				else if(F.Kind==EVirtualSensorExportKind::PointCloudPcd){TArray<uint8> Bytes;FFileHelper::LoadFileToArray(Bytes,*F.AbsolutePath);auto Meta=ReadPcdMetadata(Bytes);Test->TestTrue(TEXT("Binary PCD frame metadata parses independently"),Meta.IsValid());if(Meta){Test->TestEqual(TEXT("PCD sensor ID"),Meta->GetStringField(TEXT("sensor_id")),LidarId);Test->TestEqual(TEXT("PCD immutable frame ID"),FCString::Atoi64(*Meta->GetStringField(TEXT("sensor_frame_id"))),PinnedLidarFrame);}}
			}
			FVirtualSensorCaptureSelection S;S.bCameraImage=true;S.bCameraPayload=true;S.bPointCloud=false;S.bLidarPayload=false;S.IntervalSeconds=.05f;
			PeriodicCameraFrame=Files->GetAvailableFrameId(Camera.Get());PeriodicWorldStart=World->GetTimeSeconds();PeriodicStarted=FPlatformTime::Seconds();
			PeriodicId=Periodic->StartPeriodicSave(Camera.Get(),S);Test->TestTrue(TEXT("0.05-second periodic service active"),Periodic->GetPeriodicStatus(PeriodicId).bActive);
			const FString Other=Periodic->StartPeriodicSave(Camera.Get(),S);Test->TestFalse(TEXT("same-camera second periodic session rejected"),Periodic->GetPeriodicStatus(Other).bActive);
			Stage=4;return false;
		}
		if(Stage==4)
		{
			PeriodicResult=Periodic->GetPeriodicStatus(PeriodicId);
			if(PeriodicResult.FailedCount>0){Test->AddError(PeriodicResult.LastMessage);return Finish(TEXT("periodic failed"));}
			int32 Pending=0;
			for(const auto& R:Files->GetRecentRequests())
			{
				if(R.SensorId==CameraId&&!R.IsTerminal())++Pending;
				if(R.State==EVirtualSensorFileSaveState::Succeeded&&!R.FileResults.IsEmpty()&&R.FileResults[0].AbsolutePath.StartsWith(PeriodicResult.Folder)&&!SeenPeriodicRequests.Contains(R.RequestId))
				{SeenPeriodicRequests.Add(R.RequestId);Test->TestFalse(TEXT("periodic never saves same acquisition twice"),SavedPeriodicFrames.Contains(R.FrameId));SavedPeriodicFrames.Add(R.FrameId);for(const auto& F:R.FileResults){Test->TestTrue(TEXT("hidden UI periodic output exists"),IFileManager::Get().FileSize(*F.AbsolutePath)>0);Test->TestTrue(TEXT("timed Camera folder retained"),F.AbsolutePath.Contains(TEXT("/Camera/")));}}
			}
			MaxPendingForCamera=FMath::Max(MaxPendingForCamera,Pending);Test->TestTrue(TEXT("sensor has at most one in-flight file request"),Pending<=1);
			Test->TestTrue(TEXT("UI remains hidden while save service progresses"),Panel->GetVisibility()==ESlateVisibility::Collapsed);
			if(FPlatformTime::Seconds()-PeriodicStarted<3.0)return false;
			Test->TestTrue(TEXT("periodic made progress for at least three seconds"),PeriodicResult.CompletedCount>=5);
			Test->TestTrue(TEXT("20Hz save opportunities skip rather than recapture 10Hz Camera"),PeriodicResult.SkippedCount>0);
			PeriodicWorldEnd=World->GetTimeSeconds();PeriodicEndFrame=Files->GetAvailableFrameId(Camera.Get());
			const int64 CadenceCeiling=FMath::CeilToInt((PeriodicWorldEnd-PeriodicWorldStart)/.1)+2;
			Test->TestTrue(TEXT("file saving does not increase regular Camera cadence"),PeriodicEndFrame-PeriodicCameraFrame<=CadenceCeiling);
			Test->TestTrue(TEXT("saved frames cannot outnumber completed acquisitions"),PeriodicResult.CompletedCount<=PeriodicEndFrame-PeriodicCameraFrame+1);
			Periodic->StopPeriodicSave(PeriodicId);StopStarted=FPlatformTime::Seconds();Stage=5;return false;
		}
		if(Stage==5)
		{
			PeriodicResult=Periodic->GetPeriodicStatus(PeriodicId);if(PeriodicResult.bPending){if(FPlatformTime::Seconds()-StopStarted>10){Test->AddError(TEXT("Periodic stop did not drain."));return Finish(TEXT("drain timeout"));}return false;}
			Test->TestFalse(TEXT("periodic stop prevents new admissions"),PeriodicResult.bActive);Test->TestEqual(TEXT("periodic completed without failed writes"),PeriodicResult.FailedCount,int64(0));
			FVirtualSensorEditableState State;Camera->ReadEditableState(State);Test->TestEqual(TEXT("file-only requests preserve PreviewOnly mode"),State.CameraCaptureMode,EVirtualCameraCaptureMode::PreviewOnly);
			Test->TestEqual(TEXT("file-only requests preserve transport mode"),Coordinator->SharedTransportComponent->TransportMode,OriginalTransportMode);
			FVirtualSensorCaptureSelection S;S.bCameraImage=true;S.bCameraPayload=true;S.bPointCloud=false;S.bLidarPayload=false;
			CancelledRequest=Files->RequestSave(EVirtualSensorFileSaveMode::NewFrame,Camera.Get(),S);Camera->StopSensor();CancelledAt=FPlatformTime::Seconds();Stage=6;return false;
		}
		if(Stage==6)
		{
			const auto Cancelled=Files->GetRequestStatus(CancelledRequest);if(!Cancelled.IsTerminal()||FPlatformTime::Seconds()-CancelledAt<.3)return false;
			RecordRequest(Cancelled);Test->TestEqual(TEXT("stop/generation change cancels awaiting camera result"),Cancelled.State,EVirtualSensorFileSaveState::Cancelled);Test->TestTrue(TEXT("cancelled generation cannot publish saved files"),Cancelled.FileResults.IsEmpty());
			for(const auto& Status:Coordinator->StreamPublisherComponent->GetStreamStatuses())Test->TestFalse(TEXT("file requests do not enable a Topic stream"),Status.bEnabled);
			return Finish(TEXT("real RHI immutable file save / selection / periodic / cancellation workflow completed"));
		}
		return false;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorFileSaveRhiTest,"MA0T10.SensorFiles.RealRhiWorkflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorFileSaveRhiTest::RunTest(const FString&)
{
	if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SENSOR_FILES_RHI"))!=TEXT("1")){AddInfo(TEXT("Skipped actual RHI file workflow; set MA0T10_SENSOR_FILES_RHI=1."));return true;}
	if(!GDynamicRHI||FString(GDynamicRHI->GetName()).Contains(TEXT("Null"),ESearchCase::IgnoreCase)){AddError(TEXT("This test requires a real RHI; NullRHI is not file/image evidence."));return false;}
	if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FSensorFileSaveRhiCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
