#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorRayQuery.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Json.h"
#include "HAL/FileManager.h"

struct FHatchCabProbe
{
 static bool Queue(UShipHatchMonitorWidget& M,FVector O,FVector D){return M.QueueOrExecuteDigRay(O,D);}
 static bool Pending(UShipHatchMonitorWidget& M){return M.bPendingCabDig;}
};
namespace
{
FString CabRoot(){FString R;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),R);return R.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/FOLLOWUP-26-28/progress/32/runtime"):R;}
class FCabRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* T;int Step=0;double Start=FPlatformTime::Seconds(),Changed=0,StoppedY=0,BeforeDigVolume=0;
    TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> Old;
    FTransform OldAcquisition;FVector FrozenCab,TraceA,TraceB;
    void Shot(UWorld& W,AShipHatchMonitorHost& H,AShipHatchSimulation& S,int Index)
    {
        IFileManager::Get().MakeDirectory(*CabRoot(),true);TArray<FColor> Pixels;FIntVector Size;T->TestTrue(TEXT("actual D3D12 comparison pixels"),FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size));
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(CabRoot()/FString::Printf(TEXT("32-%d.png"),Index)));
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("cab_x_m"),S.CabX);R->SetNumberField(TEXT("cab_y_m"),S.CabY);R->SetNumberField(TEXT("cab_roof_z_m"),S.CabRoofZ());R->SetNumberField(TEXT("boom_soffit_z_m"),S.BoomSoffitZ());R->SetNumberField(TEXT("quay_z_m"),S.QuayZ());R->SetNumberField(TEXT("water_z_m"),S.WaterZ());R->SetNumberField(TEXT("coaming_top_z_m"),S.HoldDepth+S.CoamingHeight);R->SetNumberField(TEXT("operating_draft_m"),S.OperatingDraft);R->SetNumberField(TEXT("design_draft_m"),S.DesignDraft);R->SetNumberField(TEXT("beam_m"),S.VesselBeam);R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);
        double Opening,Floor,Area;S.Coverage(Opening,Floor,Area);R->SetNumberField(TEXT("opening_fov_percent"),Opening);R->SetNumberField(TEXT("potential_floor_visible_percent"),Floor);R->SetNumberField(TEXT("potential_floor_area_m2"),Area);
        auto Vector=[](FVector P){TArray<TSharedPtr<FJsonValue>> A;for(double V:{P.X,P.Y,P.Z})A.Add(MakeShared<FJsonValueNumber>(V));return A;};R->SetArrayField(TEXT("camera_world_cm"),Vector(H.ViewCamera->GetActorLocation()));R->SetArrayField(TEXT("camera_pitch_yaw_roll"),Vector(FVector(H.ViewCamera->GetActorRotation().Pitch,H.ViewCamera->GetActorRotation().Yaw,H.ViewCamera->GetActorRotation().Roll)));
        const auto Rect=H.Monitor->GetSceneRect();const auto Geometry=H.Monitor->GetCachedGeometry().GetLocalSize();R->SetNumberField(TEXT("scene_left_px"),Rect.Left*Size.X/Geometry.X);R->SetNumberField(TEXT("scene_top_px"),Rect.Top*Size.Y/Geometry.Y);R->SetNumberField(TEXT("scene_right_px"),Rect.Right*Size.X/Geometry.X);R->SetNumberField(TEXT("scene_bottom_px"),Rect.Bottom*Size.Y/Geometry.Y);
        TArray<TSharedPtr<FJsonValue>> Rows;for(int I=0;I<3;++I){auto O=MakeShared<FJsonObject>();auto* A=S.SensorAt(I);const auto Status=A->ScanComponent->GetRuntimeStatus();O->SetStringField(TEXT("backend"),Status.ActiveAcquisitionBackend);O->SetNumberField(TEXT("hz"),Status.MeasuredAcquisitionRateHz);O->SetNumberField(TEXT("rays"),A->ScanComponent->HorizontalSamples*A->ScanComponent->VerticalChannels);O->SetNumberField(TEXT("hits"),Status.HitPointCount);Rows.Add(MakeShared<FJsonValueObject>(O));}R->SetArrayField(TEXT("sensors"),Rows);
        FString Json;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(CabRoot()/FString::Printf(TEXT("32-%d.json"),Index)));
    }
    void Camera(AShipHatchMonitorHost& H,AShipHatchSimulation& S,int Index)
    {
        if(Index){H.Monitor->SetCameraPreset(3);return;}
        const double OldQuay=S.KeelZ()+S.DesignDraft+S.QuayAboveWater;
        const auto P=S.GetActorTransform().TransformPosition(FVector(100,-22,OldQuay+8)*100),Aim=S.GetActorTransform().TransformPosition(FVector(0,-22,OldQuay+24)*100);
        H.ViewCamera->SetActorLocationAndRotation(P,(Aim-P).Rotation());H.ViewCamera->GetCameraComponent()->SetFieldOfView(58);
    }
public:
    explicit FCabRun(FAutomationTestBase* Test):T(Test){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Start>90){T->AddError(TEXT("independent cab sequence timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        AShipHatchMonitorHost* H=nullptr;for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;if(!H||!H->Simulation||!H->Monitor||!H->Simulation->HasFreshMeasurement())return false;
        auto& S=*H->Simulation;
        if(Step==0)
        {
            T->TestEqual(TEXT("default geometry remains on hardware backend"),S.SensorAt(1)->ScanComponent->GetRuntimeStatus().ActiveAcquisitionBackend,FString(TEXT("hardware_ray_tracing")));
            T->TestTrue(TEXT("default cab parked over target hatch"),S.IsCabReady()&&FMath::IsNearlyZero(S.CabY));
            T->TestTrue(TEXT("cab roof within one metre below boom soffit"),FMath::IsNearlyEqual(S.BoomSoffitZ()-S.CabRoofZ(),.9,.001));
            T->TestTrue(TEXT("soffit model assumption rail plus25m"),FMath::IsNearlyEqual(S.BoomSoffitZ()-S.QuayZ(),25.,.001));
            T->TestTrue(TEXT("cab/grab full-width lanes keep at least1m clearance"),FMath::Abs(S.CabX)-1.2-2.1>=1.);
            T->TestFalse(TEXT("cab cannot leave boom rail"),S.RequestCabPosition(S.CabTravelMaxY()+.01));
            Old=S.CaptureRayQuery(nullptr,ECC_Visibility);OldAcquisition=S.SensorAt(1)->ScanComponent->GetLastFrameSnapshot()->AcquisitionTransform;
            TraceA=S.GetActorTransform().TransformPosition(FVector(S.CabX,S.CabY-1.35,S.CabRoofZ()+.2)*100);TraceB=TraceA-S.GetActorTransform().GetUnitAxis(EAxis::Z)*100;
            T->TestTrue(TEXT("cab first departs independently"),S.RequestCabPosition(.5));T->TestFalse(TEXT("moving cab blocks grab work"),S.BeginGrabWork());Changed=Now;++Step;return false;
        }
        if(Step==1)
        {
            if(Now-Changed<.8)return false;T->TestTrue(TEXT("25m/min bounded physical travel"),S.CabY>0&&S.CabY<=.5);S.StopCab();StoppedY=S.CabY;Changed=Now;++Step;return false;
        }
        if(Step==2)
        {
            if(Now-Changed<.3)return false;T->TestEqual(TEXT("stop holds exact cab position"),S.CabY,StoppedY);
            T->TestTrue(TEXT("old acquisition transform remains immutable"),OldAcquisition.GetLocation().Equals(S.GetActorTransform().TransformPosition(FVector(S.CabX-1.3,0,S.HoldDepth+S.CabHeight)*100),.001));
            T->TestTrue(TEXT("attached cab sensor follows actual carriage"),S.SensorAt(1)->GetActorLocation().Equals(S.GetActorTransform().TransformPosition(S.SensorOrigin(1)*100),.001));
            FHitResult Before,After;T->TestTrue(TEXT("old CPU snapshot retains old cab edge"),Old->Trace(TraceA,TraceB,Before));T->TestEqual(TEXT("old snapshot cab surface"),Before.GetComponent(),S.CabPrimitive());
            auto Current=S.CaptureRayQuery(nullptr,ECC_Visibility);T->TestFalse(TEXT("new snapshot no longer has the old cab edge"),Current->Trace(TraceA,TraceB,After));
            TArray<FVirtualSensorCollisionMesh> A,B;Old->CollisionMeshes(A);Current->CollisionMeshes(B);T->TestEqual(TEXT("cab is an independent fourth snapshot mesh"),B.Num(),4);if(A.Num()==4&&B.Num()==4){T->TestTrue(TEXT("travel reuses GPU vertices and BLAS revision"),A[3].Vertices==B[3].Vertices&&A[3].Revision==B[3].Revision);T->TestFalse(TEXT("immutable GPU instances have different poses"),A[3].LocalToWorld.Equals(B[3].LocalToWorld));}
            BeforeDigVolume=S.Surface().Volume;
            const FVector O=S.GetActorTransform().TransformPosition(FVector(-5,4,30)*100),D=-S.GetActorTransform().GetUnitAxis(EAxis::Z);
            T->TestTrue(TEXT("UI queues dig behind independent cab alignment"),FHatchCabProbe::Queue(*H->Monitor,O,D));T->TestTrue(TEXT("pending work is visible to UI state"),FHatchCabProbe::Pending(*H->Monitor));T->TestEqual(TEXT("no excavation while cab is travelling"),S.Surface().Volume,BeforeDigVolume);++Step;return false;
        }
        if(Step==3)
        {
            if(S.IsCabMoving()||FHatchCabProbe::Pending(*H->Monitor))return false;T->TestTrue(TEXT("queued UI excavation executes only after parking"),S.Surface().Volume<BeforeDigVolume);S.RestoreCargo();T->TestTrue(TEXT("parked cab admits work"),S.BeginGrabWork());FrozenCab=FVector(S.CabX,S.CabY,S.CabHeight);T->TestFalse(TEXT("grab work rejects cab relocation"),S.RequestCabPosition(1));S.SetGrabVisible(true);
            T->TestTrue(TEXT("safe hold pose accepted"),S.TrySetGrabPose(FVector(0,4,S.Surface().Height(0,4)+1.2),0));T->TestFalse(TEXT("rope through cab rejected"),S.TrySetGrabPose(FVector(S.CabX,S.CabY,S.HoldDepth),0));T->TestFalse(TEXT("overhead bucket interference rejected"),S.TrySetGrabPose(FVector(0,4,S.BoomSoffitZ()-4),0));
            const int32 Commits=S.GrabMeshCommits;H->Monitor->Refresh();T->TestTrue(TEXT("idle UI refresh preserves external grab work and pose ownership"),S.IsGrabWorkActive()&&S.GrabMeshCommits==Commits);
            S.EndGrabWork();S.ResetGrabPose();S.SetGrabVisible(false);H->Monitor->SelectView(0);H->Monitor->SetSplitView(false);H->Monitor->SetModelPresentation(true);Camera(*H,S,0);Changed=Now;++Step;return false;
        }
        if(Now-Changed<1.5)return false;
        T->TestTrue(TEXT("cab is unchanged through grab work and trolley poses"),FrozenCab.Equals(FVector(S.CabX,S.CabY,S.CabHeight),0));Shot(*W,*H,S,Step-4);
        if(Step++==4){Camera(*H,S,1);Changed=Now;return false;}return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndependentCab,"MA0T10.Hatch.IndependentCabRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIndependentCab::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCabRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
