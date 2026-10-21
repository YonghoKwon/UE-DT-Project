#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Camera/CameraActor.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace
{
FString QualityRoot(){FString S;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),S);return S.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-06/v2/quality"):S;}
void QualityJson(const FString& Name,TSharedRef<FJsonObject> R){FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));IFileManager::Get().MakeDirectory(*QualityRoot(),true);FFileHelper::SaveStringToFile(S,*(QualityRoot()/Name));}
class FHatchQualityRun final:public IAutomationLatentCommand
{
 FAutomationTestBase* T;int32 Step=0;double Began=FPlatformTime::Seconds(),Changed=0;bool Setup=false;TArray<TSharedPtr<FJsonValue>> Details;
 void Camera(AShipHatchMonitorHost& H,AShipHatchSimulation& S,FVector P,FVector Aim){const auto Pose=S.GetActorTransform();P=Pose.TransformPosition(P*100);Aim=Pose.TransformPosition(Aim*100);H.ViewCamera->SetActorLocationAndRotation(P,(Aim-P).Rotation());}
 void Configure(AShipHatchMonitorHost& H,AShipHatchSimulation& S,UShipHatchMonitorWidget& M)
 {
  M.SelectView(0);M.SetSplitView(false);M.SetModelPresentation(true);
  if(Step==0){M.SetCameraPreset(3);M.SetModelPresentation(true);}
  if(Step==1)Camera(H,S,FVector(100,-22,S.QuayZ()+8),FVector(0,-22,S.QuayZ()+24));
  if(Step==2)Camera(H,S,FVector(80,S.QuayEdgeY()-25,S.QuayZ()+1.8),FVector(0,(S.BoomTipY()+S.SeaRailY()-S.RailSpan)*.5,(S.QuayZ()+S.HoldDepth+S.BoomHeight+16.2)*.5));
  if(Step==3)Camera(H,S,FVector(16,S.SeaRailY()+18,S.HoldDepth+S.BoomHeight+8),FVector(0,S.SeaRailY()-3,S.HoldDepth+S.BoomHeight+3));
  if(Step==4)Camera(H,S,FVector(24,S.HopperCenterY()-15,S.QuayZ()+15),FVector(3,S.HopperCenterY(),S.QuayZ()+12));
  if(Step==5)Camera(H,S,FVector(-95,-58,S.HoldDepth+28),FVector(-128,0,S.HoldDepth+13));
  if(Step>=6)
  {
   const bool Coal=Step>=10;if(S.bCoal!=Coal){S.bCoal=Coal;S.Regenerate(false);}
   const int State=(Step-6)%4;
   if(State==0)S.RestoreCargo();
   if(State==2){const auto Pose=S.GetActorTransform();const FVector O=Pose.TransformPosition(FVector(6,-6,40)*100),D=Pose.TransformVectorNoScale(FVector(0,0,-1));T->TestTrue(TEXT("quality cut uses physical production excavation"),S.Dig(O,D,2,1.5));}
   const double Z=S.Surface().Height(6,-6);
   Camera(H,S,State%2?FVector(10,-13,Z+5):FVector(23,-27,Z+23),FVector(6,-6,Z-.2));
  }
 }
 void Shot(UWorld& W,UShipHatchMonitorWidget& M)
 {
  const TCHAR* Names[]={TEXT("ref4-v2"),TEXT("ref1-v2"),TEXT("ref5-v2"),TEXT("girder-detail-v2"),TEXT("hopper-stair-detail-v2"),TEXT("bridge-detail-v2"),TEXT("cargo-quality-iron-wide-before"),TEXT("cargo-quality-iron-close-before"),TEXT("cargo-quality-iron-wide-after"),TEXT("cargo-quality-iron-close-after"),TEXT("cargo-quality-coal-wide-before"),TEXT("cargo-quality-coal-close-before"),TEXT("cargo-quality-coal-wide-after"),TEXT("cargo-quality-coal-close-after")};
  TArray<FColor> Pixels;FIntVector Size;if(!T->TestTrue(TEXT("quality evidence uses actual D3D12 pixels"),FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);const FString Dir=QualityRoot()/TEXT("captures");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveArrayToFile(Png,*(Dir/(FString(Names[Step])+TEXT(".png"))));
  const FString Progress=FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-06/progress")/(TEXT("40-")+FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M-"))+Names[Step]+TEXT(".png"));FFileHelper::SaveArrayToFile(Png,*Progress);
  auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("kind"),TEXT("actual D3D12 automated quality evidence; not direct UI"));R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);FString Commit;FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),Commit);R->SetStringField(TEXT("source_commit"),Commit);const auto Rect=M.GetSceneRect();const auto Local=M.GetCachedGeometry().GetLocalSize();R->SetNumberField(TEXT("scene_left_px"),Rect.Left*Size.X/Local.X);R->SetNumberField(TEXT("scene_top_px"),Rect.Top*Size.Y/Local.Y);R->SetNumberField(TEXT("scene_right_px"),Rect.Right*Size.X/Local.X);R->SetNumberField(TEXT("scene_bottom_px"),Rect.Bottom*Size.Y/Local.Y);R->SetBoolField(TEXT("clean_model_view"),M.IsModelPresentation());QualityJson(FString(TEXT("captures/"))+Names[Step]+TEXT(".json"),R);
 }
public:
 explicit FHatchQualityRun(FAutomationTestBase* In):T(In){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(Now-Began>180){T->AddError(TEXT("model quality evidence timeout"));return true;}UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
  AShipHatchMonitorHost* H=nullptr;for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;if(!H||!H->Simulation||!H->Monitor||H->Simulation->IsGenerating())return false;auto& S=*H->Simulation;auto& M=*H->Monitor;
  if(!Setup)
  {
   int Count=0;for(TActorIterator<AActor>I(W);I;++I){TInlineComponentArray<UProceduralMeshComponent*> Parts(*I);for(auto* P:Parts)if(P->ComponentHasTag(TEXT("HatchFineDetail"))){++Count;T->TestEqual(TEXT("small render fittings have no measurement collision"),P->GetCollisionEnabled(),ECollisionEnabled::NoCollision);T->TestTrue(TEXT("fine detail is distance culled at180-220m"),P->LDMaxDrawDistance>=18000&&P->LDMaxDrawDistance<=22000);auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("component"),P->GetName());const auto* Sec=P->GetProcMeshSection(0);T->TestTrue(TEXT("render-only detail has a bounded populated mesh"),Sec&&Sec->ProcVertexBuffer.Num()>500&&Sec->ProcIndexBuffer.Num()<180000);if(Sec){R->SetNumberField(TEXT("vertices"),Sec->ProcVertexBuffer.Num());R->SetNumberField(TEXT("triangles"),Sec->ProcIndexBuffer.Num()/3);}R->SetNumberField(TEXT("cull_m"),P->LDMaxDrawDistance*.01);Details.Add(MakeShared<FJsonValueObject>(R));}}
   T->TestEqual(TEXT("one retained detail component each for vessel and unloader"),Count,2);Setup=true;Configure(*H,S,M);Changed=Now;return false;
  }
  if(Now-Changed<1.5||!S.HasFreshMeasurement())return false;Shot(*W,M);
  if(++Step<14){Configure(*H,S,M);Changed=Now;return false;}
  auto R=MakeShared<FJsonObject>();R->SetArrayField(TEXT("detail_meshes"),Details);R->SetBoolField(TEXT("valid"),!T->HasAnyErrors());QualityJson(TEXT("quality-acceptance.json"),R);return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchQuality,"HATCH06.ModelQualityRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchQuality::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchQualityRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
