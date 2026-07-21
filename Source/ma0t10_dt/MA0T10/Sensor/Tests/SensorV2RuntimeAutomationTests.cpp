#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Json.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"

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
		if (!Coordinator || !Coordinator->StreamPublisherComponent || !Coordinator->SharedTransportComponent || !Lidar || !Camera || !ReceiverHost)
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
			Profile.MaxMessageBytes = 32 * 1024 * 1024;
			Transport->ConfigureTransportProfile(Profile);
			Transport->SetSessionCredentials(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_PASSWORD")), FString());
			Transport->TransportMode = EVirtualSensorTransportMode::StompWebSocket;
			ReceiverHost->ConfigureReceiverTopics(Profile.LidarTopic, Profile.CameraTopic, Profile.ExportTopic);
			Transport->TestConnection();
			bConnectionRequested = true;
			return false;
		}
		if (!Transport->IsStompConnected())
		{
			if (FPlatformTime::Seconds() - StartedAtSeconds < 12.0) return false;
			Test->AddError(TEXT("SensorRefactorTestMap could not connect to the Artemis STOMP broker."));
			return true;
		}

		if (!bStreamsStarted)
		{
			for (EVirtualSensorStreamKind Kind : {EVirtualSensorStreamKind::LidarPayload, EVirtualSensorStreamKind::CameraImage, EVirtualSensorStreamKind::PointCloud})
			{
				FVirtualSensorStreamConfig Config;
				Config.StreamKind = Kind;
				Config.bEnabled = true;
				Config.FrameStride = 1;
				Config.ReceiptSampleInterval = 2;
				Config.PointCloudFormat = EVirtualPointCloudStreamFormat::CSV;
				Publisher->ConfigureStream(Config);
			}
			StreamsStartedAtSeconds = FPlatformTime::Seconds();
			bStreamsStarted = true;
			return false;
		}

		TMap<EVirtualSensorStreamKind, FVirtualSensorStreamStatus> StatusByKind;
		for (const FVirtualSensorStreamStatus& Status : Publisher->GetStreamStatuses())
		{
			if (Status.SensorId.IsEmpty()) StatusByKind.Add(Status.StreamKind, Status);
		}
		bool bAllReady = StatusByKind.Num() == 3;
		for (EVirtualSensorStreamKind Kind : {EVirtualSensorStreamKind::LidarPayload, EVirtualSensorStreamKind::CameraImage, EVirtualSensorStreamKind::PointCloud})
		{
			const FVirtualSensorStreamStatus* Status = StatusByKind.Find(Kind);
			bAllReady &= Status && Status->InputFrameCount >= 2 && Status->SubmittedFrameCount >= 2 && Status->ReceiptReceivedCount >= 1;
		}
		const TArray<FVirtualSensorTopicReceiverStatus> ReceiverStatuses = ReceiverHost->GetTopicReceiverStatuses();
		bAllReady &= ReceiverStatuses.Num() == 3;
		for (const FVirtualSensorTopicReceiverStatus& Status : ReceiverStatuses)
		{
			bAllReady &= Status.State == EVirtualSensorTopicReceiverState::Active && Status.ValidatedCount >= 2;
		}
		const double StreamElapsedSeconds = FPlatformTime::Seconds() - StreamsStartedAtSeconds;
		if (StreamElapsedSeconds >= 2.0)
		{
			const double SampleNow = FPlatformTime::Seconds();
			if (LastFrameSampleSeconds > 0.0) FrameTimesMs.Add((SampleNow - LastFrameSampleSeconds) * 1000.0);
			LastFrameSampleSeconds = SampleNow;
		}
		if ((!bAllReady || StreamElapsedSeconds < 10.0) && StreamElapsedSeconds < 15.0) return false;

		Test->TestEqual(TEXT("three global stream runtimes are active"), StatusByKind.Num(), 3);
		for (EVirtualSensorStreamKind Kind : {EVirtualSensorStreamKind::LidarPayload, EVirtualSensorStreamKind::CameraImage, EVirtualSensorStreamKind::PointCloud})
		{
			const FVirtualSensorStreamStatus* Status = StatusByKind.Find(Kind);
			Test->TestNotNull(TEXT("stream status exists"), Status);
			if (!Status) continue;
			Test->TestTrue(TEXT("stream receives repeated acquisition frames"), Status->InputFrameCount >= 2);
			Test->TestTrue(TEXT("stream submits repeated broker messages"), Status->SubmittedFrameCount >= 2);
			Test->TestTrue(TEXT("sampled broker receipt is correlated"), Status->ReceiptReceivedCount >= 1);
			Test->TestEqual(TEXT("stream encoding stays healthy"), Status->EncodeFailureCount, static_cast<int64>(0));
			Test->TestEqual(TEXT("stream receipt stays healthy"), Status->ReceiptTimeoutCount, static_cast<int64>(0));
		}
		TSharedPtr<FJsonObject> CameraPayload;
		FString CameraEncoding;
		FString CameraImage;
		const FString CameraJson = Camera->CaptureComponent ? Camera->CaptureComponent->GetLastJsonPayload() : FString();
		const bool bCameraPayloadParsed = FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(CameraJson), CameraPayload) &&
			CameraPayload.IsValid() && CameraPayload->TryGetStringField(TEXT("encoding"), CameraEncoding) &&
			CameraPayload->TryGetStringField(TEXT("image"), CameraImage);
		Test->TestTrue(TEXT("camera stream payload contains a Base64 JPEG"),
			bCameraPayloadParsed && CameraEncoding == TEXT("jpeg/base64") && !CameraImage.IsEmpty());
		Test->TestTrue(TEXT("point-cloud stream is fed by measured LiDAR hits"),
			Lidar->ScanComponent && Lidar->ScanComponent->GetLastHitPointCount() > 0);
		Test->TestEqual(TEXT("three internal DTCore Topic receivers are present"), ReceiverStatuses.Num(), 3);
		for (const FVirtualSensorTopicReceiverStatus& Status : ReceiverStatuses)
		{
			Test->TestEqual(TEXT("internal receiver stays active"), Status.State, EVirtualSensorTopicReceiverState::Active);
			Test->TestTrue(TEXT("internal receiver validates repeated frames"), Status.ValidatedCount >= 2);
			Test->TestEqual(TEXT("internal receiver has no validation failures"), Status.ValidationFailureCount, static_cast<int64>(0));
			Test->TestTrue(TEXT("internal receiver retains at most bounded work"), Status.ReplacedPendingCount >= 0);
		}
		UE_LOG(LogTemp, Display, TEXT("[SensorTopicReceiverRhi] lidar=%lld camera=%lld pointcloud=%lld failures=%lld"),
			ReceiverStatuses.IsValidIndex(0) ? ReceiverStatuses[0].ValidatedCount : 0,
			ReceiverStatuses.IsValidIndex(1) ? ReceiverStatuses[1].ValidatedCount : 0,
			ReceiverStatuses.IsValidIndex(2) ? ReceiverStatuses[2].ValidatedCount : 0,
			ReceiverStatuses.IsValidIndex(0) && ReceiverStatuses.IsValidIndex(1) && ReceiverStatuses.IsValidIndex(2)
				? ReceiverStatuses[0].ValidationFailureCount + ReceiverStatuses[1].ValidationFailureCount + ReceiverStatuses[2].ValidationFailureCount : -1);
		Test->TestTrue(TEXT("stream performance collected enough rendered frames"), FrameTimesMs.Num() >= 120);
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
		Publisher->StopAllStreams(FString());
		ReceiverHost->StopTopicReceivers();
		for (const FVirtualSensorTopicReceiverStatus& Status : ReceiverHost->GetTopicReceiverStatuses())
		{
			Test->TestEqual(TEXT("manual receiver stop clears every subscription state"), Status.State, EVirtualSensorTopicReceiverState::Stopped);
		}
		ReceiverHost->ReconnectTopicReceivers();
		Test->TestTrue(TEXT("manual receiver reconnect re-enables requested state"), ReceiverHost->AreTopicReceiversRequested());
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAtSeconds = -1.0;
	double StreamsStartedAtSeconds = -1.0;
	bool bConnectionRequested = false;
	bool bStreamsStarted = false;
	double LastFrameSampleSeconds = -1.0;
	TArray<double> FrameTimesMs;
};
}

bool FSensorV2RuntimeContinuousStreamTest::RunTest(const FString& Parameters)
{
	if (!FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE")).Equals(TEXT("1")))
	{
		AddInfo(TEXT("Continuous SensorRefactorTestMap stream smoke skipped. Use Scripts/run_sensor_map_stream_rhi_smoke.ps1."));
		return true;
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
