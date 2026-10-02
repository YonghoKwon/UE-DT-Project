#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "InputCoreTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "DTCoreContractListener.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/DxWidgetSubsystem.h"
#include "UObject/UnrealType.h"

void UDTCoreMainAcceptanceWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("AddWidgetPanel"));
    Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);WidgetTree->RootWidget=Canvas;
    auto* Button=WidgetTree->ConstructWidget<UButton>();auto* Text=WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(TEXT("Foreign fixture control")));auto Font=Text->GetFont();Font.Size=23;Text->SetFont(Font);Button->AddChild(Text);
    auto* CanvasSlot=Canvas->AddChildToCanvas(Button);CanvasSlot->SetPosition(FVector2D(16,580));CanvasSlot->SetSize(FVector2D(280,48));
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

namespace
{
class FInputAcceptanceCheck:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Start=FPlatformTime::Seconds(),PhaseAt=0;int32 Phase=0;
    TWeakObjectPtr<AVirtualSensorActorBase> Target;int64 FrameBefore=0;
    TWeakObjectPtr<UDxWidgetSubsystem> Widgets;TWeakObjectPtr<UDxWidget> OriginalMain,FixtureMain;
public:
    explicit FInputAcceptanceCheck(FAutomationTestBase* T):Test(T){}
    ~FInputAcceptanceCheck()
    {
        if(FixtureMain.IsValid())FixtureMain->RemoveFromParent();
        if(Widgets.IsValid())if(auto* P=FindFProperty<FObjectProperty>(UDxWidgetSubsystem::StaticClass(),TEXT("MainWidgetInstance")))P->SetObjectPropertyValue_InContainer(Widgets.Get(),OriginalMain.Get());
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>20){Test->AddError(TEXT("Input acceptance timed out"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||!W->GetGameInstance()||!FinalAcceptanceViewportReady(W))return false;
        auto* Workspace=W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Coordinator=Workspace->GetCoordinator();if(!Coordinator)return false;
        if(Phase==0)
        {
            if(FPlatformTime::Seconds()-Start<3)return false;
            Workspace->SetPanelOpen(ESensorToolPanelRole::Settings,true);
            auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Settings));
            Target=Coordinator->GetSelectedSensorActor();Settings->SetSensorManipulationEnabled(true);
            auto View=W->GetGameInstance()->GetGameViewportClient()->GetGameViewportWidget();auto& App=FSlateApplication::Get();
            auto Window=App.FindWidgetWindow(View.ToSharedRef());Window->BringToFront(true);App.SetKeyboardFocus(View,EFocusCause::SetDirectly);
            Test->TestTrue(TEXT("active own viewport focus recognized"),Settings->GetTransformGizmoActor()->IsInputFocusOwned());
            const bool Down=App.ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,27));
            Test->TestTrue(TEXT("first Escape consumed before Editor stop"),Down);
            Test->TestFalse(TEXT("Escape restores acquired actor"),Target->IsInteractiveManipulationActive());
            Test->TestFalse(TEXT("Escape restores Gizmo"),Settings->GetTransformGizmoActor()->IsManipulationEnabled());
            Test->TestTrue(TEXT("same Escape repeat consumed"),App.ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,true,0,27)));
            App.ProcessKeyUpEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,27));
            FrameBefore=Target->GetSensorRuntimeStatus().FrameId;PhaseAt=FPlatformTime::Seconds();Phase=1;return false;
        }
        if(Phase==1)
        {
            if(FPlatformTime::Seconds()-PhaseAt<1)return false;
            Test->TestTrue(TEXT("PIE and post-Escape sensor frames remain alive"),Target.IsValid()&&Target->GetSensorRuntimeStatus().FrameId>FrameBefore);
            Workspace->SetPanelOpen(ESensorToolPanelRole::Data,true);Phase=2;PhaseAt=FPlatformTime::Seconds();return false;
        }
        if(FPlatformTime::Seconds()-PhaseAt<.3)return false;
        auto* Data=Workspace->GetOwnedPanel(ESensorToolPanelRole::Data);
        if(Phase==2)
        {
            Test->TestNotNull(TEXT("Data panel exists"),Data);
            if(Data){Test->TestTrue(TEXT("Data actually visible"),Data->IsVisible());Test->TestTrue(TEXT("Data has usable rendered geometry"),Data->GetCachedGeometry().GetLocalSize().X>=360&&Data->GetCachedGeometry().GetLocalSize().Y>=280);}
            Workspace->SetPanelOpen(ESensorToolPanelRole::Data,false);
            Widgets=W->GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>();OriginalMain=Widgets->GetMainWidget();
            FixtureMain=CreateWidget<UDTCoreMainAcceptanceWidget>(W,UDTCoreMainAcceptanceWidget::StaticClass());FixtureMain->AddToViewport();
            auto* Property=FindFProperty<FObjectProperty>(UDxWidgetSubsystem::StaticClass(),TEXT("MainWidgetInstance"));Property->SetObjectPropertyValue_InContainer(Widgets.Get(),FixtureMain.Get());
            Phase=3;PhaseAt=FPlatformTime::Seconds();return false;
        }
        auto* InnerCanvas=Workspace->GetOwnedPanel(ESensorToolPanelRole::Monitor)->GetParent();
        auto* RootWidget=InnerCanvas?InnerCanvas->GetTypedOuter<UUserWidget>():nullptr;
        Test->TestTrue(TEXT("workspace rehosts to actual DTCore Main AddWidgetPanel"),RootWidget&&RootWidget->GetParent()==Widgets->GetAddWidgetPanel());
        const float OldScale=Workspace->GetOwnedPanelFontScale();Workspace->SetOwnedPanelFontScale(1.5f);
        TArray<UWidget*> Children;FixtureMain->WidgetTree->GetAllWidgets(Children);
        for(auto* Child:Children)if(auto* Text=Cast<UTextBlock>(Child))Test->TestEqual(TEXT("foreign Main control font stays unchanged"),Text->GetFont().Size,23.0f);
        Workspace->SetOwnedPanelFontScale(OldScale);
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorInputAcceptanceTest,"MA0T10.SensorV2.UI.InputAcceptanceRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorInputAcceptanceTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_INPUT_ACCEPTANCE_RHI"))!=TEXT("1")){AddInfo(TEXT("SKIP: set MA0T10_INPUT_ACCEPTANCE_RHI=1 for actual Slate/PIE input"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FInputAcceptanceCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
