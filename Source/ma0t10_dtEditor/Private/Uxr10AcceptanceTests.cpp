#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/UI/SensorPlacementInteraction.h"
#include "ma0t10_dt/MA0T10/UI/Tests/SensorPresentationTestHelpers.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Debugging/SlateDebugging.h"
#include "Framework/Application/SlateUser.h"
#include "UxrScenarioUiFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUXR10NavigationTest,"MA0T10.UXR10.BackPathsPreserveState",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUXR10NavigationTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* C=World->SpawnActor<AVirtualSensorCoordinator>();auto* L=World->SpawnActor<AVirtualLidarSensorActor>();C->RegisterSensorActor(L);W->SetCoordinator(C);
    auto* P=NewObject<UVirtualSensorPlacementPanelWidget>(World);auto* S=NewObject<UVirtualSensorSettingsPanelWidget>(World);auto* D=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);
    auto* Progress=NewObject<USlabProgressPanelWidget>(World);auto* Replay=NewObject<USlabScenarioReplayPanelWidget>(World);auto* Charts=NewObject<USlabChartsPanelWidget>(World);
    W->RegisterOwnedPanel(ESensorToolPanelRole::Placement,P);W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,S);W->RegisterOwnedPanel(ESensorToolPanelRole::Data,D);
    W->RegisterOwnedPanel(ESensorToolPanelRole::SlabProgress,Progress);W->RegisterOwnedPanel(ESensorToolPanelRole::Replay,Replay);W->RegisterOwnedPanel(ESensorToolPanelRole::SlabCharts,Charts);
    auto* Bar=NewObject<USensorToolToolbarWidget>(World);W->Toolbar=Bar;S->BindSensorManager(C);D->BindSensorManager(C);C->SelectSensorActor(L);
    const auto Transform=L->GetActorTransform();const auto Revision=L->GetConfigurationRevision();const bool Running=L->IsSensorRunning();
    W->SetPanelOpen(ESensorToolPanelRole::Placement,true);W->SetPanelOpen(ESensorToolPanelRole::Settings,true);
    TestEqual(TEXT("equipment details return to placement"),S->GetPanelBackLabel().ToString(),SensorToolUiText::BackPlacement().ToString());
    S->PendingState.LidarScanInterval+=.123f;const auto Pending=S->GetPendingState();
    for(int32 Tab:{1,2})
    {
        S->SelectOwnedSettingsTab(Tab);TestEqual(TEXT("advanced sections expose their parent"),S->GetPanelBackLabel().ToString(),SensorToolUiText::BackSettings().ToString());
        TestTrue(TEXT("section Back succeeds"),S->NavigatePanelBack());
        TestTrue(TEXT("unsaved edits survive section Back"),FVirtualSensorEditableState::StaticStruct()->CompareScriptStruct(&Pending,&S->GetPendingState(),0));
    }
    TestTrue(TEXT("details Back succeeds"),S->NavigatePanelBack());TestTrue(TEXT("placement is visible"),W->IsPanelOpen(ESensorToolPanelRole::Placement));
    W->SetPanelOpen(ESensorToolPanelRole::Settings,true);S->PendingState.LidarScanInterval+=.123f;const auto CrossPanelPending=S->GetPendingState();
    W->SetPanelOpen(ESensorToolPanelRole::Data,true);TestTrue(TEXT("child data Back returns to settings"),D->NavigatePanelBack());
    TestTrue(TEXT("returning does not rebind and discard unsaved settings"),FVirtualSensorEditableState::StaticStruct()->CompareScriptStruct(&CrossPanelPending,&S->GetPendingState(),0));
    TestTrue(TEXT("nested return path then reaches placement"),S->NavigatePanelBack()&&W->IsPanelOpen(ESensorToolPanelRole::Placement));
    W->SetPanelOpen(ESensorToolPanelRole::SlabProgress,true);W->SetPanelOpen(ESensorToolPanelRole::Replay,true);
    const auto Outputs=Replay->GetReplayOutputs();TestTrue(TEXT("archive Back succeeds"),Replay->NavigatePanelBack());
    TestTrue(TEXT("archive Back returns to progress"),W->IsPanelOpen(ESensorToolPanelRole::SlabProgress));
    const auto AfterOutputs=Replay->GetReplayOutputs();
    TestTrue(TEXT("replay output selection unchanged"),Outputs.bPointCloud==AfterOutputs.bPointCloud&&Outputs.bCameraImage==AfterOutputs.bCameraImage&&Outputs.bLidarTelemetry==AfterOutputs.bLidarTelemetry);
    W->SetPanelOpen(ESensorToolPanelRole::SlabCharts,true);TestTrue(TEXT("charts Back returns to progress"),Charts->NavigatePanelBack()&&W->IsPanelOpen(ESensorToolPanelRole::SlabProgress));
    Bar->OpenOwnedDestination(ESensorToolDestination::PcdTransfer);D->OpenOwnedTab(ESensorOwnedDataTab::Results);D->OpenOwnedTab(ESensorOwnedDataTab::ServerConnection);D->OpenOwnedTab(ESensorOwnedDataTab::Diagnostics);
    for(auto Tab:{ESensorOwnedDataTab::ServerConnection,ESensorOwnedDataTab::Results,ESensorOwnedDataTab::Transfer}){TestTrue(TEXT("data Back succeeds"),D->NavigatePanelBack());TestTrue(TEXT("data Back restores previous tab"),D->GetOwnedTab()==Tab);}
    TestTrue(TEXT("primary data entry has no stale Back"),D->GetPanelBackLabel().IsEmpty());
    for(auto Route:{ESensorToolDestination::FileSave,ESensorToolDestination::ServerSettings,ESensorToolDestination::Diagnostics}){Bar->OpenOwnedDestination(Route);TestEqual(TEXT("tool destinations have tools parent"),D->GetPanelBackLabel().ToString(),SensorToolUiText::BackTools().ToString());}
    Bar->OpenOwnedDestination(ESensorToolDestination::Compatibility);TestTrue(TEXT("compatibility first returns to basic"),S->NavigatePanelBack());TestEqual(TEXT("basic then returns to tools"),S->GetPanelBackLabel().ToString(),SensorToolUiText::BackTools().ToString());
    Bar->OpenOwnedDestination(ESensorToolDestination::Scenario);W->SetPanelOpen(ESensorToolPanelRole::Replay,true);Bar->ToggleOwnedDestination(ESensorToolDestination::Scenario);
    TestFalse(TEXT("rail reclick collapses archive"),W->IsPanelOpen(ESensorToolPanelRole::Replay));Bar->ToggleOwnedDestination(ESensorToolDestination::Scenario);
    TestTrue(TEXT("rail reopening starts at progress"),W->IsPanelOpen(ESensorToolPanelRole::SlabProgress));
    TArray<uint8> Before,After;UGameplayStatics::SaveGameToMemory(W->Preferences,Before);const float Width=W->GetSidebarWidth();W->ToggleSidebarWidth();TestTrue(TEXT("wide sidebar is wider"),W->GetSidebarWidth()>Width);W->ToggleSidebarWidth();UGameplayStatics::SaveGameToMemory(W->Preferences,After);
    TestTrue(TEXT("width toggle preserves Workspace v1 bytes"),Before==After);TestEqual(TEXT("width toggle returns to original"),W->GetSidebarWidth(),Width);
    TestTrue(TEXT("navigation preserves pose"),Transform.Equals(L->GetActorTransform()));TestEqual(TEXT("navigation preserves sensor revision"),L->GetConfigurationRevision(),Revision);TestEqual(TEXT("navigation preserves running state"),L->IsSensorRunning(),Running);
    W->SetSidebarMode(false);TestTrue(TEXT("floating has no sidebar Back"),S->GetPanelBackLabel().IsEmpty()&&D->GetPanelBackLabel().IsEmpty());
    auto* Foreign=NewObject<USlabProgressPanelWidget>(World);TestTrue(TEXT("unregistered panel has no Back"),Foreign->GetPanelBackLabel().IsEmpty());
    W->Toolbar=nullptr;for(auto* Panel:TArray<UVirtualSensorPanelWidgetBase*>{P,S,D,Progress,Replay,Charts})W->UnregisterPanel(Panel);C->Destroy();L->Destroy();return true;
}

namespace
{
FPointerEvent ResizePointer(FVector2D Now,FVector2D Last,bool Held,FKey Button=FKey())
{
    static const TSet<FKey> Released,Pressed{EKeys::LeftMouseButton};
    return FPointerEvent(FSlateApplication::CursorPointerIndex,Now,Last,Held?Pressed:Released,Button,0,FModifierKeysState());
}
// ProcessMouseMoveEvent alone does not move the hardware cursor. Slate can
// synthesize a held move from that stale position between latent commands and
// undo the resize. Keep this synthetic fixture on Slate's faux cursor, including
// its normal frame-by-frame cursor queries; the native route has UXR11 coverage.
class FUxr10CursorScope
{
    FVector2D PreviousPosition;
    TWeakPtr<SWidget> PreviousFocus;
    bool WasActive;
public:
    FUxr10CursorScope()
    {
        auto& App=FSlateApplication::Get();auto User=App.GetCursorUser();
        PreviousPosition=App.GetCursorPos();
        PreviousFocus=App.GetKeyboardFocusedWidget();WasActive=App.IsActive();
        App.DismissAllMenus();App.CloseToolTip();User->ReleaseAllCapture();
        App.ProcessReply(FWidgetPath(),FReply::Handled().ReleaseMouseLock(),nullptr,nullptr,User->GetUserIndex());
        App.UsePlatformCursorForCursorUser(false);App.OnApplicationActivationChanged(true);
    }
    ~FUxr10CursorScope()
    {
        if(!FSlateApplication::IsInitialized())return;
        auto& App=FSlateApplication::Get();auto User=App.GetCursorUser();const FVector2D Pos=App.GetCursorPos();
        if(App.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
            App.ProcessMouseButtonUpEvent(ResizePointer(Pos,Pos,false,EKeys::LeftMouseButton));
        User->ReleaseAllCapture();
        App.ProcessReply(FWidgetPath(),FReply::Handled().ReleaseMouseLock(),nullptr,nullptr,User->GetUserIndex());
        App.UsePlatformCursorForCursorUser(true);App.SetCursorPos(PreviousPosition);
        if(auto Focus=PreviousFocus.Pin())App.SetKeyboardFocus(Focus,EFocusCause::SetDirectly);
        App.OnApplicationActivationChanged(WasActive);
    }
};
class FUXR10ResizeAndRender : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),Next=0;int32 Phase=0,View=0;bool Requested=false;
    FVector2D Press,Before,Delta;FString Path;FDateTime Previous;TUniquePtr<TGuardValue<bool>> Debug;
    TUniquePtr<FUxr10CursorScope> CursorScope;
    TWeakObjectPtr<USlabProgressPanelWidget> TracedPanel;
    uint64 LastLayoutFrame=MAX_uint64;int32 StableLayoutFrames=0;FVector2D LastLayoutOrigin;
    FDelegateHandle InputTrace;
    void TraceState(const TCHAR* Stage, USlabProgressPanelWidget* Panel)
    {
        auto& App=FSlateApplication::Get();const auto& G=Panel->GetCachedGeometry();
        const auto Window=App.FindWidgetWindow(Panel->GetCachedWidget().ToSharedRef());
        const auto Hit=App.LocateWindowUnderMouse(Press,App.GetInteractiveTopLevelWindows(),false);
        FString Types;for(int32 I=0;I<Hit.Widgets.Num();++I)Types+=Hit.Widgets[I].Widget->GetTypeAsString()+TEXT("/");
        const auto Focus=App.GetKeyboardFocusedWidget();
        UE_LOG(LogTemp,Display,TEXT("[UXR10Trace] %s phase=%d pos=%s size=%s geometry=%s origin=%s scale=%.3f min=%s configuredMax=900x800 viewport=%s press=%s target=%s cursor=%s active=%d capture=%d focus=%s window=%s dpi=%.3f hit=%s"),Stage,Phase,*Panel->GetCurrentPanelPosition().ToString(),*Panel->GetPanelExpandedSize().ToString(),*G.GetLocalSize().ToString(),*G.LocalToAbsolute(FVector2D::ZeroVector).ToString(),G.Scale,*Panel->GetResolvedPanelMinimum().ToString(),*Panel->GetPanelLayoutViewport().ToString(),*Press.ToString(),*(Press+Delta).ToString(),*FVector2D(App.GetCursorPos()).ToString(),App.IsActive(),Panel->GetCachedWidget()->HasMouseCapture(),Focus.IsValid()?*Focus->GetTypeAsString():TEXT("none"),Window.IsValid()?*Window->GetSizeInScreen().ToString():TEXT("none"),Window.IsValid()?Window->GetDPIScaleFactor():0,*Types);
    }
public:
    explicit FUXR10ResizeAndRender(FAutomationTestBase* T):Test(T){}
    ~FUXR10ResizeAndRender()
    {
#if WITH_SLATE_DEBUGGING
        FSlateDebugging::InputEvent.Remove(InputTrace);
#endif
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>150){Test->AddError(TEXT("UXR10 resize/render timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World)||FPlatformTime::Seconds()<Next)return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());
        auto* Panel=Cast<USlabProgressPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::SlabProgress));if(!Bar||!Panel||!W->GetCoordinator())return false;
        auto& App=FSlateApplication::Get();
        if(Phase==0)
        {
            if(FPlatformTime::Seconds()-Started<3)return false;
            Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            CursorScope=MakeUnique<FUxr10CursorScope>();TracedPanel=Panel;
#if WITH_SLATE_DEBUGGING
            InputTrace=FSlateDebugging::InputEvent.AddLambda([this](const FSlateDebuggingInputEventArgs& E)
            {
                if(Phase<2||Phase>7||E.InputEventType!=ESlateDebuggingInputEvent::MouseMove||!TracedPanel.IsValid()||E.HandlerWidget!=TracedPanel->GetCachedWidget())return;
                const auto& P=static_cast<const FPointerEvent&>(*E.InputEvent);
                UE_LOG(LogTemp,Display,TEXT("[UXR10Input] phase=%d type=%d widget=%s point=%s last=%s held=%d handled=%d"),Phase,int32(E.InputEventType),E.HandlerWidget.IsValid()?*E.HandlerWidget->GetTypeAsString():TEXT("none"),*P.GetScreenSpacePosition().ToString(),*P.GetLastScreenSpacePosition().ToString(),P.IsMouseButtonDown(EKeys::LeftMouseButton),E.Reply.IsEventHandled());
            });
#endif
            W->ResetOwnedWorkspaceLayout();W->SetSidebarMode(false);
            for(int32 I=0;I<7;++I)W->SetPanelOpen(static_cast<ESensorToolPanelRole>(I),false);
            W->SetPanelOpen(ESensorToolPanelRole::SlabProgress,true);W->SetOwnedPanelFontScale(1.5f);
            for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);
            for(TActorIterator<ASlabActor> It(World);It;++It){Test->TestTrue(TEXT("memory-only observation scenario"),StartUxrScenarioUiFixture(*It));break;}
            Phase=1;Next=FPlatformTime::Seconds()+1;return false;
        }
        if(Phase==1)
        {
            Panel->SetPanelResizeLimits(FVector2D(360,280),FVector2D(900,800));Panel->SetPanelExpandedSize(FVector2D(700,600),false);Panel->SetWorkspacePosition(FVector2D(90,W->GetToolbarReservedTop()+8));
            Phase=2;Next=FPlatformTime::Seconds()+.5;return false;
        }
        if(Phase==2||Phase==5)
        {
            Before=Panel->GetPanelExpandedSize();const auto& G=Panel->GetCachedGeometry();
            if(!G.GetLocalSize().Equals(Before,1)||G.Scale<=0){StableLayoutFrames=0;return false;}
            if(LastLayoutFrame==GFrameCounter)return false;
            LastLayoutFrame=GFrameCounter;const auto Origin=G.LocalToAbsolute(FVector2D::ZeroVector);
            StableLayoutFrames=Origin.Equals(LastLayoutOrigin,.1)?StableLayoutFrames+1:1;LastLayoutOrigin=Origin;
            if(StableLayoutFrames<2)return false;StableLayoutFrames=0;
            Press=G.LocalToAbsolute(G.GetLocalSize()-FVector2D(8,8));Delta=Phase==2?FVector2D(-2000,-2000):FVector2D(3000,3000);
            auto Window=App.FindWidgetWindow(Panel->GetCachedWidget().ToSharedRef());App.SetCursorPos(Press);App.ProcessMouseMoveEvent(ResizePointer(Press,Press,false),false);
            Test->TestTrue(TEXT("synthetic fixture cursor follows the routed press"),FVector2D(App.GetCursorPos()).Equals(Press,1));
            TraceState(TEXT("before-down"),Panel);
            const auto Hit=App.LocateWindowUnderMouse(Press,App.GetInteractiveTopLevelWindows(),false);
            Test->TestTrue(TEXT("stable panel corner is on the actual Slate hit path"),Hit.ContainsWidget(Panel->GetCachedWidget().Get()));
            Test->AddInfo(FString::Printf(TEXT("resize font=%.2f windowDpi=%.2f geometryScale=%.2f logicalViewport=%s before=%s"),W->GetOwnedPanelFontScale(),Window->GetDPIScaleFactor(),G.Scale,*Panel->GetPanelLayoutViewport().ToString(),*Before.ToString()));
            App.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),ResizePointer(Press,Press,true,EKeys::LeftMouseButton));Test->TestTrue(TEXT("actual Slate corner press captures panel"),Panel->GetCachedWidget()->HasMouseCapture());++Phase;return false;
        }
        if(Phase==3||Phase==6){TraceState(TEXT("before-move"),Panel);App.SetCursorPos(Press+Delta);App.ProcessMouseMoveEvent(ResizePointer(Press+Delta,Press,true),false);Test->TestTrue(TEXT("synthetic fixture cursor follows the routed move"),FVector2D(App.GetCursorPos()).Equals(Press+Delta,1));TraceState(TEXT("after-move"),Panel);++Phase;return false;}
        if(Phase==4||Phase==7)
        {
            // Exercise a stationary held move after the intervening frame, using
            // the cursor position Slate queries rather than the scripted target.
            const FVector2D Cursor=App.GetCursorPos();
            App.ProcessMouseMoveEvent(ResizePointer(Cursor,Cursor,true),true);
            App.ProcessMouseButtonUpEvent(ResizePointer(Press+Delta,Press+Delta,false,EKeys::LeftMouseButton));Test->TestFalse(TEXT("release clears resize capture"),App.HasAnyMouseCaptor());
            const auto Size=Panel->GetPanelExpandedSize();
            TraceState(TEXT("released"),Panel);
            Test->AddInfo(FString::Printf(TEXT("resize released phase=%d size=%s reservedTop=%.2f viewport=%s"),Phase,*Size.ToString(),W->GetToolbarReservedTop(),*Panel->GetPanelLayoutViewport().ToString()));
            if(Phase==4)Test->TestTrue(TEXT("drag clamps to font-scaled minimum"),Size.Equals(Panel->GetResolvedPanelMinimum(),1));else Test->TestTrue(TEXT("drag clamps to configured maximum"),Size.Equals(FVector2D(900,800),1));
            const auto Pos=Panel->GetCurrentPanelPosition(),V=Panel->GetPanelLayoutViewport();Test->TestTrue(TEXT("resized window remains fully inside owned viewport"),Pos.X>=0&&Pos.Y>=W->GetToolbarReservedTop()&&Pos.X+Size.X<=V.X+1&&Pos.Y+Size.Y<=V.Y+1);
            if(Phase==7)
            {
                const auto Saved=Cast<USensorToolWorkspaceSaveGame>(UGameplayStatics::LoadGameFromSlot(UVirtualSensorToolWorkspaceSubsystem::SlotName,0));
                Test->TestNotNull(TEXT("existing Workspace slot saved"),Saved);if(Saved){Test->TestEqual(TEXT("save schema remains version one"),Saved->Version,1);Test->TestTrue(TEXT("released size saved"),Saved->Panels.FindChecked(ESensorToolPanelRole::SlabProgress).Layout.ExpandedSize.Equals(Size,1));}
                Panel->TogglePanelCollapsed();Panel->TogglePanelCollapsed();Test->TestTrue(TEXT("collapse restores expanded size"),Panel->GetPanelExpandedSize().Equals(Size,1));
                Panel->SetPanelExpandedSize(FVector2D(600,400),false);W->RestorePanel(ESensorToolPanelRole::SlabProgress);Test->TestTrue(TEXT("saved layout restores resized window"),Panel->GetPanelExpandedSize().Equals(Size,1));
                W->SetSidebarMode(true);W->SetSidebarMode(false);Test->TestTrue(TEXT("docking roundtrip preserves resized window"),Panel->GetPanelExpandedSize().Equals(Size,1));
            }
            ++Phase;Next=FPlatformTime::Seconds()+.5;return false;
        }
        if(!Requested)
        {
            W->SetOwnedPanelFontScale(1);App.DismissAllMenus();
            const TCHAR* Names[]={TEXT("scenario-default"),TEXT("scenario-wide"),TEXT("archive-back"),TEXT("charts-back"),TEXT("settings-back"),TEXT("compatibility-back"),TEXT("data-tab-back"),TEXT("tool-file-back"),TEXT("floating-resized")};
            if(Phase==8)
            {
                W->SetSidebarMode(View!=8);
                if(View==0||View==1)Bar->OpenOwnedDestination(ESensorToolDestination::Scenario);
                if(View==1)W->ToggleSidebarWidth();
                if(View==2||View==3){Bar->OpenOwnedDestination(ESensorToolDestination::Scenario);W->SetPanelOpen(View==2?ESensorToolPanelRole::Replay:ESensorToolPanelRole::SlabCharts,true);}
                if(View==4){Bar->OpenOwnedDestination(ESensorToolDestination::Placement);W->SetPanelOpen(ESensorToolPanelRole::Settings,true);}
                if(View==5)Bar->OpenOwnedDestination(ESensorToolDestination::Compatibility);
                if(View==6){Bar->OpenOwnedDestination(ESensorToolDestination::PcdTransfer);Cast<UVirtualSensorCaptureExportPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Data))->OpenOwnedTab(ESensorOwnedDataTab::Results);}
                if(View==7)Bar->OpenOwnedDestination(ESensorToolDestination::FileSave);
                if(View==8){for(int32 I=0;I<7;++I)W->SetPanelOpen(static_cast<ESensorToolPanelRole>(I),false);W->SetPanelOpen(ESensorToolPanelRole::SlabProgress,true);Panel->SetPanelExpandedSize(FVector2D(900,800),true);Panel->SetWorkspacePosition(FVector2D(90,W->GetToolbarReservedTop()+8));}
                const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));Path=(Root.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-10/after"):Root)/FString::Printf(TEXT("rendered-1920x1080-100-%s.png"),Names[View]);
                Phase=9;Next=FPlatformTime::Seconds()+.8;return false;
            }
            Previous=IFileManager::Get().GetTimeStamp(*Path);App.CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=Previous)return false;
        Test->AddInfo(FString::Printf(TEXT("actual viewport %dx%d, font100; automated render %s"),World->GetGameViewport()->Viewport->GetSizeXY().X,World->GetGameViewport()->Viewport->GetSizeXY().Y,*Path));Requested=false;Phase=8;
        if(++View<9)return false;
        W->SetSidebarMode(true);Bar->OpenOwnedDestination(ESensorToolDestination::FileSave);auto* Data=Cast<UVirtualSensorCaptureExportPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Data));
        Test->TestTrue(TEXT("tool Back opens tools menu"),Data->NavigatePanelBack());Test->TestFalse(TEXT("tool Back hides child"),W->IsPanelOpen(ESensorToolPanelRole::Data));App.DismissAllMenus();
        Bar->OpenOwnedDestination(ESensorToolDestination::Placement);W->SetPanelOpen(ESensorToolPanelRole::Settings,true);
        auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));auto* Actor=W->GetCoordinator()->GetSelectedSensorActor();
        Settings->SetSensorManipulationEnabled(true);Test->TestTrue(TEXT("fixture enters manipulation"),Actor->IsInteractiveManipulationActive());
        Settings->SelectOwnedSettingsTab(1);Test->TestTrue(TEXT("local section Back preserves active manipulation"),Settings->NavigatePanelBack()&&Actor->IsInteractiveManipulationActive());
        Test->TestFalse(TEXT("cross-panel Back does not silently end manipulation"),Settings->CanNavigatePanelBack()||Settings->NavigatePanelBack());
        Test->TestTrue(TEXT("blocked Back keeps manipulation active"),Actor->IsInteractiveManipulationActive());Settings->SetSensorManipulationEnabled(false);
        const bool WasRunning=Actor->IsSensorRunning();const int32 Revision=Actor->GetConfigurationRevision();const auto Pose=Actor->GetActorTransform();
        Test->TestTrue(TEXT("cross-panel Back after explicit manipulation end"),Settings->NavigatePanelBack());
        Test->TestEqual(TEXT("Back preserves measurement state"),Actor->IsSensorRunning(),WasRunning);Test->TestEqual(TEXT("Back preserves config revision"),Actor->GetConfigurationRevision(),Revision);Test->TestTrue(TEXT("Back preserves actor pose"),Pose.Equals(Actor->GetActorTransform()));return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUXR10ResizeRhiTest,"MA0T10.UXR10.ResizePersistenceAndRenderRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUXR10ResizeRhiTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP resize routing/render requires real RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUXR10ResizeAndRender(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
