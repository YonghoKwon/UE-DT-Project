#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabPreviewIsolationTest,"MA0T10.SlabReliability.PreviewHasNoPhysicsOrNavigation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabPreviewIsolationTest::RunTest(const FString&)
{
 auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* Sensor=W->SpawnActor<AVirtualLidarSensorActor>();Sensor->StopSensor();Sensor->ScanComponent->SetPointCloudPreviewEnabled(true);
 TArray<UInstancedStaticMeshComponent*> Components;Sensor->GetComponents(Components);UInstancedStaticMeshComponent* Preview=nullptr;
 for(auto* C:Components)if(C->GetName()==TEXT("VirtualLidarPointCloudPreview"))Preview=C;
 if(!TestNotNull(TEXT("CPU preview exists"),Preview))return false;
 Preview->AddInstance(FTransform::Identity);
 TestEqual(TEXT("diagnostics cannot be collision targets"),Preview->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
 TestFalse(TEXT("diagnostics have no overlap work"),Preview->GetGenerateOverlapEvents());TestFalse(TEXT("diagnostics do not update navigation"),Preview->CanEverAffectNavigation());TestFalse(TEXT("diagnostics create no physics state"),Preview->IsPhysicsStateCreated());
 TestTrue(TEXT("CPU point markers are excluded from sensor SceneCapture views"),bool(Preview->bHiddenInSceneCapture));
 Sensor->Destroy();return true;
}
#endif
