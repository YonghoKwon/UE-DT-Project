#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorRayQuery.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Json.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
class FHatchQueryRun final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;double Started=FPlatformTime::Seconds();int Checked=0,Mismatch=0,BoundaryMismatch=0;double MaxError=0;FRandomStream Random{60621};TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> Query;TArray<TSharedPtr<FJsonValue>> Differences;
 static constexpr int RandomRays=4096,BoundaryRays=24576,TotalRays=RandomRays+BoundaryRays;
public:
 explicit FHatchQueryRun(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("snapshot query validation timeout"));return true;}
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
  AShipHatchSimulation* S=nullptr;for(TActorIterator<AShipHatchSimulation>I(W);I;++I)S=*I;if(!S||S->IsGenerating()||!S->HasFreshMeasurement())return false;
  if(!Query){Query=S->CaptureRayQuery(S->SensorAt(0),ECC_Visibility);if(!Test->TestTrue(TEXT("hatch owns an optional immutable scene query"),Query.IsValid()))return true;Test->TestFalse(TEXT("unsupported channels retain engine query behavior"),S->CaptureRayQuery(S->SensorAt(0),ECC_Camera).IsValid());}
  const int End=FMath::Min(TotalRays,Checked+512);
  for(;Checked<End;++Checked)
  {
   auto Origin=S->GetActorTransform().TransformPosition(S->SensorOrigin(Checked%3)*100);FVector Direction=Random.VRand();
   if(Checked<RandomRays)
   {if(Checked%2==0){const FVector Target=S->GetActorTransform().TransformPosition(FVector(Random.FRandRange(-18,18),Random.FRandRange(-28,28),Random.FRandRange(0,25))*100);Direction=(Target-Origin).GetSafeNormal();}}
   else
   {
    const int I=Checked-RandomRays,Plane=(I/4096)%3;const double Angle=2*PI*(I%4096)/4096.;
    FVector Local=S->SensorOrigin((I/12288)%2);
    // Sweep both signs in each coordinate plane, with origins exactly on
    // collision-grid split planes. Include signed zero and near-zero beams.
    Local[Plane]=0;FVector D=FVector::ZeroVector;
    D[(Plane+1)%3]=FMath::Cos(Angle);D[(Plane+2)%3]=FMath::Sin(Angle);
    if(I%1024==0){D=FVector::ZeroVector;D[(I%2048==0?(Plane+1):(Plane+2))%3]=I%4096<2048?1:-1;}
    if(I>=12288)D[Plane]=(I%2?1:-1)*1.e-10;
    Origin=S->GetActorTransform().TransformPosition(Local*100);
    Direction=S->GetActorTransform().TransformVectorNoScale(D).GetSafeNormal();
   }
   const auto Target=Origin+Direction*8000;FHitResult A,B;FCollisionQueryParams Params(SCENE_QUERY_STAT(HatchQueryReference),false,S->SensorAt(0));
   const bool HitA=Query->Trace(Origin,Target,A),HitB=W->LineTraceSingleByChannel(B,Origin,Target,ECC_Visibility,Params);
   const double Error=HitA&&HitB?FVector::Distance(A.ImpactPoint,B.ImpactPoint)*.01:0;if(HitA&&HitB)MaxError=FMath::Max(MaxError,Error);
   if(HitA!=HitB||(HitA&&(Error>.001||A.GetComponent()!=B.GetComponent())))
   {
    ++Mismatch;BoundaryMismatch+=Checked>=RandomRays;
    if(Differences.Num()<64)
    {
     auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("ray"),Checked);
     for(int Axis=0;Axis<3;++Axis){R->SetNumberField(FString::Printf(TEXT("origin_%d"),Axis),Origin[Axis]);R->SetNumberField(FString::Printf(TEXT("end_%d"),Axis),Target[Axis]);}
     R->SetBoolField(TEXT("bvh_hit"),HitA);R->SetBoolField(TEXT("engine_hit"),HitB);
     R->SetNumberField(TEXT("bvh_distance_cm"),A.Distance);R->SetNumberField(TEXT("engine_distance_cm"),B.Distance);
     for(int Axis=0;Axis<3;++Axis){R->SetNumberField(FString::Printf(TEXT("bvh_point_%d"),Axis),A.ImpactPoint[Axis]);R->SetNumberField(FString::Printf(TEXT("engine_point_%d"),Axis),B.ImpactPoint[Axis]);}
     R->SetStringField(TEXT("bvh_actor"),GetNameSafe(A.GetActor()));R->SetStringField(TEXT("engine_actor"),GetNameSafe(B.GetActor()));
     R->SetStringField(TEXT("bvh_component"),GetNameSafe(A.GetComponent()));R->SetStringField(TEXT("engine_component"),GetNameSafe(B.GetComponent()));
     R->SetNumberField(TEXT("error_m"),Error);R->SetStringField(TEXT("origin"),Origin.ToString());R->SetStringField(TEXT("end"),Target.ToString());Differences.Add(MakeShared<FJsonValueObject>(R));
    }
   }
  }
  if(Checked<TotalRays)return false;
  if(Mismatch)
  {
   // Preserve exact acquisition geometry when an edge regression fails, so a
   // nearest-hit crack can be reproduced without changing/saving the fixture.
   FString Root;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),Root);
   if(!Root.IsEmpty())
   {
    TArray<FVirtualSensorCollisionMesh> Meshes;Query->CollisionMeshes(Meshes);
    for(int I=0;I<Meshes.Num();++I)if(Meshes[I].Vertices)
    {TArray<uint8> Bytes;Bytes.Append(reinterpret_cast<const uint8*>(Meshes[I].Vertices->GetData()),Meshes[I].Vertices->Num()*sizeof(FVector3f));FFileHelper::SaveArrayToFile(Bytes,*(Root/FString::Printf(TEXT("query-mesh-%d.bin"),I)));}
   }
  }
  Test->TestEqual(TEXT("4096 random plus24576 boundary engine-vs-BVH hit/miss/component checks agree"),Mismatch,0);Test->TestTrue(TEXT("actual collision surface agreement within1mm"),MaxError<=.001);
  const auto Hash=S->Surface().Hash();const FVector O=S->GetActorTransform().TransformPosition(FVector(5,-5,S->HoldDepth+6)*100),D=S->GetActorTransform().TransformVectorNoScale(FVector(0,0,-1));FHitResult Before,Old,Fresh;
  Test->TestTrue(TEXT("clear production cargo reference"),Query->Trace(O,O+D*10000,Before));Test->TestTrue(TEXT("real excavation updates scene revision"),S->Dig(O,D,2,1.5));
  auto Next=S->CaptureRayQuery(S->SensorAt(0),ECC_Visibility);Test->TestTrue(TEXT("old acquisition geometry remains immutable across excavation"),Query->Trace(O,O+D*10000,Old)&&Old.ImpactPoint.Equals(Before.ImpactPoint,.001));
  Test->TestTrue(TEXT("new acquisition uses new physical surface"),Next&&Next->Trace(O,O+D*10000,Fresh)&&FVector::Distance(Fresh.ImpactPoint,Before.ImpactPoint)>5);
  Test->TestTrue(TEXT("query cache never changes undo truth"),S->UndoDig()&&S->Surface().Hash()==Hash);
  // A genuine non-owned physics body must still occlude the accelerated scene.
  auto* Blocker=W->SpawnActor<AActor>();auto* Cube=NewObject<UStaticMeshComponent>(Blocker);Blocker->SetRootComponent(Cube);
  Cube->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Cube->SetCollisionProfileName(TEXT("BlockAll"));Cube->RegisterComponent();Cube->SetWorldLocation(Before.ImpactPoint+FVector(0,0,300));
  auto Foreign=S->CaptureRayQuery(S->SensorAt(0),ECC_Visibility);FHitResult Blocked;
  Test->TestTrue(TEXT("non-owned physical geometry retains first-hit occlusion"),Foreign&&Foreign->Trace(O,O+D*10000,Blocked)&&Blocked.GetComponent()==Cube);Blocker->Destroy();
  auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("rays"),Checked);R->SetNumberField(TEXT("random_rays"),RandomRays);R->SetNumberField(TEXT("boundary_rays"),BoundaryRays);R->SetNumberField(TEXT("boundary_mismatches"),BoundaryMismatch);R->SetNumberField(TEXT("mismatches"),Mismatch);R->SetArrayField(TEXT("differences"),Differences);R->SetNumberField(TEXT("max_position_error_m"),MaxError);R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());const auto Tri=S->RayQueryTriangleCounts();R->SetNumberField(TEXT("static_triangles"),Tri.X);R->SetNumberField(TEXT("cargo_triangles"),Tri.Y);
  FString Root,Text;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),Root);if(Root.IsEmpty())Root=FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-06/query");IFileManager::Get().MakeDirectory(*Root,true);FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Root/TEXT("scene-query-accuracy.json")));return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchQueryTest,"HATCH06.SceneQueriesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchQueryTest::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchQueryRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchPointPoolLifetimeTest,"MA0T10.Hatch.PointPoolLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchPointPoolLifetimeTest::RunTest(const FString&)
{
 auto* Scan=NewObject<UVirtualLidarScanComponent>();
 Scan->AcquireScenePointStorage(64);Scan->ScheduledPoints.SetNum(1);Scan->ScheduledPoints[0].WorldLocation=FVector(1,2,3);
 auto A=Scan->CompleteScenePointStorage();const auto* Address=A->GetData();TWeakPtr<TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe> OldWeak=A;
 A.Reset();TestFalse(TEXT("released publication expires weak pointers even though storage is retained"),OldWeak.IsValid());
 TestTrue(TEXT("exclusive point storage cleanup finishes before reuse assertion"),Scan->WaitForScenePointPoolCleanupForTesting());
 Scan->AcquireScenePointStorage(64);TestTrue(TEXT("same allocation reused with a new publication control block"),Scan->ScheduledPoints.GetData()==Address);Scan->ScheduledPoints.SetNum(1);Scan->ScheduledPoints[0].WorldLocation=FVector(4,5,6);
 auto Held=Scan->CompleteScenePointStorage();TWeakPtr<TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe> HeldWeak=Held;
 for(int I=0;I<8;++I){Scan->AcquireScenePointStorage(64);TestTrue(TEXT("held immutable storage cannot be leased"),Scan->ScheduledPoints.GetData()!=Held->GetData());Scan->ScheduledPoints.SetNum(1);auto Next=Scan->CompleteScenePointStorage();}
 TestTrue(TEXT("held point values survive repeated acquisitions"),(*Held)[0].WorldLocation==FVector(4,5,6));
 Scan->AcquireScenePointStorage(128);Scan->RecycleScenePointStorage();TestTrue(TEXT("profile resize preserves held old publication"),HeldWeak.IsValid()&&(*Held)[0].WorldLocation==FVector(4,5,6));Held.Reset();TestFalse(TEXT("old pool weak pointer expires after final consumer"),HeldWeak.IsValid());
 const uint64 BeforeFallbacks=Scan->GetScenePointPoolFallbacksForTesting();
 TArray<TSharedPtr<TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>> Leases;
 for(int I=0;I<4;++I){Scan->AcquireScenePointStorage(32);Scan->ScheduledPoints.SetNum(1);Leases.Add(Scan->CompleteScenePointStorage());}
 TestEqual(TEXT("diagnostic counts a real retained-lease fallback"),Scan->GetScenePointPoolFallbacksForTesting(),BeforeFallbacks+1);
 Scan->RecycleScenePointStorage();Leases.Reset();
 return true;
}
#endif
