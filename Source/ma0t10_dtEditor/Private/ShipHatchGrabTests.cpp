#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchOverlay.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "ProceduralMeshComponent.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"

namespace
{
FString GrabRoot(){FString R;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),R);return R.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-06/grab"):R;}
void GrabJson(const FString& Name,const TSharedRef<FJsonObject>& R){FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));IFileManager::Get().MakeDirectory(*GrabRoot(),true);FFileHelper::SaveStringToFile(S,*(GrabRoot()/Name));}
bool Press(UShipHatchMonitorWidget& M,const FString& Label)
{TArray<UWidget*> Widgets;M.WidgetTree->GetAllWidgets(Widgets);for(auto* W:Widgets)if(auto* B=Cast<UButton>(W))if(auto* T=Cast<UTextBlock>(B->GetContent()))if(T->GetText().ToString().StartsWith(Label)){B->OnClicked.Broadcast();return true;}return false;}
FPointerEvent GrabPointer(FVector2D P,bool Held){static const TSet<FKey> Down{EKeys::LeftMouseButton},Up;return FPointerEvent(0,P,P,Held?Down:Up,EKeys::LeftMouseButton,0,FModifierKeysState());}
bool Scoop(UShipHatchMonitorWidget& M,AShipHatchSimulation& S)
{
 auto* PC=M.GetOwningPlayer();int W=0,H=0;PC->GetViewportSize(W,H);const auto G=M.GetCachedGeometry();
 for(auto XY:{FVector2D(7,-7),FVector2D(5,-5),FVector2D(8,0),FVector2D(-8,0),FVector2D(7,5),FVector2D(-7,5)})
 {FVector2D Pixel;if(!PC->ProjectWorldLocationToScreen(S.GetActorTransform().TransformPosition(FVector(XY,S.Surface().Height(XY.X,XY.Y))*100),Pixel))continue;FVector O,D,Hit;if(!PC->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,O,D)||!S.PickCargo(O,D,Hit))continue;const auto P=G.LocalToAbsolute(Pixel*G.GetLocalSize()/FVector2D(W,H));const auto Before=S.Revision();M.TakeWidget()->OnMouseButtonDown(G,GrabPointer(P,true));M.TakeWidget()->OnMouseButtonUp(G,GrabPointer(P,false));if(S.Revision()!=Before)return true;}
 return false;
}
void CloseCamera(AShipHatchMonitorHost& H,AShipHatchSimulation& S)
{const auto Pose=S.GetActorTransform();const FVector P=Pose.TransformPosition(FVector(8,8,S.HoldDepth+5)*100),Aim=Pose.TransformPosition(FVector(0,4,S.Surface().Height(0,4)+2.5)*100);H.ViewCamera->SetActorLocationAndRotation(P,(Aim-P).Rotation());}
void HopperCamera(AShipHatchMonitorHost& H,AShipHatchSimulation& S)
{const double Y=S.SeaRailY()-S.RailSpan*.5,Z=S.QuayZ()+15;const auto Pose=S.GetActorTransform();const auto P=Pose.TransformPosition(FVector(18,Y+12,Z+10)*100),Aim=Pose.TransformPosition(FVector(0,Y,Z+1)*100);H.ViewCamera->SetActorLocationAndRotation(P,(Aim-P).Rotation());}
class FGrabPresentationRun:public IAutomationLatentCommand
{
 FAutomationTestBase* T;int Phase=0,CostState=0;double Began=FPlatformTime::Seconds(),Changed=0,Measure=0;uint64 LastFrame=0;uint32 Original=0,PointCrc=0;
 TSharedPtr<const FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe> Retained;TUniquePtr<TGuardValue<bool>> Messages;TArray<double> Engine,Game,Render;TArray<TSharedPtr<FJsonValue>> Costs;
 static double Mean(const TArray<double>& A){double S=0;for(double V:A)S+=V;return A.Num()?S/A.Num():0;}
 void Instances(AShipHatchSimulation& S)
 {
  auto* G=Cast<UProceduralMeshComponent>(S.GrabPrimitive());T->TestNotNull(TEXT("single retained grab component exists"),G);if(!G)return;TArray<UProceduralMeshComponent*> Parts;G->GetOwner()->GetComponents(Parts);int N=0;for(auto* P:Parts)N+=P->ComponentHasTag(TEXT("ShipHatchGrab"));T->TestEqual(TEXT("exactly one grab mesh instance"),N,1);T->TestEqual(TEXT("one material/draw section"),G->GetNumSections(),1);
  const auto* Sec=G->GetProcMeshSection(0);T->TestTrue(TEXT("bounded detailed procedural bucket geometry"),Sec&&Sec->ProcVertexBuffer.Num()>500&&Sec->ProcVertexBuffer.Num()<5000&&Sec->ProcIndexBuffer.Num()/3<5000);T->TestEqual(TEXT("visibility matches physical presence toggle"),G->IsVisible(),S.IsGrabVisible());T->TestEqual(TEXT("hidden grab never occludes sensor queries"),G->GetCollisionEnabled(),S.IsGrabVisible()?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);T->TestTrue(TEXT("four arms retain3.3m length within1mm"),S.GrabArmLengthErrorMeters<.001);
 }
 void Shot(UWorld& W,UShipHatchMonitorWidget& M,AShipHatchSimulation& S,const FString& Name)
 {
  TArray<FColor> Pixels;FIntVector Size;if(!T->TestTrue(TEXT("grab evidence uses actual RHI pixels"),FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);const auto Dir=GrabRoot()/TEXT("captures");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveArrayToFile(Bytes,*(Dir/(Name+TEXT(".png"))));const auto Progress=FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-06/progress")/(TEXT("30-")+FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M-"))+Name+TEXT(".png"));FFileHelper::SaveArrayToFile(Bytes,*Progress);
  auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("kind"),TEXT("automated D3D12; not direct Computer Use"));R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);R->SetNumberField(TEXT("font_percent"),M.GetFontScale()*100);R->SetBoolField(TEXT("grab_visible"),S.IsGrabVisible());R->SetNumberField(TEXT("removed_m3"),S.TotalRemovedVolume());R->SetNumberField(TEXT("surface_crc"),S.Surface().Hash());R->SetNumberField(TEXT("max_grab_build_ms"),S.MaxGrabMeshBuildMs);R->SetNumberField(TEXT("rod_error_m"),S.GrabArmLengthErrorMeters);FString Commit;FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),Commit);R->SetStringField(TEXT("source_commit"),Commit);FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(Name+TEXT(".json"))));
 }
 void Stop(AShipHatchSimulation& S){for(int I=0;I<3;++I)if(auto* A=S.SensorAt(I))A->StopSensor();}
public:
 explicit FGrabPresentationRun(FAutomationTestBase* Test):T(Test){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(Now-Began>240){T->AddError(TEXT("grab presentation timeout"));return true;}UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
  AShipHatchMonitorHost* H=nullptr;for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;if(!H||!H->Monitor||!H->Simulation||H->Simulation->IsGenerating())return false;auto& M=*H->Monitor;auto& S=*H->Simulation;
  if(Phase==0){if(!M.GetAnalysis())return false;Messages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);int Font=100;FParse::Value(FCommandLine::Get(),TEXT("HatchFontPercent="),Font);M.SetFontPercent(Font);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(1);Press(M,TEXT("원료 확대"));T->TestFalse(TEXT("defaultOFF pending direct visual/site acceptance"),S.IsGrabVisible());Instances(S);Original=S.Surface().Hash();Retained=S.SensorAt(0)->ScanComponent->GetLastFrameSnapshot();PointCrc=FCrc::MemCrc32(Retained->Points->GetData(),Retained->Points->Num()*sizeof(FVirtualLidarPoint));Changed=Now;Phase=1;return false;}
  if(Phase==1){if(Now-Changed<1)return false;Shot(*W,M,S,TEXT("grab-off-before"));T->TestTrue(TEXT("production dig toggle"),Press(M,TEXT("퍼내기 켜기")));T->TestTrue(TEXT("hidden grab still excavates immediately through native pointer"),Scoop(M,S));Changed=Now;Phase=2;return false;}
  if(Phase==2){if(!M.GetAnalysis()||!S.HasFreshMeasurement()||Now-Changed<2)return false;T->TestTrue(TEXT("off digging changes volume and refreshes measurement"),S.TotalRemovedVolume()>0);if(auto* O=H->FindComponentByClass<UShipHatchOverlay>())for(int I=1;I<O->BatchedMeshes.Num();++I)T->TestTrue(TEXT("off has no bucket/discharge/mist overlay"),O->BatchedMeshes[I].MeshVerts.IsEmpty());Instances(S);Shot(*W,M,S,TEXT("grab-off-after"));T->TestTrue(TEXT("undo remains exact with grab off"),S.UndoDig()&&S.Surface().Hash()==Original);T->TestTrue(TEXT("header toggle enables single physical grab"),Press(M,TEXT("그랩 표시"))&&S.IsGrabVisible());CloseCamera(*H,S);Changed=Now;Phase=3;return false;}
  if(Phase==3){if(!M.GetAnalysis()||Now-Changed<2)return false;Instances(S);Shot(*W,M,S,TEXT("grab-near-closed"));T->TestTrue(TEXT("shown bucket physically occludes center sightline"),S.DiagnoseSight(0,FVector(0,4,S.Surface().Height(0,4))).Cause==TEXT("Grab"));S.SetGrabPose(FVector(0,4,S.Surface().Height(0,4)+.2),1);Changed=Now;Phase=4;return false;}
  if(Phase==4){if(Now-Changed<1)return false;Instances(S);Shot(*W,M,S,TEXT("grab-near-open"));S.ResetGrabPose();M.SetCameraPreset(1);Press(M,TEXT("원료 확대"));Changed=Now;Phase=5;return false;}
  if(Phase==5){if(Now-Changed<1)return false;T->TestTrue(TEXT("visible grab dig starts coupled animation"),Scoop(M,S));HopperCamera(*H,S);Changed=Now;Phase=6;return false;}
  if(Phase==6){if(Now-Changed<3.8)return false;Instances(S);Shot(*W,M,S,TEXT("grab-discharge"));if(auto* O=H->FindComponentByClass<UShipHatchOverlay>()){T->TestTrue(TEXT("no duplicate animated grab mesh"),O->BatchedMeshes[1].MeshVerts.IsEmpty());T->TestTrue(TEXT("lightweight flow and mist"),O->BatchedMeshes[2].MeshVerts.Num()<=32&&O->BatchedMeshes[3].MeshVerts.Num()>0);}T->TestTrue(TEXT("off cancels during discharge without undoing excavation"),Press(M,TEXT("그랩 표시"))&&!S.IsGrabVisible()&&S.TotalRemovedVolume()>0);Changed=Now;Phase=7;return false;}
  if(Phase==7){if(!M.GetAnalysis()||!S.HasFreshMeasurement()||Now-Changed<2)return false;Instances(S);for(int I=0;I<3;++I)T->TestTrue(TEXT("off sightline diagnostic has no grab cause"),S.DiagnoseSight(I,FVector(0,4,S.Surface().Height(0,4))).Cause!=TEXT("Grab"));Shot(*W,M,S,TEXT("grab-off-after-animation"));T->TestEqual(TEXT("show/hide/motion never mutates retained measured points"),FCrc::MemCrc32(Retained->Points->GetData(),Retained->Points->Num()*sizeof(FVirtualLidarPoint)),PointCrc);S.RestoreCargo();Press(M,TEXT("그랩 표시"));M.SetSplitView(false);M.SelectView(0);CloseCamera(*H,S);Stop(S);Changed=Now;Phase=8;return false;}
  if(Phase==8){if(Now-Changed<2)return false;Engine.Reset();Game.Reset();Render.Reset();LastFrame=GFrameCounter;Measure=Now;Phase=9;return false;}
  if(Phase==9)
  {
   if(GFrameCounter!=LastFrame){LastFrame=GFrameCounter;Engine.Add(W->GetDeltaSeconds()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Render.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));}if(Now-Measure<5)return false;auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("grab_on"),S.IsGrabVisible());R->SetNumberField(TEXT("condition_index"),CostState);R->SetBoolField(TEXT("rig_visible"),CostState!=3);R->SetStringField(TEXT("camera"),CostState<2?TEXT("grab close"):TEXT("site ref4"));R->SetNumberField(TEXT("measurement_seconds"),Now-Measure);R->SetNumberField(TEXT("engine_fps"),1000/Mean(Engine));R->SetNumberField(TEXT("game_mean_ms"),Mean(Game));R->SetNumberField(TEXT("render_thread_mean_ms"),Mean(Render));R->SetNumberField(TEXT("engine_max_ms"),FMath::Max(Engine));R->SetNumberField(TEXT("game_max_ms"),FMath::Max(Game));Costs.Add(MakeShared<FJsonValueObject>(R));T->TestTrue(TEXT("grab render-only window has no150ms engine/game frame"),Engine.Num()>0&&FMath::Max(Engine)<=150&&FMath::Max(Game)<=150);
   if(CostState<4){++CostState;if(CostState==1)Press(M,TEXT("그랩 표시"));else if(CostState==2)M.SetCameraPreset(3);else S.ShowUnloader(CostState!=3);Stop(S);Changed=Now;Phase=8;return false;}
   auto Result=MakeShared<FJsonObject>();Result->SetStringField(TEXT("method"),TEXT("render-only: grabOn close, grabOff close, grabOff siteRigOn, siteRigOff, siteRigOn; rig on/off/on uses identical site camera; all3 acquisitions stopped; each2s warm+5s measurement; no capture during windows; separate from60sABAB"));Result->SetArrayField(TEXT("conditions"),Costs);Result->SetNumberField(TEXT("max_mesh_build_ms"),S.MaxGrabMeshBuildMs);auto* G=Cast<UProceduralMeshComponent>(S.GrabPrimitive());Result->SetNumberField(TEXT("mesh_instances"),1);Result->SetNumberField(TEXT("sections"),G->GetNumSections());Result->SetNumberField(TEXT("vertices"),G->GetProcMeshSection(0)->ProcVertexBuffer.Num());Result->SetNumberField(TEXT("triangles"),G->GetProcMeshSection(0)->ProcIndexBuffer.Num()/3);Result->SetNumberField(TEXT("constant_arm_error_m"),S.GrabArmLengthErrorMeters);if(auto* Rig=Cast<UProceduralMeshComponent>(G->GetOwner()->GetRootComponent())){int Vertices=0,Triangles=0;for(int I=0;I<Rig->GetNumSections();++I)if(const auto* Sec=Rig->GetProcMeshSection(I)){Vertices+=Sec->ProcVertexBuffer.Num();Triangles+=Sec->ProcIndexBuffer.Num()/3;}Result->SetNumberField(TEXT("rig_vertices"),Vertices);Result->SetNumberField(TEXT("rig_triangles"),Triangles);Result->SetNumberField(TEXT("rig_draw_sections"),Rig->GetNumSections());}GrabJson(TEXT("grab-render-cost.json"),Result);for(int I=0;I<3;++I)if(auto* A=S.SensorAt(I))A->StartSensor();Changed=Now;Phase=10;return false;
  }
  if(Phase==10){if(!M.GetAnalysis()||!S.HasFreshMeasurement()||Now-Changed<1)return false;Instances(S);T->TestFalse(TEXT("ends in defaultOFF after successful3-device restart"),S.IsGrabVisible());auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("valid"),!T->HasAnyErrors());GrabJson(TEXT("grab-acceptance.json"),R);return true;}
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchGrabPresentation,"HATCH06.GrabPresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchGrabPresentation::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FGrabPresentationRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
