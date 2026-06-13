#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraAct.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorAct.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorMapAssetsLoadTest, "M7AT10.EditorSmoke.MapAssetsLoad", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorMapSensorCompositionTest, "M7AT10.EditorSmoke.MapSensorComposition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
struct FEditorSmokeMapSummary
{
    int32 ManagerCount = 0;
    int32 LidarActorCount = 0;
    int32 CameraActorCount = 0;
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
        }
        if (Actor->IsA<AVirtualCameraAct>())
        {
            ++Summary.CameraActorCount;
        }
    }
}
}

bool FEditorMapAssetsLoadTest::RunTest(const FString& Parameters)
{
    bool bAllLoaded = true;
    bAllLoaded &= TestMapAssetLoads(*this, TEXT("/Game/M7AT10/Maps/BasicMap.BasicMap"));
    bAllLoaded &= TestMapAssetLoads(*this, TEXT("/Game/M7AT10/Maps/TestMap.TestMap"));
    return bAllLoaded;
}

bool FEditorMapSensorCompositionTest::RunTest(const FString& Parameters)
{
    FEditorSmokeMapSummary Summary;
    AddMapComposition(LoadSmokeMap(*this, TEXT("/Game/M7AT10/Maps/BasicMap.BasicMap")), Summary);
    AddMapComposition(LoadSmokeMap(*this, TEXT("/Game/M7AT10/Maps/TestMap.TestMap")), Summary);

    TestTrue(TEXT("validation maps include at least one AVirtualSensorManager"), Summary.ManagerCount > 0);
    TestTrue(TEXT("validation maps include at least one AVirtualSensorAct"), Summary.LidarActorCount > 0);
    TestTrue(TEXT("validation maps include at least one AVirtualCameraAct"), Summary.CameraActorCount > 0);
    return Summary.ManagerCount > 0 && Summary.LidarActorCount > 0 && Summary.CameraActorCount > 0;
}

#endif
