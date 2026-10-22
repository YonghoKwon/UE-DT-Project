#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorSemanticLabels.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "EngineGlobals.h"
#include "Misc/App.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraActor.h"
#include "PhysicsEngine/BodySetup.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ShaderCompiler.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
FString VariantRoot(){FString P;FParse::Value(FCommandLine::Get(),TEXT("CoilReport="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/COIL-01c/variants"):P/TEXT("variants");}
void VariantJson(const FString& Name,TSharedRef<FJsonObject> O){FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*VariantRoot(),true);FFileHelper::SaveStringToFile(Text,*(VariantRoot()/Name));}
class FCoilVariantRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;bool Stress;int32 Step=0;double Began=FPlatformTime::Seconds(),Changed=0,Last=0,MeasureStart=0;int64 StartFrame=0;TArray<double> Wall,Engine,Game;
    TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();
    void Capture(UWorld& W,const FString& Name)
    {
        TArray<FColor> Pixels;FIntVector Size;if(!Test->TestTrue(TEXT("actual HR/CR or stress RHI pixels"),FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;
        TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);const FString Dir=VariantRoot()/TEXT("captures");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveArrayToFile(Bytes,*(Dir/(Name+TEXT(".png"))));
        const FString Progress=FPaths::ProjectSavedDir()/TEXT("Reports/COIL-01c/progress")/(Name+TEXT(".png"));FFileHelper::SaveArrayToFile(Bytes,*Progress);
        Report->SetNumberField(TEXT("client_width"),Size.X);Report->SetNumberField(TEXT("client_height"),Size.Y);
    }
    void Camera(ACoilLoadingMonitorHost& H,FVector Position,FVector Target)
    {const auto Pose=H.Simulation->GetActorTransform();H.ViewCamera->SetActorLocationAndRotation(Pose.TransformPosition(Position*100),Pose.GetRotation()*(Target-Position).Rotation().Quaternion());}
public:
    FCoilVariantRun(FAutomationTestBase* T,bool InStress):Test(T),Stress(InStress){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Began>240){Test->AddError(TEXT("coil variants timeout"));return true;}
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())
        {if(Stress&&Step==2){Test->AddError(TEXT("shader compilation entered the measurement window"));return true;}Changed=Now;return false;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        ACoilLoadingMonitorHost* H=nullptr;for(TActorIterator<ACoilLoadingMonitorHost> I(W);I;++I)H=*I;if(!H||!H->Simulation||!H->Monitor)return false;auto& S=*H->Simulation;
        if(Step==0)
        {
            IFileManager::Get().MakeDirectory(*VariantRoot(),true);
            const double SetupStart=FPlatformTime::Seconds();
            if(Stress)
            {
                for(int Row=1;Row<=15;++Row){S.NextTier=1;S.NextRow=Row;S.NextColumn=1;S.NextSpec=CoilLoading::FSpec::Representative(Row%2?CoilLoading::ECoilType::HotRolled:CoilLoading::ECoilType::ColdRolled);S.NextSpec.CoilId=FString::Printf(TEXT("ROW%02d-BASE"),Row);Test->TestTrue(TEXT("new18.8m hold full row has11 coils"),S.AddTier());}
                Test->TestEqual(TEXT("15 complete base rows =165"),S.Plan().Num(),165);
                for(int Row=1;Row<=13;++Row){S.NextTier=2;S.NextRow=Row;S.NextColumn=1;S.NextSpec=CoilLoading::FSpec::Representative(Row%2?CoilLoading::ECoilType::HotRolled:CoilLoading::ECoilType::ColdRolled);S.NextSpec.CoilId=FString::Printf(TEXT("ROW%02d-UPPER"),Row);Test->TestTrue(TEXT("supported second tier full row has10 coils"),S.AddTier());}
                S.NextTier=2;S.NextRow=14;S.NextColumn=1;S.NextSpec=CoilLoading::FSpec::Representative(CoilLoading::ECoilType::ColdRolled);S.NextSpec.CoilId=TEXT("ROW14-PARTIAL");for(int I=0;I<5;++I)Test->TestTrue(TEXT("partial second row"),S.PlaceNext(false));
                const auto Layout=CoilLoading::Analyze(S.Settings,S.Plan());Test->TestFalse(TEXT("all300 fit the smaller actual hold"),Layout.Issues.ContainsByPredicate([](const CoilLoading::FIssue& I){return I.Code==TEXT("HoldInterference")||I.Code==TEXT("UnsupportedKey");}));
                Test->TestEqual(TEXT("300 mixed coils no artificial count/tier limit"),S.Plan().Num(),300);H->Monitor->SyncInputs();H->SetView(0);Report->SetNumberField(TEXT("setup_seconds"),FPlatformTime::Seconds()-SetupStart);Changed=FPlatformTime::Seconds();Step=1;return false;
            }
            S.NextSpec=CoilLoading::FSpec::Representative(CoilLoading::ECoilType::HotRolled);S.NextSpec.CoilId=TEXT("HR-DEMO-01");Test->TestTrue(TEXT("HR placed"),S.PlaceNext(false));
            S.NextSpec=CoilLoading::FSpec::Representative(CoilLoading::ECoilType::ColdRolled);S.NextSpec.CoilId=TEXT("CR-DEMO-02");Test->TestTrue(TEXT("CR placed with gross package dimensions"),S.PlaceNext(false));
            S.NextTier=2;S.NextColumn=1;S.NextSpec.CoilId=TEXT("CR-KEY-03");Test->TestTrue(TEXT("mixed HR/CR key geometry uses gross diameter"),S.PlaceNext(false));
            S.NextTier=1;S.NextRow=2;S.NextColumn=1;S.NextSpec=CoilLoading::FSpec::Representative(CoilLoading::ECoilType::HotRolled);S.NextSpec.CoilId=TEXT("NEXT-HR-04");S.RefreshPreview();H->Monitor->SyncInputs();H->Focus(1);S.Ghost->SetActorHiddenInGame(true);Camera(*H,{S.Plan()[0].Center.X-4.655,S.Plan()[0].Center.Y-2.8,4.2},{S.Plan()[0].Center.X,S.Plan()[0].Center.Y+.8,1.55});
            int Coils=0;for(TActorIterator<ACoilLoadingBody> I(W);I;++I)if(I->ActorHasTag(TEXT("Coil")))
            {++Coils;Test->TestEqual(TEXT("lap/strap/cover detail never adds collision"),I->Detail->GetCollisionEnabled(),ECollisionEnabled::NoCollision);Test->TestEqual(TEXT("ID label never adds measurement collision"),I->Label->GetCollisionEnabled(),ECollisionEnabled::NoCollision);Test->TestEqual(TEXT("dynamic annulus retains32 hollow convex pieces"),I->Body->GetBodySetup()->AggGeom.ConvexElems.Num(),32);}
            Test->TestTrue(TEXT("real HR and CR measured actors present"),Coils>=3);Test->TestEqual(TEXT("existingSlab semanticID remains2"),int32(VirtualSensorSemanticLabels::ClassId(TEXT("Slab"))),2);Test->TestEqual(TEXT("Coil appended as7, old class IDs stable"),int32(VirtualSensorSemanticLabels::ClassId(TEXT("Coil"))),7);
            Report->SetNumberField(TEXT("hr_mass_t"),CoilLoading::FSpec::Representative(CoilLoading::ECoilType::HotRolled).GrossMass());Report->SetNumberField(TEXT("cr_net_mass_t"),CoilLoading::FSpec::Representative(CoilLoading::ECoilType::ColdRolled).GrossMass());Report->SetStringField(TEXT("packaging_geometry"),TEXT("synthetic input: radial1.5mm / each end2mm; not supplier standard. Packaging gross mass and friction unconfirmed."));Changed=Now;Step=1;return false;
        }
        if(Stress)
        {
            if(Step==1){if(Now-Changed<10)return false;MeasureStart=Last=Now;StartFrame=S.Lidar->GetSensorRuntimeStatus().FrameId;Step=2;return false;}
            Wall.Add((Now-Last)*1000);Engine.Add(FApp::GetDeltaTime()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Last=Now;if(Now-MeasureStart<30)return false;
            const auto Mean=[](const TArray<double>& V){double Sum=0;for(double X:V)Sum+=X;return V.Num()?Sum/V.Num():0;};
            auto Sorted=Engine;Sorted.Sort();const auto Status=S.Lidar->GetSensorRuntimeStatus();
            Report->SetNumberField(TEXT("average_fps"),1000/Mean(Engine));Report->SetNumberField(TEXT("callback_average_fps"),1000/Mean(Wall));Report->SetNumberField(TEXT("frame_p95_ms"),Sorted[FMath::FloorToInt((Sorted.Num()-1)*.95)]);Report->SetNumberField(TEXT("game_thread_mean_ms"),Mean(Game));Report->SetNumberField(TEXT("acquisition_hz"),(Status.FrameId-StartFrame)/(Now-MeasureStart));Report->SetStringField(TEXT("backend"),Status.ActiveAcquisitionBackend);Report->SetNumberField(TEXT("warmup_seconds"),10);Report->SetNumberField(TEXT("measurement_seconds"),Now-MeasureStart);Report->SetNumberField(TEXT("coils"),S.Plan().Num());
            Test->TestTrue(TEXT("stress actually acquires HWRT frames"),Status.FrameId>StartFrame&&Status.ActiveAcquisitionBackend==TEXT("hardware_ray_tracing"));
            int32 Instances=0;TInlineComponentArray<UInstancedStaticMeshComponent*> Batch(&S);for(auto* B:Batch)if(B->ComponentHasTag(TEXT("CoilSettledBatch")))Instances+=B->GetInstanceCount();Test->TestTrue(TEXT("hundreds of bodies and details render by instancing"),Instances>=600);Report->SetNumberField(TEXT("render_instances"),Instances);
            FString Csv=TEXT("callback_ms,engine_ms,game_thread_ms\n");for(int I=0;I<Engine.Num();++I)Csv+=FString::Printf(TEXT("%.6f,%.6f,%.6f\n"),Wall[I],Engine[I],Game[I]);FFileHelper::SaveStringToFile(Csv,*(VariantRoot()/TEXT("stress-300-raw.csv")));Capture(*W,TEXT("coil-300-mixed"));Report->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());VariantJson(TEXT("stress-300.json"),Report);return true;
        }
        if(Now-Changed<1.5)return false;
        if(Step==1){Capture(*W,TEXT("coil-hr-vs-cr"));Camera(*H,{S.Plan()[0].Center.X-3.455,S.Plan()[0].Center.Y-2,2.4},S.Plan()[0].Center);Step=2;Changed=Now;return false;}
        if(Step==2){Capture(*W,TEXT("coil-hr-close"));Camera(*H,{S.Plan()[1].Center.X-3.455,S.Plan()[1].Center.Y-2,S.Plan()[1].Center.Z+1.55},S.Plan()[1].Center);Step=3;Changed=Now;return false;}
        Capture(*W,TEXT("coil-cr-close"));Report->SetStringField(TEXT("scope"),TEXT("actual D3D12, same lighting/FOV/distance for HR and CR closeups, separate from mouse acceptance"));Report->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());VariantJson(TEXT("hr-cr.json"),Report);return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilVariants,"MA0T10.CoilLoading.HRCRVisualRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilVariants::RunTest(const FString&){if(!GDynamicRHI){AddError(TEXT("HR/CR visual evidence requires RHI"));return false;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilVariantRun(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilStress,"MA0T10.CoilLoading.HundredsRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilStress::RunTest(const FString&){if(!GDynamicRHI){AddError(TEXT("hundreds of coils requires RHI"));return false;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilVariantRun(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
