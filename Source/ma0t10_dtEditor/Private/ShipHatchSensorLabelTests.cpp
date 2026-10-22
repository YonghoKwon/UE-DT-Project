#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

struct FHatchLabelProbe
{
    static void View(UShipHatchMonitorWidget& M,int32 State)
    {
        M.SelectView(State==4?1:0);M.SetSplitView(State!=3);M.SetCameraPreset(State==0||State==3?0:1);
        if(State==2){M.OrbitYaw=-132;M.UpdateCamera();}
    }
    static TSharedRef<FJsonObject> Inspect(FAutomationTestBase& T,UShipHatchMonitorWidget& M,bool Baseline)
    {
        auto R=MakeShared<FJsonObject>();auto& S=*M.Simulation;auto* PC=M.GetOwningPlayer();const auto G=M.GetCachedGeometry();int32 W,H;PC->GetViewportSize(W,H);
        auto Vector=[](FVector P){TArray<TSharedPtr<FJsonValue>> A;for(double V:{P.X,P.Y,P.Z})A.Add(MakeShared<FJsonValueNumber>(V));return A;};
        auto Screen=[&](FVector World){FVector2D P;T.TestTrue(TEXT("actual actor/mesh projects into player view"),PC->ProjectWorldLocationToScreen(World,P));return P*G.GetLocalSize()/FVector2D(W,H);};
        auto ScreenRect=[&](UWidget* B){const auto BG=B->GetCachedGeometry();const auto A=G.AbsoluteToLocal(BG.LocalToAbsolute(FVector2D::ZeroVector)),Z=G.AbsoluteToLocal(BG.LocalToAbsolute(BG.GetLocalSize()));return FSlateRect(A.X,A.Y,Z.X,Z.Y);};
        AShipHatchPart* Unloader=nullptr;for(TActorIterator<AShipHatchPart>I(S.GetWorld());I;++I)if(I->ActorHasTag(TEXT("ShipHatchUnloader")))Unloader=*I;
        T.TestNotNull(TEXT("existing unloader display exists"),Unloader);if(!Unloader)return R;
        const auto* Mesh=Unloader->Mesh->GetProcMeshSection(0);auto* CabMesh=Cast<UProceduralMeshComponent>(S.CabPrimitive());
        if(!T.TestTrue(TEXT("separate boom rail and independently travelling physical cab"),Mesh&&Mesh->ProcVertexBuffer.Num()>=40&&CabMesh&&CabMesh->GetProcMeshSection(0)))return R;
        FVector Cab=FVector::ZeroVector,Boom=FVector::ZeroVector;for(int I=0;I<8;++I)Cab+=CabMesh->GetProcMeshSection(0)->ProcVertexBuffer[I].Position/8;for(int I=0;I<16;++I)Boom+=Mesh->ProcVertexBuffer[I].Position/16;
        const auto Pose=Unloader->Mesh->GetComponentTransform();const auto CabWorld=CabMesh->GetComponentTransform().TransformPosition(Cab),BoomWorld=Pose.TransformPosition(Boom);const auto CabLocal=S.GetActorTransform().InverseTransformPosition(CabWorld)*.01;
        R->SetArrayField(TEXT("cab_mesh_center_world_cm"),Vector(CabWorld));R->SetArrayField(TEXT("cab_mesh_center_hold_m"),Vector(CabLocal));R->SetArrayField(TEXT("boom_rails_center_world_cm"),Vector(BoomWorld));R->SetArrayField(TEXT("boom_at_cab_y_hold_m"),Vector(FVector(0,S.CabY,S.HoldDepth+S.BoomHeight+1.2)));
        R->SetArrayField(TEXT("camera_world_cm"),Vector(M.Host->ViewCamera->GetActorLocation()));R->SetArrayField(TEXT("camera_rotation_degrees"),Vector(FVector(M.Host->ViewCamera->GetActorRotation().Pitch,M.Host->ViewCamera->GetActorRotation().Yaw,M.Host->ViewCamera->GetActorRotation().Roll)));
        const auto CabPixel=Screen(CabWorld);R->SetArrayField(TEXT("cab_center_root_xy"),Vector(FVector(CabPixel,0)));
        TArray<TSharedPtr<FJsonValue>> Rows;FVector2D P[3];FSlateRect Boxes[3];
        for(int I=0;I<3;++I)
        {
            auto* A=S.SensorAt(I);T.TestNotNull(TEXT("actual detector"),A);if(!A)continue;const auto Local=S.GetActorTransform().InverseTransformPosition(A->GetActorLocation())*.01;P[I]=Screen(A->GetActorLocation());Boxes[I]=ScreenRect(M.SensorBadges[I]);
            T.TestTrue(TEXT("actual acquisition origin equals established installation, without UI relocation"),Local.Equals(S.SensorOrigin(I),1.e-6));
            // Body half-width1.20m +0.10m mounting bracket =1.30m.
            // Compare signed positions on both sides, allowing at most1cm;
            // a sensor farther out is a failure, not a looser lower bound.
            if(I){const double Expected=(I==1?-1.:1.)*1.30+S.SensorOffsets[I].X;T.TestTrue(TEXT("cab LiDAR signed mount is1.30m from mesh center within1cm"),FMath::Abs((Local.X-CabLocal.X)-Expected)<=.01);}
            const TCHAR* Names[]={TEXT("붐 끝"),TEXT("운전실 좌"),TEXT("운전실 우")};T.TestEqual(TEXT("physical sensor identity never follows camera screen order"),CastChecked<UTextBlock>(M.SensorBadges[I]->GetContent())->GetText().ToString(),FString(Names[I]));
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("index"),I);Row->SetStringField(TEXT("physical_role"),Names[I]);Row->SetArrayField(TEXT("actor_world_cm"),Vector(A->GetActorLocation()));Row->SetArrayField(TEXT("origin_hold_m"),Vector(Local));Row->SetArrayField(TEXT("projected_root_xy"),Vector(FVector(P[I],0)));
            TArray<TSharedPtr<FJsonValue>> Rect;for(double V:{double(Boxes[I].Left),double(Boxes[I].Top),double(Boxes[I].Right),double(Boxes[I].Bottom)})Rect.Add(MakeShared<FJsonValueNumber>(V));Row->SetArrayField(TEXT("badge_root_ltrb"),Rect);Rows.Add(MakeShared<FJsonValueObject>(Row));
            const auto Frame=A->ScanComponent->GetLastFrameSnapshot();if(Frame){Row->SetArrayField(TEXT("acquisition_world_cm"),Vector(Frame->AcquisitionTransform.GetLocation()));Row->SetNumberField(TEXT("frame_id"),A->ScanComponent->GetRuntimeStatus().FrameId);T.TestTrue(TEXT("latched static acquisition origin agrees with actual detector"),Frame->AcquisitionTransform.GetLocation().Equals(A->GetActorLocation(),.001));}
            if(!Baseline){FVector2D Anchor,End;T.TestTrue(TEXT("visible label owns an explicit anchor and connector"),M.GetSensorBadgeLink(I,Anchor,End));T.TestTrue(TEXT("anchor matches independent actual detector projection <= 1 logical pixel"),Anchor.Equals(P[I],1));T.TestTrue(TEXT("connector terminates on its own label boundary"),End.X>=Boxes[I].Left-.1&&End.X<=Boxes[I].Right+.1&&End.Y>=Boxes[I].Top-.1&&End.Y<=Boxes[I].Bottom+.1);Row->SetArrayField(TEXT("anchor_root_xy"),Vector(FVector(Anchor,0)));Row->SetArrayField(TEXT("connector_end_root_xy"),Vector(FVector(End,0)));}
            if(!Baseline)T.TestTrue(TEXT("badge remains in owned 3D scene rectangle"),Boxes[I].Left>=M.SceneRect.Left-.1&&Boxes[I].Top>=M.SceneRect.Top-.1&&Boxes[I].Right<=M.SceneRect.Right+.1&&Boxes[I].Bottom<=M.SceneRect.Bottom+.1);
        }
        R->SetArrayField(TEXT("sensors"),Rows);R->SetNumberField(TEXT("geometry_revision"),S.Revision());R->SetNumberField(TEXT("surface_crc"),S.Surface().Hash());R->SetBoolField(TEXT("baseline_capture_only"),Baseline);
        if(!Baseline)
        {
            for(int I=0;I<3;++I)for(int J=I+1;J<3;++J)T.TestFalse(TEXT("projected sensor labels do not overlap"),Boxes[I].Left<Boxes[J].Right&&Boxes[I].Right>Boxes[J].Left&&Boxes[I].Top<Boxes[J].Bottom&&Boxes[I].Bottom>Boxes[J].Top);
            if(FMath::Abs(P[1].X-P[2].X)>1){const int Left=P[1].X<P[2].X?1:2,Right=3-Left;T.TestTrue(TEXT("cab labels sit outside their own projected sensor, on opposing sides"),Boxes[Left].Right<=P[Left].X-8*M.GetFontScale()&&Boxes[Right].Left>=P[Right].X+8*M.GetFontScale());}
        }
        return R;
    }
    static void Raster(FAutomationTestBase& T,UShipHatchMonitorWidget& M,const TArray<FColor>& Pixels,FIntVector Size)
    {
        const auto Root=M.GetCachedGeometry(),VG=M.GetWorld()->GetGameViewport()->GetGameViewportWidget()->GetCachedGeometry();const FLinearColor Colors[]={FLinearColor(.1,.9,1),FLinearColor(1,.6,.06),FLinearColor(.9,.2,.8)};
        for(int I=0;I<3;++I)
        {
            FVector2D Anchor,End;if(!M.GetSensorBadgeLink(I,Anchor,End))continue;
            const auto P=VG.AbsoluteToLocal(Root.LocalToAbsolute(Anchor))*FVector2D(Size.X,Size.Y)/VG.GetLocalSize();const auto C=Colors[I].ToFColorSRGB();int Hits=0;
            for(int Y=-3;Y<=3;++Y)for(int X=-3;X<=3;++X){const auto Q=Pixels[FMath::Clamp(FMath::RoundToInt(P.Y)+Y,0,Size.Y-1)*Size.X+FMath::Clamp(FMath::RoundToInt(P.X)+X,0,Size.X-1)];Hits+=FMath::Abs(int(Q.R)-C.R)<8&&FMath::Abs(int(Q.G)-C.G)<8&&FMath::Abs(int(Q.B)-C.B)<8;}
            if(M.IsOverlayAnchorVisible(Anchor))T.TestTrue(TEXT("rendered role-colored anchor exists at detector pixel outside clear center"),Hits>=2);
            else T.TestEqual(TEXT("user2026-10-21: center sensor anchor is suppressed in actual rendered pixels"),Hits,0);
        }
    }
};
namespace
{
FString LabelRoot(){FString P;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-05-LABELS/focus"):P;}
class FLabelRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int State=-1;double Start=FPlatformTime::Seconds(),Changed=0;TUniquePtr<TGuardValue<bool>> Messages;
public:
    explicit FLabelRun(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Start>120){Test->AddError(TEXT("sensor label acceptance timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        AShipHatchMonitorHost* Host=nullptr;for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)Host=*I;if(!Host||!Host->Monitor||!Host->Monitor->GetAnalysis())return false;auto& M=*Host->Monitor;
        if(State<0){Messages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);int Font=100;FParse::Value(FCommandLine::Get(),TEXT("HatchFontPercent="),Font);M.SetFontPercent(Font);State=0;FHatchLabelProbe::View(M,State);Changed=Now;return false;}
        if(Now-Changed<1)return false;auto R=FHatchLabelProbe::Inspect(*Test,M,FParse::Param(FCommandLine::Get(),TEXT("HatchBadgeBaseline")));
        TArray<FColor> Pixels;FIntVector Size;if(!Test->TestTrue(TEXT("actual D3D12 client screenshot"),FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return true;
        if(!FParse::Param(FCommandLine::Get(),TEXT("HatchBadgeBaseline")))FHatchLabelProbe::Raster(*Test,M,Pixels,Size);
        const TCHAR* Names[]={TEXT("top"),TEXT("oblique"),TEXT("opposite"),TEXT("top-max"),TEXT("points")};const FString Path=LabelRoot()/TEXT("captures")/FString(Names[State]);IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Path+TEXT(".png")));
        R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);R->SetNumberField(TEXT("font_percent"),M.GetFontScale()*100);R->SetStringField(TEXT("captured_at_utc"),FDateTime::UtcNow().ToIso8601());FString Commit;FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),Commit);R->SetStringField(TEXT("commit"),Commit);R->SetStringField(TEXT("kind"),TEXT("automated D3D12, not direct Computer Use"));FString JSON;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON));FFileHelper::SaveStringToFile(JSON,*(Path+TEXT(".json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        if(++State<5){FHatchLabelProbe::View(M,State);Changed=Now;return false;}return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLabelCapture,"HATCHLABELS.CapturesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLabelCapture::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FLabelRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLabelRhi,"MA0T10.Hatch.SensorLabelAnchorsRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLabelRhi::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FLabelRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
