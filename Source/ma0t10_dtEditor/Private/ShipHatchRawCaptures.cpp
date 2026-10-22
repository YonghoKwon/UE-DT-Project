#include "SensorOwnedRayTracing.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "DynamicRHI.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

// RAW-CAP-01: editor-only capture fixture. No product, map or asset writes.
namespace
{
FString RawRoot()
{FString P;FParse::Value(FCommandLine::Get(),TEXT("RawCaptureOutput="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/RAW-CAP-01"):P;}
void RawJson(const FString& Name,const TSharedRef<FJsonObject>& R)
{FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));IFileManager::Get().MakeDirectory(*RawRoot(),true);FFileHelper::SaveStringToFile(S,*(RawRoot()/Name),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);}
TArray<TSharedPtr<FJsonValue>> RawVector(const FVector& P)
{return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)};}
class FRawCaptureRun final:public IAutomationLatentCommand
{
 FAutomationTestBase* T;TWeakObjectPtr<AShipHatchMonitorHost> Host;
 double Started=FPlatformTime::Seconds(),Changed=0,InitialVolume=0,BeforeGrip=0;
 int Shot=0,Digs=0,Images=0;bool Initialized=false,Digging=false;
 FVector CameraLocal,AimLocal;TArray<TWeakObjectPtr<UPrimitiveComponent>> PlayerHides;
 const TCHAR* Names[14]={TEXT("01-unloader-vessel-normal"),TEXT("02-unloader-vessel-points"),TEXT("03-unloader-vessel-overlay"),TEXT("04-cab-centered-grab-lowering"),TEXT("05-grab-in-hold-excavation"),TEXT("06-monitor-ouster-three"),TEXT("07-height-heatmap"),TEXT("08-hold-section"),TEXT("09a-dig-initial"),TEXT("09b-dig-middle"),TEXT("09c-dig-floor-exposed"),TEXT("10a-iron-ore-full"),TEXT("10b-coal-full"),TEXT("11-overhead-free-camera")};
 void Regenerate(AShipHatchSimulation& S,bool Coal,double Fill)
 {S.EndGrabWork();S.ResetGrabPose();S.SetGrabVisible(false);S.Seed=19019;S.bCoal=Coal;S.FillMin=S.FillMax=Fill;T->TestTrue(TEXT("capture uses production generation"),S.Regenerate(false));Digs=0;}
 bool DigAt(AShipHatchSimulation& S,FVector2D XY)
 {const auto P=S.GetActorTransform();const bool Ok=S.Dig(P.TransformPosition(FVector(XY,S.HoldDepth+10)*100),P.TransformVectorNoScale(FVector(0,0,-1)),5,3);if(Ok)++Digs;return Ok;}
 void Frame(AShipHatchMonitorHost& H,AShipHatchSimulation& S,UShipHatchMonitorWidget& M)
 {
  if(auto* PC=M.GetOwningPlayer())for(const auto& C:PlayerHides)PC->HiddenPrimitiveComponents.RemoveSingle(C);PlayerHides.Reset();
  M.SetFreeCamera(false);M.SelectView(0);M.SetSplitView(false);M.SetModelPresentation(true);M.SetRenderOpacity(Shot>=5&&Shot<=7?1:0);
  S.ShowUnloader(true);S.SetScanDisplay(false);S.SetSensorMarkersVisible(false);
  H.ViewCamera->GetCameraComponent()->SetFieldOfView(58);
  CameraLocal=FVector(65,-80,92);AimLocal=FVector(0,-19,28);
  if(Shot==1||Shot==2)
  {
   M.SelectView(1);M.SetModelPresentation(true);if(!M.IsSensorColoring())M.ProcessEvent(M.FindFunctionChecked(TEXT("OnSensorColors")),nullptr);
   if(Shot==2){H.Coordinator->SetPointCloudOnlyMode(false);M.SelectColor(0);M.ProcessEvent(M.FindFunctionChecked(TEXT("OnSensorColors")),nullptr);}
   else if(auto* PC=M.GetOwningPlayer())if(auto* Cab=S.CabPrimitive())
   {TInlineComponentArray<UPrimitiveComponent*> Parts(Cab->GetOwner());for(auto* C:Parts)if(!PC->HiddenPrimitiveComponents.Contains(C)){PC->HiddenPrimitiveComponents.Add(C);PlayerHides.Add(C);}}
  }
  if(Shot==3)
  {
   T->TestTrue(TEXT("cab request targets active hatch center"),S.RequestCabPosition(S.TargetHatchCenterY()));
   T->TestTrue(TEXT("grab starts only after cab is centered and stopped"),S.BeginGrabWork());S.SetGrabVisible(true);
   T->TestTrue(TEXT("lowering pose uses safe production grab API"),S.TrySetGrabPose(FVector(0,4,S.Surface().Height(0,4)+8),1));
   CameraLocal=FVector(32,-26,39);AimLocal=FVector(1,0,29);H.ViewCamera->GetCameraComponent()->SetFieldOfView(53);
  }
  if(Shot==4)
  {
   BeforeGrip=S.Surface().Volume;const double Z=S.Surface().Height(0,4);
   // Start below the suspended bucket; retain the normal physical pick checks.
   const auto P=S.GetActorTransform();
   const bool Removed=S.Dig(P.TransformPosition(FVector(0,4,Z+1)*100),P.TransformVectorNoScale(FVector(0,0,-1)),5,3);
   T->TestTrue(TEXT("real production excavation at grab position"),Removed);if(Removed)++Digs;
   T->TestTrue(TEXT("closed grab uses safe production geometry"),S.TrySetGrabPose(FVector(0,4,Z+.2),0));
   CameraLocal=FVector(8,9,FMath::Min(S.HoldDepth-1.,FMath::Max(S.Surface().Height(8,9)+1.8,Z+1.8)));
   AimLocal=FVector(0,4,Z+2.6);H.ViewCamera->GetCameraComponent()->SetFieldOfView(70);
  }
  if(Shot==5){S.EndGrabWork();S.ResetGrabPose();S.SetGrabVisible(false);M.SetModelPresentation(false);M.SetSplitView(true);M.SetMapHeat(true);CameraLocal=FVector(34,-35,63);AimLocal=FVector(0,0,12);}
  if(Shot==6){M.SelectView(3);M.SetModelPresentation(false);}
  if(Shot==7){M.SelectView(4);M.SetSection(true,.5);M.SetModelPresentation(false);}
  if(Shot==8){Regenerate(S,false,65);InitialVolume=S.Surface().Volume;}
  if(Shot>=8&&Shot<=10){S.SetGrabVisible(false);CameraLocal=FVector(16,-18,58);AimLocal=FVector(0,0,8);}
  if(Shot==11||Shot==12){Regenerate(S,Shot==12,95);CameraLocal=FVector(25,-31,49);AimLocal=FVector(0,0,17);H.ViewCamera->GetCameraComponent()->SetFieldOfView(55);}
  if(Shot==13){Regenerate(S,false,65);CameraLocal=FVector(16,18,58);AimLocal=FVector(0,0,8);H.ViewCamera->GetCameraComponent()->SetFieldOfView(55);}
  const auto Pose=S.GetActorTransform();const auto P=Pose.TransformPosition(CameraLocal*100),A=Pose.TransformPosition(AimLocal*100);
  H.ViewCamera->SetActorLocationAndRotation(P,(A-P).Rotation());
  if(Shot==13)M.SetFreeCamera(true);
  Changed=FPlatformTime::Seconds();
 }
 bool Capture(UWorld& W,AShipHatchMonitorHost& H,AShipHatchSimulation& S,UShipHatchMonitorWidget& M)
 {
  const bool Ui=Shot>=5&&Shot<=7;TArray<FColor> Pixels;FIntVector Size;bool Ok=false;
  if(Ui)Ok=FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size);
  else{auto* V=W.GetGameViewport()->Viewport;const auto N=V->GetSizeXY();Size=FIntVector(N.X,N.Y,0);Ok=V->ReadPixels(Pixels);}
  if(!T->TestTrue(TEXT("native capture is 1920x1080"),Ok&&Size.X==1920&&Size.Y==1080))return false;
  int Visible=0;for(int I=0;I<Pixels.Num();I+=16)Visible+=Pixels[I].R>20||Pixels[I].G>20||Pixels[I].B>20;
  if(!T->TestTrue(TEXT("capture has populated pixels"),Visible>2000))return false;
  TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);
  IFileManager::Get().MakeDirectory(*RawRoot(),true);if(!T->TestTrue(TEXT("PNG saved"),FFileHelper::SaveArrayToFile(Bytes,*(RawRoot()/(FString(Names[Shot])+TEXT(".png"))))))return false;
  auto R=MakeShared<FJsonObject>();FString Commit;FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),Commit);
  R->SetStringField(TEXT("source_commit"),Commit);R->SetStringField(TEXT("name"),Names[Shot]);R->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());
  R->SetStringField(TEXT("kind"),TEXT("native D3D12 RHI pixels; editor capture fixture; no asset writes"));R->SetStringField(TEXT("rhi"),GDynamicRHI->GetName());R->SetBoolField(TEXT("hardware_ray_tracing"),SensorOwnedRayTracing::IsAvailable());R->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
  R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);R->SetBoolField(TEXT("includes_slate_ui"),Ui);R->SetNumberField(TEXT("owned_font_scale"),M.GetFontScale());R->SetNumberField(TEXT("umg_viewport_dpi_scale"),UWidgetLayoutLibrary::GetViewportScale(&W));R->SetNumberField(TEXT("slate_application_scale"),FSlateApplication::Get().GetApplicationScale());
  R->SetStringField(TEXT("material"),S.bCoal?TEXT("coal"):TEXT("iron_ore"));R->SetNumberField(TEXT("fill_percent"),S.Surface().Fill);R->SetNumberField(TEXT("remaining_volume_m3"),S.Surface().Volume);R->SetNumberField(TEXT("removed_volume_m3"),S.TotalRemovedVolume());R->SetNumberField(TEXT("production_digs"),Digs);R->SetNumberField(TEXT("surface_crc"),S.Surface().Hash());
  double Min=DBL_MAX,Max=0,Sum=0;for(float Z:S.Surface().Heights){Min=FMath::Min(Min,double(Z));Max=FMath::Max(Max,double(Z));Sum+=Z;}R->SetNumberField(TEXT("surface_min_height_m"),Min);R->SetNumberField(TEXT("surface_max_height_m"),Max);R->SetNumberField(TEXT("surface_mean_vertex_height_m"),Sum/S.Surface().Heights.Num());
  if(const auto* A=M.GetAnalysis()){R->SetNumberField(TEXT("measured_volume_m3"),A->Volume);R->SetNumberField(TEXT("measured_coverage_percent"),A->Coverage*100);R->SetNumberField(TEXT("measured_fill_lower_bound_percent"),A->FillLowerBound);R->SetNumberField(TEXT("measured_min_height_m"),A->Min);R->SetNumberField(TEXT("measured_max_height_m"),A->Max);R->SetNumberField(TEXT("measured_mean_height_m"),A->Mean);R->SetBoolField(TEXT("analysis_is_preview"),M.IsAnalysisPreview());}
  R->SetArrayField(TEXT("actual_camera_hold_m"),RawVector(S.GetActorTransform().InverseTransformPosition(H.ViewCamera->GetActorLocation())*.01));R->SetArrayField(TEXT("aim_hold_m"),RawVector(AimLocal));R->SetNumberField(TEXT("horizontal_fov_deg"),H.ViewCamera->GetCameraComponent()->FieldOfView);
  R->SetNumberField(TEXT("cab_y_m"),S.CabY);R->SetNumberField(TEXT("cab_target_y_m"),S.CabDestinationY());R->SetBoolField(TEXT("cab_moving"),S.IsCabMoving());R->SetBoolField(TEXT("cab_ready"),S.IsCabReady());R->SetBoolField(TEXT("grab_work_active"),S.IsGrabWorkActive());R->SetArrayField(TEXT("grab_lip_hold_m"),RawVector(S.GrabLip()));R->SetNumberField(TEXT("grab_open_fraction"),S.GrabOpening());
  R->SetStringField(TEXT("grab_capture_scope"),Shot==3?TEXT("safe posed lowering after centered-cab gate; not continuous-cycle evidence"):Shot==4?TEXT("closed safe pose after real production Dig; not continuous-cycle evidence"):TEXT("not a grab-action capture"));
  TArray<TSharedPtr<FJsonValue>> Sensors;const TCHAR* Ids[3]={TEXT("os_lidar_boom"),TEXT("os_lidar_cabin_01"),TEXT("os_lidar_cabin_02")};
  for(int I=0;I<3;++I){const auto* C=S.SensorAt(I)->ScanComponent.Get();const auto F=C->GetLastFrameSnapshot();auto D=MakeShared<FJsonObject>();T->TestTrue(TEXT("Ouster sensor ID retained"),C->SensorId==Ids[I]);T->TestTrue(TEXT("Ouster Rev7 profile retained"),S.FieldProfiles[I]==8);T->TestTrue(TEXT("actual sensor frame uses HWRT"),F&&F->AcquisitionBackend==EVirtualLidarAcquisitionBackend::HardwareRayTracing);D->SetStringField(TEXT("sensor_id"),C->SensorId);D->SetStringField(TEXT("model"),C->DeviceSpec.Model);D->SetNumberField(TEXT("profile"),S.FieldProfiles[I]);D->SetStringField(TEXT("backend"),C->GetRuntimeStatus().ActiveAcquisitionBackend);D->SetNumberField(TEXT("measured_hz"),C->GetRuntimeStatus().MeasuredAcquisitionRateHz);D->SetNumberField(TEXT("frame_id"),F?double(F->FrameId):0);D->SetNumberField(TEXT("rays"),F?F->RequestedRayCount:0);D->SetStringField(TEXT("acquisition_utc"),F?F->GetAcquisitionStartUtc().ToIso8601():TEXT(""));Sensors.Add(MakeShared<FJsonValueObject>(D));}
  R->SetArrayField(TEXT("sensors"),Sensors);RawJson(FString(Names[Shot])+TEXT(".json"),R);++Images;return true;
 }
public:
 explicit FRawCaptureRun(FAutomationTestBase* Test):T(Test){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(Now-Started>900){T->AddError(TEXT("RAW-CAP-01 timeout"));return true;}
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
  if(!Host.IsValid())for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)Host=*I;
  if(!Host.IsValid()||!Host->Monitor||!Host->Simulation||Host->Simulation->IsGenerating())return false;
  auto& H=*Host.Get();auto& S=*H.Simulation;auto& M=*H.Monitor;if(!S.SensorAt(0)||!S.SensorAt(1)||!S.SensorAt(2))return false;
  if(!Initialized){if(!T->TestTrue(TEXT("requires actual D3D12 RT on"),GDynamicRHI&&FString(GDynamicRHI->GetName())==TEXT("D3D12")&&SensorOwnedRayTracing::IsAvailable()))return true;for(int I=0;I<3;++I)T->TestEqual(TEXT("default profiles are Rev7"),S.FieldProfiles[I],8);M.SetFontPercent(100);Regenerate(S,false,65);Initialized=true;Frame(H,S,M);return false;}
  if(Digging)
  {
   const double TargetFill=Shot==9?35:5;
   bool FloorReady=S.Surface().Height(0,0)<=1.e-4&&S.Surface().Height(0,4.5)<=1.e-4&&S.Surface().Height(4,0)<=1.e-4&&S.Surface().Height(4,4.5)<=1.e-4;
   if(S.Surface().Fill<=TargetFill&&(Shot==9||FloorReady)){Digging=false;Frame(H,S,M);return false;}
   if(Digs>=1000){T->AddError(TEXT("production excavation limit reached"));return true;}
   double Highest=0;FVector2D Target;
   if(Shot==10&&S.Surface().Fill<=5){for(const FVector2D P:{FVector2D(0,0),FVector2D(0,4.5),FVector2D(4,0),FVector2D(4,4.5)})if(S.Surface().Height(P.X,P.Y)>Highest){Highest=S.Surface().Height(P.X,P.Y);Target=P;}}
   else for(double X:{-9.,-4.5,0.,4.5,9.})for(double Y:{-10.,-5.,0.,5.,10.})if(S.Surface().Height(X,Y)>Highest){Highest=S.Surface().Height(X,Y);Target=FVector2D(X,Y);}
   if(!T->TestTrue(TEXT("excavation uses production Dig"),Highest>0&&DigAt(S,Target)))return true;return false;
  }
  const bool Ui=Shot>=5&&Shot<=7;if(!Ui){const auto Size=FinalAcceptanceViewportSize();H.SetSceneRect(FSlateRect(0,0,Size.X,Size.Y),FVector2D(Size.X,Size.Y),true);}
  if(Now-Changed<3||!S.HasFreshMeasurement()||M.IsAnalysisPreview())return false;
  if(Shot==4)T->TestTrue(TEXT("grip excavation really removed volume"),S.Surface().Volume<BeforeGrip);
  if(Shot==10)T->TestTrue(TEXT("floor exposed by real removal"),Digs>0&&S.TotalRemovedVolume()>InitialVolume*.9&&S.Surface().Height(0,0)<=1.e-4);
  if(!Capture(*W,H,S,M))return true;
  if(Shot==8||Shot==9){++Shot;Digging=true;return false;}
  if(++Shot<14){Frame(H,S,M);return false;}
  S.EndGrabWork();M.SetFreeCamera(false);M.SetRenderOpacity(1);auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("valid"),!T->HasAnyErrors());R->SetNumberField(TEXT("images"),Images);RawJson(TEXT("capture-result.json"),R);return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipHatchRawCaptures,"HATCH07.RawPresentationCapturesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShipHatchRawCaptures::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FRawCaptureRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
