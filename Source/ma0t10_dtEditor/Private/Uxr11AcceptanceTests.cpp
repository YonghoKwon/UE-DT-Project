#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Debugging/SlateDebugging.h"
#include "Framework/Application/SlateUser.h"
#include "GenericPlatform/GenericApplication.h"
#include "Slate/SObjectWidget.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"

namespace
{
class FUxr11NativePointer : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),Next=0;
    int32 Phase=0,Case=0,ViewportEvents=0;
    FVector2D Press,Before,Delta;
    FTransform PawnBefore;
    FRotator ViewBefore;
    FDelegateHandle Trace;
    TWeakObjectPtr<UVirtualSensorPanelWidgetBase> Panel;
    TWeakObjectPtr<AVirtualSensorActorBase> Selected;
    const bool WasActive=FSlateApplication::Get().IsActive();
    const FVector2D OriginalCursor=FSlateApplication::Get().GetCursorPos();
    const TArray<ESensorToolPanelRole> Roles{ESensorToolPanelRole::Placement,ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabProgress,ESensorToolPanelRole::SlabCharts};
public:
    explicit FUxr11NativePointer(FAutomationTestBase* T):Test(T){}
    ~FUxr11NativePointer()
    {
#if WITH_SLATE_DEBUGGING
        FSlateDebugging::InputEvent.Remove(Trace);
#endif
        if(FSlateApplication::IsInitialized())
        {
            auto& App=FSlateApplication::Get();App.ReleaseAllPointerCapture();
            App.GetPlatformApplication()->Cursor->SetPosition(OriginalCursor.X,OriginalCursor.Y);
            App.OnApplicationActivationChanged(WasActive);
        }
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("UXR11 native pointer timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World)||FPlatformTime::Seconds()<Next)return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();if(!W||!W->GetCoordinator())return false;
        auto& App=FSlateApplication::Get();auto* PC=World->GetFirstPlayerController();
        if(Phase==0)
        {
            if(FPlatformTime::Seconds()-Started<3)return false;
            if(Case==0)
            {
                W->EnsureOwnedReplayPanel();auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();
                W->GetCoordinator()->RegisterSensorActor(Camera);Camera->StopSensor();W->GetCoordinator()->SelectSensorActor(Camera);Selected=Camera;
            }
            W->SetSidebarMode(false);W->SetOwnedPanelFontScale(Case<Roles.Num()*2?1.f:1.5f);
            for(int32 I=0;I<7;++I)W->SetPanelOpen(static_cast<ESensorToolPanelRole>(I),false);
            W->SetPanelOpen(Roles[(Case/2)%Roles.Num()],true);Panel=W->GetOwnedPanel(Roles[(Case/2)%Roles.Num()]);
            if(!Panel.IsValid()){Test->AddError(TEXT("UXR11 panel missing"));return true;}
            Phase=1;Next=FPlatformTime::Seconds()+.6;return false;
        }
        if(!Panel.IsValid())return true;
        if(Phase==1)
        {
            Panel->SetPanelExpandedSize(Panel->GetResolvedPanelMinimum()+FVector2D(260,180),false);Panel->SetWorkspacePosition(FVector2D(100,150));
            Phase=10;Next=FPlatformTime::Seconds()+.2;return false;
        }
        if(Phase==10)
        {
            const auto& G=Panel->GetCachedGeometry();
            Press=G.LocalToAbsolute(Case%2==0?FVector2D(122,23):G.GetLocalSize()-FVector2D(7,7));
            Delta=Case%2==0?FVector2D(300,90):FVector2D(-160,-120);
            Before=Case%2==0?Panel->GetCurrentPanelPosition():Panel->GetPanelExpandedSize();
            ViewBefore=PC->GetControlRotation();if(PC->GetPawn())PawnBefore=PC->GetPawn()->GetActorTransform();
            App.OnApplicationActivationChanged(true);
            App.GetPlatformApplication()->Cursor->SetPosition(Press.X,Press.Y);App.OnMouseMove();
            // Reproduce native activation/background click: SViewport already has
            // capture. The old tests only routed an uncaptured synthetic pointer.
            const auto View=World->GetGameViewport()->GetGameViewportWidget().ToSharedRef();
            FWidgetPath ViewPath;App.GeneratePathToWidgetUnchecked(View,ViewPath);
            App.ProcessReply(ViewPath,FReply::Handled().UseHighPrecisionMouseMovement(View),nullptr,nullptr);
            Test->TestTrue(TEXT("fixture seeds real PIE viewport capture"),View->HasMouseCapture());
            const auto Hit=App.LocateWindowUnderMouse(Press,App.GetInteractiveTopLevelWindows());
            FString Types;for(int32 I=0;I<Hit.Widgets.Num();++I)Types+=Hit.Widgets[I].Widget->GetTypeAsString()+TEXT("/");
            Test->AddInfo(FString::Printf(TEXT("UXR11 case=%d role=%d font=%.2f dpi=%.2f controller=%s native-cursor=%s capture=SViewport hit=%s panel-local=%s"),Case,int32(Roles[(Case/2)%Roles.Num()]),W->GetOwnedPanelFontScale(),G.Scale,*PC->GetClass()->GetPathName(),*FVector2D(App.GetCursorPos()).ToString(),*Types,*G.AbsoluteToLocal(Press).ToString()));
#if WITH_SLATE_DEBUGGING
            ViewportEvents=0;
            Trace=FSlateDebugging::InputEvent.AddLambda([this,View](const FSlateDebuggingInputEventArgs& E)
            {
                if(E.HandlerWidget==View&&(E.InputEventType==ESlateDebuggingInputEvent::MouseButtonDown||E.InputEventType==ESlateDebuggingInputEvent::MouseButtonUp||E.InputEventType==ESlateDebuggingInputEvent::MouseMove))++ViewportEvents;
                if(E.InputEventType==ESlateDebuggingInputEvent::MouseButtonDown||E.InputEventType==ESlateDebuggingInputEvent::PreviewMouseButtonDown)
                {
                    const auto* P=static_cast<const FPointerEvent*>(E.InputEvent);
                    const auto Object=E.HandlerWidget.IsValid()&&E.HandlerWidget->GetType()==TEXT("SObjectWidget")?StaticCastSharedPtr<SObjectWidget>(E.HandlerWidget)->GetWidgetObject():nullptr;
                    Test->AddInfo(FString::Printf(TEXT("UXR11 route type=%d widget=%s object=%s handled=%d capture=%s same-panel=%d point=%s pointer=%d user=%d"),int32(E.InputEventType),E.HandlerWidget.IsValid()?*E.HandlerWidget->GetTypeAsString():TEXT("none"),*GetNameSafe(Object),E.Reply.IsEventHandled(),E.Reply.GetMouseCaptor().IsValid()?*E.Reply.GetMouseCaptor()->GetTypeAsString():TEXT("none"),Panel.IsValid()&&E.Reply.GetMouseCaptor()==Panel->GetCachedWidget(),P?*P->GetScreenSpacePosition().ToString():TEXT("none"),P?P->GetPointerIndex():-1,P?P->GetUserIndex():-1));
                }
            });
#endif
            auto Window=App.FindWidgetWindow(Panel->GetCachedWidget().ToSharedRef());
            App.OnMouseDown(Window->GetNativeWindow(),EMouseButtons::Left);
            Test->TestTrue(TEXT("native title/corner press transfers capture to owned panel"),Panel->GetCachedWidget()->HasMouseCapture());
            Test->TestTrue(TEXT("title/corner owns keyboard focus so viewport cannot recapture"),App.GetKeyboardFocusedWidget()==Panel->GetCachedWidget());
            Phase=2;return false;
        }
        if(Phase==2)
        {
            App.GetPlatformApplication()->Cursor->SetPosition(Press.X+Delta.X,Press.Y+Delta.Y);App.OnMouseMove();Phase=3;return false;
        }
        App.OnMouseUp(EMouseButtons::Left);
#if WITH_SLATE_DEBUGGING
        FSlateDebugging::InputEvent.Remove(Trace);Trace.Reset();
#endif
        const auto After=Case%2==0?Panel->GetCurrentPanelPosition():Panel->GetPanelExpandedSize();
        Test->AddInfo(FString::Printf(TEXT("UXR11 native before=%s after=%s"),*Before.ToString(),*After.ToString()));
        Test->TestTrue(TEXT("native title/corner changes layout"),Case%2==0?After.X>Before.X+200:After.X<Before.X-50);
        Test->TestFalse(TEXT("native release clears capture"),App.HasAnyMouseCaptor());
        Test->TestTrue(TEXT("panel gesture does not rotate PIE camera"),ViewBefore.Equals(PC->GetControlRotation(),.01));
        if(PC->GetPawn())Test->TestTrue(TEXT("panel gesture does not move PIE pawn"),PawnBefore.Equals(PC->GetPawn()->GetActorTransform(),.01));
        Test->TestEqual(TEXT("gesture down/move/up never reaches game viewport"),ViewportEvents,0);
        Test->TestTrue(TEXT("gesture does not select a world object"),W->GetCoordinator()->GetSelectedSensorActor()==Selected.Get());
        const auto Saved=Cast<USensorToolWorkspaceSaveGame>(UGameplayStatics::LoadGameFromSlot(UVirtualSensorToolWorkspaceSubsystem::SlotName,0));
        Test->TestNotNull(TEXT("existing Workspace save slot written on release"),Saved);
        if(Saved){Test->TestEqual(TEXT("Workspace schema remains v1"),Saved->Version,1);const auto& State=Saved->Panels.FindChecked(Roles[(Case/2)%Roles.Num()]).Layout;Test->TestTrue(TEXT("existing layout saves both position and size"),State.bHasSavedPosition&&State.bHasSavedSize&&State.ExpandedSize.Equals(Panel->GetPanelExpandedSize(),1));}
        const auto Position=Panel->GetCurrentPanelPosition(),Size=Panel->GetPanelExpandedSize(),Viewport=Panel->GetPanelLayoutViewport();
        Test->TestTrue(TEXT("layout stays fully inside viewport and below toolbar"),Position.X>=0&&Position.Y>=W->GetToolbarReservedTop()&&Position.X+Size.X<=Viewport.X+1&&Position.Y+Size.Y<=Viewport.Y+1);
        W->RestorePanel(Roles[(Case/2)%Roles.Num()]);Test->TestTrue(TEXT("existing layout restores position and size"),Panel->GetCurrentPanelPosition().Equals(Position,1)&&Panel->GetPanelExpandedSize().Equals(Size,1));
        if(Case==1)
        {
            const auto& G=Panel->GetCachedGeometry();const auto Corner=G.LocalToAbsolute(G.GetLocalSize()-FVector2D(7,7));
            App.GetPlatformApplication()->Cursor->SetPosition(Corner.X,Corner.Y);App.OnMouseMove();
            App.OnMouseDown(App.FindWidgetWindow(Panel->GetCachedWidget().ToSharedRef())->GetNativeWindow(),EMouseButtons::Left);
            Test->TestTrue(TEXT("second corner press captures"),Panel->GetCachedWidget()->HasMouseCapture());
            W->SetPanelOpen(Roles[(Case/2)%Roles.Num()],false);
            Test->TestFalse(TEXT("hiding owned panel ends gesture and releases capture"),App.HasAnyMouseCaptor());App.OnMouseUp(EMouseButtons::Left);
        }
        App.ReleaseAllPointerCapture();
        if(++Case<Roles.Num()*4){Phase=0;Next=FPlatformTime::Seconds()+.3;return false;}
        const auto View=World->GetGameViewport()->GetGameViewportWidget().ToSharedRef();FWidgetPath ViewPath;App.GeneratePathToWidgetUnchecked(View,ViewPath);
        const auto Outside=View->GetCachedGeometry().LocalToAbsolute(FVector2D(1860,250));
        App.GetPlatformApplication()->Cursor->SetPosition(Outside.X,Outside.Y);App.OnMouseMove();
        const auto Policy=World->GetGameViewport()->GetMouseCaptureMode();
        App.ProcessReply(ViewPath,FReply::Handled().UseHighPrecisionMouseMovement(View),nullptr,nullptr);
        App.OnMouseDown(App.FindWidgetWindow(View)->GetNativeWindow(),EMouseButtons::Left);
        Test->TestTrue(TEXT("outside owned UI the viewport keeps its capture"),View->HasMouseCapture());
        Test->TestEqual(TEXT("project adapter preserves viewport capture policy"),World->GetGameViewport()->GetMouseCaptureMode(),Policy);
        App.OnMouseUp(EMouseButtons::Left);App.ProcessReply(ViewPath,FReply::Handled().ReleaseMouseCapture().ReleaseMouseLock(),nullptr,nullptr);
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr11NativePointerTest,"MA0T10.UXR11.NativeCursorCaptureRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr11NativePointerTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP UXR11 native cursor requires real RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/PortableSetupMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr11NativePointer(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUXR11ProfileNoticeTest,"MA0T10.UXR11.ProfileReapplyNotice",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUXR11ProfileNoticeTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* C=World->SpawnActor<AVirtualSensorCoordinator>();W->SetCoordinator(C);
    auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();
    auto* S=NewObject<UVirtualSensorSettingsPanelWidget>(World);W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,S);
    TestTrue(TEXT("initialize native settings fixture"),S->Initialize());S->SetVisibility(ESlateVisibility::Visible);const auto Pin=S->TakeWidget();
    C->RegisterSensorActor(Camera);C->RegisterSensorActor(Lidar);
    for(auto* A:TArray<AVirtualSensorActorBase*>{Camera,Lidar})
    {
        C->SelectSensorActor(A);S->BindSensorManager(C);
        S->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;
        S->PendingState.CameraCaptureInterval=.1f;S->PendingState.LidarScanInterval=.1f;
        TestTrue(TEXT("custom user period applies before selecting profile"),S->ApplyPendingState());
        S->ApplyOwnedProfileSelection(EVirtualCameraDeviceProfile::IntelRealSenseD455,EVirtualLidarDeviceProfile::LivoxMid360S);
        FVirtualSensorEditableState Runtime;TestTrue(TEXT("read applied profile state"),A->ReadEditableState(Runtime));
        TestEqual(TEXT("existing full specification reapplication contract"),Runtime.SimulationQuality,EVirtualSensorSimulationQuality::FullSpec);
        const float Interval=A==Camera?Runtime.CameraCaptureInterval:Runtime.LidarScanInterval;
        TestTrue(TEXT("existing profile period contract is unchanged"),FMath::IsNearlyEqual(Interval,A==Camera?1.f/30.f:.1f,.00001f));
        AddInfo(FString::Printf(TEXT("profile kind=%d interval=%.6f status=%s visible=%d owned=%d"),int32(A->GetSensorKind()),Interval,*S->LastControlMessage,S->IsVisible(),S->IsWorkspaceOwned()));
        TestTrue(TEXT("visible result explains profile defaults"),S->OwnedStatusText.ToString().Contains(TEXT("프로필 기본값 적용: 품질")));
        TestTrue(TEXT("result includes previous custom quality and period"),S->OwnedStatusText.ToString().Contains(TEXT("이전 사용자 값: 품질 사용자 설정, 주기 0.1초")));
        TestTrue(TEXT("result reports actual applied period"),S->OwnedStatusText.ToString().Contains(FString::Printf(TEXT("주기 %.4g초"),Interval)));
        const auto Notice=S->OwnedStatusText;S->RefreshOwnedSettingsText();
        TestEqual(TEXT("5Hz text refresh retains result until next action"),S->OwnedStatusText.ToString(),Notice.ToString());
        TestTrue(TEXT("next settings change still applies"),S->ApplyPendingState());
        TestFalse(TEXT("next settings action replaces old result"),S->OwnedStatusText.ToString().Contains(TEXT("프로필 기본값 적용")));
    }
    W->UnregisterPanel(S);W->SetCoordinator(nullptr);C->Destroy();Camera->Destroy();Lidar->Destroy();return true;
}
#endif
