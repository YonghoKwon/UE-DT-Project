#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "InteractableActor/InteractableActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarAnalysisComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarExportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "ma0t10_dt/MA0T10/Sensor/RealSensorSourceComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2ActorCompositionTest,
	"MA0T10.SensorV2.Architecture.ActorComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2OutputDeduplicationTest,
	"MA0T10.SensorV2.Architecture.OutputDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2EditableStateValidationTest,
	"MA0T10.SensorV2.Architecture.EditableStateValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2SelectionIndependenceTest,
	"MA0T10.SensorV2.Architecture.SelectionIndependentFromMonitorView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2InteractionBudgetTest,
	"MA0T10.SensorV2.Architecture.InteractionBudgetRestoresFullSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2ExternalSourceHostTest,
	"MA0T10.SensorV2.Architecture.ExternalSourceHostDefaultsStopped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSensorV2ActorCompositionTest::RunTest(const FString& Parameters)
{
	AVirtualCameraSensorActor* Camera = NewObject<AVirtualCameraSensorActor>();
	AVirtualLidarSensorActor* Lidar = NewObject<AVirtualLidarSensorActor>();

	TestTrue(TEXT("Virtual sensor base derives from DTCore interactable actor"),
		AVirtualSensorActorBase::StaticClass()->IsChildOf(AInteractableActor::StaticClass()));
	TestTrue(TEXT("Camera derives through the virtual sensor base"),
		AVirtualCameraSensorActor::StaticClass()->IsChildOf(AVirtualSensorActorBase::StaticClass()));
	TestTrue(TEXT("LiDAR derives through the virtual sensor base"),
		AVirtualLidarSensorActor::StaticClass()->IsChildOf(AVirtualSensorActorBase::StaticClass()));

	TestNotNull(TEXT("Camera V2 capture component"), Camera->CaptureComponent.Get());
	TestNotNull(TEXT("Camera V2 output component"), Camera->OutputComponent.Get());
	TestEqual(TEXT("Camera V2 kind"), Camera->GetSensorKind(), EVirtualSensorKind::Camera);

	TestNotNull(TEXT("LiDAR V2 scan component"), Lidar->ScanComponent.Get());
	TestNotNull(TEXT("LiDAR V2 analysis component"), Lidar->AnalysisComponent.Get());
	TestNotNull(TEXT("LiDAR V2 visualization component"), Lidar->VisualizationComponent.Get());
	TestNotNull(TEXT("LiDAR V2 export component"), Lidar->ExportComponent.Get());
	TestNotNull(TEXT("LiDAR V2 output component"), Lidar->OutputComponent.Get());
	TestEqual(TEXT("LiDAR V2 kind"), Lidar->GetSensorKind(), EVirtualSensorKind::Lidar);
	return true;
}

bool FSensorV2OutputDeduplicationTest::RunTest(const FString& Parameters)
{
	UVirtualSensorOutputComponent* Output = NewObject<UVirtualSensorOutputComponent>();
	FVirtualSensorFrameEnvelope Frame;
	Frame.SensorId = TEXT("V2-TEST-001");
	Frame.SensorKind = EVirtualSensorKind::Camera;
	Frame.FrameId = 7;
	Frame.TimestampUtc = FDateTime::UtcNow();
	Frame.SchemaVersion = TEXT("virtual-camera.v1");
	Frame.JsonPayload = MakeShared<const FString, ESPMode::ThreadSafe>(TEXT("{\"schemaVersion\":\"virtual-camera.v1\"}"));

	TestTrue(TEXT("first frame is routed"), Output->RouteFrame(Frame));
	TestFalse(TEXT("same sensor/frame is not routed twice"), Output->RouteFrame(Frame));
	TestEqual(TEXT("last routed frame id"), Output->GetLastRoutedFrameId(), static_cast<int64>(7));
	return true;
}

bool FSensorV2EditableStateValidationTest::RunTest(const FString& Parameters)
{
	AVirtualCameraSensorActor* Camera = NewObject<AVirtualCameraSensorActor>();
	AVirtualLidarSensorActor* Lidar = NewObject<AVirtualLidarSensorActor>();
	FVirtualSensorEditableState CameraState;
	FVirtualSensorEditableState LidarState;
	FString Error;

	TestTrue(TEXT("Camera state can be read through Actor API"), Camera->ReadEditableState(CameraState));
	TestTrue(TEXT("Camera defaults pass Actor validation"), Camera->ValidateEditableState(CameraState, Error));
	CameraState.CameraResolution.X = 32;
	TestFalse(TEXT("Camera Actor rejects invalid resolution"), Camera->ValidateEditableState(CameraState, Error));

	Camera->ReadEditableState(CameraState);
	CameraState.SimulationQuality = EVirtualSensorSimulationQuality::FullSpec;
	FVirtualSensorEditableState AppliedCameraState;
	TestTrue(TEXT("Camera Actor applies a quality preset without stale advanced values"), Camera->ApplyProfileAndSimulationQuality(CameraState, AppliedCameraState, Error));
	TestEqual(TEXT("Camera preset updates width"), AppliedCameraState.CameraResolution.X, 1280);
	TestEqual(TEXT("Camera preset updates height"), AppliedCameraState.CameraResolution.Y, 720);
	TestTrue(TEXT("Camera preset updates interval"), FMath::IsNearlyEqual(AppliedCameraState.CameraCaptureInterval, 1.0f / 30.0f));

	Error.Reset();
	TestTrue(TEXT("LiDAR state can be read through Actor API"), Lidar->ReadEditableState(LidarState));
	TestTrue(TEXT("LiDAR defaults pass Actor validation"), Lidar->ValidateEditableState(LidarState, Error));
	LidarState.LidarMinVerticalAngle = LidarState.LidarMaxVerticalAngle;
	TestFalse(TEXT("LiDAR Actor rejects inverted vertical range"), Lidar->ValidateEditableState(LidarState, Error));

	Lidar->ReadEditableState(LidarState);
	LidarState.SimulationQuality = EVirtualSensorSimulationQuality::FullSpec;
	FVirtualSensorEditableState AppliedLidarState;
	TestTrue(TEXT("LiDAR Actor applies a quality preset without stale advanced values"), Lidar->ApplyProfileAndSimulationQuality(LidarState, AppliedLidarState, Error));
	TestEqual(TEXT("LiDAR preset updates horizontal samples"), AppliedLidarState.LidarHorizontalSamples, 360);
	TestEqual(TEXT("LiDAR preset updates vertical channels"), AppliedLidarState.LidarVerticalChannels, 60);
	TestTrue(TEXT("LiDAR preset updates interval"), FMath::IsNearlyEqual(AppliedLidarState.LidarScanInterval, 0.1f));
	TestEqual(TEXT("LiDAR preset updates preview stride"), AppliedLidarState.PreviewPointStride, 6);
	TestEqual(TEXT("LiDAR preset updates preview budget"), AppliedLidarState.MaxPreviewPoints, 5000);
	return true;
}

bool FSensorV2SelectionIndependenceTest::RunTest(const FString& Parameters)
{
	AVirtualSensorCoordinator* Coordinator = NewObject<AVirtualSensorCoordinator>();
	AVirtualCameraSensorActor* Camera = NewObject<AVirtualCameraSensorActor>();
	AVirtualLidarSensorActor* Lidar = NewObject<AVirtualLidarSensorActor>();
	Coordinator->RegisterSensorActor(Camera);
	Coordinator->RegisterSensorActor(Lidar);
	Coordinator->RegisterCamera(Camera->CaptureComponent);
	Coordinator->RegisterLidar(Lidar->ScanComponent);

	Coordinator->SetViewMode(EVirtualSensorViewMode::Camera);
	TestEqual(TEXT("monitor view still resolves camera"), Coordinator->GetSelectedSensorActor(), static_cast<AVirtualSensorActorBase*>(Camera));
	TestEqual(TEXT("settings can resolve LiDAR while monitor shows camera"), Coordinator->GetSelectedSensorActorByKind(EVirtualSensorKind::Lidar), static_cast<AVirtualSensorActorBase*>(Lidar));

	Coordinator->SetViewMode(EVirtualSensorViewMode::Lidar);
	TestEqual(TEXT("monitor view now resolves LiDAR"), Coordinator->GetSelectedSensorActor(), static_cast<AVirtualSensorActorBase*>(Lidar));
	TestEqual(TEXT("settings can resolve camera while monitor shows LiDAR"), Coordinator->GetSelectedSensorActorByKind(EVirtualSensorKind::Camera), static_cast<AVirtualSensorActorBase*>(Camera));
	return true;
}

bool FSensorV2InteractionBudgetTest::RunTest(const FString& Parameters)
{
	FVirtualSensorInteractionRequest Request;
	AVirtualCameraSensorActor* Camera = NewObject<AVirtualCameraSensorActor>();
	Camera->CaptureComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
	TestTrue(TEXT("Camera accepts interaction mode"), Camera->BeginInteractiveManipulation(Request));
	TestEqual(TEXT("Camera interaction uses preview width"), Camera->CaptureComponent->CaptureResolution.X, 640);
	TestEqual(TEXT("Camera interaction suppresses derived output"), Camera->CaptureComponent->CaptureMode, EVirtualCameraCaptureMode::PreviewOnly);
	Camera->EndInteractiveManipulation();
	TestEqual(TEXT("Camera restores FullSpec width"), Camera->CaptureComponent->CaptureResolution.X, 1280);

	AVirtualLidarSensorActor* Lidar = NewObject<AVirtualLidarSensorActor>();
	Lidar->ScanComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
	TestTrue(TEXT("LiDAR accepts interaction mode"), Lidar->BeginInteractiveManipulation(Request));
	TestEqual(TEXT("LiDAR interaction uses preview horizontal samples"), Lidar->ScanComponent->HorizontalSamples, 120);
	TestEqual(TEXT("LiDAR interaction uses preview vertical channels"), Lidar->ScanComponent->VerticalChannels, 24);
	Lidar->EndInteractiveManipulation();
	TestEqual(TEXT("LiDAR restores FullSpec horizontal samples"), Lidar->ScanComponent->HorizontalSamples, 360);
	TestEqual(TEXT("LiDAR restores FullSpec vertical channels"), Lidar->ScanComponent->VerticalChannels, 60);
	return true;
}

bool FSensorV2ExternalSourceHostTest::RunTest(const FString& Parameters)
{
	AVirtualSensorExternalSourceHostActor* Host = NewObject<AVirtualSensorExternalSourceHostActor>();
	TestNotNull(TEXT("CSV replay source"), Host->LidarCsvReplay.Get());
	TestNotNull(TEXT("JSONL replay source"), Host->LidarJsonLinesReplay.Get());
	TestNotNull(TEXT("buffered LiDAR JSON source"), Host->LidarBufferedJson.Get());
	TestNotNull(TEXT("HTTP LiDAR source"), Host->LidarHttpJson.Get());
	TestNotNull(TEXT("UDP LiDAR source"), Host->LidarUdpJson.Get());
	TestNotNull(TEXT("buffered camera JSON source"), Host->CameraBufferedJson.Get());
	TInlineComponentArray<URealSensorSourceComponent*> Sources;
	Host->GetComponents(Sources);
	for (const URealSensorSourceComponent* Source : Sources)
	{
		TestFalse(TEXT("external source does not auto-start"), Source->bAutoStartSource);
		TestFalse(TEXT("external input is not forwarded unless Capture/Export requests it"), Source->bSendTransportByDefault);
	}
	return true;
}

#endif
