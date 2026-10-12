#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorPlacementInteraction.h"
#include "ma0t10_dt/MA0T10/Player/DxPlayerControllerTest.h"
#include "Components/StaticMeshComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrSensorActivationTest,"MA0T10.UXR02.DtCoreSensorActivation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrSensorActivationTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* C=World->SpawnActor<AVirtualSensorCoordinator>();
    auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();
    C->RegisterSensorActor(Camera);C->RegisterSensorActor(Lidar);auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();W->SetCoordinator(C);
    auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(World);W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Settings);Settings->BindSensorManager(C);
    Camera->StopSensor();Lidar->StopSensor();const int64 CameraFrame=Camera->GetSensorRuntimeStatus().FrameId,LidarFrame=Lidar->GetSensorRuntimeStatus().FrameId;
    const bool Open=W->IsPanelOpen(ESensorToolPanelRole::Settings);Camera->Click();W->SynchronizeOwnedSelection();
    TArray<UStaticMeshComponent*> Meshes;Lidar->GetComponents(Meshes);UStaticMeshComponent* Marker=nullptr;
    for(auto* M:Meshes)if(M->GetFName()==TEXT("SensorPlacementPickMarker"))Marker=M;
    TestNotNull(TEXT("owned equipment has a selection marker"),Marker);
    if(Marker)
    {
        TestFalse(TEXT("pick sphere has no ordinary player surface"),Marker->bRenderInMainPass);
        TestFalse(TEXT("pick sphere does not occlude player depth"),Marker->bRenderInDepthPass);
        TestTrue(TEXT("visual refinement preserves original pick radius"),Marker->GetRelativeScale3D().Equals(FVector(.3)));
        Lidar->OnCursorHover(Marker);
        TestTrue(TEXT("existing DTCore hover highlights marker in custom depth"),Marker->bRenderCustomDepth);
        Lidar->OnCursorUnhover();
        TestFalse(TEXT("existing DTCore unhover clears marker highlight"),Marker->bRenderCustomDepth);
    }
    if(Marker){TestTrue(TEXT("marker excluded from all scene captures and GPU semantic proxies"),Marker->bHiddenInSceneCapture);for(int32 Channel=0;Channel<32;++Channel)TestEqual(TEXT("marker never blocks measurement channels"),Marker->GetCollisionResponseToChannel(ECollisionChannel(Channel)),ECR_Ignore);}
    Lidar->SetActorLocation(FVector(2000,0,0));TArray<FHitResult> Hits;FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    World->LineTraceMultiByObjectType(Hits,FVector(1900,0,0),FVector(2100,0,0),Objects);
    TestTrue(TEXT("DTCore object query can actually hit marker despite ignored measurement channels"),Hits.ContainsByPredicate([&](const auto& H){return H.GetActor()==Lidar;}));
    TestTrue(TEXT("DTCore virtual activation selects camera"),C->GetSelectedSensorActor()==Camera);
    Lidar->Click();W->SynchronizeOwnedSelection();TestTrue(TEXT("DTCore virtual activation selects lidar"),C->GetSelectedSensorActor()==Lidar);
    TestEqual(TEXT("owned settings follows native actor activation"),Settings->GetPendingState().SensorId,Lidar->GetSensorId());
    TestEqual(TEXT("selection does not open a panel"),W->IsPanelOpen(ESensorToolPanelRole::Settings),Open);
    TestFalse(TEXT("camera selection does not start measurement"),Camera->IsSensorRunning());TestFalse(TEXT("lidar selection does not start measurement"),Lidar->IsSensorRunning());
    TestEqual(TEXT("camera has no extra acquisition"),Camera->GetSensorRuntimeStatus().FrameId,CameraFrame);TestEqual(TEXT("lidar has no extra acquisition"),Lidar->GetSensorRuntimeStatus().FrameId,LidarFrame);
    TestEqual(TEXT("project keeps SingleRelease policy"),GetDefault<ADxPlayerControllerTest>()->ClickActivationPolicy,EDxClickActivationPolicy::SingleRelease);
    C->SelectSensorActor(Camera);C->UnregisterSensorActor(Lidar);Lidar->Click();TestTrue(TEXT("unregistered activation cannot steal selection"),C->GetSelectedSensorActor()==Camera);
    W->UnregisterPanel(Settings);C->Destroy();Camera->Destroy();Lidar->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrPlacementDeleteTest,"MA0T10.UXR02.PlacementDeleteConfirmation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrPlacementDeleteTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* C=World->SpawnActor<AVirtualSensorCoordinator>();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();W->SetCoordinator(C);
    auto* L=World->GetSubsystem<UVirtualSensorLayoutSubsystem>();FString Reason;auto* A=L->CreateSensorAtPose(EVirtualSensorKind::Camera,FTransform::Identity,Reason,false);
    if(!TestNotNull(TEXT("test-owned camera created"),A))return false;
    auto* P=NewObject<UVirtualSensorPlacementPanelWidget>(World);W->RegisterOwnedPanel(ESensorToolPanelRole::Placement,P);P->SelectEquipment(A);
    TestFalse(TEXT("first delete is confirmation only"),P->DeleteSelectedSensor());TestTrue(TEXT("equipment exists before confirmation"),L->OwnsSensor(A));
    P->CancelSelectedSensorDeletion();TestTrue(TEXT("cancel reports preserved equipment"),P->GetPlacementStatusText().Contains(TEXT("취소"))&&L->OwnsSensor(A));
    TestFalse(TEXT("delete after cancel requires confirmation again"),P->DeleteSelectedSensor());TestTrue(TEXT("second delete confirms through existing layout service"),P->DeleteSelectedSensor());
    TestFalse(TEXT("confirmed deletion retires owned equipment"),L->OwnsSensor(A));W->UnregisterPanel(P);C->Destroy();return true;
}
#endif
