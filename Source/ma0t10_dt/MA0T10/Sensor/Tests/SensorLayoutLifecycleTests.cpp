#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorLayoutLifecycleTest,"MA0T10.SensorLayout.Lifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorLayoutLifecycleTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* C=W->SpawnActor<AVirtualSensorCoordinator>();auto* L=W->GetSubsystem<UVirtualSensorLayoutSubsystem>();FString E;
    auto* Original=W->SpawnActor<AVirtualLidarSensorActor>();Original->StopSensor();C->RegisterSensorActor(Original);
    auto* A=L->CreateSensorAtPose(EVirtualSensorKind::Camera,FTransform(FVector(10,20,30)),E,false);
    auto* B=L->CreateSensorAtPose(EVirtualSensorKind::Lidar,FTransform(FVector(40,50,60)),E,false);
    TestNotNull(TEXT("camera added"),A);TestNotNull(TEXT("lidar added"),B);if(!A||!B){AddError(E);return false;}
    TestEqual(TEXT("actor inventory includes both kinds"),C->GetSensorActors().Num(),3);
    TestEqual(TEXT("camera component inventory consistent"),C->GetCameraSummaries().Num(),1);
    TestEqual(TEXT("lidar component inventory consistent"),C->GetLidarSummaries().Num(),2);
    TestTrue(TEXT("new ids unique"),A->GetSensorId()!=B->GetSensorId()&&B->GetSensorId()!=Original->GetSensorId());
    TestTrue(TEXT("selection refers to actual actor"),C->SelectSensorActor(A)&&C->GetSelectedSensorActor()==A);
    FVirtualSensorEditableState Before,During;bool Running=false;B->ReadCollectionState(Before,Running);
    TestTrue(TEXT("begin manipulation"),B->BeginInteractiveManipulation(FVirtualSensorInteractionRequest()));B->ReadCollectionState(During,Running);
    TestTrue(TEXT("collection snapshot retains original dimensions and interval"),Before.LidarHorizontalSamples==During.LidarHorizontalSamples&&Before.LidarScanInterval==During.LidarScanInterval);
    TestTrue(TEXT("readonly snapshot leaves manipulation active"),B->IsInteractiveManipulationActive());B->EndInteractiveManipulation();
    const auto InventoryRevision=L->GetRevision();B->ReadEditableState(During);During.SensorId=TEXT("LAYOUT-RENAMED-LIDAR");
    TestTrue(TEXT("identity edit through existing actor API"),B->ApplyEditableState(During,E));L->Tick(.3f);
    TestTrue(TEXT("identity change invalidates inventory cache"),L->GetRevision()>InventoryRevision&&L->GetLayoutSensors().ContainsByPredicate([](const auto& I){return I.SensorId==TEXT("LAYOUT-RENAMED-LIDAR");}));
    C->StreamPublisherComponent->StartStream(EVirtualSensorStreamKind::PointCloud,B->GetSensorId());
    TestFalse(TEXT("busy sensor deletion rejected"),L->DeleteSensor(B,E));C->StreamPublisherComponent->StopAllStreams(B->GetSensorId());
    TestFalse(TEXT("preplaced sensor deletion protected"),L->DeleteSensor(Original,E));
    TestTrue(TEXT("owned camera deletion"),L->DeleteSensor(A,E));TestEqual(TEXT("component deregistered"),C->GetCameraSummaries().Num(),0);
    C->StreamPublisherComponent->StartStream(EVirtualSensorStreamKind::PointCloud,FString());
    TestFalse(TEXT("wildcard cannot silently enroll new lidar"),L->CanCreateSensor(EVirtualSensorKind::Lidar,E));C->StreamPublisherComponent->StopAllStreams(FString());
    TestTrue(TEXT("owned lidar deletion"),L->DeleteSensor(B,E));TestEqual(TEXT("original remains"),C->GetSensorActors().Num(),1);
    return true;
}
#endif
