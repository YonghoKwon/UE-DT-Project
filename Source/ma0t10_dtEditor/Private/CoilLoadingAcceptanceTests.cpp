#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "Widgets/SWidget.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"

namespace
{
FString CoilRoot(){FString P;FParse::Value(FCommandLine::Get(),TEXT("CoilReport="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/COIL-01d/acceptance"):P;}
void CoilJson(const FString& Name,TSharedRef<FJsonObject> O){FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*CoilRoot(),true);FFileHelper::SaveStringToFile(Text,*(CoilRoot()/Name));}
class FCoilRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int32 Step=0,SavedPlanCount=0;double Began=FPlatformTime::Seconds(),Changed=0;TSharedRef<FJsonObject> Evidence=MakeShared<FJsonObject>();
    void Capture(UWorld& W,const FString& Name)
    {
        TArray<FColor> Pixels;FIntVector Size;if(!Test->TestTrue(TEXT("actual RHI viewport screenshot"),FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);const FString Dir=CoilRoot()/TEXT("captures");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveArrayToFile(Png,*(Dir/(Name+TEXT(".png"))));
        const FString Progress=FPaths::ProjectSavedDir()/TEXT("Reports/COIL-01d/progress");IFileManager::Get().MakeDirectory(*Progress,true);FFileHelper::SaveArrayToFile(Png,*(Progress/(Name+TEXT(".png"))));
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("scope"),TEXT("actual D3D12 automated capture, separate from direct mouse acceptance"));O->SetNumberField(TEXT("width"),Size.X);O->SetNumberField(TEXT("height"),Size.Y);if(auto Window=W.GetGameViewport()->GetWindow())O->SetNumberField(TEXT("window_dpi_scale"),Window->GetDPIScaleFactor());O->SetStringField(TEXT("font_scope"),TEXT("owned native sizes11-18pt; default scale; personal appearance save not changed"));CoilJson(TEXT("captures/")+Name+TEXT(".json"),O);
    }
public:
    explicit FCoilRun(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Began>200){Test->AddError(TEXT("COIL-01 RHI acceptance timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        ACoilLoadingMonitorHost* H=nullptr;for(TActorIterator<ACoilLoadingMonitorHost> I(W);I;++I)H=*I;if(!H||!H->Simulation||!H->Monitor||!H->Simulation->Lidar)return false;
        auto& S=*H->Simulation;auto& M=*H->Monitor;
        if(!Changed){Changed=Now;return false;}if(Now-Changed<1)return false;
        if(Step==0)
        {
            Test->TestTrue(TEXT("default placement has zero actual dangers"),M.DangerCount==0);Test->TestTrue(TEXT("missing data counted separately and collapsed"),M.DataNeededCount>0&&!M.DataExpanded());
            const auto Stream=H->Coordinator->StreamPublisherComponent->GetEffectiveStreamConfig(EVirtualSensorStreamKind::PointCloud,S.Lidar->GetSensorId());Test->TestTrue(TEXT("owned sensor keeps default on binary PCD ConnectedNoLoss"),Stream.bEnabled&&Stream.PointCloudFormat==EVirtualPointCloudStreamFormat::PCD&&Stream.PcdDataMode==EVirtualPcdDataMode::Binary&&Stream.DeliveryMode==EVirtualPointCloudDeliveryMode::ConnectedNoLoss);
            Evidence->SetBoolField(TEXT("pcd_default_on"),Stream.bEnabled);Evidence->SetStringField(TEXT("transport_scope"),TEXT("binary PCD / ConnectedNoLoss route configured; this scene test has no connected receiver or consumer ACK acceptance"));
            Capture(*W,TEXT("01-context-ref1-ref3"));S.NextSpec.OD=1.8;S.RefreshPreview();Test->TestTrue(TEXT("spec edit changes mass and ghost dimensions"),S.NextSpec.GrossMass()>19&&S.Ghost->Body->Bounds.BoxExtent.Y>89);
            S.NextSpec=CoilLoading::FSpec();S.RefreshPreview();Test->TestTrue(TEXT("warning does not block animated placement"),S.PlaceNext());Step=1;Changed=Now;return false;
        }
        if(Step==1){Capture(*W,TEXT("02-lift-ref2-sling"));Step=2;Changed=Now;return false;}
        if(Step==2)
        {
            if(S.Phase()!=ECoilLoadingPhase::Idle)return false;
            Test->TestEqual(TEXT("one physical landing committed"),S.Plan().Num(),1);Test->TestTrue(TEXT("actual settling duration recorded"),S.Physics().SettlingSeconds>0);
            Evidence->SetStringField(TEXT("settling_state"),S.Physics().State);Evidence->SetNumberField(TEXT("settling_s"),S.Physics().SettlingSeconds);Evidence->SetNumberField(TEXT("settling_displacement_m"),S.Physics().MaxDisplacement);Capture(*W,TEXT("03-settling-vs-analysis"));
            S.MotionDuration=20;S.RollDegrees=10;S.PitchDegrees=5;Test->TestTrue(TEXT("actual Chaos motion starts"),S.StartMotionTest());Step=3;Changed=Now;return false;
        }
        if(Step==3)
        {
            if(S.Phase()!=ECoilLoadingPhase::Idle)return false;Test->TestTrue(TEXT("motion result not unrun"),S.Physics().State!=TEXT("미실행"));
            Evidence->SetNumberField(TEXT("motion_displacement_m"),S.Physics().MaxDisplacement);Evidence->SetNumberField(TEXT("motion_rotation_deg"),S.Physics().MaxRotationDegrees);Evidence->SetBoolField(TEXT("motion_anomaly"),S.Physics().bAnomaly);
            Capture(*W,TEXT("04-motion-vs-analysis"));Test->TestTrue(TEXT("undo removes last placement"),S.Undo());Test->TestEqual(TEXT("undo exact count"),S.Plan().Num(),0);
            Test->TestTrue(TEXT("repeat placement starts"),S.PlaceNext());S.CancelOperation();Test->TestTrue(TEXT("cancel owns suspended body and exits idle"),!S.Suspended&&S.Phase()==ECoilLoadingPhase::Idle&&S.Plan().Num()==0);
            for(int Tier=0;Tier<4;++Tier)Test->TestTrue(TEXT("continue stacking through fourth tier"),S.AddTier());
            Test->TestTrue(TEXT("no arbitrary three tier cap"),S.Plan().Num()>30&&S.Plan().Last().Tier==4);
            SavedPlanCount=S.Plan().Num();Test->TestTrue(TEXT("plan save uses existing asynchronous FileSave service"),S.RequestPlanSave());Step=31;Changed=Now;return false;
        }
        if(Step==31)
        {
            if(S.IsPlanSavePending())return false;if(!Test->TestTrue(TEXT("asynchronous plan file completed"),!S.LastPlanSavePath.IsEmpty()))return true;
            Test->TestTrue(TEXT("new placement"),S.PlaceNext(false));Test->TestTrue(TEXT("plan load queued asynchronously"),S.LoadPlan(S.LastPlanSavePath));Evidence->SetStringField(TEXT("plan_json"),S.LastPlanSavePath);Step=32;Changed=Now;return false;
        }
        if(Step==32)
        {
            if(S.IsPlanLoadPending())return false;Test->TestEqual(TEXT("asynchronous load restores exact count"),S.Plan().Num(),SavedPlanCount);
            auto* Save=NewObject<UCoilLoadingSaveGame>();Save->PlanJson=CoilLoading::PlanToJson(S.Settings,S.Plan());TArray<uint8> Bytes;Test->TestTrue(TEXT("SaveGame serialization"),UGameplayStatics::SaveGameToMemory(Save,Bytes));auto* Restored=Cast<UCoilLoadingSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));Test->TestTrue(TEXT("SaveGame exact roundtrip"),Restored&&Restored->PlanJson==Save->PlanJson);
            M.SelectView(1);Step=4;Changed=Now;return false;
        }
        if(Step==4){Capture(*W,TEXT("05-hold-interior-stacking"));M.SelectView(2);Step=5;Changed=Now;return false;}
        if(Step==5){Capture(*W,TEXT("06-cross-section-four-tiers"));M.SelectView(3);Step=6;Changed=Now;return false;}
        if(Step==6){Capture(*W,TEXT("07-top-stowage-plan"));S.NextSpec.MassOverride=45;S.Settings.ApprovedPatchArea=.5;S.Settings.Ay=5;S.RefreshPreview();M.SyncInputs();M.SelectView(1);Step=7;Changed=Now;return false;}
        if(Step==7){Test->TestTrue(TEXT("actual exceedance visible above missing data"),M.WarningText().Contains(TEXT("위험"))&&M.DangerCount>0&&!M.WarningText().Contains(TEXT("자료 필요")));Capture(*W,TEXT("08-warning-example"));M.SelectView(4);Step=8;Changed=Now;return false;}
        if(Step==8)
        {
            Capture(*W,TEXT("09-point-cloud"));const auto Status=S.Lidar->GetSensorRuntimeStatus();Test->TestTrue(TEXT("real sensor frames acquired"),Status.FrameId>0);Test->TestTrue(TEXT("coil semantic points acquired"),S.LidarCoilHits>0);Test->TestTrue(TEXT("sensor-owned HWRT active"),Status.ActiveAcquisitionBackend.Contains(TEXT("Gpu"),ESearchCase::IgnoreCase)||Status.ActiveAcquisitionBackend.Contains(TEXT("Hardware"),ESearchCase::IgnoreCase));
            Test->TestTrue(TEXT("normal restored placement cylinder fitting observes centers"),S.bLidarCompared&&S.FittedCoilCount>0);Test->TestTrue(TEXT("normal center deviation p95<=3cm, strict fixture"),S.bLidarCompared&&S.LidarDeviationP95<=.03);
            Evidence->SetStringField(TEXT("backend"),Status.ActiveAcquisitionBackend);Evidence->SetStringField(TEXT("query_diagnostic"),S.QueryDiagnostic);Evidence->SetStringField(TEXT("backend_note"),Status.AcquisitionBackendMessage);Evidence->SetNumberField(TEXT("coil_hits"),S.LidarCoilHits);Evidence->SetNumberField(TEXT("acquisition_hz"),Status.MeasuredAcquisitionRateHz);Evidence->SetNumberField(TEXT("frame_id"),Status.FrameId);Evidence->SetNumberField(TEXT("deviation_p95_m"),S.LidarDeviationP95);Evidence->SetNumberField(TEXT("coil_count"),S.Plan().Num());Evidence->SetNumberField(TEXT("convex_segments"),32);Evidence->SetStringField(TEXT("chaos_scope"),TEXT("rigid annulus 32convex, label gross mass, min friction, restitution0, solver12/4; equivalent forces, no structural deformation or certified lashing"));Evidence->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());CoilJson(TEXT("acceptance.json"),Evidence);return true;
        }
        return false;
    }
};
class FCoilAsyncReview final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int32 Step=0;double Start=FPlatformTime::Seconds(),Changed=0;
    FString Path=CoilRoot()/TEXT("review-async-plan.json"),Slot=TEXT("CoilLoading_Review_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
public:
    explicit FCoilAsyncReview(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Start>45){Test->AddError(TEXT("R7 async load timeout"));UGameplayStatics::DeleteGameInSlot(Slot,0);return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        ACoilLoadingSimulation* S=nullptr;for(TActorIterator<ACoilLoadingSimulation> I(W);I;++I)S=*I;if(!S)return false;
        if(!Changed){Changed=Now;return false;}if(Step==0&&Now-Changed<1)return false;
        if(Step==0)
        {
            Test->TestTrue(TEXT("fixture first placement"),S->PlaceNext(false));IFileManager::Get().MakeDirectory(*CoilRoot(),true);Test->TestTrue(TEXT("small owned fixture write"),FFileHelper::SaveStringToFile(CoilLoading::PlanToJson(S->Settings,S->Plan()),*Path));
            Test->TestTrue(TEXT("second placement"),S->PlaceNext(false));Test->TestTrue(TEXT("file load returns accepted request"),S->LoadPlan(Path));Test->TestTrue(TEXT("request is pending before application"),S->IsPlanLoadPending());Test->TestEqual(TEXT("file not applied synchronously"),S->Plan().Num(),2);S->CancelOperation();Step=1;Changed=Now;return false;
        }
        if(Now-Changed<.5)return false;
        if(Step==1)
        {Test->TestEqual(TEXT("cancelled generation cannot apply late result"),S->Plan().Num(),2);S->LoadPlan(Path);S->NextSpec.OD=1.7;S->RefreshPreview();Step=2;Changed=Now;return false;}
        if(Step==2)
        {if(S->IsPlanLoadPending())return false;Test->TestEqual(TEXT("edited revision rejects stale file result"),S->Plan().Num(),2);Test->TestTrue(TEXT("valid load request"),S->LoadPlan(Path));Step=3;Changed=Now;return false;}
        if(Step==3)
        {if(S->IsPlanLoadPending())return false;Test->TestEqual(TEXT("background parse applied on game thread"),S->Plan().Num(),1);Test->TestTrue(TEXT("unique review slot async save"),S->SaveSlot(Slot));Step=4;Changed=Now;return false;}
        if(Step==4)
        {if(S->IsSlotSavePending())return false;Test->TestTrue(TEXT("change before slot load"),S->PlaceNext(false));Test->TestTrue(TEXT("AsyncLoadGameFromSlot request"),S->LoadSlot(Slot));Test->TestEqual(TEXT("slot not applied synchronously"),S->Plan().Num(),2);Step=5;Changed=Now;return false;}
        if(Step==5)
        {if(S->IsPlanLoadPending())return false;Test->TestEqual(TEXT("slot JSON parsed off-thread and applied"),S->Plan().Num(),1);UGameplayStatics::DeleteGameInSlot(Slot,0);S->LoadPlan(Path+TEXT(".missing"));Step=6;Changed=Now;return false;}
        if(Step==6)
        {
            if(S->IsPlanLoadPending())return false;Test->TestTrue(TEXT("async read failure reports file field"),S->Status.Contains(TEXT("file:")));Test->TestEqual(TEXT("failed load remains atomic"),S->Plan().Num(),1);
            S->LoadPlan(Path);S->LoadPlan(Path+TEXT(".missing"));Step=7;Changed=Now;return false;
        }
        if(Step==7)
        {
            if(S->IsPlanLoadPending())return false;Test->TestTrue(TEXT("latest generation owns completion"),S->Status.Contains(TEXT("file:")));Test->TestEqual(TEXT("superseded request not applied"),S->Plan().Num(),1);
            auto Evidence=MakeShared<FJsonObject>();Evidence->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());Evidence->SetStringField(TEXT("checks"),TEXT("deferred file/slot apply, cancel, revision change, supersede, read failure atomicity, unique slot cleaned; worker check enforces off-game-thread parse"));CoilJson(TEXT("review-r7-async.json"),Evidence);
            S->LoadPlan(Path);return true; // EndPlay invalidates this in-flight generation.
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilAsyncReviewTest,"MA0T10.CoilLoading.IndependentReview.R7AsyncPlanLoad",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilAsyncReviewTest::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilAsyncReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.5f));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilRhi,"MA0T10.CoilLoading.RhiWorkflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilRhi::RunTest(const FString&)
{
    if(!GDynamicRHI){AddError(TEXT("COIL-01 physics/UI acceptance requires actual D3D12 RHI"));return false;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
