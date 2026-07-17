#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2RuntimeFeatureSmokeTest,
	"MA0T10.SensorV2.Runtime.FeatureSmoke",
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
			Lidar->ScanComponent->GetLastPoints().Num() > 0;
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
		Test->TestTrue(TEXT("automatic LiDAR scan completes in PIE"), Scan->GetRuntimeStatus().FrameId > 0);
		Test->TestTrue(TEXT("automatic LiDAR scan produces points"), Scan->GetLastPoints().Num() > 0);
		Test->TestTrue(TEXT("spatial point-cloud preview is enabled"), Scan->IsPointCloudPreviewEnabled());

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
		Test->TestNotNull(TEXT("point-cloud ISM preview component is created"), PointCloudPreview);
		if (PointCloudPreview)
		{
			Test->TestTrue(TEXT("point-cloud ISM has visible instances"), PointCloudPreview->GetInstanceCount() > 0);
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
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAtSeconds = -1.0;
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

#endif
