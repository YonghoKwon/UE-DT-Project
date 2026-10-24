#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/Sensor/IndustrialLidarProfiles.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "ImageUtils.h"
#include "Json.h"
#include "ProceduralMeshComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchOverlay.h"
#include "Camera/CameraActor.h"

void ValidateHatchPresentation(FAutomationTestBase*,UShipHatchMonitorWidget*);

struct FHatch06Probe
{
 static void Cargo(UShipHatchMonitorWidget& M){M.OnFrameCargo();}
 static void Covers(UShipHatchMonitorWidget& M){M.SetCameraPreset(0);M.bFrameVessel=false;M.bFrameUnloader=false;M.OrbitDistance=8500;M.UpdateCamera();}
 static FSlateRect Scene(const UShipHatchMonitorWidget& M){return M.SceneRect;}
 static void Vessel(UShipHatchMonitorWidget& M){M.OnFrameVessel();}
 static bool StartReferenceGrab(UShipHatchMonitorWidget& M,FString& Trace)
 {
  M.bDigEnabled=true;auto& S=*M.Simulation;auto* PC=M.GetOwningPlayer();int W=0,H=0;PC->GetViewportSize(W,H);const auto G=M.GetCachedGeometry();bool Accepted=false;
  // At720p/150% a valid cargo projection can lie behind a sensor badge.
  // Use an actually visible, physically pickable production target; never
  // disable hardware collision or bypass the monitor's input guard.
  for(auto XY:{FVector2D(5,-5),FVector2D(7,-5),FVector2D(7,-7),FVector2D(8,-5),FVector2D(5,-8),FVector2D(-5,-5),FVector2D(-7,-5),FVector2D(-7,-7),FVector2D(5,5),FVector2D(7,5),FVector2D(-5,5),FVector2D(-7,5),FVector2D(8,0),FVector2D(-8,0)})
  {
   FVector2D Pixel;const FVector P=S.GetActorTransform().TransformPosition(FVector(XY,S.Surface().Height(XY.X,XY.Y))*100);if(!PC->ProjectWorldLocationToScreen(P,Pixel))continue;
   const auto Pointer=G.LocalToAbsolute(Pixel*G.GetLocalSize()/FVector2D(W,H));const bool Input=M.SceneInputAt(G,Pointer);FVector O,D,Hit;PC->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,O,D);const bool Physical=S.PickCargo(O,D,Hit);
   Trace+=FString::Printf(TEXT("target %.1f,%.1f pixel %.1f,%.1f input%d physical%d\n"),XY.X,XY.Y,Pixel.X,Pixel.Y,Input,Physical);
   if(Input&&Physical&&M.DigAt(G,Pointer)){Accepted=true;break;}
  }
  M.bDigEnabled=false;return Accepted;
 }
 static void HopperCamera(UShipHatchMonitorWidget& M)
 {
  const auto& S=*M.Simulation;const double Y=S.HopperCenterY(),Z=S.HopperRimZ();
  const auto Pose=S.GetActorTransform();const auto Target=Pose.TransformPosition(FVector(0,Y,Z+4.5)*100),Position=Pose.TransformPosition(FVector(18,Y-18,Z+22)*100);
  M.Host->ViewCamera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());
 }
 static void GroundRigCamera(UShipHatchMonitorWidget& M)
 {
  const auto& S=*M.Simulation;const auto Pose=S.GetActorTransform();const auto P=Pose.TransformPosition(FVector(80,S.QuayEdgeY()-25,S.QuayZ()+1.8)*100),Target=Pose.TransformPosition(FVector(0,(S.BoomTipY()+S.SeaRailY()-S.RailSpan)*.5,(S.QuayZ()+S.HoldDepth+S.BoomHeight+16.2)*.5)*100);M.Host->ViewCamera->SetActorLocationAndRotation(P,(Target-P).Rotation());
 }
 static void SideRigCamera(UShipHatchMonitorWidget& M)
 {
  const auto& S=*M.Simulation;const auto Pose=S.GetActorTransform();const auto P=Pose.TransformPosition(FVector(100,-22,S.QuayZ()+8)*100),Aim=Pose.TransformPosition(FVector(0,-22,S.QuayZ()+24)*100);M.Host->ViewCamera->SetActorLocationAndRotation(P,(Aim-P).Rotation());
 }
 static void HullSideCamera(UShipHatchMonitorWidget& M)
 {
  const auto& S=*M.Simulation;const auto Pose=S.GetActorTransform();const auto P=Pose.TransformPosition(FVector(75,S.QuayEdgeY()-23,S.QuayZ()+6)*100),Aim=Pose.TransformPosition(FVector(30,-S.VesselBeam*.5,S.WaterZ()+2)*100);M.Host->ViewCamera->SetActorLocationAndRotation(P,(Aim-P).Rotation());
 }
 static void GrabCloseCamera(UShipHatchMonitorWidget& M)
 {
  const auto& S=*M.Simulation;const auto Pose=S.GetActorTransform();const double Z=S.Surface().Height(0,4);const auto P=Pose.TransformPosition(FVector(14,14,Z+18)*100),Target=Pose.TransformPosition(FVector(0,4,Z+3)*100);M.Host->ViewCamera->SetActorLocationAndRotation(P,(Target-P).Rotation());
 }
 static void CheckDischarge(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
 {
  const auto* Sim=M.GetSimulation();auto* Grip=Cast<UProceduralMeshComponent>(Sim->GrabPrimitive());const auto* Section=Grip?Grip->GetProcMeshSection(0):nullptr;
  T.TestTrue(TEXT("one physical clamshell and bounded mist/stream; no duplicate overlay grab"),Sim->IsGrabVisible()&&Section&&Section->ProcVertexBuffer.Num()>500&&Section->ProcVertexBuffer.Num()<5000&&M.Overlay&&M.Overlay->BatchedMeshes.Num()==4&&M.Overlay->BatchedMeshes[1].MeshVerts.IsEmpty()&&M.Overlay->BatchedMeshes[2].MeshVerts.Num()<=32&&M.Overlay->BatchedMeshes[3].MeshVerts.Num()>0);
  T.TestTrue(TEXT("four3.3m arms stay kinematically linked to shell angle"),Sim->GrabArmLengthErrorMeters<.001);

  T.TestEqual(TEXT("display effect adds no measurement collision"),M.Overlay->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
 }
 static void FullRig(UShipHatchMonitorWidget& M){M.OnFrameUnloader();}
 static void Site(UShipHatchMonitorWidget& M){M.OnSiteOverview();}
 static void Clean(UShipHatchMonitorWidget& M){M.SetModelPresentation(true);}
 static void PointControls(UShipHatchMonitorWidget& M){M.OnPointControlsToggle();}
 static void CheckPointState(FAutomationTestBase& T,UShipHatchMonitorWidget& M,bool Expanded)
 {T.TestEqual(TEXT("point controls are collapsed by default, or explicitly expanded"),M.PointPanel->IsVisible(),Expanded);T.TestEqual(TEXT("session remembers explicit expansion"),M.bPointControlsExpanded,Expanded);}
 static void VerifyPointSession(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
 {M.SelectView(0);T.TestFalse(TEXT("general3D always hides point card"),M.PointPanel->IsVisible());M.SelectView(1);T.TestTrue(TEXT("explicit expanded state returns in point mode"),M.PointPanel->IsVisible());M.SetCameraPreset(0);T.TestFalse(TEXT("camera preset auto-hides card without forgetting preference"),M.PointPanel->IsVisible());T.TestTrue(TEXT("preset preserves session preference"),M.bPointControlsExpanded);M.bPointControlsExpanded=false;M.SelectView(0);}
 static bool IsCleanModel(const UShipHatchMonitorWidget& M){return M.bModelPresentation;}
 static TSharedRef<FJsonObject> OverlayContract(FAutomationTestBase& T,UWorld& W,UShipHatchMonitorWidget& M)
 {
  auto R=MakeShared<FJsonObject>();const auto Center=M.OverlayExclusionRect();const auto Root=M.GetCachedGeometry();
  TArray<UWidget*> Widgets;for(auto B:M.SensorBadges)Widgets.Add(B.Get());for(auto B:M.DirectionBadges)Widgets.Add(B.Get());Widgets.Add(M.PointPanel.Get());Widgets.Add(M.PointControlsToggle.Get());
  double Area=0;for(auto* B:Widgets)if(B&&B->IsVisible())
  {
   const auto G=B->GetCachedGeometry();const auto A=Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector)),Z=Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()));
   Area+=FMath::Max(0.,FMath::Min(double(Center.Right),Z.X)-FMath::Max(double(Center.Left),A.X))*FMath::Max(0.,FMath::Min(double(Center.Bottom),Z.Y)-FMath::Max(double(Center.Top),A.Y));
  }
  T.TestEqual(TEXT("center60percent has zero HUD widget intersection area"),Area,0.);R->SetNumberField(TEXT("widget_intersection_area"),Area);
  TArray<FColor> With,Without;FIntVector A=FIntVector::ZeroValue,B=FIntVector::ZeroValue;
  const auto View=W.GetGameViewport()->GetGameViewportWidget().ToSharedRef();
  if(!T.TestTrue(TEXT("capture overlay comparison within the same engine update"),FSlateApplication::Get().TakeScreenshot(View,With,A)))return R;
  M.bHudSuppressed=true;M.LastSensorBadgeUpdate=-1;M.UpdateSensorBadges();M.UpdateOverlayControlVisibility();M.ForceLayoutPrepass();
  const bool Captured=FSlateApplication::Get().TakeScreenshot(View,Without,B);
  M.bHudSuppressed=false;M.LastSensorBadgeUpdate=-1;M.UpdateSensorBadges();M.UpdateOverlayControlVisibility();M.ForceLayoutPrepass();
  R->SetBoolField(TEXT("second_capture_succeeded"),Captured);R->SetNumberField(TEXT("with_width"),A.X);R->SetNumberField(TEXT("without_width"),B.X);
  if(!T.TestTrue(TEXT("same-sized overlay-free comparison frame"),Captured&&A.X==B.X&&A.Y==B.Y&&With.Num()==Without.Num()))return R;
  const auto VG=View->GetCachedGeometry();const auto P=VG.AbsoluteToLocal(Root.LocalToAbsolute(FVector2D(Center.Left,Center.Top)))*FVector2D(A.X,A.Y)/VG.GetLocalSize(),Q=VG.AbsoluteToLocal(Root.LocalToAbsolute(FVector2D(Center.Right,Center.Bottom)))*FVector2D(A.X,A.Y)/VG.GetLocalSize();
  int Changed=0,Count=0;for(int Y=FMath::CeilToInt(P.Y);Y<FMath::FloorToInt(Q.Y);++Y)for(int X=FMath::CeilToInt(P.X);X<FMath::FloorToInt(Q.X);++X){const int I=Y*A.X+X;if(With.IsValidIndex(I)&&Without.IsValidIndex(I)){++Count;Changed+=With[I]!=Without[I];}}
  T.TestTrue(TEXT("HUD removal changes zero rendered pixels in central60percent"),Count>0&&Changed==0);R->SetNumberField(TEXT("central_pixels"),Count);R->SetNumberField(TEXT("changed_pixels"),Changed);R->SetStringField(TEXT("method"),TEXT("same engine update: actual D3D12 with/without HUD; geometry and acquisition unchanged"));return R;
 }
 static void FloorCamera(UShipHatchMonitorWidget& M,FVector2D Target)
 {
  const auto Pose=M.Simulation->GetActorTransform();const auto P=Pose.TransformPosition(FVector(Target.X+3,Target.Y-2,15)*100),Aim=Pose.TransformPosition(FVector(Target,0)*100);
  // UXR-P2b (fadf810) made toolbar height part of Layout's cache key.
  // A later layout pass legitimately reapplies the monitor's camera state.
  // Use the production free-camera state so the fixture survives that pass;
  // keep the original eye/target, geometry, collision and pixel predicates.
  M.SetFreeCamera(true);M.FreeCameraPosition=P;M.FreeCameraRotation=(Aim-P).Rotation();M.UpdateCamera();
 }
 static bool UsesFreeCamera(const UShipHatchMonitorWidget& M){return M.bFreeCamera;}
 static void Settings(UShipHatchMonitorWidget& M,bool Open){if(M.bSettings!=Open)M.OnSettings();}
 static void CheckLayout(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
 {
  const auto* D=M.LayoutSlots[8].Get();const auto P=D->GetPosition(),Z=D->GetSize();
  for(int I:{4,5}){const auto* Slot=M.LayoutSlots[I].Get();T.TestTrue(TEXT("both right cards remain visible with settings open"),Slot->Content->IsVisible());const auto Q=Slot->GetPosition(),S=Slot->GetSize();const double Intersection=FMath::Max(0.,FMath::Min(P.X+Z.X,Q.X+S.X)-FMath::Max(P.X,Q.X))*FMath::Max(0.,FMath::Min(P.Y+Z.Y,Q.Y+S.Y)-FMath::Max(P.Y,Q.Y));T.TestEqual(TEXT("drawer occlusion of each right card is exactly zero pixels (visible area100%)"),Intersection,0.);}
 }
 static void Scale(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
 {
  auto& S=*M.Simulation;T.TestEqual(TEXT("user2026-10-22 default boom is OS1-64 Rev7"),S.SensorAt(0)->ScanComponent->DeviceProfile,EVirtualLidarDeviceProfile::OusterOS1_64_Rev7);for(int I=1;I<3;++I)T.TestEqual(TEXT("cab defaults are OS1-64 Rev7"),S.SensorAt(I)->ScanComponent->DeviceProfile,EVirtualLidarDeviceProfile::OusterOS1_64_Rev7);FString Error;T.TestTrue(TEXT("live vessel and rig dimensions satisfy real-scale ranges"),S.ValidateSceneScale(Error));
  T.TestTrue(TEXT("rail is on land side of quay edge; boom reaches into vessel"),S.SeaRailY()<S.QuayEdgeY()&&S.QuayEdgeY()<-S.VesselBeam*.5&&S.BoomTipY()>0&&S.BoomTipY()<S.VesselBeam*.5);
  T.TestTrue(TEXT("tank top is reachable within reference25m hoist below rail"),S.QuayZ()>0&&S.QuayZ()<25);
  T.TestTrue(TEXT("hopper near face is attached to sea portal, not isolated at rail midpoint"),FMath::Abs(S.HopperCenterY()+4.8-S.SeaRailY())<.001&&S.HopperCenterY()>S.SeaRailY()-S.RailSpan*.5&&S.HopperRimZ()<S.HoldDepth+S.BoomHeight);
  const double Saved=S.VesselLength;S.VesselLength=120;T.TestFalse(TEXT("unrealistic short vessel is rejected"),S.ValidateSceneScale(Error));S.VesselLength=Saved;
  for(TActorIterator<AShipHatchVessel>I(S.GetWorld());I;++I)
  {
   const auto* Hull=I->Mesh->GetProcMeshSection(0);T.TestNotNull(TEXT("actual hull mesh exists"),Hull);if(!Hull)continue;
   const auto B=Hull->SectionLocalBox.GetSize()*.01;
   T.TestTrue(TEXT("rendered hull actual LOA/beam match SI configuration within1cm"),FMath::Abs(B.X-S.VesselLength)<.01&&FMath::Abs(B.Y-S.VesselBeam)<.01);
   // Moulded depth is an amidships section dimension. The new end sheer
   // raises the fore/aft deck; a whole-hull AABB is no longer that dimension.
   // Intersect actual triangle edges with X=0, independently of generator math.
   double Low=DBL_MAX,High=-DBL_MAX;int Crossings=0;
   for(int J=0;J<Hull->ProcIndexBuffer.Num();J+=3)for(int Edge=0;Edge<3;++Edge)
   {
    const FVector A(Hull->ProcVertexBuffer[Hull->ProcIndexBuffer[J+Edge]].Position),C(Hull->ProcVertexBuffer[Hull->ProcIndexBuffer[J+(Edge+1)%3]].Position);
    if((A.X<=0&&C.X>=0)||(C.X<=0&&A.X>=0))
    {if(FMath::Abs(C.X-A.X)>1.e-8){const double Z=FMath::Lerp(A.Z,C.Z,-A.X/(C.X-A.X))*.01;Low=FMath::Min(Low,Z);High=FMath::Max(High,Z);++Crossings;}}
   }
   T.TestTrue(TEXT("actual amidships hull section moulded depth matches SI configuration within1cm"),Crossings>0&&FMath::Abs(High-Low-S.MouldedDepth)<.01&&FMath::Abs(High-S.HoldDepth)<.01&&FMath::Abs(Low-S.KeelZ())<.01);
   T.TestTrue(TEXT("actual bow/stern sheer adds exactly75cm above amidships side deck within1cm"),FMath::Abs(Hull->SectionLocalBox.Max.Z*.01-S.HoldDepth-.75)<.01&&FMath::Abs(B.Z-S.MouldedDepth-.75)<.01);
   const auto* Covers=I->Mesh->GetProcMeshSection(3);T.TestTrue(TEXT("two real side-rolling panel meshes exist"),Covers&&Covers->ProcVertexBuffer.Num()>=16);
   if(Covers&&Covers->ProcVertexBuffer.Num()>=16)for(int Side=0;Side<2;++Side){FBox Box(ForceInit);for(int J=0;J<8;++J)Box+=FVector(Covers->ProcVertexBuffer[Side*8+J].Position)*.01;const auto Center=Box.GetCenter(),Size=Box.GetSize();T.TestTrue(TEXT("open panel travels toward land/sea Y, with no fore-aft folding"),FMath::Abs(Center.X)<.01&&FMath::Abs(Center.Y)>S.OpeningWidth*.5&&FMath::Abs(Size.Y-S.OpeningWidth*.5)<.01&&Size.Z<.5);T.TestTrue(TEXT("both open panels clear aperture and stay within ship beam"),FMath::Abs(Center.Y)-Size.Y*.5>=S.OpeningWidth*.5&&FMath::Abs(Center.Y)+Size.Y*.5<=S.VesselBeam*.5);}
   T.TestFalse(TEXT("1.8m scale reference defaults off"),I->ScaleMarker->IsVisible());const auto* Ref=I->ScaleMarker->GetProcMeshSection(0);T.TestTrue(TEXT("actual worker/scale geometry is1.8m within1cm"),Ref&&FMath::Abs(Ref->SectionLocalBox.GetSize().Z*.01-1.8)<.01);
   T.TestEqual(TEXT("scale reference cannot contaminate physical acquisition"),I->ScaleMarker->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
  }
 }
};
namespace
{
FString Root06(){FString P;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-06/rhi"):P;}
void Save06(const FString& Name,const TSharedRef<FJsonObject>& R){FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Root06(),true);FFileHelper::SaveStringToFile(Text,*(Root06()/Name),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);}
double Mean06(const TArray<double>& A){double S=0;for(double V:A)S+=V;return A.Num()?S/A.Num():0;}
double P9506(TArray<double> A){A.Sort();return A.Num()?A[FMath::Clamp(FMath::CeilToInt(A.Num()*.95)-1,0,A.Num()-1)]:0;}
class FHatch06Run:public IAutomationLatentCommand
{
 FAutomationTestBase* T;bool OverlayOnly=false;int Phase=0,Model=0,Material=0,Edits=0,View=0;double Start=FPlatformTime::Seconds(),Changed=0,LastWall=0;uint64 LastFrame=0;
 TArray<double> Engine,Game,Wall;TArray<TSharedPtr<FJsonValue>> Rows;TUniquePtr<TGuardValue<bool>> Messages;
 FVector2D Target;uint32 OriginalHash=0;uint64 RssBefore=0;
 int64 ModelStartFrames[3]={0,0,0};
 TSharedPtr<FJsonObject> LowBoomBefore;
 TSharedRef<FJsonObject> BoomEvidence(const AShipHatchSimulation& S,int Sensor=0)
 {
  auto R=MakeShared<FJsonObject>();const auto* A=S.SensorAt(Sensor);const auto Frame=A->ScanComponent->GetLastFrameSnapshot();TArray<ShipHatch::FObservation> Points;int Floor=0;
  if(Frame&&Frame->Points)for(const auto& P:*Frame->Points)if(P.bHit){if(P.SemanticLabel==TEXT("HatchCargo"))Points.Add({S.GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01,P.SemanticLabel});if(P.SemanticLabel==TEXT("HatchHold")&&FMath::Abs(S.GetActorTransform().InverseTransformPosition(P.WorldLocation).Z)<2)++Floor;}
  const auto Analysis=ShipHatch::Analyze(S.Settings(),1,Points);const auto Sight=S.DiagnoseSight(Sensor,FVector::ZeroVector);
  R->SetStringField(TEXT("model"),A->ScanComponent->DeviceSpec.Model);R->SetNumberField(TEXT("cargo_points"),Points.Num());R->SetNumberField(TEXT("floor_points"),Floor);R->SetNumberField(TEXT("observed_percent"),100*Analysis.Coverage);R->SetNumberField(TEXT("unobserved_percent"),100*(1-Analysis.Coverage));R->SetStringField(TEXT("floor_center_cause"),Sight.Cause.ToString());R->SetNumberField(TEXT("floor_center_distance_m"),Sight.DistanceMeters);R->SetNumberField(TEXT("range_m"),Sight.RangeMeters);R->SetNumberField(TEXT("mount_z_m"),S.SensorOrigin(Sensor).Z);R->SetNumberField(TEXT("requested_hz"),1.f/A->ScanComponent->ScanInterval);R->SetNumberField(TEXT("measured_hz"),A->ScanComponent->GetRuntimeStatus().MeasuredAcquisitionRateHz);if(Frame){R->SetNumberField(TEXT("frame_id"),double(Frame->FrameId));R->SetNumberField(TEXT("requested_rays"),Frame->RequestedRayCount);}return R;
 }
 void DefaultEvidence(const AShipHatchSimulation& S)
 {
  auto R=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Devices;for(int I=0;I<3;++I){auto D=BoomEvidence(S,I);if(I==0)T->TestTrue(TEXT("new Ouster default contributes at least50percent initial hold coverage"),D->GetNumberField(TEXT("observed_percent"))>=50);Devices.Add(MakeShared<FJsonValueObject>(D));}R->SetArrayField(TEXT("sensors"),Devices);TArray<ShipHatch::FObservation> Points;S.CollectObservations(Points);const auto A=ShipHatch::Analyze(S.Settings(),1,Points);R->SetNumberField(TEXT("combined_observed_percent"),100*A.Coverage);R->SetNumberField(TEXT("combined_unobserved_percent"),100*(1-A.Coverage));R->SetStringField(TEXT("authorization"),TEXT("user2026-10-22: OS1-64 Rev7 x3; performance/RSS gates unchanged"));Save06(TEXT("default-configuration.json"),R);
 }
 void DiagnoseGray(UShipHatchMonitorWidget& M)
 {
  const auto* S=M.GetSimulation();const auto* A=M.GetDisplayAnalysis();if(!S||!A)return;auto Root=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Cells;bool Grab=false;
  for(int I=0;I<A->Cells.Num();++I)
  {
   const double X=(I%A->NX+.5)*A->DX-S->HoldLength*.5,Y=(I/A->NX+.5)*A->DY-S->HoldWidth*.5;if(A->Cells[I].Count||FMath::Abs(X)>2||Y<2||Y>6)continue;
   auto C=MakeShared<FJsonObject>();C->SetNumberField(TEXT("x_m"),X);C->SetNumberField(TEXT("y_m"),Y);C->SetNumberField(TEXT("detected_points"),0);TArray<TSharedPtr<FJsonValue>> Rays;
   for(int Sensor=0;Sensor<3;++Sensor){const auto D=S->DiagnoseSight(Sensor,FVector(X,Y,S->Surface().Height(X,Y)));auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("sensor"),Sensor);R->SetStringField(TEXT("cause"),D.Cause.ToString());R->SetStringField(TEXT("description"),D.Description);R->SetNumberField(TEXT("distance_m"),D.DistanceMeters);R->SetNumberField(TEXT("range_m"),D.RangeMeters);R->SetNumberField(TEXT("first_x_m"),D.FirstHitMeters.X);R->SetNumberField(TEXT("first_y_m"),D.FirstHitMeters.Y);R->SetNumberField(TEXT("first_z_m"),D.FirstHitMeters.Z);Rays.Add(MakeShared<FJsonValueObject>(R));if(D.Cause==TEXT("Grab")){Grab=true;T->TestTrue(TEXT("gray-cell tooltip names actual grab first-hit cause"),M.CellTooltip(I).ToString().Contains(TEXT("그랩")));}}
   C->SetArrayField(TEXT("current_center_sightlines"),Rays);Cells.Add(MakeShared<FJsonValueObject>(C));
  }
  Root->SetArrayField(TEXT("unobserved_cells_near_grab"),Cells);Root->SetStringField(TEXT("method"),TEXT("same-revision current cell-center first collision, separate from acquired sampling; no geometry or frame mutation"));Root->SetBoolField(TEXT("actual_grab_occlusion_found"),Grab);Save06(TEXT("visibility-diagnostics.json"),Root);T->TestTrue(TEXT("default grab-off excludes physical grab occlusion"),!Grab);
 }
 void Shot(UWorld* W,UShipHatchMonitorWidget& M,const FString& Name,bool Floor=false)
 {
  TArray<FColor> Pixels;FIntVector Size;if(!T->TestTrue(TEXT("real RHI pixels captured"),FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);const FString Path=Root06()/TEXT("captures")/(Name+TEXT(".png"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);FFileHelper::SaveArrayToFile(PNG,*Path);
  const FString Progress=FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-06/progress")/(FString::Printf(TEXT("%02d-"),int(Rows.Num()+Material*10+View))+FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M"))+TEXT("-")+Name+TEXT(".png"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Progress),true);FFileHelper::SaveArrayToFile(PNG,*Progress);
  auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());R->SetStringField(TEXT("kind"),TEXT("automated D3D12, not direct UI"));R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);R->SetNumberField(TEXT("font_percent"),M.GetFontScale()*100);R->SetNumberField(TEXT("default_radius_m"),2);R->SetNumberField(TEXT("default_depth_m"),1.5);R->SetNumberField(TEXT("edits"),Edits);
  const auto* Sim=M.GetSimulation();R->SetNumberField(TEXT("surface_crc"),Sim->Surface().Hash());R->SetNumberField(TEXT("seed"),Sim->Seed);R->SetNumberField(TEXT("loa_m"),Sim->VesselLength);R->SetNumberField(TEXT("beam_m"),Sim->VesselBeam);R->SetNumberField(TEXT("moulded_depth_m"),Sim->MouldedDepth);R->SetNumberField(TEXT("rail_span_m"),Sim->RailSpan);R->SetNumberField(TEXT("outreach_m"),Sim->BoomLength);R->SetNumberField(TEXT("boom_sensor_above_deck_m"),Sim->SensorOrigin(0).Z-Sim->HoldDepth);R->SetNumberField(TEXT("cab_sensor_above_deck_m"),Sim->SensorOrigin(1).Z-Sim->HoldDepth);R->SetNumberField(TEXT("water_z_m"),Sim->WaterZ());R->SetNumberField(TEXT("quay_z_m"),Sim->QuayZ());R->SetNumberField(TEXT("hopper_y_m"),Sim->HopperCenterY());R->SetNumberField(TEXT("hopper_rim_z_m"),Sim->HopperRimZ());R->SetNumberField(TEXT("umg_scale"),UWidgetLayoutLibrary::GetViewportScale(&M));
  const auto Window=FSlateApplication::Get().FindWidgetWindow(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef());R->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);FString Commit;FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),Commit);R->SetStringField(TEXT("source_commit"),Commit);R->SetBoolField(TEXT("clean_model_view"),M.IsModelPresentation());const auto Scene=M.GetSceneRect();const auto LocalSize=M.GetCachedGeometry().GetLocalSize();R->SetNumberField(TEXT("scene_left_px"),Scene.Left*Size.X/LocalSize.X);R->SetNumberField(TEXT("scene_top_px"),Scene.Top*Size.Y/LocalSize.Y);R->SetNumberField(TEXT("scene_right_px"),Scene.Right*Size.X/LocalSize.X);R->SetNumberField(TEXT("scene_bottom_px"),Scene.Bottom*Size.Y/LocalSize.Y);
  if(Name==TEXT("covers-top-projection"))for(TActorIterator<AShipHatchVessel>I(W);I;++I)
  {
   const auto* C=I->Mesh->GetProcMeshSection(3);if(!C||C->ProcVertexBuffer.Num()<16)continue;FVector2D P[2];TArray<TSharedPtr<FJsonValue>> Panels;
   for(int Side=0;Side<2;++Side){FVector Center=FVector::ZeroVector;for(int J=0;J<8;++J){const FVector Vertex(C->ProcVertexBuffer[Side*8+J].Position);Center+=Vertex/8;FVector2D Pixel;W->GetFirstPlayerController()->ProjectWorldLocationToScreen(I->Mesh->GetComponentTransform().TransformPosition(Vertex),Pixel);T->TestTrue(TEXT("complete open panel fits visible3D pane in dedicated cover verification view"),FHatch06Probe::Scene(M).ContainsPoint(Pixel*M.GetCachedGeometry().GetLocalSize()/FVector2D(Size.X,Size.Y)));}W->GetFirstPlayerController()->ProjectWorldLocationToScreen(I->Mesh->GetComponentTransform().TransformPosition(Center),P[Side]);const auto Local=Sim->GetActorTransform().InverseTransformPosition(I->Mesh->GetComponentTransform().TransformPosition(Center))*.01;auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("hold_x_m"),Local.X);V->SetNumberField(TEXT("hold_y_m"),Local.Y);V->SetNumberField(TEXT("pixel_x"),P[Side].X);V->SetNumberField(TEXT("pixel_y"),P[Side].Y);Panels.Add(MakeShared<FJsonValueObject>(V));}
   R->SetArrayField(TEXT("actual_open_panel_centers"),Panels);R->SetStringField(TEXT("gray_side_panels"),TEXT("closed neighboring hatches at fore/aft +/-X; not the active open panels"));T->TestTrue(TEXT("independent actual mesh camera projection: +Y open cover above -Y, same screen X"),P[1].Y<P[0].Y&&FMath::Abs(P[1].X-P[0].X)<.1);
  }
  if(Floor)
  {
   FHitResult First;const FVector Aim=Sim->GetActorTransform().TransformPosition(FVector(Target,-.05)*100);FCollisionQueryParams Query(SCENE_QUERY_STAT(Hatch06FloorPixel),true);
   FVector Eye;FRotator EyeRotation;W->GetFirstPlayerController()->GetPlayerViewPoint(Eye,EyeRotation);
   const auto Pose=Sim->GetActorTransform();const FVector ExpectedEye=Pose.TransformPosition(FVector(Target.X+3,Target.Y-2,15)*100),ExpectedAim=Pose.TransformPosition(FVector(Target,0)*100);
   const bool StableCamera=FHatch06Probe::UsesFreeCamera(M)&&Eye.Equals(ExpectedEye,.1)&&EyeRotation.Equals((ExpectedAim-ExpectedEye).Rotation(),.001);
   R->SetBoolField(TEXT("production_free_camera_pose_preserved"),StableCamera);R->SetNumberField(TEXT("camera_position_error_cm"),FVector::Distance(Eye,ExpectedEye));
   T->TestTrue(TEXT("production free-camera floor fixture survives layout updates at its original eye and target"),StableCamera);
   const bool HasHit=W->LineTraceSingleByChannel(First,Eye,Aim,ECC_Visibility,Query);
   const double FirstZ=HasHit?Sim->GetActorTransform().InverseTransformPosition(First.ImpactPoint).Z*.01:1.e9;
   const bool TankTop=HasHit&&First.GetActor()&&First.GetActor()->ActorHasTag(TEXT("HatchHold"))&&FMath::Abs(FirstZ)<.005;
   R->SetBoolField(TEXT("first_visible_hit_is_tank_top"),TankTop);R->SetNumberField(TEXT("first_hit_local_z_m"),FirstZ);
   R->SetStringField(TEXT("first_hit_actor"),GetNameSafe(First.GetActor()));
   T->TestTrue(TEXT("pixel sample sightline first hits actual Z0 tank-top, with no crane/cargo occlusion"),TankTop);
   FVector2D P=FVector2D::ZeroVector;auto* PC=W->GetFirstPlayerController();const bool Projected=PC->ProjectWorldLocationToScreen(ExpectedAim,P,false);
   const auto PixelToLocal=M.GetCachedGeometry().GetLocalSize()/FVector2D(Size.X,Size.Y);const auto FloorScene=FHatch06Probe::Scene(M);
   T->TestTrue(TEXT("complete7x7 floor sample projects inside the visible3D pane"),Projected&&FloorScene.ContainsPoint((P-FVector2D(3,3))*PixelToLocal)&&FloorScene.ContainsPoint((P+FVector2D(3,3))*PixelToLocal));
   int Hit=0,Total=0,FloorHits=0;TArray<TSharedPtr<FJsonValue>> Samples;
   for(int Y=-3;Y<=3;++Y)for(int X=-3;X<=3;++X)
   {
    const int PX=FMath::RoundToInt(P.X)+X,PY=FMath::RoundToInt(P.Y)+Y;if(PX<0||PY<0||PX>=Size.X||PY>=Size.Y)continue;
    const auto C=Pixels[PY*Size.X+PX];++Total;const int Low=FMath::Min3(int(C.R),int(C.G),int(C.B)),High=FMath::Max3(int(C.R),int(C.G),int(C.B));Hit+=Low>=40&&High<=180&&High-Low<=24;
    FVector O,D;FHitResult PixelHit;const bool Physical=PC->DeprojectScreenPositionToWorld(PX,PY,O,D)&&W->LineTraceSingleByChannel(PixelHit,O,O+D*20000,ECC_Visibility,Query);
    const double Z=Physical?Pose.InverseTransformPosition(PixelHit.ImpactPoint).Z*.01:1.e9;
    const bool FloorPixel=Physical&&PixelHit.GetActor()&&PixelHit.GetActor()->ActorHasTag(TEXT("HatchHold"))&&FMath::Abs(Z)<.005;FloorHits+=FloorPixel;
    auto Sample=MakeShared<FJsonObject>();Sample->SetNumberField(TEXT("x"),PX);Sample->SetNumberField(TEXT("y"),PY);Sample->SetNumberField(TEXT("r"),C.R);Sample->SetNumberField(TEXT("g"),C.G);Sample->SetNumberField(TEXT("b"),C.B);Sample->SetBoolField(TEXT("first_hit_is_tank_top"),FloorPixel);Sample->SetNumberField(TEXT("first_hit_local_z_m"),Z);Samples.Add(MakeShared<FJsonValueObject>(Sample));
   }
   R->SetNumberField(TEXT("tank_top_sample_rays"),FloorHits);R->SetArrayField(TEXT("floor_samples"),Samples);
   T->TestEqual(TEXT("all49 rendered sample rays first hit actual Z0 tank-top"),FloorHits,49);
   R->SetNumberField(TEXT("floor_pixel_x"),P.X);R->SetNumberField(TEXT("floor_pixel_y"),P.Y);R->SetNumberField(TEXT("neutral_steel_pixels"),Hit);R->SetNumberField(TEXT("sample_pixels"),Total);
   T->TestTrue(TEXT("actual exposed tank-top center is neutral gray steel (all49 of7x7 pixels, no water-blue tint)"),Total==49&&Hit==49);
  }
  FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*FPaths::ChangeExtension(Path,TEXT("json")));
 }
public:
 explicit FHatch06Run(FAutomationTestBase* Test,bool OnlyOverlays=false):T(Test),OverlayOnly(OnlyOverlays){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(Now-Start>1800){T->AddError(TEXT("HATCH06 timed out"));return true;}
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
  AShipHatchMonitorHost* H=nullptr;for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;if(!H||!H->Monitor)return false;
  auto& M=*H->Monitor;auto* S=const_cast<AShipHatchSimulation*>(M.GetSimulation());if(!S||S->IsGenerating())return false;
  if(Phase==0)
  {
   if(!M.GetAnalysis())return false;FHatch06Probe::Scale(*T,M);Messages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);int Font=100;FParse::Value(FCommandLine::Get(),TEXT("HatchFontPercent="),Font);M.SetFontPercent(Font);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(1);FHatch06Probe::Cargo(M);Changed=Now;Phase=1;return false;
  }
  if(Phase==1)
  {
   if(Now-Changed<1)return false;Shot(W,M,TEXT("cargo"));DefaultEvidence(*S);DiagnoseGray(M);M.SelectView(1);FHatch06Probe::Cargo(M);Changed=Now;Phase=27;return false;
  }
  if(Phase==27){if(Now-Changed<1)return false;FHatch06Probe::CheckPointState(*T,M,false);Save06(TEXT("overlay-collapsed.json"),FHatch06Probe::OverlayContract(*T,*W,M));Shot(W,M,TEXT("points-controls-collapsed"));FHatch06Probe::PointControls(M);Changed=Now;Phase=28;return false;}
  if(Phase==28){if(Now-Changed<1)return false;FHatch06Probe::CheckPointState(*T,M,true);Save06(TEXT("overlay-expanded.json"),FHatch06Probe::OverlayContract(*T,*W,M));Shot(W,M,TEXT("points-controls-expanded"));FHatch06Probe::VerifyPointSession(*T,M);FHatch06Probe::Covers(M);Changed=Now;Phase=22;return false;}
  if(Phase==22){if(Now-Changed<1)return false;Shot(W,M,TEXT("covers-top-projection"));M.SetCameraPreset(1);FHatch06Probe::FullRig(M);Changed=Now;Phase=10;return false;}
  if(Phase==10){if(Now-Changed<1)return false;Shot(W,M,TEXT("gtsu-full"));FHatch06Probe::Site(M);Changed=Now;Phase=26;return false;}
  if(Phase==26){if(Now-Changed<1)return false;Shot(W,M,TEXT("site-ref4-oblique"));FHatch06Probe::GroundRigCamera(M);Changed=Now;Phase=19;return false;}
  if(Phase==14){if(Now-Changed<1)return false;Shot(W,M,TEXT("gtsu-scale-1p8m"));S->ShowScaleReference(false);FHatch06Probe::Vessel(M);Changed=Now;Phase=13;return false;}
  if(Phase==13){if(Now-Changed<1)return false;Shot(W,M,TEXT("vessel-overview"));S->ShowWater(false);Changed=Now;Phase=15;return false;}
  if(Phase==15){if(Now-Changed<1)return false;Shot(W,M,TEXT("vessel-hull-water-hidden"));S->ShowWater(true);M.SetCameraPreset(1);FHatch06Probe::Settings(M,true);Changed=Now;Phase=2;return false;}
  if(Phase==2)
  {
   if(Now-Changed<1)return false;FHatch06Probe::CheckLayout(*T,M);ValidateHatchPresentation(T,&M);Shot(W,M,TEXT("settings-three-views"));
   if(FParse::Param(FCommandLine::Get(),TEXT("Hatch06Manual"))){FFileHelper::SaveStringToFile(TEXT("Ready for direct Computer Use; waiting for manual.done"),*(Root06()/TEXT("manual.ready")));Phase=12;return false;}
   FHatch06Probe::Settings(M,false);S->SetGrabVisible(true);FHatch06Probe::GrabCloseCamera(M);FHatch06Probe::Clean(M);Changed=Now;Phase=20;return false;
  }
  if(Phase==12)return FPaths::FileExists(Root06()/TEXT("manual.done"));
  if(Phase==19){if(Now-Changed<1)return false;Shot(W,M,TEXT("gtsu-reference-ground"));FHatch06Probe::SideRigCamera(M);Changed=Now;Phase=24;return false;}
  if(Phase==24){if(Now-Changed<1)return false;Shot(W,M,TEXT("gtsu-reference-side"));FHatch06Probe::HullSideCamera(M);Changed=Now;Phase=25;return false;}
  if(Phase==25){if(Now-Changed<1)return false;Shot(W,M,TEXT("vessel-quay-side"));M.SetCameraPreset(1);FHatch06Probe::FullRig(M);S->ShowScaleReference(true);Changed=Now;Phase=14;return false;}
  if(Phase==20){if(Now-Changed<1)return false;Shot(W,M,TEXT("grab-hold-close"));M.SetCameraPreset(0);FHatch06Probe::Cargo(M);Changed=Now;Phase=16;return false;}
  if(Phase==16){if(Now-Changed<1)return false;FString Trace;const bool Started=FHatch06Probe::StartReferenceGrab(M,Trace);auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("actual_projected_candidates"),Trace);R->SetBoolField(TEXT("accepted_production_pick"),Started);Save06(TEXT("reference-pointer-probe.json"),R);if(!T->TestTrue(TEXT("reference effect starts through production pointer path"),Started))return true;FHatch06Probe::HopperCamera(M);FHatch06Probe::Clean(M);Changed=Now;Phase=17;return false;}
  if(Phase==17){if(Now-Changed<3.8)return false;FHatch06Probe::CheckDischarge(*T,M);Shot(W,M,TEXT("grab-hopper-discharge"));Changed=Now;Phase=18;return false;}
  if(Phase==18){if(Now-Changed<1.6)return false;S->SetGrabVisible(false);S->RestoreCargo();if(OverlayOnly){auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("valid"),!T->HasAnyErrors());R->SetBoolField(TEXT("overlay_and_model_only"),true);Save06(TEXT("overlay-model-acceptance.json"),R);return true;}M.SetCameraPreset(1);FHatch06Probe::Cargo(M);for(int I=0;I<3;++I)S->SetFieldSensor(I,true,3+Model);M.SelectView(1);M.SetSplitView(true);Changed=Now;Phase=3;return false;}
  if(Phase==3)
  {
   if(!S->HasFreshMeasurement()||!M.GetAnalysis()||Now-Changed<10)return false;
   Engine.Reset();Game.Reset();Wall.Reset();LastWall=Now;LastFrame=GFrameCounter;Changed=Now;RssBefore=FPlatformMemory::GetStats().UsedPhysical;for(int I=0;I<3;++I)ModelStartFrames[I]=S->SensorAt(I)->ScanComponent->GetRuntimeStatus().FrameId;Phase=4;return false;
  }
  if(Phase==4)
  {
   if(GFrameCounter!=LastFrame){LastFrame=GFrameCounter;Engine.Add(W->GetDeltaSeconds()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Wall.Add((Now-LastWall)*1000);LastWall=Now;}
   const bool Fast=FParse::Param(FCommandLine::Get(),TEXT("Hatch06Fast"));if(Now-Changed<(Fast?2:30))return false;
   const auto& Spec=IndustrialLidar::Specs()[Model];auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("model"),Spec.Model);R->SetNumberField(TEXT("fps"),1000/Mean06(Engine));R->SetNumberField(TEXT("engine_p95_ms"),P9506(Engine));R->SetNumberField(TEXT("game_mean_ms"),Mean06(Game));R->SetNumberField(TEXT("wall_p95_ms"),P9506(Wall));R->SetNumberField(TEXT("measurement_seconds"),Now-Changed);R->SetNumberField(TEXT("rss_before_mib"),RssBefore/1048576.);R->SetNumberField(TEXT("rss_after_mib"),FPlatformMemory::GetStats().UsedPhysical/1048576.);
   T->TestTrue(TEXT("three-device measurement engine has zero >150ms frames"),Engine.Num()>0&&FMath::Max(Engine)<=150);T->TestTrue(TEXT("three-device game thread has zero >150ms frames"),Game.Num()>0&&FMath::Max(Game)<=150);
   R->SetNumberField(TEXT("engine_max_ms"),FMath::Max(Engine));R->SetNumberField(TEXT("game_max_ms"),FMath::Max(Game));R->SetBoolField(TEXT("performance_gate"),!Fast);R->SetStringField(TEXT("rate_method"),TEXT("completed acquisitions / monotonic measurement seconds; instantaneous UI rate recorded separately"));
   TArray<TSharedPtr<FJsonValue>> Devices;
   for(int I=0;I<3;++I)
   {
    auto* A=S->SensorAt(I);const auto F=A->ScanComponent->GetLastFrameSnapshot();T->TestTrue(TEXT("each selected industrial model produces a fresh acquisition"),F&&F->Points&&F->Points->Num());if(!F||!F->Points)continue;
    T->TestEqual(TEXT("actual acquired ray budget matches published selected mode"),F->RequestedRayCount,Spec.Columns*Spec.Rows);T->TestEqual(TEXT("actual selected device"),A->ScanComponent->DeviceProfile,Spec.Profile);
    TArray<ShipHatch::FObservation> P;int Hits=0;double MaxHeightError=0,HeightSquares=0;for(const auto& Point:*F->Points)if(Point.bHit){++Hits;T->TestTrue(TEXT("physical hit respects model range"),Point.Distance<=Spec.RangeMeters*100+.01);T->TestTrue(TEXT("physical return respects echo capability"),Point.EchoCount<=Spec.Echoes);if(Point.SemanticLabel==TEXT("HatchCargo")){const auto Local=S->GetActorTransform().InverseTransformPosition(Point.WorldLocation)*.01;P.Add({Local,Point.SemanticLabel});const double Error=FMath::Abs(Local.Z-S->Surface().Height(Local.X,Local.Y));MaxHeightError=FMath::Max(MaxHeightError,Error);HeightSquares+=Error*Error;}}
    const auto Analysis=ShipHatch::Analyze(S->Settings(),1,P);auto D=MakeShared<FJsonObject>();D->SetNumberField(TEXT("sensor"),I);D->SetNumberField(TEXT("hit_points"),Hits);D->SetNumberField(TEXT("cargo_points"),P.Num());D->SetNumberField(TEXT("observed_percent"),100*Analysis.Coverage);D->SetNumberField(TEXT("unobserved_percent"),100*(1-Analysis.Coverage));const int64 Completed=A->ScanComponent->GetRuntimeStatus().FrameId-ModelStartFrames[I];D->SetNumberField(TEXT("completed_acquisitions"),double(Completed));D->SetNumberField(TEXT("measured_hz"),Completed/(Now-Changed));D->SetNumberField(TEXT("instantaneous_hz"),A->ScanComponent->GetRuntimeStatus().MeasuredAcquisitionRateHz);D->SetNumberField(TEXT("requested_hz"),1.f/A->ScanComponent->ScanInterval);D->SetNumberField(TEXT("requested_rays"),F->RequestedRayCount);D->SetNumberField(TEXT("height_max_error_m"),MaxHeightError);D->SetNumberField(TEXT("height_rms_error_m"),P.Num()?FMath::Sqrt(HeightSquares/P.Num()):0);D->SetStringField(TEXT("backend"),A->ScanComponent->GetRuntimeStatus().ActiveAcquisitionBackend);Devices.Add(MakeShared<FJsonValueObject>(D));T->TestTrue(TEXT("each model completes acquisitions inside measurement interval"),Completed>0);T->TestTrue(TEXT("mounted industrial model sees cargo"),P.Num()>0);T->TestTrue(TEXT("each mounted model preserves15cm cargo height accuracy"),P.Num()>0&&MaxHeightError<.15);
   }
   R->SetArrayField(TEXT("sensors"),Devices);Rows.Add(MakeShared<FJsonValueObject>(R));Save06(TEXT("models.json"),[&](){auto J=MakeShared<FJsonObject>();J->SetArrayField(TEXT("models"),Rows);return J;}());Phase=5;Changed=Now;return false;
  }
  if(Phase==5)
  {
   if(Now-Changed<1)return false;Shot(W,M,FString::Printf(TEXT("model-%d"),Model));if(++Model<5){for(int I=0;I<3;++I)S->SetFieldSensor(I,true,3+Model);Changed=Now;Phase=3;return false;}
   for(int I=0;I<3;++I)S->SetFieldSensor(I,true,I==0?6:1);S->bCoal=false;S->FillMin=35;S->FillMax=65;S->Regenerate(false);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(0);Changed=Now;Phase=6;return false;
  }
  if(Phase==6)
  {
   if(!S->HasFreshMeasurement()||Now-Changed<2)return false;
   const auto& F=S->Surface();Target=FVector2D(FMath::RoundToInt((5+F.Settings.Length*.5)/F.DX)*F.DX-F.Settings.Length*.5,FMath::RoundToInt((-5+F.Settings.Width*.5)/F.DY)*F.DY-F.Settings.Width*.5);
   OriginalHash=F.Hash();Edits=0;FHatch06Probe::Cargo(M);Changed=Now;Phase=11;return false;
  }
  if(Phase==11){if(Now-Changed<1)return false;Shot(W,M,Material?TEXT("coal-full-before"):TEXT("iron-full-before"));Phase=7;return false;
  }
  if(Phase==7)
  {
   if(S->Surface().Height(Target.X,Target.Y)>1.e-4&&Edits<1500)
   {
    const FVector O=S->GetActorTransform().TransformPosition(FVector(Target,S->HoldDepth+6)*100),D=S->GetActorTransform().TransformVectorNoScale(FVector(0,0,-1));
    if(!S->Dig(O,D,2,1.5)){T->AddError(TEXT("default-radius repeated physical grab did not progress"));return true;}++Edits;return false;
   }
   T->TestTrue(TEXT("default fill and repeated default grabs reach zero thickness"),S->Surface().Height(Target.X,Target.Y)<=1.e-4);M.SetSection(true,Target.Y/S->HoldWidth+.5+.04);FHatch06Probe::FloorCamera(M,Target);Changed=Now;View=0;Phase=8;return false;
  }
  if(Phase==8)
  {
   if(!S->HasFreshMeasurement()||!M.GetAnalysis()||Now-Changed<2)return false;
   Shot(W,M,FString::Printf(TEXT("%s-full-floor-%s"),Material?TEXT("coal"):TEXT("iron"),View==0?TEXT("3d"):View==1?TEXT("points"):TEXT("section")),View==0);
   if(++View<3){M.SelectView(View==1?1:4);if(View==2)M.SetSection(true,Target.Y/S->HoldWidth+.5);Changed=Now;return false;}
   if(Material==1){LowBoomBefore=BoomEvidence(*S);T->TestTrue(TEXT("new default Ouster boom observes actual exposed floor"),LowBoomBefore->GetNumberField(TEXT("floor_points"))>0);S->SetFieldSensor(0,true,1);M.SelectView(1);M.SetSplitView(true);M.SetCameraPreset(0);FHatch06Probe::Cargo(M);Changed=Now;Phase=21;return false;}
   T->TestTrue(TEXT("undo restores a prior physical excavation"),S->UndoDig());T->TestTrue(TEXT("restore returns bit-exact original detail"),S->RestoreCargo()&&S->Surface().Hash()==OriginalHash);
   if(Material++==0){S->bCoal=true;S->Regenerate(false);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(0);Changed=Now;Phase=6;return false;}
   auto Done=MakeShared<FJsonObject>();Done->SetBoolField(TEXT("valid"),!T->HasAnyErrors());Done->SetArrayField(TEXT("models"),Rows);Save06(TEXT("acceptance.json"),Done);return true;
  }
  if(Phase==21)
  {
   if(!S->HasFreshMeasurement()||!M.GetAnalysis()||Now-Changed<10)return false;const auto Mid=BoomEvidence(*S);auto R=MakeShared<FJsonObject>();R->SetObjectField(TEXT("default_os1_rev7"),LowBoomBefore);R->SetObjectField(TEXT("comparison_mid360s_40m"),Mid);Save06(TEXT("low-fill-boom-comparison.json"),R);
   T->TestEqual(TEXT("40m comparison keeps actual boom mounting position"),Mid->GetNumberField(TEXT("mount_z_m")),LowBoomBefore->GetNumberField(TEXT("mount_z_m")));T->TestEqual(TEXT("40m comparison cannot reach empty floor center"),Mid->GetStringField(TEXT("floor_center_cause")),FString(TEXT("Range")));T->TestTrue(TEXT("new default Ouster increases actual low-fill boom coverage over40m comparison"),LowBoomBefore->GetNumberField(TEXT("observed_percent"))>Mid->GetNumberField(TEXT("observed_percent")));Shot(W,M,TEXT("coal-low-fill-boom-mid360s-40m"));S->SetFieldSensor(0,true,8);Changed=Now;Phase=23;return false;
  }
  if(Phase==23)
  {
   if(!S->HasFreshMeasurement()||!M.GetAnalysis()||Now-Changed<2)return false;Shot(W,M,TEXT("coal-low-fill-boom-xt32-default"));T->TestTrue(TEXT("undo restores a prior physical excavation"),S->UndoDig());T->TestTrue(TEXT("restore returns bit-exact original detail"),S->RestoreCargo()&&S->Surface().Hash()==OriginalHash);auto Done=MakeShared<FJsonObject>();Done->SetBoolField(TEXT("valid"),!T->HasAnyErrors());Done->SetArrayField(TEXT("models"),Rows);Save06(TEXT("acceptance.json"),Done);return true;
  }
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatch06Acceptance,"HATCH06.AcceptanceRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatch06Acceptance::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatch06Run(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatch06Overlay,"HATCH06.OverlayAndModelRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatch06Overlay::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatch06Run(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
