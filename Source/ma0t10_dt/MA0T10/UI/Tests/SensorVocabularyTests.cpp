#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "SensorPresentationTestHelpers.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/IToolTip.h"

namespace
{
// Inspect labels/hints and tooltip content, but not editable user IDs, addresses or file paths.
void CollectText(const TSharedRef<SWidget>& Widget,TArray<FString>& Text,bool Tooltips=true)
{
    FSlateAttributeMetaData::UpdateOnlyVisibilityAttributes(Widget.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
    if(!Widget->GetVisibility().IsVisible())return;
    FSlateAttributeMetaData::UpdateExceptVisibilityAttributes(Widget.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
    if(Widget->GetType()==TEXT("STextBlock"))Text.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
    if(Widget->GetType()==TEXT("SEditableTextBox"))Text.Add(StaticCastSharedRef<SEditableTextBox>(Widget)->GetHintText().ToString());
    if(Tooltips)if(const auto Tip=Widget->GetToolTip())if(!Tip->IsEmpty())CollectText(Tip->GetContentWidget(),Text,false);
    auto* Children=Widget->GetChildren();
    for(int32 I=0;I<Children->Num();++I)CollectText(Children->GetChildAt(I),Text,Tooltips);
}
void ExpandArea(const TSharedRef<SWidget>& Widget,const FString& Header)
{
    if(Widget->GetType()==TEXT("SExpandableArea"))
    {
        const auto Area=StaticCastSharedRef<SExpandableArea>(Widget);
        TArray<FString> Text;CollectText(Widget,Text,false);
        if(Text.Contains(Header))Area->SetExpanded(true);
    }
    auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)ExpandArea(Children->GetChildAt(I),Header);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorVocabularyTest,"MA0T10.UXR07.OwnedScreenVocabulary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorVocabularyTest::RunTest(const FString&)
{
    FString PolicyText;TSharedPtr<FJsonObject> Policy;
    if(!TestTrue(TEXT("load shared vocabulary policy"),FFileHelper::LoadFileToString(PolicyText,*(FPaths::ProjectDir()/TEXT("Scripts/fixtures/sensor_ui_vocabulary.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PolicyText),Policy)))return false;
    TArray<FString> Forbidden,Allowed,Required;
    Policy->TryGetStringArrayField(TEXT("forbidden"),Forbidden);Policy->TryGetStringArrayField(TEXT("allowedAreas"),Allowed);Policy->TryGetStringArrayField(TEXT("requiredAreas"),Required);
    TestTrue(TEXT("policy is not empty"),Forbidden.Num()>=20&&Required.Num()>=20);
    TArray<TSharedPtr<FJsonValue>> Areas;TSet<FString> Seen;
    auto InspectText=[&](const FString& Name,const TArray<FString>& Text)
    {
        TestFalse(*FString::Printf(TEXT("%s text collection must not be empty"),*Name),Text.IsEmpty());Seen.Add(Name);
        auto Area=MakeShared<FJsonObject>();Area->SetStringField(TEXT("name"),Name);Area->SetBoolField(TEXT("allowed"),Allowed.Contains(Name));
        TArray<TSharedPtr<FJsonValue>> Values;
        for(const auto& Value:Text)
        {
            Values.Add(MakeShared<FJsonValueString>(Value));
            if(!Allowed.Contains(Name))for(const auto& Word:Forbidden)if(Value.Contains(Word,ESearchCase::IgnoreCase))AddError(FString::Printf(TEXT("%s: forbidden '%s' in '%s'"),*Name,*Word,*Value));
        }
        Area->SetArrayField(TEXT("texts"),Values);Areas.Add(MakeShared<FJsonValueObject>(Area));
    };
    auto Inspect=[&](const FString& Name,const TSharedRef<SWidget>& Slate){Slate->SlatePrepass();TArray<FString> Text;CollectText(Slate,Text);InspectText(Name,Text);};
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* C=World->SpawnActor<AVirtualSensorCoordinator>();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();W->SetCoordinator(C);
    auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Camera->StopSensor();Lidar->StopSensor();C->RegisterSensorActor(Camera);C->RegisterSensorActor(Lidar);
    auto* Slab=World->SpawnActor<ASlabActor>();
    auto* Placement=NewObject<UVirtualSensorPlacementPanelWidget>(World);auto* Monitor=NewObject<UVirtualSensorMonitorPanelWidget>(World);auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(World);auto* Data=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);
    auto* Progress=NewObject<USlabProgressPanelWidget>(World);auto* Replay=NewObject<USlabScenarioReplayPanelWidget>(World);auto* Charts=NewObject<USlabChartsPanelWidget>(World);
    W->RegisterOwnedPanel(ESensorToolPanelRole::Placement,Placement);W->RegisterOwnedPanel(ESensorToolPanelRole::Monitor,Monitor);W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Settings);W->RegisterOwnedPanel(ESensorToolPanelRole::Data,Data);
    W->RegisterOwnedPanel(ESensorToolPanelRole::SlabProgress,Progress);W->RegisterOwnedPanel(ESensorToolPanelRole::Replay,Replay);W->RegisterOwnedPanel(ESensorToolPanelRole::SlabCharts,Charts);
    Monitor->bPersistMonitorPreferences=false;Monitor->BindSensorManager(C);Settings->BindSensorManager(C);Data->BindSensorManager(C);Data->BindScenarioSlabActor(Slab);Progress->BindSlabActor(Slab);Charts->BindSlabActor(Slab);
    for(auto* Panel:TArray<UVirtualSensorPanelWidgetBase*>{Placement,Monitor,Settings,Data,Progress,Replay,Charts}){Panel->Initialize();Panel->SetVisibility(ESlateVisibility::Visible);}
    // Hold each UMG wrapper until cleanup. Dropping the last wrapper invokes NativeDestruct
    // and unregisters the panel, which would accidentally inspect the legacy path next time.
    const auto PlacementSlate=Placement->TakeWidget();const auto MonitorSlate=Monitor->TakeWidget();const auto SettingsSlate=Settings->TakeWidget();const auto DataSlate=Data->TakeWidget();
    const auto ProgressSlate=Progress->TakeWidget();const auto ReplaySlate=Replay->TakeWidget();const auto ChartsSlate=Charts->TakeWidget();
    auto* Toolbar=NewObject<USensorToolToolbarWidget>(World);W->Toolbar=Toolbar;Toolbar->Initialize();const auto ToolbarSlate=Toolbar->TakeWidget();Toolbar->RefreshOwnedStatus(FPlatformTime::Seconds()+1);Inspect(TEXT("toolbar"),ToolbarSlate);Inspect(TEXT("status-strip"),Toolbar->BuildStatusStrip());Inspect(TEXT("equipment-menu"),Toolbar->BuildEquipmentMenu());Inspect(TEXT("tools-menu"),Toolbar->BuildToolsMenu());Inspect(TEXT("appearance-menu"),Toolbar->BuildAppearanceMenu());
    TArray<FString> BarLabels;CollectText(ToolbarSlate,BarLabels);
    for(const FText& Label:{SensorToolUiText::Placement(),SensorToolUiText::Preview(),SensorToolUiText::Scenario(),FText::FromString(TEXT("PCD")),SensorToolUiText::Results()})TestTrue(TEXT("actual owned sidebar rail, not the legacy fallback, is inspected"),BarLabels.Contains(Label.ToString()));
    for(auto* Actor:TArray<AVirtualSensorActorBase*>{Camera,Lidar})
    {
        const FString Kind=Actor==Camera?TEXT("camera"):TEXT("lidar");C->SelectSensorActor(Actor);
        Inspect(TEXT("placement-")+Kind,PlacementSlate);
        if(Actor==Camera){Monitor->BindVirtualCamera(Camera->CaptureComponent);Monitor->ShowCameraView();}else{Monitor->BindVirtualLidar(Lidar->ScanComponent);Monitor->ShowLidarView();}
        Monitor->RefreshOwnedMonitorText();Inspect(TEXT("preview-")+Kind,MonitorSlate);
        Monitor->bOwnedAdvancedMenuOpen=true;Monitor->bMonitorDetailsExpanded=false;Monitor->RefreshOwnedMonitorText();Inspect(TEXT("preview-")+Kind+TEXT("-view-settings"),Monitor->BuildOwnedAdvancedViewMenu());Monitor->bOwnedAdvancedMenuOpen=false;
        Settings->SynchronizeWorkspaceSelection(true);
        for(int32 Tab=0;Tab<3;++Tab){Settings->WorkspaceSettingsTab=Tab;Settings->bOwnedSettingHelpOpen=false;Settings->RefreshOwnedSettingsText();Inspect(TEXT("settings-")+Kind+(Tab==0?TEXT("-basic"):Tab==1?TEXT("-advanced"):TEXT("-compatibility")),SettingsSlate);}
        Settings->WorkspaceSettingsTab=0;Settings->OpenOwnedSettingHelp(TEXT("DeviceProfile"));Settings->RefreshOwnedSettingsText();Inspect(TEXT("settings-")+Kind+TEXT("-help"),SettingsSlate);Settings->bOwnedSettingHelpOpen=false;
    }
    Monitor->bOwnedAdvancedMenuOpen=true;Monitor->bMonitorDetailsExpanded=false;Monitor->RefreshOwnedMonitorText();Inspect(TEXT("preview-view-settings"),Monitor->BuildOwnedAdvancedViewMenu());
    Monitor->bMonitorDetailsExpanded=true;Monitor->RefreshOwnedMonitorText();Inspect(TEXT("preview-developer-info"),Monitor->BuildOwnedAdvancedViewMenu());
    Progress->RefreshOwnedStatus();Inspect(TEXT("scenario-progress"),ProgressSlate);Inspect(TEXT("scenario-archive"),ReplaySlate);Inspect(TEXT("scenario-charts"),ChartsSlate);
    Data->OpenOwnedTab(ESensorOwnedDataTab::Transfer);Data->bOwnedScenarioStreamView=true;
    Data->bOwnedReplayPreflight=false;Data->TickOwnedUi(1);Inspect(TEXT("transfer-new"),Data->BuildOwnedLiveTab());
    Data->bOwnedReplayPreflight=true;Data->TickOwnedUi(2);Inspect(TEXT("transfer-replay"),Data->BuildOwnedLiveTab());
    Data->bOwnedScenarioStreamView=false;const auto Direct=Data->BuildOwnedLiveTab();ExpandArea(Direct,SensorToolUiText::DirectTransfer().ToString());Data->TickOwnedUi(3);Inspect(TEXT("transfer-direct"),Direct);
    const auto Details=Data->BuildOwnedLiveTab();ExpandArea(Details,SensorToolUiText::PreflightDetails().ToString());Data->TickOwnedUi(4);Inspect(TEXT("transfer-preflight-details"),Details);
    const auto Advanced=Data->BuildOwnedLiveTab();ExpandArea(Advanced,SensorToolUiText::Advanced().ToString());Data->TickOwnedUi(5);Inspect(TEXT("transfer-advanced"),Advanced);
    Data->OpenOwnedTab(ESensorOwnedDataTab::Results);Data->TickOwnedUi(6);Inspect(TEXT("results-empty"),Data->BuildOwnedResultsTab());
    Data->OpenOwnedTab(ESensorOwnedDataTab::FileSave);Data->TickOwnedUi(7);Inspect(TEXT("file-save"),Data->BuildOwnedFileTab());
    Data->OpenOwnedTab(ESensorOwnedDataTab::ServerConnection);Data->TickOwnedUi(8);Inspect(TEXT("server-connection"),Data->BuildOwnedConnectionTab());
    Data->OpenOwnedTab(ESensorOwnedDataTab::Diagnostics);Data->TickOwnedUi(9);Inspect(TEXT("data-diagnostics"),Data->BuildOwnedDiagnosticsTab());
    Data->LastUiMessage=TEXT("송신 실패 · Topic receipt FIFO STOMP · Capture/Export 내보내기");Data->OpenOwnedTab(ESensorOwnedDataTab::Transfer);Data->TickOwnedUi(10);InspectText(TEXT("dynamic-user-message"),{Data->OwnedUiMessage});
    // Exercise every technical term through the presentation adapter, including uppercase variants.
    TArray<FString> Reasons;for(const auto& Word:Forbidden){Reasons.Add(SensorToolUiText::UserReason(Word));Reasons.Add(SensorToolUiText::UserReason(Word.ToUpper()));}InspectText(TEXT("dynamic-user-reason"),Reasons);
    TestEqual(TEXT("receipt means server acceptance only"),SensorToolUiText::UserReason(TEXT("receipt")),FString(TEXT("서버 수락")));
    for(const auto& Name:Required)TestTrue(*FString::Printf(TEXT("required default area %s inspected"),*Name),Seen.Contains(Name));
    for(const auto& Name:Allowed)TestTrue(*FString::Printf(TEXT("allowlisted area %s recorded separately"),*Name),Seen.Contains(Name));
    auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("schemaVersion"),1);Report->SetStringField(TEXT("scope"),TEXT("owned native fixture Slate labels, hints and tooltips; no mouse acceptance"));Report->SetArrayField(TEXT("areas"),Areas);
    const FString ReportPath=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Saved/Reports/UXR/UXR-07/visible-text.json"));
    FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));TestTrue(TEXT("text inventory written"),FFileHelper::SaveStringToFile(Json,*ReportPath));AddInfo(TEXT("Visible text report: ")+ReportPath);
    for(auto* Panel:TArray<UVirtualSensorPanelWidgetBase*>{Placement,Monitor,Settings,Data,Progress,Replay,Charts}){W->UnregisterPanel(Panel);Panel->RemoveFromParent();}
    Camera->Destroy();Lidar->Destroy();Slab->Destroy();C->Destroy();return true;
}
#endif
