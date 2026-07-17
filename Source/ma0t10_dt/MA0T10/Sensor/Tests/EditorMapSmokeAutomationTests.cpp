#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorMapAssetsLoadTest, "MA0T10.EditorSmoke.MapAssetsLoad", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorMapSensorCompositionTest, "MA0T10.EditorSmoke.MapSensorComposition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorV2RefactorMapCompositionTest, "MA0T10.SensorV2.EditorSmoke.RefactorMapComposition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
struct FEditorSmokeMapSummary
{
    int32 ManagerCount = 0;
    int32 LidarActorCount = 0;
    int32 CameraActorCount = 0;
    int32 MonitorHostCount = 0;
    int32 ReadyMonitorHostCount = 0;
    int32 SensorTestTargetCount = 0;
    int32 HallWallCount = 0;
    int32 CeilingRectLightCount = 0;
    int32 SkyLightCount = 0;
    bool bCameraPlacementRealistic = false;
    bool bLidarPlacementRealistic = false;
    bool bLegacyLidarDebugDisabled = false;
    bool bLidarPointCloudPreviewEnabled = false;
};

UWorld* LoadSmokeMap(FAutomationTestBase& Test, const TCHAR* MapObjectPath)
{
    UWorld* LoadedWorld = LoadObject<UWorld>(nullptr, MapObjectPath);
    Test.TestNotNull(FString::Printf(TEXT("map asset loads: %s"), MapObjectPath), LoadedWorld);
    if (!LoadedWorld)
    {
        return nullptr;
    }

    ULevel* PersistentLevel = LoadedWorld->PersistentLevel.Get();
    Test.TestNotNull(FString::Printf(TEXT("map has persistent level: %s"), MapObjectPath), PersistentLevel);
    if (PersistentLevel)
    {
        Test.TestTrue(FString::Printf(TEXT("map has at least one actor slot: %s"), MapObjectPath), PersistentLevel->Actors.Num() > 0);
    }
    return LoadedWorld;
}

bool TestMapAssetLoads(FAutomationTestBase& Test, const TCHAR* MapObjectPath)
{
    return LoadSmokeMap(Test, MapObjectPath) != nullptr;
}

void AddMapComposition(UWorld* LoadedWorld, FEditorSmokeMapSummary& Summary)
{
    if (!LoadedWorld || !LoadedWorld->PersistentLevel)
    {
        return;
    }

    for (AActor* Actor : LoadedWorld->PersistentLevel->Actors)
    {
        if (!Actor)
        {
            continue;
        }
        if (Actor->IsA<AVirtualSensorCoordinator>())
        {
            ++Summary.ManagerCount;
        }
        if (Actor->IsA<AVirtualLidarSensorActor>())
        {
            ++Summary.LidarActorCount;
            const AVirtualLidarSensorActor* LidarActor = CastChecked<AVirtualLidarSensorActor>(Actor);
            Summary.bLidarPlacementRealistic |= FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 150.0f, 1.0f);
            Summary.bLegacyLidarDebugDisabled |= LidarActor->ScanComponent && !LidarActor->ScanComponent->bDrawDebugRays;
            Summary.bLidarPointCloudPreviewEnabled |= LidarActor->ScanComponent && LidarActor->ScanComponent->IsPointCloudPreviewEnabled();
        }
        if (Actor->IsA<AVirtualCameraSensorActor>())
        {
            ++Summary.CameraActorCount;
            Summary.bCameraPlacementRealistic |= FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 170.0f, 1.0f);
        }
        if (Actor->IsA<AVirtualSensorUiHostActor>())
        {
            ++Summary.MonitorHostCount;
            const AVirtualSensorUiHostActor* Host = CastChecked<AVirtualSensorUiHostActor>(Actor);
            if (Host->MonitorWidgetClass && Host->bAutoCreateOnBeginPlay &&
                Host->SettingsWidgetClass && Host->CaptureExportWidgetClass &&
                Host->bAutoCreateToolPanels && Host->bAllowViewportFallback && Host->bConfigurePlayerInputOnCreate)
            {
                ++Summary.ReadyMonitorHostCount;
            }
        }
        if (Actor->ActorHasTag(TEXT("SensorTestTarget")))
        {
            ++Summary.SensorTestTargetCount;
        }
#if WITH_EDITOR
        if (Actor->GetActorLabel().StartsWith(TEXT("SensorTest_Hall")))
        {
            ++Summary.HallWallCount;
        }
#endif
        if (Actor->IsA<ARectLight>()) ++Summary.CeilingRectLightCount;
        if (Actor->IsA<ASkyLight>()) ++Summary.SkyLightCount;
    }
}
}

bool FEditorMapAssetsLoadTest::RunTest(const FString& Parameters)
{
    bool bAllLoaded = true;
    bAllLoaded &= TestMapAssetLoads(*this, TEXT("/Game/MA0T10/Maps/BasicMap.BasicMap"));
    bAllLoaded &= TestMapAssetLoads(*this, TEXT("/Game/MA0T10/Maps/SensorTestMap.SensorTestMap"));
    bAllLoaded &= TestMapAssetLoads(*this, TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap.SensorRefactorTestMap"));
    TestNotNull(
        TEXT("WBP_VirtualSensorMonitorPanel generated class loads"),
        LoadObject<UClass>(nullptr, TEXT("/Game/MA0T10/UI/WBP_VirtualSensorMonitorPanel.WBP_VirtualSensorMonitorPanel_C")));
    TestNotNull(
        TEXT("WBP_VirtualSensorSettingsPanel generated class loads"),
        LoadObject<UClass>(nullptr, TEXT("/Game/MA0T10/UI/WBP_VirtualSensorSettingsPanel.WBP_VirtualSensorSettingsPanel_C")));
    TestNotNull(
        TEXT("WBP_VirtualSensorCaptureExportPanel generated class loads"),
        LoadObject<UClass>(nullptr, TEXT("/Game/MA0T10/UI/WBP_VirtualSensorCaptureExportPanel.WBP_VirtualSensorCaptureExportPanel_C")));
    return bAllLoaded;
}

bool FEditorMapSensorCompositionTest::RunTest(const FString& Parameters)
{
    FEditorSmokeMapSummary Summary;
    AddMapComposition(LoadSmokeMap(*this, TEXT("/Game/MA0T10/Maps/SensorTestMap.SensorTestMap")), Summary);

    TestTrue(TEXT("SensorTestMap includes an AVirtualSensorCoordinator"), Summary.ManagerCount > 0);
    TestTrue(TEXT("SensorTestMap includes an AVirtualLidarSensorActor"), Summary.LidarActorCount > 0);
    TestTrue(TEXT("SensorTestMap includes an AVirtualCameraSensorActor"), Summary.CameraActorCount > 0);
    TestTrue(TEXT("SensorTestMap includes an AVirtualSensorUiHostActor"), Summary.MonitorHostCount > 0);
    TestTrue(TEXT("SensorTestMap monitor host is configured for immediate interaction"), Summary.ReadyMonitorHostCount > 0);
    TestTrue(TEXT("SensorTestMap includes generic sensor targets"), Summary.SensorTestTargetCount > 0);
    TestEqual(TEXT("SensorTestMap has three open-front hall walls"), Summary.HallWallCount, 3);
    TestEqual(TEXT("SensorTestMap has four ceiling rect lights"), Summary.CeilingRectLightCount, 4);
    TestTrue(TEXT("SensorTestMap has ambient skylight"), Summary.SkyLightCount > 0);
    TestTrue(TEXT("camera is placed at realistic test height"), Summary.bCameraPlacementRealistic);
    TestTrue(TEXT("LiDAR is placed at realistic test height"), Summary.bLidarPlacementRealistic);
    TestTrue(TEXT("legacy LiDAR per-ray debug is disabled"), Summary.bLegacyLidarDebugDisabled);
    TestTrue(TEXT("LiDAR spatial point-cloud preview is enabled for immediate testing"), Summary.bLidarPointCloudPreviewEnabled);
    return Summary.ManagerCount > 0 &&
        Summary.LidarActorCount > 0 &&
        Summary.CameraActorCount > 0 &&
        Summary.MonitorHostCount > 0 &&
        Summary.ReadyMonitorHostCount > 0 &&
        Summary.SensorTestTargetCount > 0 &&
        Summary.HallWallCount == 3 &&
        Summary.CeilingRectLightCount == 4 &&
        Summary.SkyLightCount > 0 &&
        Summary.bCameraPlacementRealistic &&
        Summary.bLidarPlacementRealistic &&
        Summary.bLegacyLidarDebugDisabled &&
        Summary.bLidarPointCloudPreviewEnabled;
}

bool FSensorV2RefactorMapCompositionTest::RunTest(const FString& Parameters)
{
    FEditorSmokeMapSummary Summary;
    AddMapComposition(LoadSmokeMap(*this, TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap.SensorRefactorTestMap")), Summary);
    TestEqual(TEXT("V2 regression map has one coordinator"), Summary.ManagerCount, 1);
    TestTrue(TEXT("V2 regression map has Camera V2"), Summary.CameraActorCount > 0);
    TestTrue(TEXT("V2 regression map has LiDAR V2"), Summary.LidarActorCount > 0);
    TestEqual(TEXT("V2 regression map has one UI host"), Summary.MonitorHostCount, 1);
    TestTrue(TEXT("V2 regression map UI host is ready"), Summary.ReadyMonitorHostCount == 1);
    TestTrue(TEXT("V2 regression map enables the LiDAR point-cloud preview"), Summary.bLidarPointCloudPreviewEnabled);
    return Summary.ManagerCount == 1 && Summary.CameraActorCount > 0 && Summary.LidarActorCount > 0 &&
        Summary.MonitorHostCount == 1 && Summary.ReadyMonitorHostCount == 1 && Summary.bLidarPointCloudPreviewEnabled;
}

#endif
