#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ArchitectureMonitorProbe.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarJsonLiveSourceComponent.h"
#include "UObject/UnrealType.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArchitectureMonitorTest,"MA0T10.Architecture.MonitorRouting",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArchitectureMonitorTest::RunTest(const FString&)
{
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* C=World->SpawnActor<AVirtualSensorCoordinator>();
    C->bStartSensorsOnBeginPlay=false;
    auto* Probe=NewObject<UArchitectureMonitorProbe>(World);
    C->OnViewModeChanged.AddDynamic(Probe,&UArchitectureMonitorProbe::ModeChanged);
    C->BindMonitorWidget(Probe);
    TestEqual(TEXT("initial binding order unchanged"),FString::Join(Probe->Calls,TEXT(",")),FString(TEXT("Camera,Lidar,External,ShowCamera,Event")));
    auto* Camera=NewObject<UVirtualCameraCaptureComponent>(C);
    auto* Lidar=NewObject<UVirtualLidarScanComponent>(C);
    auto* Source=NewObject<ULidarJsonLiveSourceComponent>(C);
    C->RegisterCamera(Camera); C->RegisterLidar(Lidar); C->RegisterRealSensorSource(Source);
    TestTrue(TEXT("selected components reach presentation interface"),Probe->LastCamera==Camera&&Probe->LastLidar==Lidar&&Probe->LastSource==Source);
    auto* SecondCamera=NewObject<UVirtualCameraCaptureComponent>(C); C->RegisterCamera(SecondCamera);
    Probe->Calls.Reset(); C->SelectCameraByIndex(1);
    TestEqual(TEXT("camera selection follows through bridge"),Probe->LastCamera,SecondCamera);
    TestEqual(TEXT("selection preserves bind/view/event order"),FString::Join(Probe->Calls,TEXT(",")),FString(TEXT("Camera,Lidar,External,ShowCamera,Event")));
    Probe->Calls.Reset(); C->SetViewMode(EVirtualSensorViewMode::Lidar);
    TestEqual(TEXT("view before broadcast"),FString::Join(Probe->Calls,TEXT(",")),FString(TEXT("ShowLidar,Event")));
    Probe->ViewController=World->SpawnActor<APlayerController>();
    Probe->Calls.Reset(); C->SetPointCloudOnlyMode(true);
    TestEqual(TEXT("owning-player query before auto lidar view"),FString::Join(Probe->Calls,TEXT(",")),FString(TEXT("Owner,ShowLidar,Event")));
    Probe->Calls.Reset(); C->BindMonitorWidget(Probe);
    TestEqual(TEXT("binding point-only does not add a view event"),FString::Join(Probe->Calls,TEXT(",")),FString(TEXT("Camera,Lidar,External,ShowLidar")));
    Probe->Calls.Reset(); C->SetPointCloudOnlyMode(false);
    TestEqual(TEXT("previous lidar view restored before event"),FString::Join(Probe->Calls,TEXT(",")),FString(TEXT("ShowLidar,Event")));
    C->bPointCloudOnlyAutoSelectLidarView=false; C->bPointCloudOnlyHideWorld=false;
    Probe->Calls.Reset(); C->SetPointCloudOnlyMode(true);
    TestEqual(TEXT("disabled auto view remains disabled"),FString::Join(Probe->Calls,TEXT(",")),FString(TEXT("Event")));
    C->SetPointCloudOnlyMode(false); Probe->Calls.Reset(); C->BindMonitorWidget(nullptr);
    TestTrue(TEXT("null binding remains a no-op"),Probe->Calls.IsEmpty());
    UFunction* Bind=C->FindFunction(TEXT("BindMonitorWidget"));
    TestNotNull(TEXT("legacy Blueprint node exists"),Bind);
    if(Bind)
    {
        TestEqual(TEXT("legacy category unchanged"),Bind->GetMetaData(TEXT("Category")),FString(TEXT("DigitalTwin|SensorManager")));
        TestEqual(TEXT("legacy one-parameter signature"),int32(Bind->NumParms),1);
        auto* Param=FindFProperty<FObjectPropertyBase>(Bind,TEXT("MonitorWidget"));
        TestTrue(TEXT("legacy concrete widget pin type unchanged"),Param&&Param->PropertyClass==UVirtualSensorMonitorPanelWidget::StaticClass());
    }
    auto* Bound=FindFProperty<FObjectPropertyBase>(C->GetClass(),TEXT("BoundMonitorWidget"));
    TestTrue(TEXT("bound widget GC property remains transient and concrete"),Bound&&Bound->HasAnyPropertyFlags(CPF_Transient)&&Bound->PropertyClass==UVirtualSensorMonitorPanelWidget::StaticClass());
    C->OnViewModeChanged.RemoveDynamic(Probe,&UArchitectureMonitorProbe::ModeChanged);
    return true;
}
#endif
