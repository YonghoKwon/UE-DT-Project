#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarStaticCollisionQuery.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "SensorOwnedRayTracing.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuPointModelSnapshotTest,"MA0T10.GpuLidar.PointModelSnapshot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuPointModelSnapshotTest::RunTest(const FString&)
{
 auto* Scan=NewObject<UVirtualLidarScanComponent>();Scan->FrameId=71;Scan->DatasetNoiseSequence=TPair<int32,int64>(812,47);
 Scan->MaxDistance=10000;Scan->FidelityMode=EVirtualSensorFidelityMode::PublicSpecBased;Scan->bEnableRangeNoise=true;
 Scan->RangeNoiseStandardDeviationMillimeters=5;Scan->MaximumRangeErrorMillimeters=20;
 for(bool Material:{false,true})
 {
  Scan->bEnableMaterialResponse=Material;const auto Frozen=Scan->CapturePointModel();const auto Surface=Scan->ResolveSurfaceResponse(nullptr);
  for(int I=0;I<1024;++I)
  {
   TestEqual(TEXT("frozen worker random sequence matches CPU acquisition sequence"),Frozen.DeterministicUnitRandom(I,0,0x4d4c5833u),Scan->DeterministicUnitRandom(I,0,0x4d4c5833u));
   FHitResult Hit;Hit.Distance=100+I*5;Hit.ImpactNormal=FVector(-1,0,0);FVirtualLidarPoint Cpu,Gpu;
   const bool A=Scan->ApplyPhysicalHitModel(Cpu,Hit,FVector::ForwardVector,FTransform::Identity,I,0);
   const bool B=Frozen.Apply(Gpu,Surface,FVector::ForwardVector,FTransform::Identity,Hit.Distance,1,I,0);
   if(A!=B||Cpu.RangeMillimeters!=Gpu.RangeMillimeters||Cpu.RawIntensity!=Gpu.RawIntensity||Cpu.Validity!=Gpu.Validity||Cpu.Confidence!=Gpu.Confidence||Cpu.WorldLocation!=Gpu.WorldLocation)
   {AddError(FString::Printf(TEXT("physical/optical parity differs at ray %d"),I));return false;}
  }
  Scan->AmbientLightLux+=100;TestTrue(TEXT("frozen options do not observe later mutable sensor values"),Frozen.AmbientLightLux!=Scan->AmbientLightLux);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarFallbackTest,"MA0T10.GpuLidar.FallbackSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarFallbackTest::RunTest(const FString&)
{
 auto* Cvar=IConsoleManager::Get().FindConsoleVariable(TEXT("ma0t10.Lidar.HardwareRayTracing"));if(!Cvar){AddError(TEXT("HWRT fallback switch missing"));return false;}
 const int Old=Cvar->GetInt();const auto Priority=EConsoleVariableFlags(Cvar->GetFlags()&ECVF_SetByMask);Cvar->Set(0,Priority);
 auto* Scan=NewObject<UVirtualLidarScanComponent>();Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::HardwareRayTracing;FString Reason;
 TestFalse(TEXT("backend diagnostic disable is independent from renderer settings"),SensorOwnedRayTracing::IsAvailable());
 TestEqual(TEXT("explicit unavailable HWRT retains CPU fallback"),Scan->GetResolvedAcquisitionBackend(Reason),EVirtualLidarAcquisitionBackend::AccurateCpuTrace);
 TestFalse(TEXT("fallback provides an explanation"),Reason.IsEmpty());
 Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::Auto;TestEqual(TEXT("Auto retains CPU when HWRT unavailable"),Scan->GetResolvedAcquisitionBackend(Reason),EVirtualLidarAcquisitionBackend::AccurateCpuTrace);
 Cvar->Set(Old,Priority);Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
 TestEqual(TEXT("explicit CPU selection is preserved on RT-capable machines"),Scan->GetResolvedAcquisitionBackend(Reason),EVirtualLidarAcquisitionBackend::AccurateCpuTrace);return !HasAnyErrors();
}
namespace
{
class FStaticGpuCompare final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AActor> Actor;
 TSharedPtr<ISensorRtScan,ESPMode::ThreadSafe> Scan;TArray<FSensorRtRay> Rays;TArray<FHitResult> References;
 double Started=FPlatformTime::Seconds();bool Initialized=false;
public:
 FStaticGpuCompare(FAutomationTestBase* T,UWorld* W,AActor* A):Test(T),World(W),Actor(A){}
 bool Update() override
 {
  if(!World.IsValid()||!Actor.IsValid()){Test->AddError(TEXT("static collision test ownership lost"));return true;}
  if(FPlatformTime::Seconds()-Started>20){Test->AddError(TEXT("static GPU collision comparison timed out"));if(Scan)Scan->Cancel();Actor->Destroy();return true;}
  if(!Initialized)
  {
   Initialized=true;FString Reason;auto Query=CaptureLidarStaticCollision(World.Get(),nullptr,ECC_Visibility,Reason);
   if(!Query){Test->AddError(Reason);Actor->Destroy();return true;}
   TArray<FVirtualSensorCollisionMesh> Meshes;Query->CollisionMeshes(Meshes);FSensorRtRequest Request;
   for(const auto& M:Meshes)Request.Meshes.Add({M.Revision,M.Vertices,M.NormalsAndSurfaces,M.LocalToWorld});
   for(int I=0;I<256;++I)
   {
    const FVector D=FVector(1,(I-128)*.005,.1).GetSafeNormal();FSensorRtRay Ray{FVector3f::ZeroVector,2000,FVector3f(D),0};Rays.Add(Ray);
    FHitResult Reference;World->LineTraceSingleByChannel(Reference,FVector::ZeroVector,D*2000,ECC_Visibility,FCollisionQueryParams(SCENE_QUERY_STAT(GpuStaticReference),false));References.Add(Reference);
   }
   Request.Rays=Rays;Scan=SensorOwnedRayTracing::Submit(MoveTemp(Request));
   if(!Scan){Test->AddError(TEXT("static GPU path was not admitted on real RHI"));Actor->Destroy();return true;}return false;
  }
  TArray<FSensorRtHit> Hits;FString Error;if(!Scan->Poll(Hits,Error))return false;
  if(!Error.IsEmpty())Test->AddError(Error);
  if(Hits.Num()!=References.Num())Test->AddError(TEXT("static GPU result cardinality mismatch"));
  else for(int I=0;I<Hits.Num();++I)
  {
   const bool Gpu=Hits[I].NormalAndDistance.W>=0,Cpu=References[I].bBlockingHit;
   if(Gpu!=Cpu||(Gpu&&FMath::Abs(Hits[I].NormalAndDistance.W-References[I].Distance)>.01))
   {Test->AddError(FString::Printf(TEXT("static collision differs from native physics at ray %d"),I));break;}
  }
  auto* Sphere=World->SpawnActor<AActor>();Sphere->Tags.Add(TEXT("SensorTestManaged"));auto* Shape=NewObject<USphereComponent>(Sphere);Sphere->SetRootComponent(Shape);Shape->SetCollisionProfileName(TEXT("BlockAll"));Shape->RegisterComponent();
  FString Reason;Test->TestFalse(TEXT("unsupported analytic geometry retains entire CPU acquisition"),CaptureLidarStaticCollision(World.Get(),nullptr,ECC_Visibility,Reason).IsValid());Test->TestFalse(TEXT("unsupported geometry fallback is explained"),Reason.IsEmpty());
  Sphere->Destroy();Actor->Destroy();return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarStaticCollisionTest,"MA0T10.GpuLidar.StaticCollisionRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarStaticCollisionTest::RunTest(const FString&)
{
 auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Actor=World->SpawnActor<AActor>();Actor->Tags.Add(TEXT("SensorTestManaged"));auto* Cube=NewObject<UStaticMeshComponent>(Actor);Actor->SetRootComponent(Cube);
 Cube->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Cube->SetCollisionProfileName(TEXT("BlockAll"));Cube->RegisterComponent();Cube->SetWorldLocation(FVector(300,0,0));Cube->SetWorldScale3D(FVector(1,5,5));
 ADD_LATENT_AUTOMATION_COMMAND(FStaticGpuCompare(this,World,Actor));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarBackendSettingsTest,"MA0T10.GpuLidar.BackendSettings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarBackendSettingsTest::RunTest(const FString&)
{
 auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>();
 auto* A=World->SpawnActor<AVirtualLidarSensorActor>();auto* B=World->SpawnActor<AVirtualLidarSensorActor>();Manager->RegisterSensorActor(A);Manager->RegisterSensorActor(B);
 TestEqual(TEXT("manager applies backend through registered native Actor contracts"),Manager->SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend::AccurateCpuTrace),2);
 TestEqual(TEXT("second sensor receives manager CPU selection"),B->ScanComponent->AcquisitionBackend,EVirtualLidarAcquisitionBackend::AccurateCpuTrace);
 A->StartSensor();const int Generation=A->ScanComponent->GetFileCaptureRevision(),Revision=A->GetConfigurationRevision();FString Reason;
 TestTrue(TEXT("individual sensor can select GPU while running"),A->SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend::HardwareRayTracing,Reason));
 TestTrue(TEXT("backend switch cancels obsolete generation and preserves running intent"),A->ScanComponent->GetFileCaptureRevision()>Generation&&A->IsSensorRunning());
 TestTrue(TEXT("configuration revision changes for existing 5Hz UI refresh"),A->GetConfigurationRevision()>Revision);
 TestEqual(TEXT("individual selection does not affect another sensor"),B->ScanComponent->AcquisitionBackend,EVirtualLidarAcquisitionBackend::AccurateCpuTrace);
 FVirtualSensorEditableState State,Applied;A->ReadEditableState(State);++State.PreviewPointStride;
 TestTrue(TEXT("ordinary editable-state transaction remains available"),A->ApplyEditableState(State,Reason));
 TestEqual(TEXT("ordinary settings changes preserve explicit GPU backend"),A->ScanComponent->AcquisitionBackend,EVirtualLidarAcquisitionBackend::HardwareRayTracing);
 B->ReadEditableState(State);State.LidarProfile=EVirtualLidarDeviceProfile::HesaiXT32;State.SimulationQuality=EVirtualSensorSimulationQuality::FullSpec;
 TestTrue(TEXT("profile transaction remains available"),B->ApplyProfileAndSimulationQuality(State,Applied,Reason));
 TestEqual(TEXT("profile selection preserves explicit CPU backend"),B->ScanComponent->AcquisitionBackend,EVirtualLidarAcquisitionBackend::AccurateCpuTrace);

 A->StopSensor();A->Destroy();B->Destroy();Manager->Destroy();return !HasAnyErrors();
}

namespace
{
class FGpuRigidInstanceRun final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;FSensorRtMesh Mesh;TSharedPtr<ISensorRtScan,ESPMode::ThreadSafe> Scan;
 int Phase=0;uint64 Builds=0;double Started=FPlatformTime::Seconds();
 void Submit()
 {
  FSensorRtRequest R;R.Meshes.Add(Mesh);R.Rays.Add({FVector3f::ZeroVector,1000,FVector3f(1,0,0),0});Scan=SensorOwnedRayTracing::Submit(MoveTemp(R));
  if(!Scan)Test->AddError(TEXT("rigid instance request was not admitted"));
 }
public:
 explicit FGpuRigidInstanceRun(FAutomationTestBase* T):Test(T)
 {
  auto V=MakeShared<TArray<FVector3f>,ESPMode::ThreadSafe>();*V={FVector3f(0,-100,-100),FVector3f(0,100,-100),FVector3f(0,0,100)};
  auto N=MakeShared<TArray<FVector4f>,ESPMode::ThreadSafe>();N->Add(FVector4f(1,0,0,0));
  Mesh.Revision=0x4750554c49444152ull;Mesh.Vertices=V;Mesh.NormalsAndSurfaces=N;Mesh.LocalToWorld=FTransform(FVector(300,0,0));Submit();
 }
 bool Update() override
 {
  if(!Scan)return true;if(FPlatformTime::Seconds()-Started>10){Test->AddError(TEXT("rigid instance RHI timeout"));Scan->Cancel();return true;}
  TArray<FSensorRtHit> Hits;FString Error;if(!Scan->Poll(Hits,Error))return false;if(!Error.IsEmpty()){Test->AddError(Error);return true;}
  if(Hits.Num()!=1){Test->AddError(TEXT("rigid instance result count mismatch"));return true;}
  Test->TestTrue(TEXT("instance transform updates measured distance"),FMath::Abs(Hits[0].NormalAndDistance.W-(Phase==0?300:500))<.01);
  const FVector Expected=-Mesh.LocalToWorld.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal();
  Test->TestTrue(TEXT("normal uses acquisition instance inverse transpose"),FVector::DotProduct(FVector(FVector3f(Hits[0].NormalAndDistance)),Expected)>.9999);
  if(Phase==0){Builds=SensorOwnedRayTracing::GetBlasBuildCount();Mesh.LocalToWorld=FTransform(FRotator(0,45,0),FVector(500,0,0));Phase=1;Submit();return false;}
  Test->TestEqual(TEXT("rigid motion reuses immutable BLAS and only updates TLAS instance"),SensorOwnedRayTracing::GetBlasBuildCount(),Builds);return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarRigidInstanceTest,"MA0T10.GpuLidar.RigidInstanceRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarRigidInstanceTest::RunTest(const FString&)
{if(!TestTrue(TEXT("real HWRT pipeline support required"),SensorOwnedRayTracing::IsAvailable()))return false;ADD_LATENT_AUTOMATION_COMMAND(FGpuRigidInstanceRun(this));return true;}
#endif
