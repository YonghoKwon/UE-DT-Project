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
#include "SensorOwnedRayTracing.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "DynamicRHI.h"
#include "UnrealClient.h"
#include "Json.h"

namespace
{
FString CornerOutput()
{
    FString Path;
    FParse::Value(FCommandLine::Get(),TEXT("HatchCornerOutput="),Path);
    return Path.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/GPU-LIDAR-01/captures/corners"):Path;
}
void WriteCornerJson(const FString& Path,const TSharedRef<FJsonObject>& Object)
{
    FString Text;FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Text));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    FFileHelper::SaveStringToFile(Text,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& V)
{
    return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};
}
class FHatchCornerCaptureRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Phase=0,Shot=0,Edits=0;
    double Started=FPlatformTime::Seconds(),Changed=0,LastDig=0,InitialVolume=0;
    FVector CameraLocal,AimLocal;
    uint32 ResidualCrc=0;
    bool PolishFloor=false;
    TWeakObjectPtr<AShipHatchMonitorHost> Host;
    const TCHAR* Names[6]={TEXT("01-iron-full-seaward-bow"),TEXT("02-iron-full-seaward-stern"),
        TEXT("03-iron-full-landward-bow"),TEXT("04-iron-full-landward-stern"),
        TEXT("05-coal-full-seaward-bow"),TEXT("06-iron-low-residual-seaward-bow")};
    void FullCargo(AShipHatchSimulation& S,bool Coal)
    {
        S.bCoal=Coal;S.Seed=19019;S.FillMin=S.FillMax=95;
        Test->TestTrue(TEXT("full cargo uses production regeneration at supported95 percent"),S.Regenerate(false));
        Changed=FPlatformTime::Seconds();
    }
    void FrameCorner(AShipHatchMonitorHost& H,AShipHatchSimulation& S,bool Points=false)
    {
        auto& M=*H.Monitor;M.SelectView(Points?1:0);M.SetSplitView(false);M.SetModelPresentation(true);
        M.SetRenderOpacity(0); // capture-only HUD suppression; underlying RHI scene is unchanged
        const int SX=(Shot==1||Shot==3)?-1:1,SY=(Shot==2||Shot==3)?-1:1;
        if(Shot<5)
        {
            CameraLocal=FVector(SX*3.1,SY*3.8,S.HoldDepth+5.5);
            const double X=SX*(S.OpeningLength*.5-.6),Y=SY*(S.OpeningWidth*.5-.6);
            AimLocal=FVector(X,Y,S.Surface().Height(X,Y)+.1);
            H.ViewCamera->GetCameraComponent()->SetFieldOfView(55);
        }
        else
        {
            CameraLocal=Points?FVector(25,-5,65):FVector(0,-2,16);
            AimLocal=Points?FVector(0,2,2):FVector(10.5,18,1);
            H.ViewCamera->GetCameraComponent()->SetFieldOfView(Points?55:74);
        }
        const auto Pose=S.GetActorTransform();const auto P=Pose.TransformPosition(CameraLocal*100),Aim=Pose.TransformPosition(AimLocal*100);
        H.ViewCamera->SetActorLocationAndRotation(P,(Aim-P).Rotation());
        H.SetSceneRect(FSlateRect(0,0,1920,1080),FVector2D(1920,1080),true);
        Changed=FPlatformTime::Seconds();
    }
    TSharedRef<FJsonObject> Metadata(UWorld& W,AShipHatchSimulation& S,bool Points)
    {
        auto R=MakeShared<FJsonObject>();
        FString Commit;FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),Commit);
        R->SetStringField(TEXT("source_commit"),Commit);R->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());
        R->SetStringField(TEXT("map"),TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap; opened only, never saved"));
        R->SetStringField(TEXT("kind"),TEXT("automated offscreen actual D3D12 RHI; no Windows input"));
        R->SetStringField(TEXT("rhi"),GDynamicRHI?GDynamicRHI->GetName():TEXT("none"));
        R->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
        R->SetStringField(TEXT("material"),S.bCoal?TEXT("coal"):TEXT("iron_ore"));
        R->SetStringField(TEXT("side"),Shot==2||Shot==3?TEXT("landward"):TEXT("seaward"));
        R->SetStringField(TEXT("end"),Shot==1||Shot==3?TEXT("stern"):TEXT("bow"));
        R->SetStringField(TEXT("axis_convention"),TEXT("hold-local meters: bow +X, stern -X, seaward +Y, landward -Y, up +Z"));
        R->SetStringField(TEXT("corner_definition"),Shot<5?TEXT("hatch opening / coaming corner"):TEXT("lower hold wall corner after real production excavation"));
        R->SetBoolField(TEXT("internal_hopper_slopes_modeled"),false);
        R->SetStringField(TEXT("model_limitation"),TEXT("Current target hold has vertical corrugated walls and tank top; lower internal hopper slopes are not modeled. Quayside receiving hopper is separate. No geometry added for capture."));
        R->SetNumberField(TEXT("width"),1920);R->SetNumberField(TEXT("height"),1080);
        R->SetArrayField(TEXT("camera_hold_m"),VectorJson(CameraLocal));R->SetArrayField(TEXT("aim_hold_m"),VectorJson(AimLocal));
        const auto* Camera=Host->ViewCamera.Get();const auto Rotation=Camera->GetActorRotation();
        R->SetArrayField(TEXT("camera_world_cm"),VectorJson(Camera->GetActorLocation()));
        R->SetArrayField(TEXT("camera_rotation_pitch_yaw_roll"),VectorJson(FVector(Rotation.Pitch,Rotation.Yaw,Rotation.Roll)));
        R->SetNumberField(TEXT("horizontal_fov_deg"),Camera->GetCameraComponent()->FieldOfView);
        R->SetNumberField(TEXT("seed"),S.Seed);R->SetNumberField(TEXT("initial_fill_percent"),95);
        R->SetNumberField(TEXT("remaining_fill_percent"),S.Surface().Fill);R->SetNumberField(TEXT("remaining_volume_m3"),S.Surface().Volume);
        R->SetNumberField(TEXT("removed_volume_m3"),S.TotalRemovedVolume());R->SetNumberField(TEXT("production_dig_count"),Edits);
        R->SetNumberField(TEXT("surface_revision"),double(S.Revision()));R->SetNumberField(TEXT("surface_crc"),S.Surface().Hash());
        R->SetNumberField(TEXT("center_height_m"),S.Surface().Height(0,0));
        R->SetNumberField(TEXT("foreground_floor_height_m"),S.Surface().Height(4,4.5));
        R->SetNumberField(TEXT("corner_height_m"),S.Surface().Height(S.HoldLength*.5-.6,S.HoldWidth*.5-.7));
        R->SetStringField(TEXT("pixel_capture_method"),TEXT("GameViewport ReadPixels: scene render target, excluding Slate HUD"));
        R->SetStringField(TEXT("point_view_projection"),Points?TEXT("overall hold view for actual sensor coverage; same unchanged residual geometry"):TEXT("corner closeup"));
        R->SetBoolField(TEXT("point_cloud_view"),Points);R->SetBoolField(TEXT("hud_suppressed_for_capture"),true);
        R->SetBoolField(TEXT("owned_unloader_hidden_for_closeup"),true);
        FVector2D Pixel;const bool Projected=W.GetFirstPlayerController()->ProjectWorldLocationToScreen(S.GetActorTransform().TransformPosition(AimLocal*100),Pixel);
        R->SetBoolField(TEXT("aim_projects_inside_central_frame"),Projected&&Pixel.X>192&&Pixel.X<1728&&Pixel.Y>108&&Pixel.Y<972);
        Test->TestTrue(TEXT("corner target projects centrally; camera is not pointed at deck/sky"),Projected&&Pixel.X>192&&Pixel.X<1728&&Pixel.Y>108&&Pixel.Y<972);
        Test->TestTrue(TEXT("camera is directed down into the hold"),Rotation.Pitch<-5);
        TArray<TSharedPtr<FJsonValue>> Sensors;
        for(int I=0;I<3;++I)if(auto* A=S.SensorAt(I))
        {
            const auto* Scan=A->ScanComponent.Get();const auto F=Scan->GetLastFrameSnapshot();auto Row=MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("sensor_id"),Scan->SensorId);Row->SetStringField(TEXT("backend"),Scan->GetRuntimeStatus().ActiveAcquisitionBackend);
            Row->SetNumberField(TEXT("frame_id"),F?double(F->FrameId):0);Row->SetNumberField(TEXT("ray_count"),F&&F->Points?F->Points->Num():0);
            Row->SetStringField(TEXT("acquisition_utc"),F?F->GetAcquisitionStartUtc().ToIso8601():TEXT(""));
            Row->SetBoolField(TEXT("acquired_after_last_dig"),F&&F->AcquisitionStartedMonotonic>=LastDig);
            Sensors.Add(MakeShared<FJsonValueObject>(Row));
        }
        R->SetArrayField(TEXT("sensors"),Sensors);return R;
    }
    bool Capture(UWorld& W,AShipHatchSimulation& S,bool Points=false)
    {
        TArray<FColor> Pixels;auto* Viewport=W.GetGameViewport()->Viewport;const auto Dimensions=Viewport->GetSizeXY();const FIntVector Size(Dimensions.X,Dimensions.Y,0);
        if(!Test->TestTrue(TEXT("offscreen RHI scene readback succeeds"),Viewport->ReadPixels(Pixels)))return false;
        if(!Test->TestTrue(TEXT("capture is exactly1920x1080"),Size.X==1920&&Size.Y==1080))return false;
        int Visible=0;for(int I=0;I<Pixels.Num();I+=16){const auto& C=Pixels[I];Visible+=C.R>20||C.G>20||C.B>20;}
        if(!Test->TestTrue(TEXT("RHI image contains visible scene or cloud pixels instead of a blank view"),Visible>2000))return false;
        auto Meta=Metadata(W,S,Points);Meta->SetNumberField(TEXT("visible_sampled_pixels"),Visible);
        const FString Name=Points?TEXT("session/r4/raw/06-gpu-point-cloud"):FString(Names[Shot]);
        const FString Path=CornerOutput()/(Name+TEXT(".png"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        if(!Test->TestTrue(TEXT("PNG evidence written"),FFileHelper::SaveArrayToFile(Png,*Path)))return false;
        WriteCornerJson(FPaths::ChangeExtension(Path,TEXT("json")),Meta);return true;
    }
public:
    explicit FHatchCornerCaptureRun(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>600){Test->AddError(TEXT("corner capture timeout"));return true;}
        UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)W=Context.World();
        if(!W||!FinalAcceptanceViewportReady(W))return false;
        if(!Host.IsValid())for(TActorIterator<AShipHatchMonitorHost> I(W);I;++I)Host=*I;
        if(!Host.IsValid()||!Host->Monitor||!Host->Simulation||Host->Simulation->IsGenerating())return false;
        auto& S=*Host->Simulation;auto& M=*Host->Monitor;
        if(!S.MLX||!S.Livox||!S.CabRight)return false;
        if(Phase==0)
        {
            if(!Test->TestTrue(TEXT("capture uses actual D3D12, never NullRHI"),GDynamicRHI&&FString(GDynamicRHI->GetName()).Contains(TEXT("D3D12"))))return true;
            if(!Test->TestTrue(TEXT("GPU point-cloud evidence requires real sensor-owned HWRT"),SensorOwnedRayTracing::IsAvailable()))return true;
            S.ShowUnloader(false);S.SetGrabVisible(false);S.SetScanDisplay(false);M.SetFontPercent(100);
            for(int I=0;I<3;++I){FString Reason;S.SensorAt(I)->SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend::Auto,Reason);}
            FullCargo(S,false);Phase=1;return false;
        }
        if(Phase==1)
        {
            if(Now-Changed<3||!S.HasFreshMeasurement())return false;
            if(Shot==0)InitialVolume=S.Surface().Volume;
            FrameCorner(*Host,S);Phase=2;return false;
        }
        Host->SetSceneRect(FSlateRect(0,0,1920,1080),FVector2D(1920,1080),true);
        if(Phase==2)
        {
            if(Now-Changed<2)return false;
            if(!Capture(*W,S))return true;
            if(++Shot<4){FrameCorner(*Host,S);return false;}
            if(Shot==4){FullCargo(S,true);Phase=1;return false;}
            FullCargo(S,false);Phase=3;return false;
        }
        if(Phase==3)
        {
            if(Now-Changed<3||!S.HasFreshMeasurement())return false;
            double Highest=0;FVector2D Target;
            for(double X:{-8.,-4.,0.,4.,8.})for(double Y:{-9.,-4.5,0.,4.5,9.})
                if(S.Surface().Height(X,Y)>Highest){Highest=S.Surface().Height(X,Y);Target=FVector2D(X,Y);}
            if(Highest<=.5&&S.Surface().Fill<=5)PolishFloor=true;
            if(PolishFloor)
            {
                Highest=0;
                for(const FVector2D P:{FVector2D(0,0),FVector2D(0,4.5),FVector2D(4,0),FVector2D(4,4.5)})
                    if(S.Surface().Height(P.X,P.Y)>Highest){Highest=S.Surface().Height(P.X,P.Y);Target=P;}
            }
            if(PolishFloor&&Highest<=1.e-4)
            {
                Test->TestTrue(TEXT("residual comes from physical excavation of the95percent iron load"),Edits>0&&S.TotalRemovedVolume()>InitialVolume*.9);
                ResidualCrc=S.Surface().Hash();LastDig=FMath::Max(LastDig,Now-.001);FrameCorner(*Host,S);Phase=4;return false;
            }
            if(Edits>=1000||Highest<=1.e-4){Test->AddError(TEXT("production digging did not reach near-floor residual within bounded capture work"));return true;}
            const auto Pose=S.GetActorTransform();
            if(!S.Dig(Pose.TransformPosition(FVector(Target,S.HoldDepth+10)*100),Pose.TransformVectorNoScale(FVector(0,0,-1)),5,3))
            {Test->AddError(TEXT("production collision-aware excavation rejected the open-aperture target"));return true;}
            ++Edits;LastDig=Now;Changed=Now-3; // next engine frame may perform the next real edit
            if(Edits%25==0){auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("digs"),Edits);R->SetNumberField(TEXT("fill_percent"),S.Surface().Fill);R->SetNumberField(TEXT("center_height_m"),S.Surface().Height(0,0));WriteCornerJson(CornerOutput()/TEXT("session/r4/excavation-progress.json"),R);}
            return false;
        }
        if(Phase==4)
        {
            if(Now-Changed<4||!S.HasFreshMeasurement())return false;
            for(int I=0;I<3;++I)
            {
                auto* Scan=S.SensorAt(I)->ScanComponent.Get();const auto F=Scan->GetLastFrameSnapshot();
                if(!F||F->AcquisitionStartedMonotonic<LastDig)return false;
                if(!Test->TestEqual(TEXT("residual point cloud was acquired by HWRT"),Scan->GetActiveAcquisitionBackend(),EVirtualLidarAcquisitionBackend::HardwareRayTracing))return true;
            }
            if(!Capture(*W,S))return true;
            FrameCorner(*Host,S,true);M.SelectColor(1);Phase=5;return false;
        }
        if(Phase==5)
        {
            if(Now-Changed<3)return false;
            Test->TestEqual(TEXT("3D and GPU point-cloud captures have the same unchanged surface"),S.Surface().Hash(),ResidualCrc);
            if(!Capture(*W,S,true))return true;
            M.SetRenderOpacity(1);auto Done=MakeShared<FJsonObject>();Done->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());Done->SetNumberField(TEXT("main_images"),6);Done->SetNumberField(TEXT("production_digs"),Edits);Done->SetNumberField(TEXT("remaining_fill_percent"),S.Surface().Fill);
            WriteCornerJson(CornerOutput()/TEXT("session/r4/capture-result.json"),Done);return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchCornerCaptures,"HATCH06.CornerCapturesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchCornerCaptures::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());
    ADD_LATENT_AUTOMATION_COMMAND(FHatchCornerCaptureRun(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
