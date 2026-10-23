#include "SensorOwnedRayTracing.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorRayQuery.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "ImageUtils.h"
#include "Json.h"

namespace
{
class FGpuLifetimeRun final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int Phase=0,Edits=0,Cancels=0;double Started=FPlatformTime::Seconds(),Pause=0,EditStarted=0,LastLog=0,DigFinished=0;
 AShipHatchSimulation* Sim=nullptr;AVirtualLidarSensorActor* Sensor=nullptr;AActor* Cargo=nullptr;UVirtualLidarSurfaceResponseComponent* Optical=nullptr;
 FTransform OriginalPose;TArray<FName> OriginalTags;float OriginalReflectivity=0;
 int64 Baseline=0;uint64 MemoryBefore=0,MemoryPeak=0;TArray<TSharedPtr<FJsonValue>> Samples;
 TSharedPtr<const FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe> Retained;uint32 Crc=0;
 FString Root()
 {FString P;if(FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),P))return P/TEXT("lifetime");return FPaths::ProjectSavedDir()/TEXT("Reports/GPU-LIDAR-01/progress/lifetime");}
 uint32 Hash(const FVirtualLidarFrameSnapshot& F) const{return FCrc::MemCrc32(F.Points->GetData(),F.Points->Num()*sizeof(FVirtualLidarPoint));}
 void RestoreIdentity()
 {if(Cargo)Cargo->Tags=OriginalTags;if(Optical)Optical->Reflectivity940Nm=OriginalReflectivity;if(Sensor){Sensor->StopSensor();Sensor->SetActorTransform(OriginalPose);Sensor->ScanComponent->InvalidateSurfaceResponseCache();}}
 void MutateAfterAdmission()
 {Cargo->Tags.Remove(TEXT("HatchCargo"));Cargo->Tags.Add(TEXT("GpuLidarChangedAfterAcquisition"));Optical->Reflectivity940Nm=.91f;
  Sensor->SetActorLocation(OriginalPose.GetLocation()+FVector(321,123,0));Phase=1;}
 void Save()
 {
  auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("production_digs"),Edits);R->SetNumberField(TEXT("cancelled_acquisitions"),Cancels);
  R->SetNumberField(TEXT("rss_before_bytes"),MemoryBefore);R->SetNumberField(TEXT("rss_after_bytes"),FPlatformMemory::GetStats().UsedPhysical);R->SetNumberField(TEXT("rss_peak_bytes"),MemoryPeak);
  R->SetNumberField(TEXT("dig_wall_seconds"),FPlatformTime::Seconds()-EditStarted);R->SetArrayField(TEXT("samples"),Samples);R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());
  R->SetStringField(TEXT("rss_policy"),TEXT("measurement only; no RSS acceptance assertion; HATCH06 #26 remains separate"));
  FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Root(),true);FFileHelper::SaveStringToFile(Text,*(Root()/TEXT("gpu-50-digs-rss.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 }
public:
 explicit FGpuLifetimeRun(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(Now-Started>180){Test->AddError(TEXT("GPU lifetime/50-dig validation timeout"));RestoreIdentity();Save();return true;}
  if(Phase==0)
  {
   UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!World||!FinalAcceptanceViewportReady(World))return false;
   for(TActorIterator<AShipHatchSimulation>I(World);I;++I)Sim=*I;if(!Sim||Sim->IsGenerating()||!Sim->HasFreshMeasurement())return false;
   for(int I=0;I<3;++I)if(auto* A=Sim->SensorAt(I))A->StopSensor();Sensor=Sim->SensorAt(0);OriginalPose=Sensor->GetActorTransform();
   auto Query=Sim->CaptureRayQuery(Sensor,ECC_Visibility);TArray<FVirtualSensorCollisionMesh> Meshes;
   if(!Query||!Query->CollisionMeshes(Meshes)||Meshes.Num()<2||Meshes[1].Surfaces.IsEmpty()){Test->AddError(TEXT("cargo collision identity missing"));return true;}
   Cargo=Meshes[1].Surfaces[0]->GetOwner();Optical=Cargo->FindComponentByClass<UVirtualLidarSurfaceResponseComponent>();if(!Optical){Test->AddError(TEXT("cargo optical snapshot source missing"));return true;}
   OriginalTags=Cargo->Tags;OriginalReflectivity=Optical->Reflectivity940Nm;
   auto* Scan=Sensor->ScanComponent.Get();Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::HardwareRayTracing;Scan->ScanInterval=.05f;Baseline=Scan->GetRuntimeStatus().FrameId;
   Sensor->StartSensor();Scan->RequestImmediateScheduledScan();Scan->PrepareScheduledScan(Sensor->GetWorld()->GetTimeSeconds());
   // A restart retains the one-admission-per-engine-frame latch. Never mutate
   // the fixture before a real acquisition has taken its immutable snapshot.
   if(!Scan->GetRuntimeStatus().bAcquisitionInFlight){Test->AddInfo(TEXT("Identity fixture waits for actual admission after same-frame restart"));Phase=6;return false;}
   MutateAfterAdmission();return false;
  }
  auto* Scan=Sensor->ScanComponent.Get();
  if(Phase==6)
  {Scan->PrepareScheduledScan(Sensor->GetWorld()->GetTimeSeconds());if(!Scan->GetRuntimeStatus().bAcquisitionInFlight)return false;MutateAfterAdmission();return false;}
  if(Now-LastLog>5){LastLog=Now;UE_LOG(LogTemp,Display,TEXT("[GpuLifetime] phase=%d digs=%d cancels=%d running=%d frame=%lld baseline=%lld"),Phase,Edits,Cancels,Scan->IsScanRunning()?1:0,Scan->GetRuntimeStatus().FrameId,Baseline);}
  if(Phase==1)
  {
   const auto Frame=Scan->GetLastFrameSnapshot();if(!Frame||Frame->FrameId<=Baseline)return false;
   Test->TestTrue(TEXT("acquisition transform is not replaced by completion-time sensor pose"),Frame->AcquisitionTransform.Equals(OriginalPose,.001));int CargoPoints=0;
   for(const auto& P:*Frame->Points)if(P.bHit&&P.HitActorName==Cargo->GetFName())
   {++CargoPoints;if(P.SemanticLabel!=TEXT("HatchCargo")||P.HitActorTags.Contains(TEXT("GpuLidarChangedAfterAcquisition"))||!FMath::IsNearlyEqual(P.SurfaceReflectivity,OriginalReflectivity,1.e-6f)){Test->AddError(TEXT("GPU label/optical identity was replaced by live Actor state"));break;}}
   Test->TestTrue(TEXT("snapshot identity test exercised real cargo returns"),CargoPoints>0);Retained=Frame;Crc=Hash(*Retained);RestoreIdentity();Pause=Now;Phase=2;return false;
  }
  if(Phase==2)
  {
   if(Now-Pause<.05)return false;
   if(Cancels<20){Baseline=Scan->GetRuntimeStatus().FrameId;Sensor->StartSensor();Scan->RequestImmediateScheduledScan();Scan->PrepareScheduledScan(Sensor->GetWorld()->GetTimeSeconds());Sensor->StopSensor();++Cancels;Pause=Now;return false;}
   Test->TestEqual(TEXT("stopped generations do not publish late GPU/worker results"),Scan->GetRuntimeStatus().FrameId,Baseline);
   Sensor->StartSensor();Scan->RequestImmediateScheduledScan();Pause=Now;Phase=5;return false;
  }
  if(Phase==5)
  {
   if(Now-Pause<2)return false;MemoryBefore=MemoryPeak=FPlatformMemory::GetStats().UsedPhysical;EditStarted=Now;Phase=3;
  }
  if(Phase==3)
  {
   const double X=5+(Edits%5)*.2,Y=-5+(Edits/5%5)*.2;
   const FVector O=Sim->GetActorTransform().TransformPosition(FVector(X,Y,Sim->HoldDepth+6)*100),D=Sim->GetActorTransform().TransformVectorNoScale(FVector(0,0,-1));
   Baseline=Scan->GetRuntimeStatus().FrameId;if(!Sim->Dig(O,D,.7,.15)){Test->AddError(FString::Printf(TEXT("production dig %d was rejected"),Edits+1));Save();return true;}
   for(int I=1;I<3;++I)if(auto* A=Sim->SensorAt(I))A->StopSensor();DigFinished=FPlatformTime::Seconds();++Edits;Phase=4;return false;
  }
  if(Scan->GetRuntimeStatus().FrameId<=Baseline||Scan->GetLastFrameSnapshot()->AcquisitionStartedMonotonic<DigFinished)return false;
  Test->TestEqual(TEXT("next completed acquisition after every dig is real HWRT"),Scan->GetLastFrameSnapshot()->AcquisitionBackend,EVirtualLidarAcquisitionBackend::HardwareRayTracing);
  Test->TestEqual(TEXT("retained immutable acquisition remains unchanged across cancel/dig/BLAS revisions"),Hash(*Retained),Crc);
  const uint64 Memory=FPlatformMemory::GetStats().UsedPhysical;MemoryPeak=FMath::Max(MemoryPeak,Memory);auto Sample=MakeShared<FJsonObject>();Sample->SetNumberField(TEXT("dig"),Edits);Sample->SetNumberField(TEXT("rss_bytes"),Memory);Sample->SetNumberField(TEXT("surface_revision"),Sim->Revision());Samples.Add(MakeShared<FJsonValueObject>(Sample));
  if(Edits<50){Phase=3;return false;}
  Test->TestEqual(TEXT("all fifty production excavations were exercised"),Edits,50);Save();Scan->StopScan();return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarLifetimeTest,"MA0T10.GpuLidar.IdentityCancelAndFiftyDigsRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarLifetimeTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FGpuLifetimeRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
