#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraAct.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorAct.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorManager.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorHostActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorMapAssetsLoadTest, "MA0T10.EditorSmoke.MapAssetsLoad", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorMapSensorCompositionTest, "MA0T10.EditorSmoke.MapSensorComposition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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
        if (Actor->IsA<AVirtualSensorManager>())
        {
            ++Summary.ManagerCount;
        }
        if (Actor->IsA<AVirtualSensorAct>())
        {
            ++Summary.LidarActorCount;
            const AVirtualSensorAct* LidarActor = CastChecked<AVirtualSensorAct>(Actor);
            Summary.bLidarPlacementRealistic |= FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 150.0f, 1.0f);
            Summary.bLegacyLidarDebugDisabled |= LidarActor->LidarSensorComp && !LidarActor->LidarSensorComp->bDrawDebugRays;
        }
        if (Actor->IsA<AVirtualCameraAct>())
        {
            ++Summary.CameraActorCount;
            Summary.bCameraPlacementRealistic |= FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 170.0f, 1.0f);
        }
        if (Actor->IsA<AVirtualSensorMonitorHostActor>())
        {
            ++Summary.MonitorHostCount;
            const AVirtualSensorMonitorHostActor* Host = CastChecked<AVirtualSensorMonitorHostActor>(Actor);
            if (Host->MonitorWidgetClass && Host->bAutoCreateOnBeginPlay &&
                Host->SettingsWidgetClass && Host->CaptureExportWidgetClass &&
                Host->bAutoCreateToolPanels && Host->bAddToViewport && Host->bConfigurePlayerInputOnCreate)
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
    TestNotNull(
        TEXT("WBP_VirtualSensorMonitor generated class loads"),
        LoadObject<UClass>(nullptr, TEXT("/Game/MA0T10/UI/WBP_VirtualSensorMonitor.WBP_VirtualSensorMonitor_C")));
    TestNotNull(
        TEXT("WBP_VirtualSensorSettings generated class loads"),
        LoadObject<UClass>(nullptr, TEXT("/Game/MA0T10/UI/WBP_VirtualSensorSettings.WBP_VirtualSensorSettings_C")));
    TestNotNull(
        TEXT("WBP_VirtualSensorCaptureExport generated class loads"),
        LoadObject<UClass>(nullptr, TEXT("/Game/MA0T10/UI/WBP_VirtualSensorCaptureExport.WBP_VirtualSensorCaptureExport_C")));
    return bAllLoaded;
}

bool FEditorMapSensorCompositionTest::RunTest(const FString& Parameters)
{
    FEditorSmokeMapSummary Summary;
    AddMapComposition(LoadSmokeMap(*this, TEXT("/Game/MA0T10/Maps/SensorTestMap.SensorTestMap")), Summary);

    TestTrue(TEXT("SensorTestMap includes an AVirtualSensorManager"), Summary.ManagerCount > 0);
    TestTrue(TEXT("SensorTestMap includes an AVirtualSensorAct"), Summary.LidarActorCount > 0);
    TestTrue(TEXT("SensorTestMap includes an AVirtualCameraAct"), Summary.CameraActorCount > 0);
    TestTrue(TEXT("SensorTestMap includes an AVirtualSensorMonitorHostActor"), Summary.MonitorHostCount > 0);
    TestTrue(TEXT("SensorTestMap monitor host is configured for immediate interaction"), Summary.ReadyMonitorHostCount > 0);
    TestTrue(TEXT("SensorTestMap includes generic sensor targets"), Summary.SensorTestTargetCount > 0);
    TestEqual(TEXT("SensorTestMap has three open-front hall walls"), Summary.HallWallCount, 3);
    TestEqual(TEXT("SensorTestMap has four ceiling rect lights"), Summary.CeilingRectLightCount, 4);
    TestTrue(TEXT("SensorTestMap has ambient skylight"), Summary.SkyLightCount > 0);
    TestTrue(TEXT("camera is placed at realistic test height"), Summary.bCameraPlacementRealistic);
    TestTrue(TEXT("LiDAR is placed at realistic test height"), Summary.bLidarPlacementRealistic);
    TestTrue(TEXT("legacy LiDAR per-ray debug is disabled"), Summary.bLegacyLidarDebugDisabled);
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
        Summary.bLegacyLidarDebugDisabled;
}

#endif
