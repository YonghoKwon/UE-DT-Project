#include "VirtualCameraCaptureComponent.h"
#include "Async/Async.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "RHIGPUReadback.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include <atomic>

struct FVirtualCameraLocalReadbackState
{
	FVirtualCameraLocalSaveFrame Frame;
	FVirtualCameraLocalSaveCallback Callback;
	TSharedPtr<FRHIGPUTextureReadback,ESPMode::ThreadSafe> Readback;
	std::atomic<bool> Cancelled{false};
	int32 Generation=0;
	int32 Quality=80;
	bool WaitingForNewFrame=false;
	bool Mapping=false;
};

int64 UVirtualCameraCaptureComponent::GetAvailableLocalFrameId() const
{
	return bHasFileAcquisition&&CameraRenderTarget&&LastFileAcquisition.SensorId==SensorId&&
		CameraRenderTarget->SizeX==LastFileAcquisition.Width&&CameraRenderTarget->SizeY==LastFileAcquisition.Height?LastFileAcquisition.FrameId:-1;
}

bool UVirtualCameraCaptureComponent::RequestLocalFileFrame(bool bNewFrame,FVirtualCameraLocalSaveCallback Callback,FString& Error)
{
	check(IsInGameThread());
	if(LocalFileReadback) { Error=TEXT("Camera 파일 캡처 요청이 이미 진행 중입니다."); return false; }
	if(bNewFrame&&!IsCaptureRunning()) {Error=TEXT("센서 측정을 시작한 뒤 새 프레임 저장을 요청하세요.");return false;}
	if(!bNewFrame&&(!bHasFileAcquisition||!CameraRenderTarget)) {Error=TEXT("저장할 완료된 Camera 프레임이 없습니다.");return false;}
	if(!bNewFrame&&LastFileAcquisition.SensorId!=SensorId) {Error=TEXT("현재 SensorId에 속하는 Camera 프레임이 없습니다.");return false;}
	if(!bNewFrame&&(CameraRenderTarget->SizeX!=LastFileAcquisition.Width||CameraRenderTarget->SizeY!=LastFileAcquisition.Height))
	{Error=TEXT("Camera 렌더 타깃 크기가 바뀌었습니다. 새 측정 프레임을 기다리세요.");return false;}
	auto State=MakeShared<FVirtualCameraLocalReadbackState,ESPMode::ThreadSafe>();
	State->Callback=MoveTemp(Callback); State->Generation=ScheduledGeneration; State->Quality=FMath::Clamp(JpegQuality,1,100); State->WaitingForNewFrame=bNewFrame;
	LocalFileReadback=State;
	if(!bNewFrame) QueueLocalFileReadback();
	return true;
}
void UVirtualCameraCaptureComponent::RecordLocalFileAcquisition(double)
{
	PendingFileAcquisition.SensorId=SensorId; PendingFileAcquisition.FrameId=FrameId;
	PendingFileAcquisition.Manufacturer=DeviceSpec.Manufacturer;PendingFileAcquisition.Model=DeviceSpec.Model;
	const TCHAR* Quality=SimulationQuality==EVirtualSensorSimulationQuality::Debug?TEXT("Debug"):SimulationQuality==EVirtualSensorSimulationQuality::Balanced?TEXT("Balanced"):SimulationQuality==EVirtualSensorSimulationQuality::FullSpec?TEXT("FullSpec"):TEXT("RealTimePreview");
	PendingFileAcquisition.SimulationQuality=Quality;bFileAcquisitionPendingRender=true;
	if(const auto* Context=SlabCaptureContexts.Find(FrameId))PendingFileSlabContext=*Context;else PendingFileSlabContext=FVirtualSlabFrameContext();
}
void UVirtualCameraCaptureComponent::UpdateSceneCaptureContents(FSceneInterface* Scene)
{
	// This callback is invoked by the engine at this component's natural capture point.
	// Never dispatch another component's deferred capture in response to a file request.
	const double ActualStart=FPlatformTime::Seconds();
	FVirtualCameraPayloadSnapshot Snapshot=PendingFileAcquisition;
#if WITH_DEV_AUTOMATION_TESTS
	if(BeforeSceneCaptureForTests&&bFileAcquisitionPendingRender)BeforeSceneCaptureForTests(Snapshot.FrameId);
#endif
	Snapshot.TimestampUtc=FDateTime::UtcNow();Snapshot.Width=CameraRenderTarget?CameraRenderTarget->SizeX:CaptureResolution.X;
	Snapshot.AcquisitionRevision=ScheduledGeneration;
	Snapshot.Height=CameraRenderTarget?CameraRenderTarget->SizeY:CaptureResolution.Y;Snapshot.HorizontalFov=FOVAngle;Snapshot.VerticalFov=DeviceSpec.VerticalFovDegrees;
	Snapshot.Location=GetComponentLocation();Snapshot.Rotation=GetComponentRotation();Snapshot.Forward=GetForwardVector();Snapshot.Up=GetUpVector();
	Super::UpdateSceneCaptureContents(Scene);
#if WITH_DEV_AUTOMATION_TESTS
	++LocalFileRenderDispatchCount;
#endif
	if(!bFileAcquisitionPendingRender)return;
	bFileAcquisitionPendingRender=false;LastFileAcquisition=MoveTemp(Snapshot);LastFileSlabContext=PendingFileSlabContext;LastFileAcquisitionSeconds=ActualStart;bHasFileAcquisition=true;ExternalFileFrame.Reset();
	if(ShouldGeneratePayload()&&!PendingReadbackRequests.IsEmpty())
	{
		ScheduledAcquisitionSnapshots.Add(LastFileAcquisition.FrameId,LastFileAcquisition);
		QueuePendingGpuReadbacks();
	}
	if(LocalFileReadback&&LocalFileReadback->WaitingForNewFrame) {LocalFileReadback->WaitingForNewFrame=false;QueueLocalFileReadback();}
}
void UVirtualCameraCaptureComponent::QueueLocalFileReadback()
{
	const auto State=LocalFileReadback;
	if(!State||State->Cancelled||State->Readback||!GetWorld()) return;
	// Current: copy the last dispatched capture before a later deferred capture is queued.
	// New: called immediately after our own capture command by UpdateSceneCaptureContents.
	FTextureRenderTargetResource* Resource=CameraRenderTarget?CameraRenderTarget->GameThread_GetRenderTargetResource():nullptr;
	FTextureRHIRef Texture=Resource?Resource->GetRenderTargetTexture():FTextureRHIRef();
	if(!Texture.IsValid())
	{
		auto Callback=MoveTemp(State->Callback);LocalFileReadback.Reset();if(Callback)Callback(nullptr,TEXT("Camera GPU 프레임을 읽을 수 없습니다."));return;
	}
	State->Frame.Payload=LastFileAcquisition; State->Frame.AcquisitionSeconds=LastFileAcquisitionSeconds;
	State->Frame.SlabContext=LastFileSlabContext;
	State->Readback=MakeShared<FRHIGPUTextureReadback,ESPMode::ThreadSafe>(TEXT("SensorLocalFileReadback"));
	const auto Copy=State->Readback;
	ENQUEUE_RENDER_COMMAND(SensorLocalFileCopy)([Copy,Texture](FRHICommandListImmediate& R){Copy->EnqueueCopy(R,Texture);});
}
void UVirtualCameraCaptureComponent::PollLocalFileFrame()
{
	check(IsInGameThread()); const auto State=LocalFileReadback;
	if(!State||State->Cancelled||State->WaitingForNewFrame||State->Mapping||!State->Readback||!State->Readback->IsReady()) return;
	if(State->Generation!=ScheduledGeneration) {CancelLocalFileFrame();return;}
	if(!TryAcquireLocalFileEncodeSlot()) return;
	State->Mapping=true; auto Copy=MoveTemp(State->Readback);
	IImageWrapperModule* Module=&FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TWeakObjectPtr<UVirtualCameraCaptureComponent> Weak(this);
	ENQUEUE_RENDER_COMMAND(SensorLocalFileMap)([Copy,State,Module,Weak](FRHICommandListImmediate&) mutable
	{
		TArray<FColor> Pixels;int32 Pitch=0;const FColor* Raw=static_cast<const FColor*>(Copy->Lock(Pitch));
		const int32 W=State->Frame.Payload.Width,H=State->Frame.Payload.Height;
		if(Raw&&Pitch>=W&&W>0&&H>0&&!State->Cancelled)
		{Pixels.SetNumUninitialized(W*H);for(int32 Y=0;Y<H;++Y)FMemory::Memcpy(Pixels.GetData()+Y*W,Raw+Y*Pitch,W*sizeof(FColor));}
		if(Raw)Copy->Unlock();Copy.Reset();
		Async(EAsyncExecution::ThreadPool,[State,Module,Weak,Pixels=MoveTemp(Pixels),W,H]() mutable
		{
			TArray64<uint8> Bytes;FString Error;
			if(!State->Cancelled&&!Pixels.IsEmpty())
			{
				auto Wrapper=Module->CreateImageWrapper(EImageFormat::JPEG);
				if(Wrapper&&Wrapper->SetRaw(Pixels.GetData(),int64(Pixels.Num())*sizeof(FColor),W,H,ERGBFormat::BGRA,8)) Bytes=Wrapper->GetCompressed(State->Quality);
			}
			ReleaseLocalFileEncodeSlot();
			if(Bytes.IsEmpty()) Error=TEXT("Camera 비동기 JPEG 생성에 실패했습니다.");
			auto Frame=MakeShared<FVirtualCameraLocalSaveFrame,ESPMode::ThreadSafe>(State->Frame);Frame->Jpeg=MakeShared<const TArray64<uint8>,ESPMode::ThreadSafe>(MoveTemp(Bytes));
			AsyncTask(ENamedThreads::GameThread,[State,Weak,Frame,Error]() mutable
			{
				if(!Weak.IsValid()||State->Cancelled||Weak->ScheduledGeneration!=State->Generation||Weak->LocalFileReadback!=State)return;
				Weak->LocalFileReadback.Reset();auto Callback=MoveTemp(State->Callback);if(Callback)Callback(Frame,Error);
			});
		});
	});
}
void UVirtualCameraCaptureComponent::CancelLocalFileFrame()
{
	if(!LocalFileReadback) return;
	LocalFileReadback->Cancelled=true;
	if(LocalFileReadback->Readback)
	{
		auto Copy=MoveTemp(LocalFileReadback->Readback);ENQUEUE_RENDER_COMMAND(SensorLocalFileRelease)([Copy](FRHICommandListImmediate&)mutable{Copy.Reset();});
	}
	LocalFileReadback.Reset();
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "RHIGlobals.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorCameraLocalDispatchTest,"MA0T10.SensorFiles.CameraCaptureDispatchIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorCameraLocalDispatchTest::RunTest(const FString&)
{
	if(GUsingNullRHI||!FApp::CanEverRender()){AddInfo(TEXT("SKIP: camera capture dispatch isolation requires rendering."));return true;}
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* A=World->SpawnActor<AVirtualCameraSensorActor>();auto* B=World->SpawnActor<AVirtualCameraSensorActor>();
	A->CaptureComponent->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;A->CaptureComponent->CaptureResolution=FIntPoint(4,4);
	// Existing public one-shot creates the fixture frame, not the production file request.
	A->CaptureComponent->CaptureAndSendImage();
	TestTrue(TEXT("fixture has a dispatched local frame"),A->CaptureComponent->GetAvailableLocalFrameId()>=0);
	B->CaptureComponent->CaptureSceneDeferred();const int32 Before=B->CaptureComponent->LocalFileRenderDispatchCount;
	const int32 OwnBefore=A->CaptureComponent->LocalFileRenderDispatchCount;FString Error;
	const bool Accepted=A->CaptureComponent->RequestLocalFileFrame(false,[](auto,const FString&){},Error);
	TestTrue(TEXT("current local copy accepted"),Accepted);
	TestEqual(TEXT("file request never dispatches another deferred camera"),B->CaptureComponent->LocalFileRenderDispatchCount,Before);
	TestEqual(TEXT("current frame copy creates no new own acquisition"),A->CaptureComponent->LocalFileRenderDispatchCount,OwnBefore);
	A->CaptureComponent->CancelLocalFileFrame();
	// Explicitly advance the test's engine stage, proving B really was pending all along.
	USceneCaptureComponent::UpdateDeferredCaptures(World->Scene);
	TestTrue(TEXT("unrelated capture remains pending until the engine capture stage"),B->CaptureComponent->LocalFileRenderDispatchCount>Before);
	A->Destroy();B->Destroy();return true;
}
#endif
