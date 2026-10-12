#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "EngineUtils.h"
#include "RenderTimer.h"
#include "Misc/App.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorPlacementInteraction.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "ma0t10_dt/MA0T10/Player/DxPlayerControllerTest.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "UxrScenarioUiFixture.h"

/** Identical test-only instrument on both production revisions. No transmission or local frame saving. */
class FUxrPlacementPerformance : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Created=FPlatformTime::Seconds(),Started=0,Last=0;
    TArray<double> Frames,GameThreads,EngineFrames;
    TArray<TWeakObjectPtr<AVirtualSensorActorBase>> Sensors;
public:
    explicit FUxrPlacementPerformance(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Created>100){Test->AddError(TEXT("UXR performance fixture timeout"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World)||FPlatformTime::Seconds()-Created<3)return false;
        const double Now=FPlatformTime::Seconds();
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();if(!C)return false;
        if(!Started)
        {
            int32 Cameras=0,Lidars=0;
            for(auto* A:C->GetSensorActors())if(A){Sensors.Add(A);Cameras+=A->GetSensorKind()==EVirtualSensorKind::Camera;Lidars+=A->GetSensorKind()==EVirtualSensorKind::Lidar;}
            if(Cameras!=1||Lidars!=1){Test->AddError(TEXT("UXR comparison requires exactly one Camera and one LiDAR"));return true;}
            for(const auto& Weak:Sensors)if(auto* A=Weak.Get())
            {
                A->EndInteractiveManipulation();A->StopSensor();FVirtualSensorEditableState S,Applied;A->ReadEditableState(S);
                S.SimulationQuality=EVirtualSensorSimulationQuality::FullSpec;
                S.CameraProfile=EVirtualCameraDeviceProfile::IntelRealSenseD455;S.LidarProfile=EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;
                FString Error;if(!A->ApplyProfileAndSimulationQuality(S,Applied,Error)){Test->AddError(Error);return true;}A->StartSensor();
            }
            W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(1);
            const FString PerfView=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_PERF_VIEW"));
            if(PerfView==TEXT("scenario")||PerfView==TEXT("scenario-monitor"))
            {
                W->EnsureOwnedReplayPanel();
                for(const auto Role:{ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Placement,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::SlabCharts})W->SetPanelOpen(Role,false);
                W->SetPanelOpen(ESensorToolPanelRole::SlabProgress,true);W->SetPanelOpen(ESensorToolPanelRole::Replay,true);
                W->SetPanelOpen(ESensorToolPanelRole::Monitor,PerfView==TEXT("scenario-monitor"));
                ASlabActor* Slab=nullptr;for(TActorIterator<ASlabActor> It(World);It;++It){Slab=*It;break;}
                if(!StartUxrScenarioUiFixture(Slab)){Test->AddError(TEXT("Scenario UI performance input rejected"));return true;}
            }
            if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_PERF_VIEW"))==TEXT("transfer"))
            {
                for(const auto Role:{ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Placement,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabCharts,ESensorToolPanelRole::SlabProgress})W->SetPanelOpen(Role,false);
                W->SetPanelOpen(ESensorToolPanelRole::Data,true);
                auto* Data=Cast<UVirtualSensorCaptureExportPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Data));
                if(!Data){Test->AddError(TEXT("Transfer performance requires the owned data panel"));return true;}
                Data->OpenOwnedTab(ESensorOwnedDataTab::Transfer);
            }
            Test->AddInfo(TEXT("Actual client 1920x1080; 1 Camera + 1 LiDAR; full profiles; preview/placement open; 10s warmup + 30s wall-clock sampling"));
            Started=Now;Last=Now;return false;
        }
        const double Elapsed=Now-Started;
        if(Elapsed>=10&&Elapsed<40){Frames.Add((Now-Last)*1000);EngineFrames.Add(FApp::GetDeltaTime()*1000);GameThreads.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));}
        Last=Now;if(Elapsed<40)return false;
        for(const auto& A:Sensors)Test->TestTrue(TEXT("both devices remain measuring"),A.IsValid()&&A->IsSensorRunning()&&A->GetSensorRuntimeStatus().FrameId>0);
        if(Frames.IsEmpty()){Test->AddError(TEXT("No performance samples"));return true;}
        double Total=0,GtTotal=0;for(double V:Frames)Total+=V;for(double V:GameThreads)GtTotal+=V;
        Frames.Sort();EngineFrames.Sort();GameThreads.Sort();auto Json=MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("average_fps"),Frames.Num()*1000.0/Total);Json->SetNumberField(TEXT("frame_p95_ms"),Frames[FMath::Clamp(FMath::CeilToInt(Frames.Num()*.95)-1,0,Frames.Num()-1)]);
        Json->SetNumberField(TEXT("game_thread_mean_ms"),GtTotal/GameThreads.Num());Json->SetNumberField(TEXT("game_thread_p95_ms"),GameThreads[FMath::Clamp(FMath::CeilToInt(GameThreads.Num()*.95)-1,0,GameThreads.Num()-1)]);
        Json->SetNumberField(TEXT("samples"),Frames.Num());Json->SetStringField(TEXT("timing"),TEXT("automation callback wall pacing; engine GGameThreadTime cycles separately"));
        Json->SetNumberField(TEXT("engine_frame_p95_ms"),EngineFrames[FMath::Clamp(FMath::CeilToInt(EngineFrames.Num()*.95)-1,0,EngineFrames.Num()-1)]);
        Json->SetStringField(TEXT("engine_frame_timing"),TEXT("FApp::GetDeltaTime; includes engine pacing"));
        Json->SetStringField(TEXT("view"),FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_PERF_VIEW")));
        Json->SetStringField(TEXT("actual_client"),TEXT("1920x1080"));Json->SetNumberField(TEXT("warmup_seconds"),10);Json->SetNumberField(TEXT("measurement_seconds"),30);
        FString Output;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Output));
        const FString Name=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_PERF_NAME"));
        const FString ReportRoot=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
        const FString Path=(ReportRoot.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-02/performance"):ReportRoot/TEXT("performance"))/(Name+TEXT(".json"));
        Test->TestTrue(TEXT("performance evidence written"),FFileHelper::SaveStringToFile(Output,*Path));Test->AddInfo(Output);return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrPlacementPerformanceTest,"UXR02.Performance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrPlacementPerformanceTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_PERF"))!=TEXT("1")){AddInfo(TEXT("SKIP explicit UXR performance comparison only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrPlacementPerformance(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

class FUxrPlacementOnlyWorkflow : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Created=FPlatformTime::Seconds();int32 Step=0;
    TWeakObjectPtr<AVirtualSensorActorBase> Added;
    TStrongObjectPtr<UVirtualSensorSettingsPanelWidget> BlueprintControls;
    FString PoseRequest,LayoutRequest,LayoutName=TEXT("UXR02-")+FGuid::NewGuid().ToString();FTransform SavedPose;
public:
    explicit FUxrPlacementOnlyWorkflow(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Created>40){Test->AddError(TEXT("placement-only workflow timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!World)return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* P=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement));auto* C=W->GetCoordinator();if(!P||!C)return false;
        auto* Poses=World->GetSubsystem<UVirtualSensorPlacementSubsystem>();auto* Layout=World->GetSubsystem<UVirtualSensorLayoutSubsystem>();
        if(Step==0)
        {
            if(FPlatformTime::Seconds()-Created<3)return false;
            const FString Root=FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-02/workflow")/FGuid::NewGuid().ToString();Poses->SetAutomationStorageRoot(Root/TEXT("poses"));Layout->StorageRootOverrideForTests=Root/TEXT("layouts");
            if(auto* S=W->GetOwnedPanel(ESensorToolPanelRole::Settings)){S->RemoveFromParent();W->UnregisterPanel(S);}
            Test->TestNull(TEXT("no settings panel required"),W->GetOwnedPanel(ESensorToolPanelRole::Settings));
            if(!Test->TestTrue(TEXT("placement adds a camera"),P->AddSensor(EVirtualSensorKind::Camera)))return true;
            Added=C->GetSelectedSensorActor();P->SetSelectedManipulating(false);
            Test->TestTrue(TEXT("measurement stop via placement"),P->SetSelectedMeasuring(false)&&!Added->IsSensorRunning());
            Test->TestTrue(TEXT("measurement start via placement"),P->SetSelectedMeasuring(true)&&Added->IsSensorRunning());P->SetSelectedMeasuring(false);
            SavedPose=Added->GetActorTransform();PoseRequest=P->SaveCurrentPose(TEXT("A"));Step=1;return false;
        }
        if(Step==1)
        {
            const auto Status=Poses->GetPoseSaveStatus(PoseRequest);if(Status.State==EVirtualSensorPoseSaveState::Writing)return false;
            if(!Test->TestEqual(TEXT("placement bookmark saved"),Status.State,EVirtualSensorPoseSaveState::Saved))return true;
            Test->TestTrue(TEXT("placement bookmark applies"),P->MoveToBookmark(Status.BookmarkId));
            BlueprintControls.Reset(CreateWidget<UVirtualSensorSettingsPanelWidget>(World));W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,BlueprintControls.Get());BlueprintControls->BindSensorManager(C);
            Test->TestTrue(TEXT("settings BP API shares same gizmo"),BlueprintControls->GetTransformGizmoActor()==W->GetPlacementInteraction()->GetExistingGizmo());
            PoseRequest=BlueprintControls->RequestSaveSelectedSensorPose(TEXT("BP delegation"));LayoutRequest=P->SaveWholeLayout(LayoutName);Step=2;return false;
        }
        if(Step==2)
        {
            if(Poses->GetPoseSaveStatus(PoseRequest).State==EVirtualSensorPoseSaveState::Writing||Layout->GetOperationStatus(LayoutRequest).State==EVirtualSensorLayoutOperation::Writing)return false;
            Test->TestEqual(TEXT("settings BP save uses pose service"),Poses->GetPoseSaveStatus(PoseRequest).State,EVirtualSensorPoseSaveState::Saved);
            if(!Test->TestEqual(TEXT("whole layout saved via placement"),Layout->GetOperationStatus(LayoutRequest).State,EVirtualSensorLayoutOperation::Saved))return true;
            auto Other=SavedPose;Other.AddToTranslation(FVector(123,0,0));FString Reason;W->GetPlacementInteraction()->ApplyPose(Other,Reason);
            P->SelectLayoutForRestore(LayoutName);const FString Preview=P->PreviewSelectedLayoutRestore();
            Test->TestEqual(TEXT("placement restore preview ready"),Layout->GetOperationStatus(Preview).State,EVirtualSensorLayoutOperation::ReadyToApply);
            Test->TestTrue(TEXT("placement applies whole layout"),P->ApplySelectedLayoutRestore());
            Test->TestTrue(TEXT("saved position restored"),Added.IsValid()&&Added->GetActorTransform().Equals(SavedPose,.01));
            P->EndPlacement();if(BlueprintControls.IsValid()){BlueprintControls->RemoveFromParent();W->UnregisterPanel(BlueprintControls.Get());BlueprintControls.Reset();}return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrPlacementOnlyWorkflowTest,"MA0T10.UXR02.SettingsFreeWorkflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrPlacementOnlyWorkflowTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP actual RHI is required for placement creation"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrPlacementOnlyWorkflow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

class FUxrPlacementRender : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Created=FPlatformTime::Seconds(),ReadyAt=0;int32 Phase=0;bool Requested=false;
    FString Path;FDateTime OldTime;TArray<TWeakObjectPtr<AVirtualSensorActorBase>> OriginalSensors;
    TUniquePtr<TGuardValue<bool>> DebugGuard;
public:
    explicit FUxrPlacementRender(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Created>60){Test->AddError(TEXT("UXR-02 render fixture timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* P=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement));auto* C=W->GetCoordinator();if(!P||!C)return false;
        const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();const float Scale=Phase==1?1.5f:1.f;
        if(!ReadyAt)
        {
            if(!DebugGuard)DebugGuard=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(Scale);W->SetPanelOpen(ESensorToolPanelRole::Placement,true);
            if(Phase==2){for(auto* A:C->GetSensorActors()){OriginalSensors.Add(A);A->StopSensor();C->UnregisterSensorActor(A);}}
            if(Phase==3){for(const auto& A:OriginalSensors)if(A.IsValid())C->RegisterSensorActor(A.Get());Test->TestTrue(TEXT("manipulation evidence uses real generated equipment and long id"),P->AddSensor(EVirtualSensorKind::Camera));}
            ReadyAt=FPlatformTime::Seconds()+1.5;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        if(!Requested)
        {
            if(Phase==3)Test->TestTrue(TEXT("inventory/selection synchronization must not end manipulation"),W->GetPlacementInteraction()->IsManipulating());
            const FString Name=Phase<2?FString::Printf(TEXT("rendered-%dx%d-%d.png"),Size.X,Size.Y,FMath::RoundToInt(Scale*100)):Phase==2?TEXT("rendered-empty.png"):TEXT("rendered-manipulating.png");
            Path=FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-02")/Name;OldTime=IFileManager::Get().GetTimeStamp(*Path);FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());
            Test->AddInfo(FString::Printf(TEXT("UXR02 rendered actual client=%dx%d font=%d windowDpi=%.3f phase=%d; automated render, not mouse verification"),Size.X,Size.Y,FMath::RoundToInt(Scale*100),Window?Window->GetDPIScaleFactor():0,Phase));return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=OldTime)return false;
        ++Phase;if(Phase==(Size.X==1280?4:2)){P->EndPlacement();return true;}Requested=false;ReadyAt=0;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrPlacementRenderTest,"UXR02.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrPlacementRenderTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR02_RENDER"))!=TEXT("1")){AddInfo(TEXT("SKIP explicit UXR-02 render only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrPlacementRender(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

class FUxrPlacementManualWait : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Created=FPlatformTime::Seconds(),Next=0;bool Ready=false;
    TUniquePtr<TGuardValue<bool>> DebugGuard;
    TWeakObjectPtr<AWorldSettings> EditorSettings;
    TSubclassOf<AGameModeBase> OriginalMode;
public:
    explicit FUxrPlacementManualWait(FAutomationTestBase* T):Test(T)
    {
        // Existing project input assets, transient test configuration only. Never save/reconfigure a map.
        if(auto* World=GEditor->GetEditorWorldContext().World())
        {EditorSettings=World->GetWorldSettings();OriginalMode=EditorSettings->DefaultGameMode;EditorSettings->DefaultGameMode=LoadClass<AGameModeBase>(nullptr,TEXT("/Game/MA0T10/Core/BP_DxGameMode.BP_DxGameMode_C"));}
    }
    ~FUxrPlacementManualWait(){if(EditorSettings.IsValid())EditorSettings->DefaultGameMode=OriginalMode;}
    bool Update() override
    {
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(World&&FinalAcceptanceViewportReady(World))
        {
            if(EditorSettings.IsValid()){EditorSettings->DefaultGameMode=OriginalMode;EditorSettings.Reset();}
            auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
            if(!Ready){DebugGuard=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);const FString Root=FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-02/manual/fixtures");World->GetSubsystem<UVirtualSensorPlacementSubsystem>()->SetAutomationStorageRoot(Root/TEXT("poses"));World->GetSubsystem<UVirtualSensorLayoutSubsystem>()->StorageRootOverrideForTests=Root/TEXT("layouts");W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(1);Ready=true;Test->AddInfo(FString::Printf(TEXT("Manual PIE actual client %dx%d; actions must be performed through Computer Use; isolated pose/layout files"),FinalAcceptanceViewportSize().X,FinalAcceptanceViewportSize().Y));}
            if(FPlatformTime::Seconds()>=Next)
            {
                Next=FPlatformTime::Seconds()+.5;auto Json=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
                if(auto* PC=World->GetFirstPlayerController()){Json->SetStringField(TEXT("controller"),PC->GetClass()->GetPathName());Json->SetStringField(TEXT("pawn"),GetNameSafe(PC->GetPawn()));if(auto* Dx=Cast<ADxPlayerControllerBase>(PC))Json->SetStringField(TEXT("click_policy"),Dx->ClickActivationPolicy==EDxClickActivationPolicy::SingleRelease?TEXT("SingleRelease"):TEXT("DoublePress"));}
                if(auto* C=W->GetCoordinator())for(auto* A:C->GetSensorActors())if(A){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),A->GetSensorId());Row->SetStringField(TEXT("pose"),A->GetActorTransform().ToString());Row->SetBoolField(TEXT("selected"),C->GetSelectedSensorActor()==A);Row->SetBoolField(TEXT("measuring"),A->IsSensorRunning());Row->SetBoolField(TEXT("manipulating"),A->IsInteractiveManipulationActive());Rows.Add(MakeShared<FJsonValueObject>(Row));}
                Json->SetArrayField(TEXT("devices"),Rows);if(auto* P=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement)))Json->SetStringField(TEXT("placement_message"),P->GetPlacementStatusText());
                FString Output;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Output));FFileHelper::SaveStringToFile(Output,*(FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-02/manual/current-state.json")));
            }
        }
        if(IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-02/manual/finish.flag"))))return true;
        if(FPlatformTime::Seconds()-Created>1200){Test->AddError(TEXT("Manual verification expired"));return true;}return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrPlacementManualTest,"UXR02.Manual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrPlacementManualTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR02_MANUAL"))!=TEXT("1")){AddInfo(TEXT("SKIP explicit Computer Use only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrPlacementManualWait(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
