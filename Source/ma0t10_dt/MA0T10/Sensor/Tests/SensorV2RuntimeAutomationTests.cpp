#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Json.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "VirtualSlabSensorTestDriver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2RuntimeFeatureSmokeTest,
	"MA0T10.SensorV2.Runtime.FeatureSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2RuntimeContinuousStreamTest,
	"MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UWorld* FindPieWorld()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World()) return Context.World();
	}
	return nullptr;
}

class FSensorV2RuntimeFeatureCheckCommand final : public IAutomationLatentCommand
{
public:
	explicit FSensorV2RuntimeFeatureCheckCommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		if (StartedAtSeconds < 0.0) StartedAtSeconds = FPlatformTime::Seconds();
		if (bScreenshotRequested)
		{
			const bool bScreenshotWritten = IFileManager::Get().FileSize(*ScreenshotPath) > 4096;
			if (!bScreenshotWritten && FPlatformTime::Seconds() - ScreenshotRequestedAtSeconds < 8.0)
			{
				return false;
			}
			Test->TestTrue(TEXT("RHI point-cloud screenshot is written"), bScreenshotWritten);
			return true;
		}
		UWorld* World = FindPieWorld();
		AVirtualSensorCoordinator* Coordinator = nullptr;
		AVirtualLidarSensorActor* Lidar = nullptr;
		AVirtualCameraSensorActor* Camera = nullptr;
		AVirtualSensorUiHostActor* UiHost = nullptr;
		if (World)
		{
			for (TActorIterator<AVirtualSensorCoordinator> It(World); It; ++It) { Coordinator = *It; break; }
			for (TActorIterator<AVirtualLidarSensorActor> It(World); It; ++It) { Lidar = *It; break; }
			for (TActorIterator<AVirtualCameraSensorActor> It(World); It; ++It) { Camera = *It; break; }
			for (TActorIterator<AVirtualSensorUiHostActor> It(World); It; ++It) { UiHost = *It; break; }
		}

		const bool bRuntimeReady = Coordinator && Lidar && Camera && UiHost && UiHost->GetSettingsWidget() &&
			Lidar->ScanComponent && Lidar->ScanComponent->GetRuntimeStatus().FrameId > 0 &&
			Lidar->ScanComponent->GetLastPoints().Num() > 0 && Lidar->ScanComponent->GetLastHitPointCount() > 0;
		if (!bRuntimeReady && FPlatformTime::Seconds() - StartedAtSeconds < 8.0)
		{
			return false;
		}

		Test->TestNotNull(TEXT("PIE coordinator exists"), Coordinator);
		Test->TestNotNull(TEXT("PIE Camera V2 exists"), Camera);
		Test->TestNotNull(TEXT("PIE LiDAR V2 exists"), Lidar);
		Test->TestNotNull(TEXT("PIE UI host exists"), UiHost);
		if (!bRuntimeReady) return true;

		UVirtualLidarScanComponent* Scan = Lidar->ScanComponent;
		if (Lidar->VisualizationComponent)
		{
			// UI preferences are intentionally user-specific. Force the renderer on; NullRHI
			// may either initialize Niagara or select the CPU fallback depending on the RHI.
			Lidar->VisualizationComponent->SetWorldPointCloudEnabled(true);
			Lidar->VisualizationComponent->RefreshLatestFrame();
		}
		Test->TestTrue(TEXT("automatic LiDAR scan completes in PIE"), Scan->GetRuntimeStatus().FrameId > 0);
		Test->TestTrue(TEXT("automatic LiDAR scan produces points"), Scan->GetLastPoints().Num() > 0);
		Test->TestTrue(TEXT("automatic LiDAR scan produces renderable hit points"), Scan->GetLastHitPointCount() > 0);
		const bool bNiagaraActive = Scan->IsGpuPreviewBackendActive();
		Test->TestTrue(TEXT("spatial point-cloud preview is enabled"),
			bNiagaraActive || Scan->IsPointCloudPreviewEnabled());
		if (bNiagaraActive && Lidar->VisualizationComponent)
		{
			Test->TestTrue(TEXT("Niagara preview uploads visible points"), Lidar->VisualizationComponent->GetVisiblePointCount() > 0);
			Test->TestTrue(TEXT("Niagara renderer reports the active path"), Lidar->VisualizationComponent->GetActiveRendererName().Contains(TEXT("Niagara")));
		}

		// A real CPU fallback must contain render instances, not just a non-empty
		// point array. This keeps the button useful even if Niagara compilation or
		// feature-level support is unavailable on the current machine.
		Lidar->VisualizationComponent->SetPointCloudRenderPolicy(ELidarPointCloudRenderPolicy::ForceCpu);
		Lidar->VisualizationComponent->SetWorldPointCloudEnabled(true);
		Lidar->VisualizationComponent->RefreshLatestFrame();
		Test->TestEqual(TEXT("forced CPU reports fallback state"),
			Lidar->VisualizationComponent->GetRendererTelemetry().State,
			ELidarPointCloudRendererState::CpuFallback);

		TArray<UInstancedStaticMeshComponent*> PreviewComponents;
		Lidar->GetComponents<UInstancedStaticMeshComponent>(PreviewComponents);
		UInstancedStaticMeshComponent* PointCloudPreview = nullptr;
		for (UInstancedStaticMeshComponent* Candidate : PreviewComponents)
		{
			if (Candidate && Candidate->GetName().Contains(TEXT("VirtualLidarPointCloudPreview")))
			{
				PointCloudPreview = Candidate;
				break;
			}
		}
		Test->TestNotNull(TEXT("point-cloud ISM preview component is created for fallback"), PointCloudPreview);
		if (PointCloudPreview)
		{
			Test->TestTrue(TEXT("point-cloud ISM has visible instances"), PointCloudPreview->GetInstanceCount() > 0);
			bool bAnyPointProjectsInsideViewport = false;
			APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
			int32 ViewportWidth = 0;
			int32 ViewportHeight = 0;
			if (PlayerController) PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
			if (PointCloudPreview->GetInstanceCount() > 0)
			{
				FTransform InstanceWorldTransform;
				PointCloudPreview->GetInstanceTransform(0, InstanceWorldTransform, true);
				const bool bMatchesMeasuredPoint = Scan->GetLastPoints().ContainsByPredicate([&InstanceWorldTransform](const FVirtualLidarPoint& Point)
				{
					return Point.WorldLocation.Equals(InstanceWorldTransform.GetLocation(), 1.0f);
				});
				Test->TestTrue(TEXT("point-cloud instance uses a measured world location"), bMatchesMeasuredPoint);
			}
			if (PlayerController && ViewportWidth > 0 && ViewportHeight > 0)
			{
				const int32 ProjectionSampleCount = FMath::Min(PointCloudPreview->GetInstanceCount(), 256);
				for (int32 InstanceIndex = 0; InstanceIndex < ProjectionSampleCount; ++InstanceIndex)
				{
					FTransform InstanceWorldTransform;
					FVector2D ScreenPosition;
					if (PointCloudPreview->GetInstanceTransform(InstanceIndex, InstanceWorldTransform, true) &&
						PlayerController->ProjectWorldLocationToScreen(InstanceWorldTransform.GetLocation(), ScreenPosition, false) &&
						ScreenPosition.X >= 0.0f && ScreenPosition.X < ViewportWidth &&
						ScreenPosition.Y >= 0.0f && ScreenPosition.Y < ViewportHeight)
					{
						bAnyPointProjectsInsideViewport = true;
						break;
					}
				}
			}
			Test->TestTrue(TEXT("point-cloud bounds overlap the game viewport"), bAnyPointProjectsInsideViewport);
		}

		UVirtualSensorSettingsPanelWidget* Settings = UiHost->GetSettingsWidget();
		Settings->SelectTargetKind(EVirtualSensorTargetKind::Lidar);
		Test->TestEqual(TEXT("settings selection switches to LiDAR"), Settings->GetPendingState().TargetKind, EVirtualSensorTargetKind::Lidar);
		Test->TestEqual(TEXT("settings selection reads the LiDAR SensorId"), Settings->GetPendingState().SensorId, Scan->SensorId);
		Test->TestTrue(TEXT("settings applies LiDAR FullSpec quality"), Settings->SetSelectedSimulationQuality(EVirtualSensorSimulationQuality::FullSpec));
		Test->TestEqual(TEXT("settings refreshes LiDAR horizontal samples"), Settings->GetPendingState().LidarHorizontalSamples, 360);
		Test->TestEqual(TEXT("settings refreshes LiDAR vertical channels"), Settings->GetPendingState().LidarVerticalChannels, 60);
		Test->TestTrue(TEXT("settings refreshes LiDAR scan interval"), FMath::IsNearlyEqual(Settings->GetPendingState().LidarScanInterval, 0.1f));
		Test->TestEqual(TEXT("runtime LiDAR receives FullSpec horizontal samples"), Scan->HorizontalSamples, 360);
		Test->TestEqual(TEXT("runtime LiDAR receives FullSpec preview stride"), Scan->PreviewPointStride, 6);
		Test->TestNotNull(TEXT("settings creates a runtime transform gizmo"), Settings->GetTransformGizmoActor());
		if (Settings->GetTransformGizmoActor())
		{
			Test->TestEqual(TEXT("gizmo follows the selected LiDAR"), Settings->GetTransformGizmoActor()->GetBoundTargetActor(), static_cast<AActor*>(Lidar));
			Settings->SetSensorManipulationEnabled(true);
			Test->TestTrue(TEXT("sensor manipulation mode enables the gizmo"), Settings->GetTransformGizmoActor()->IsManipulationEnabled());
		}
		const FVector BeforeNudge = Lidar->GetActorLocation();
		Settings->SetSensorCoordinateSpace(EVirtualSensorCoordinateSpace::World);
		Settings->NudgeSelectedSensor(FVector(10.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		Test->TestTrue(TEXT("settings movement changes the selected LiDAR Actor immediately"), Lidar->GetActorLocation().Equals(BeforeNudge + FVector(10.0f, 0.0f, 0.0f), 0.1f));

		Settings->SelectTargetKind(EVirtualSensorTargetKind::Camera);
		Test->TestEqual(TEXT("settings selection switches back to Camera"), Settings->GetPendingState().TargetKind, EVirtualSensorTargetKind::Camera);
		Test->TestEqual(TEXT("settings selection reads the Camera SensorId"), Settings->GetPendingState().SensorId, Camera->GetSensorId());
		Test->TestTrue(TEXT("settings applies Camera FullSpec quality"), Settings->SetSelectedSimulationQuality(EVirtualSensorSimulationQuality::FullSpec));
		Test->TestEqual(TEXT("settings refreshes Camera width"), Settings->GetPendingState().CameraResolution.X, 1280);
		Test->TestEqual(TEXT("settings refreshes Camera height"), Settings->GetPendingState().CameraResolution.Y, 720);
		Test->TestTrue(TEXT("settings refreshes Camera capture interval"), FMath::IsNearlyEqual(Settings->GetPendingState().CameraCaptureInterval, 1.0f / 30.0f));
		Test->TestEqual(TEXT("runtime Camera receives FullSpec width"), Camera->CaptureComponent->CaptureResolution.X, 1280);
		if (Settings->GetTransformGizmoActor())
		{
			Test->TestEqual(TEXT("gizmo follows the selected Camera"), Settings->GetTransformGizmoActor()->GetBoundTargetActor(), static_cast<AActor*>(Camera));
		}

		if (!GUsingNullRHI)
		{
			Settings->SelectTargetKind(EVirtualSensorTargetKind::Lidar);
			Lidar->VisualizationComponent->SetPointCloudRenderPolicy(ELidarPointCloudRenderPolicy::ForceNiagara);
			Lidar->VisualizationComponent->SetWorldPointCloudEnabled(true);
			Lidar->VisualizationComponent->RefreshLatestFrame();
			const FVirtualLidarRendererTelemetry NiagaraTelemetry = Lidar->VisualizationComponent->GetRendererTelemetry();
			Test->TestTrue(TEXT("forced Niagara reports active rendering or an explicit error"),
				NiagaraTelemetry.State == ELidarPointCloudRendererState::NiagaraActive ||
				NiagaraTelemetry.State == ELidarPointCloudRendererState::Error);
			Lidar->VisualizationComponent->SetPointCloudRenderPolicy(ELidarPointCloudRenderPolicy::AutoPreferNiagara);
			Lidar->VisualizationComponent->RefreshLatestFrame();
			const FVirtualLidarRendererTelemetry AutoTelemetry = Lidar->VisualizationComponent->GetRendererTelemetry();
			Test->TestTrue(TEXT("automatic renderer selects live Niagara or CPU fallback"),
				AutoTelemetry.State == ELidarPointCloudRendererState::NiagaraActive ||
				AutoTelemetry.State == ELidarPointCloudRendererState::CpuFallback);
			Test->TestTrue(TEXT("automatic renderer keeps visible hit points"), AutoTelemetry.VisiblePointCount > 0);
			ScreenshotPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Reports/point_cloud_rhi_smoke.png"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
			IFileManager::Get().Delete(*ScreenshotPath, false, true);
			FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
			bScreenshotRequested = true;
			ScreenshotRequestedAtSeconds = FPlatformTime::Seconds();
			return false;
		}
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAtSeconds = -1.0;
	double ScreenshotRequestedAtSeconds = -1.0;
	bool bScreenshotRequested = false;
	FString ScreenshotPath;
};
}

bool FSensorV2RuntimeFeatureSmokeTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"), true))
	{
		AddError(TEXT("SensorRefactorTestMap could not be opened"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FSensorV2RuntimeFeatureCheckCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

namespace
{
class FSensorV2RuntimeContinuousStreamCommand final : public IAutomationLatentCommand
{
public:
	explicit FSensorV2RuntimeContinuousStreamCommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		if (StartedAtSeconds < 0.0) StartedAtSeconds = FPlatformTime::Seconds();
		UWorld* World = FindPieWorld();
		AVirtualSensorCoordinator* Coordinator = nullptr;
		AVirtualLidarSensorActor* Lidar = nullptr;
		AVirtualCameraSensorActor* Camera = nullptr;
		AVirtualSensorExternalSourceHostActor* ReceiverHost = nullptr;
		if (World)
		{
			for (TActorIterator<AVirtualSensorCoordinator> It(World); It; ++It) { Coordinator = *It; break; }
			for (TActorIterator<AVirtualLidarSensorActor> It(World); It; ++It) { Lidar = *It; break; }
			for (TActorIterator<AVirtualCameraSensorActor> It(World); It; ++It) { Camera = *It; break; }
			for (TActorIterator<AVirtualSensorExternalSourceHostActor> It(World); It; ++It) { ReceiverHost = *It; break; }
		}
		if (!Coordinator || !Coordinator->StreamPublisherComponent || !Coordinator->SharedTransportComponent || !Lidar || !Camera)
		{
			if (FPlatformTime::Seconds() - StartedAtSeconds < 8.0) return false;
			Test->AddError(TEXT("SensorRefactorTestMap stream services were not ready."));
			return true;
		}

		UVirtualSensorTransportComponent* Transport = Coordinator->SharedTransportComponent;
		UVirtualSensorStreamPublisherComponent* Publisher = Coordinator->StreamPublisherComponent;
		// The map also starts DTCore WebSocket clients during PIE bootstrap. Match
		// the real UI workflow by allowing those handshakes to settle before the
		// user-facing sensor transport opens its independent STOMP connection.
		if (!bConnectionRequested && FPlatformTime::Seconds() - StartedAtSeconds < 2.0) return false;
		if (!bConnectionRequested)
		{
			FVirtualSensorTransportProfile Profile;
			const FString BrokerUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_URL"));
			const FString UserName = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_USER"));
			if (!BrokerUrl.IsEmpty()) Profile.BrokerUrl = BrokerUrl;
			if (!UserName.IsEmpty()) Profile.UserName = UserName;
			Profile.MaxMessageBytes = 8 * 1024 * 1024;
			Transport->ConfigureTransportProfile(Profile);
			Transport->SetSessionCredentials(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_PASSWORD")), FString());
			Transport->TransportMode = EVirtualSensorTransportMode::StompWebSocket;
			if (ReceiverHost)
			{
				ReceiverHost->ConfigureReceiverTopics(Profile.LidarTopic, Profile.CameraTopic, Profile.ExportTopic);
				// Raw TCP self-validation replaces the game-thread WebSocket receivers
				// during the performance acceptance workload.
				ReceiverHost->StartTopicReceivers();
			}
			bConnectionRequested = true;
			return false;
		}

		if (!bStreamsStarted)
		{
			bPointCloudOnly=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD_ONLY")).Equals(TEXT("1"));
			bScenarioMode=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RUN_SLAB_SCENARIO_SMOKE")).Equals(TEXT("1"));
			// Exercise the production contract, not the legacy CSV envelope used by
			// the early three-stream smoke. Applying the profile once keeps the
			// acquisition at 576x56, 20 Hz while the stream subscribes to completed
			// immutable frames without triggering an extra scan.
			Lidar->ScanComponent->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);
			Lidar->ScanComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
			// Acceptance is exactly one D455 FullSpec camera plus one ML-X(80)
			// Native LiDAR. The overhead test camera remains in the map but is
			// stopped so it cannot alter the declared workload.
			bool bPrimaryCameraConfigured = false;
			for (TActorIterator<AVirtualCameraSensorActor> It(World); It; ++It)
			{
				if (!It->CaptureComponent) continue;
				if (!bPrimaryCameraConfigured)
				{
					Camera = *It;
					It->CaptureComponent->ApplyDeviceProfile(EVirtualCameraDeviceProfile::IntelRealSenseD455);
					It->CaptureComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
					It->CaptureComponent->CaptureMode = EVirtualCameraCaptureMode::PreviewOnly;
					bPrimaryCameraConfigured = true;
				}
				else
				{
					It->CaptureComponent->StopCapture();
				}
			}
			Lidar->ScanComponent->ServerPayloadStride = 32;
			Lidar->ScanComponent->MaxServerPayloadPoints = 1024;
			Lidar->ScanComponent->bIncludeMissPointsInServerPayload = false;
			if (Lidar->VisualizationComponent)
			{
				Lidar->VisualizationComponent->SetWorldPointCloudEnabled(false);
			}
			for (EVirtualSensorStreamKind Kind : {EVirtualSensorStreamKind::LidarPayload, EVirtualSensorStreamKind::CameraImage, EVirtualSensorStreamKind::PointCloud})
			{
				FVirtualSensorStreamConfig Config;
				Config.StreamKind = Kind;
				Config.bEnabled = !bScenarioMode && (!bPointCloudOnly || Kind==EVirtualSensorStreamKind::PointCloud);
				Config.TransportBackend = EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;
				Config.FrameStride = 1;
				Config.ReceiptSampleInterval = 1;
				Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
				Config.PcdDataMode = EVirtualPcdDataMode::Binary;
				Config.DeliveryMode = Kind == EVirtualSensorStreamKind::PointCloud
					? EVirtualPointCloudDeliveryMode::ConnectedNoLoss
					: EVirtualPointCloudDeliveryMode::LatestFrame;
				Config.MaxBufferedFrames = 20;
				Config.MaxReceiptRetries = 3;
				Publisher->ConfigureStream(Config);
			}
			const FString RequestedWarmup = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_STREAM_WARMUP_SECONDS"));
			const FString RequestedSeconds = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_STREAM_MEASURE_SECONDS"));
			WarmupSeconds = RequestedWarmup.IsEmpty() ? 10.0 : FMath::Clamp(FCString::Atod(*RequestedWarmup), 1.0, 3600.0);
			MeasurementSeconds = RequestedSeconds.IsEmpty() ? 60.0 : FMath::Clamp(FCString::Atod(*RequestedSeconds), 5.0, 3600.0);
			StreamsStartedAtSeconds = FPlatformTime::Seconds();
			bStreamsStarted = true;
			return false;
		}
		if (bScenarioMode)
		{
			if (!ScenarioDriver.IsValid() && FPlatformTime::Seconds()-StreamsStartedAtSeconds>=WarmupSeconds)
			{
				ScenarioDriver=World->SpawnActor<AVirtualSlabSensorTestDriver>();
				Test->TestTrue(TEXT("bulk fixture begins"), ScenarioDriver->StartTest(2,{Camera->GetSensorId(),Lidar->GetSensorId()},bPointCloudOnly));
			}
			if (ScenarioDriver.IsValid() && ScenarioDriver->HasFailed()) { Test->AddError(TEXT("Slab fixture failed; inspect session_runs.json")); return true; }
		}

		TMap<EVirtualSensorStreamKind, FVirtualSensorStreamStatus> StatusByKind;
		for (const FVirtualSensorStreamStatus& Status : Publisher->GetStreamStatuses())
		{
			if(bPointCloudOnly && Status.StreamKind!=EVirtualSensorStreamKind::PointCloud) continue;
			if ((!bScenarioMode && Status.SensorId.IsEmpty()) || (bScenarioMode && !Status.SensorId.IsEmpty())) StatusByKind.Add(Status.StreamKind, Status);
		}
		bool bAllReady = StatusByKind.Num() == (bPointCloudOnly?1:3);
		for (EVirtualSensorStreamKind Kind : {EVirtualSensorStreamKind::LidarPayload, EVirtualSensorStreamKind::CameraImage, EVirtualSensorStreamKind::PointCloud})
		{
			if(bPointCloudOnly && Kind!=EVirtualSensorStreamKind::PointCloud) continue;
			const FVirtualSensorStreamStatus* Status = StatusByKind.Find(Kind);
			bAllReady &= Status && Status->InputFrameCount >= 2 && Status->SubmittedFrameCount >= 2 &&
				Status->ReceiptReceivedCount >= 2 && Status->ConsumerReceivedCount >= 2 &&
				Status->ConsumerValidationFailureCount == 0;
		}
		const double StreamElapsedSeconds = FPlatformTime::Seconds() - StreamsStartedAtSeconds;
		if (StreamElapsedSeconds >= WarmupSeconds)
		{
			auto* DepthCapture = Lidar->FindComponentByClass<UVirtualLidarGpuDepthProjectionComponent>();
			if (!bCaptureViewStatesChecked)
			{
				bCaptureViewStatesChecked = true;
				Test->TestTrue(TEXT("scheduled Camera retains render state"), Camera->CaptureComponent->bAlwaysPersistRenderingState);
				Test->TestTrue(TEXT("scheduled depth capture retains render state"), DepthCapture && DepthCapture->bAlwaysPersistRenderingState);
				CameraViewState = Camera->CaptureComponent->GetViewState(0);
				LidarViewState = DepthCapture ? DepthCapture->GetViewState(0) : nullptr;
				Test->TestNotNull(TEXT("Camera has persistent view state"), CameraViewState);
				Test->TestNotNull(TEXT("LiDAR has persistent view state"), LidarViewState);
			}
			if (CameraViewState != Camera->CaptureComponent->GetViewState(0) ||
				!DepthCapture || LidarViewState != DepthCapture->GetViewState(0))
			{
				Test->AddError(TEXT("Sensor capture view state was replaced during streaming"));
				return true;
			}
			const double SampleNow = FPlatformTime::Seconds();
			const double GameFrameMs = FApp::GetDeltaTime() * 1000.0;
			if (GameFrameMs > 0.0 && GameFrameMs < 1000.0) FrameTimesMs.Add(GameFrameMs);
			if (LastFrameSampleSeconds > 0.0) WallPacingTimesMs.Add((SampleNow - LastFrameSampleSeconds) * 1000.0);
			LastFrameSampleSeconds = SampleNow;
		}
		if ((!bAllReady || StreamElapsedSeconds < WarmupSeconds + MeasurementSeconds) &&
			StreamElapsedSeconds < WarmupSeconds + MeasurementSeconds + 15.0) return false;
		if (bScenarioMode && ScenarioDriver.IsValid() && !ScenarioDriver->IsFinished() && StreamElapsedSeconds<100.0) return false;
		if (bScenarioMode) Test->TestTrue(TEXT("two distinct 30-second sessions finish and drain"),ScenarioDriver.IsValid() && ScenarioDriver->IsFinished() && ScenarioDriver->GetCompletedRuns()==2);

		if (!bAcquisitionStopped)
		{
			Coordinator->StopAllSensors();
			bAcquisitionStopped = true;
			DrainStartedAtSeconds = FPlatformTime::Seconds();
			return false;
		}
		const FVirtualSensorStreamStatus* PointCloudBeforeAssertions = StatusByKind.Find(EVirtualSensorStreamKind::PointCloud);
		const bool bPointCloudDrained = PointCloudBeforeAssertions && !PointCloudBeforeAssertions->bProcessing &&
			PointCloudBeforeAssertions->InputQueueDepth == 0 && PointCloudBeforeAssertions->PreparedQueueDepth == 0 &&
			PointCloudBeforeAssertions->ReceiptQueueDepth == 0 &&
			PointCloudBeforeAssertions->InputFrameCount == PointCloudBeforeAssertions->SerializedFrameCount &&
			PointCloudBeforeAssertions->InputFrameCount == PointCloudBeforeAssertions->SubmittedFrameCount &&
			PointCloudBeforeAssertions->InputFrameCount == PointCloudBeforeAssertions->ReceiptReceivedCount &&
			PointCloudBeforeAssertions->ConsumerReceivedCount == PointCloudBeforeAssertions->SubmittedFrameCount;
		if (!bPointCloudDrained && FPlatformTime::Seconds() - DrainStartedAtSeconds < 35.0) return false;

		Test->TestEqual(TEXT("three global stream runtimes are active"), StatusByKind.Num(), bPointCloudOnly?1:3);
		for (EVirtualSensorStreamKind Kind : {EVirtualSensorStreamKind::LidarPayload, EVirtualSensorStreamKind::CameraImage, EVirtualSensorStreamKind::PointCloud})
		{
			if(bPointCloudOnly && Kind!=EVirtualSensorStreamKind::PointCloud) continue;
			const FVirtualSensorStreamStatus* Status = StatusByKind.Find(Kind);
			Test->TestNotNull(TEXT("stream status exists"), Status);
			if (!Status) continue;
			Test->TestTrue(TEXT("stream receives repeated acquisition frames"), Status->InputFrameCount >= 2);
			Test->TestTrue(TEXT("stream submits repeated broker messages"), Status->SubmittedFrameCount >= 2);
			Test->TestEqual(TEXT("every high-throughput submission receives a receipt"), Status->ReceiptReceivedCount, Status->SubmittedFrameCount);
			Test->TestEqual(TEXT("self receiver validates every submitted frame"), Status->ConsumerReceivedCount, Status->SubmittedFrameCount);
			Test->TestEqual(TEXT("self receiver reports no invalid frames"), Status->ConsumerValidationFailureCount, static_cast<int64>(0));
			Test->TestEqual(TEXT("self receiver reports no FrameId gaps"), Status->ConsumerFrameGapCount, static_cast<int64>(0));
			Test->TestEqual(TEXT("self receiver reports no duplicates"), Status->ConsumerDuplicateCount, static_cast<int64>(0));
			Test->TestEqual(TEXT("stream encoding stays healthy"), Status->EncodeFailureCount, static_cast<int64>(0));
			Test->TestEqual(TEXT("stream receipt stays healthy"), Status->ReceiptTimeoutCount, static_cast<int64>(0));
			Test->TestEqual(TEXT("stream queue does not overload"), Status->OverloadCount, static_cast<int64>(0));
			if (Kind == EVirtualSensorStreamKind::CameraImage)
			{
				Test->TestTrue(TEXT("D455 JPEG submission sustains at least 29 Hz"), Status->SubmittedHz >= 29.0f);
				Test->TestTrue(TEXT("D455 JPEG consumer sustains at least 29 Hz"), Status->ConsumerReceivedHz >= 29.0f);
				Test->TestTrue(TEXT("D455 JPEG end-to-end p95 remains below 250 ms"), Status->EndToEndP95LatencyMs <= 250.0f);
			}
			else
			{
				Test->TestTrue(TEXT("ML-X stream submission sustains at least 19 Hz"), Status->SubmittedHz >= 19.0f);
				Test->TestTrue(TEXT("ML-X stream consumer sustains at least 19 Hz"), Status->ConsumerReceivedHz >= 19.0f);
			}
			if (Kind == EVirtualSensorStreamKind::PointCloud)
			{
				Test->TestEqual(TEXT("binary PCD keeps every publisher input frame"), Status->FrameGapCount, static_cast<int64>(0));
				Test->TestEqual(TEXT("every binary PCD input is serialized"), Status->SerializedFrameCount, Status->InputFrameCount);
				Test->TestEqual(TEXT("every binary PCD input is submitted"), Status->SubmittedFrameCount, Status->InputFrameCount);
				Test->TestEqual(TEXT("every binary PCD submission receives a receipt"), Status->ReceiptReceivedCount, Status->InputFrameCount);
				Test->TestEqual(TEXT("binary PCD never replaces a pending frame"), Status->ReplacedPendingFrameCount, static_cast<int64>(0));
				Test->TestEqual(TEXT("binary PCD queue does not overload"), Status->OverloadCount, static_cast<int64>(0));
				Test->TestTrue(TEXT("binary PCD serialization sustains at least 19 Hz"), Status->SerializationHz >= 19.0f);
				Test->TestTrue(TEXT("binary PCD submission sustains at least 19 Hz"), Status->SubmittedHz >= 19.0f);
				Test->TestTrue(TEXT("binary PCD serialization p95 remains below 20 ms"), Status->SerializationP95LatencyMs <= 20.0f);
				Test->TestTrue(TEXT("binary PCD queues remain bounded"),
					Status->InputQueueDepth <= 20 && Status->PreparedQueueDepth <= 20 && Status->ReceiptQueueDepth <= 20);
			}
		}
		if (ReceiverHost && PointCloudBeforeAssertions)
		{
			Test->TestTrue(TEXT("actual UI receiver remains enabled"),ReceiverHost->AreTopicReceiversRequested());
			Test->TestTrue(TEXT("UI shares actual broker MESSAGE validation"),ReceiverHost->UsesSharedReceiver());
			for(const auto& R:ReceiverHost->GetTopicReceiverStatuses())
			{
				if(R.Kind==EVirtualSensorTopicReceiveKind::PointCloud)
				{
					Test->TestEqual(TEXT("UI validates every submitted PCD"),R.ValidatedCount,PointCloudBeforeAssertions->SubmittedFrameCount);
					Test->TestEqual(TEXT("UI PCD validation failures"),R.ValidationFailureCount,static_cast<int64>(0));
					Test->TestEqual(TEXT("UI diagnostic losses"),R.DiagnosticDropCount,static_cast<int64>(0));
					UE_LOG(LogTemp,Display,TEXT("[SensorUiReceiverRhi] pcdReceived=%lld pcdValidated=%lld failures=%lld gaps=%lld"),R.ReceivedCount,R.ValidatedCount,R.ValidationFailureCount,R.FrameGapCount);
				}
				else if(bPointCloudOnly) Test->TestEqual(TEXT("PCD-only does not receive camera/lidar diagnostics"),R.ReceivedCount,static_cast<int64>(0));
			}
			for(const auto& E:ReceiverHost->GetRecentTopicReceiveLogs())
				if(E.Kind==EVirtualSensorTopicReceiveKind::PointCloud && bScenarioMode)
				{
					Test->TestEqual(TEXT("UI preserves material id"),E.MtlNo,FString(TEXT("SQ83521 047")));
					Test->TestTrue(TEXT("UI preserves independent Slab frame"),E.SlabFrameNo>=0&&E.SlabFrameNo<600&&!E.RunId.IsEmpty());
				}
		}
		if (PointCloudBeforeAssertions)
		{
			UE_LOG(LogTemp, Display, TEXT("[SensorPcdNoLossRhi] input=%lld serialized=%lld submitted=%lld receipts=%lld serializeHz=%.2f serializeP95Ms=%.2f inputQueue=%d preparedQueue=%d receiptQueue=%d gaps=%lld retries=%lld overload=%lld"),
				PointCloudBeforeAssertions->InputFrameCount, PointCloudBeforeAssertions->SerializedFrameCount,
				PointCloudBeforeAssertions->SubmittedFrameCount, PointCloudBeforeAssertions->ReceiptReceivedCount,
				PointCloudBeforeAssertions->SerializationHz, PointCloudBeforeAssertions->SerializationP95LatencyMs,
				PointCloudBeforeAssertions->InputQueueDepth, PointCloudBeforeAssertions->PreparedQueueDepth,
				PointCloudBeforeAssertions->ReceiptQueueDepth, PointCloudBeforeAssertions->FrameGapCount,
				PointCloudBeforeAssertions->RetryCount, PointCloudBeforeAssertions->OverloadCount);
		}
		const TSharedPtr<const TArray64<uint8>, ESPMode::ThreadSafe> CameraJpeg = Camera->CaptureComponent
			? Camera->CaptureComponent->GetLastJpegSnapshot() : nullptr;
		if(!bPointCloudOnly) Test->TestTrue(TEXT("camera stream retains a raw JPEG snapshot"),
			CameraJpeg.IsValid() && CameraJpeg->Num() >= 4 && (*CameraJpeg)[0] == 0xff && (*CameraJpeg)[1] == 0xd8);
		Test->TestTrue(TEXT("point-cloud stream is fed by measured LiDAR hits"),
			Lidar->ScanComponent && Lidar->ScanComponent->GetLastHitPointCount() > 0);
		const FVirtualSensorStreamStatus* CameraStatus = StatusByKind.Find(EVirtualSensorStreamKind::CameraImage);
		const FVirtualSensorStreamStatus* LidarStatus = StatusByKind.Find(EVirtualSensorStreamKind::LidarPayload);
		UE_LOG(LogTemp, Display, TEXT("[SensorHighThroughputRhi] cameraSubmitted=%lld cameraReceipt=%lld cameraConsumer=%lld cameraHz=%.2f lidarSubmitted=%lld lidarReceipt=%lld lidarConsumer=%lld lidarHz=%.2f pcdSubmitted=%lld pcdReceipt=%lld pcdConsumer=%lld pcdHz=%.2f"),
			CameraStatus ? CameraStatus->SubmittedFrameCount : 0, CameraStatus ? CameraStatus->ReceiptReceivedCount : 0,
			CameraStatus ? CameraStatus->ConsumerReceivedCount : 0, CameraStatus ? CameraStatus->SubmittedHz : 0.0f,
			LidarStatus ? LidarStatus->SubmittedFrameCount : 0, LidarStatus ? LidarStatus->ReceiptReceivedCount : 0,
			LidarStatus ? LidarStatus->ConsumerReceivedCount : 0, LidarStatus ? LidarStatus->SubmittedHz : 0.0f,
			PointCloudBeforeAssertions ? PointCloudBeforeAssertions->SubmittedFrameCount : 0,
			PointCloudBeforeAssertions ? PointCloudBeforeAssertions->ReceiptReceivedCount : 0,
			PointCloudBeforeAssertions ? PointCloudBeforeAssertions->ConsumerReceivedCount : 0,
			PointCloudBeforeAssertions ? PointCloudBeforeAssertions->SubmittedHz : 0.0f);
		Test->TestTrue(TEXT("stream performance collected enough rendered frames"), FrameTimesMs.Num() >= 120);
		Test->TestFalse(TEXT("benchmark must not use a synthetic fixed timestep"),FApp::UseFixedTimeStep() || GEngine->bUseFixedFrameRate);
		if (!FrameTimesMs.IsEmpty())
		{
			double TotalFrameMs = 0.0;
			for (const double FrameMs : FrameTimesMs) TotalFrameMs += FrameMs;
			TArray<double> SortedFrameTimes = FrameTimesMs;
			SortedFrameTimes.Sort();
			const double AverageFrameMs = TotalFrameMs / FrameTimesMs.Num();
			const double AverageFps = 1000.0 / FMath::Max(0.001, AverageFrameMs);
			const double P95FrameMs = SortedFrameTimes[FMath::Clamp(FMath::CeilToInt(SortedFrameTimes.Num() * 0.95) - 1, 0, SortedFrameTimes.Num() - 1)];
			const double P99FrameMs = SortedFrameTimes[FMath::Clamp(FMath::CeilToInt(SortedFrameTimes.Num() * 0.99) - 1, 0, SortedFrameTimes.Num() - 1)];
			const double OnePercentLowFps = 1000.0 / FMath::Max(0.001, P99FrameMs);
			UE_LOG(LogTemp, Display, TEXT("[SensorStreamRhi] averageFps=%.2f onePercentLowFps=%.2f p95FrameMs=%.2f samples=%d"),
				AverageFps, OnePercentLowFps, P95FrameMs, FrameTimesMs.Num());
			Test->TestTrue(TEXT("active three-stream average FPS remains at least 55"), AverageFps >= 55.0);
			Test->TestTrue(TEXT("active three-stream one-percent-low FPS remains at least 45"), OnePercentLowFps >= 45.0);
			Test->TestTrue(TEXT("active three-stream p95 frame time remains at most 20 ms"), P95FrameMs <= 20.0);
		}
		if (!WallPacingTimesMs.IsEmpty())
		{
			double TotalWallMs = 0.0;
			for (const double FrameMs : WallPacingTimesMs) TotalWallMs += FrameMs;
			TArray<double> SortedWallTimes = WallPacingTimesMs;
			SortedWallTimes.Sort();
			const double AverageWallFps = 1000.0 / FMath::Max(0.001, TotalWallMs / WallPacingTimesMs.Num());
			const double WallP95Ms = SortedWallTimes[FMath::Clamp(FMath::CeilToInt(SortedWallTimes.Num() * 0.95) - 1, 0, SortedWallTimes.Num() - 1)];
			UE_LOG(LogTemp, Display, TEXT("[SensorStreamWallPacing] averageFps=%.2f p95CallbackMs=%.2f samples=%d"),
				AverageWallFps, WallP95Ms, WallPacingTimesMs.Num());
		}
		Publisher->StopAllStreams(FString());
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAtSeconds = -1.0;
	double StreamsStartedAtSeconds = -1.0;
	bool bConnectionRequested = false;
	bool bStreamsStarted = false;
	bool bAcquisitionStopped = false;
	bool bScenarioMode=false;
	bool bPointCloudOnly=false;
	bool bCaptureViewStatesChecked = false;
	FSceneViewStateInterface* CameraViewState = nullptr;
	FSceneViewStateInterface* LidarViewState = nullptr;
	TWeakObjectPtr<AVirtualSlabSensorTestDriver> ScenarioDriver;
	double DrainStartedAtSeconds = -1.0;
	double LastFrameSampleSeconds = -1.0;
	double MeasurementSeconds = 60.0;
	double WarmupSeconds = 10.0;
	TArray<double> FrameTimesMs;
	TArray<double> WallPacingTimesMs;
};
}

bool FSensorV2RuntimeContinuousStreamTest::RunTest(const FString& Parameters)
{
	if (!FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE")).Equals(TEXT("1")))
	{
		AddInfo(TEXT("Continuous SensorRefactorTestMap stream smoke skipped. Use Scripts/run_sensor_map_stream_rhi_smoke.ps1."));
		return true;
	}
	// The integration runner provides credentials through process environment
	// variables. Apply them to the in-memory DTCore override before PIE creates
	// its GameInstance subsystem; do not write local Config/Game.ini.
	if (GConfig)
	{
		const FString BrokerUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_URL"));
		const FString UserName = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_USER"));
		const FString Password = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_PASSWORD"));
		GConfig->SetString(TEXT("DTCoreRuntimeOverride"), TEXT("WebSocketUrl"), *BrokerUrl, GGameIni);
		GConfig->SetString(TEXT("DTCoreRuntimeOverride"), TEXT("WebSocketLogin"), *UserName, GGameIni);
		GConfig->SetString(TEXT("DTCoreRuntimeOverride"), TEXT("WebSocketPasscode"), *Password, GGameIni);
	}
	if (!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"), true))
	{
		AddError(TEXT("SensorRefactorTestMap could not be opened"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FSensorV2RuntimeContinuousStreamCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
