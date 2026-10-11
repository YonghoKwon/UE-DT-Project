#include "SensorToolToolbarWidget.h"
#include "SensorToolWorkspaceStyle.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorToolUiText.h"
#include "VirtualSensorSettingsPanelWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#define LOCTEXT_NAMESPACE "SensorToolWorkspace"
UVirtualSensorToolWorkspaceSubsystem* USensorToolToolbarWidget::Workspace() const {return GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>():nullptr;}
void USensorToolToolbarWidget::RefreshSensors()
{
	SensorOptions.Reset();auto* W=Workspace();auto* C=W?W->GetCoordinator():nullptr;if(!C)return;
	for(auto* A:C->GetSensorActors())if(A)SensorOptions.Add(MakeShared<FString>(A->GetSensorId()));
}
void USensorToolToolbarWidget::SelectSensor(const FString& Id)
{
	auto* W=Workspace();auto* C=W?W->GetCoordinator():nullptr;if(!C)return;
	for(auto* A:C->GetSensorActors())if(A&&A->GetSensorId()==Id){C->SelectSensorActor(A);W->SynchronizeOwnedSelection();return;}
}
FText USensorToolToolbarWidget::SelectedSensorText() const
{
	auto* W=Workspace();auto* C=W?W->GetCoordinator():nullptr;FString Id=TEXT("센서 없음");
	if(W&&W->OwnsToolbar(this))
	{
		if(C)if(auto* A=C->GetSelectedSensorActor())if(const auto* Label=OwnedSensorLabels.Find(A->GetSensorId()))
			return FText::Format(NSLOCTEXT("SensorToolUiText","SelectedEquipment","장비 · {0}"),*Label);
		return SensorToolUiText::NoEquipment();
	}
	if(C)if(auto* A=C->GetSelectedSensorActor())Id=(A->GetSensorKind()==EVirtualSensorKind::Lidar?TEXT("LiDAR · "):TEXT("Camera · "))+A->GetSensorId();
	return FText::FromString(Id);
}
TSharedRef<SWidget> USensorToolToolbarWidget::RebuildWidget()
{
	auto* W=Workspace();
	if(!W||!W->OwnsToolbar(this)) return BuildLegacyToolbar();
	RefreshSensors();
	RefreshOwnedStatus(FPlatformTime::Seconds());
	auto Bar=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
	Bar->AddSlot()
	[SAssignNew(SensorCombo,SComboBox<TSharedPtr<FString>>).ComboBoxStyle(&FSensorOwnedPresentationStyle::ComboBox())
	 .OptionsSource(&SensorOptions).ForegroundColor(FSensorOwnedPresentationStyle::Text())
	 .OnComboBoxOpening_Lambda([this](){RefreshSensors();if(SensorCombo)SensorCombo->RefreshOptions();})
	 .OnGenerateWidget_Lambda([this](TSharedPtr<FString> Id){const auto* Label=Id?OwnedSensorLabels.Find(*Id):nullptr;return SNew(STextBlock).Font(ToolFont()).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text(Label?*Label:FText::FromString(Id?*Id:FString()));})
	 .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Id,ESelectInfo::Type){if(Id)SelectSensor(*Id);})
	 [SNew(SBox).MaxDesiredWidth_Lambda([this](){return 560.f*(ToolFont().Size/16.f);})
	 [SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ToolTipText_Lambda([this](){return SelectedSensorText();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text_Lambda([this](){return SelectedSensorText();})]]];
	const ESensorToolDestination Destinations[]={ESensorToolDestination::Placement,ESensorToolDestination::Preview,ESensorToolDestination::Scenario,ESensorToolDestination::PcdTransfer,ESensorToolDestination::Results};
	const FText Labels[]={SensorToolUiText::Placement(),SensorToolUiText::Preview(),SensorToolUiText::Scenario(),SensorToolUiText::PcdTransfer(),SensorToolUiText::Results()};
	for(int32 I=0;I<UE_ARRAY_COUNT(Destinations);++I)
	{
		const auto Destination=Destinations[I];
		Bar->AddSlot()[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button()).OnClicked_Lambda([this,Destination](){OpenOwnedDestination(Destination);return FReply::Handled();})[SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text(Labels[I])]];
	}
	Bar->AddSlot()[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton())
	 .OnGetMenuContent_Lambda([this](){return BuildToolsMenu();})
	 .ButtonContent()[SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text(SensorToolUiText::Tools())]];
	return SNew(SBorder).BorderImage(FSensorOwnedPresentationStyle::PanelBrush()).BorderBackgroundColor(FSensorOwnedPresentationStyle::Panel()).Padding(8)
	[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Bar]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[BuildStatusStrip()]
	 +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font_Lambda([this](){return ToolFont(14);}).AutoWrapText(true)
	 .Visibility_Lambda([this](){auto* W=Workspace();return W&&!W->GetPanelOpenMessage().IsEmpty()?EVisibility::Visible:EVisibility::Collapsed;})
	 .Text_Lambda([this](){auto* W=Workspace();return FText::FromString(W?W->GetPanelOpenMessage():FString());})]];
}
FSlateFontInfo USensorToolToolbarWidget::ToolFont(int32 Size) const
{
	const auto* W=Workspace();return FSensorToolWorkspaceStyle::Font(Size,W&&W->OwnsToolbar(this)?W->GetOwnedPanelFontScale():1.0f);
}
bool USensorToolToolbarWidget::OpenOwnedDestination(ESensorToolDestination Destination)
{
	auto* W=Workspace();if(!W||!W->OwnsToolbar(this))return false;
	if(Destination==ESensorToolDestination::Scenario)
	{
		W->SetPanelOpen(ESensorToolPanelRole::SlabProgress,true);
		W->SetPanelOpen(ESensorToolPanelRole::Replay,true);
		for(const auto Role:{ESensorToolPanelRole::SlabProgress,ESensorToolPanelRole::Replay})
			if(auto* Panel=W->GetOwnedPanel(Role))if(Panel->IsPanelCollapsed())Panel->TogglePanelCollapsed();
		return W->GetOwnedPanel(ESensorToolPanelRole::SlabProgress)&&W->GetOwnedPanel(ESensorToolPanelRole::Replay);
	}
	const auto Role=Destination==ESensorToolDestination::Placement?ESensorToolPanelRole::Placement:
		Destination==ESensorToolDestination::Preview?ESensorToolPanelRole::Monitor:
		Destination==ESensorToolDestination::ScenarioCharts?ESensorToolPanelRole::SlabCharts:
		(Destination==ESensorToolDestination::SensorSettings||Destination==ESensorToolDestination::Compatibility)?ESensorToolPanelRole::Settings:ESensorToolPanelRole::Data;
	W->SetPanelOpen(Role,true);
	auto* Panel=W->GetOwnedPanel(Role);if(!Panel)return false;
	if(Panel->IsPanelCollapsed())Panel->TogglePanelCollapsed();
	if(auto* S=Cast<UVirtualSensorSettingsPanelWidget>(Panel))S->OpenOwnedSettingsSection(Destination==ESensorToolDestination::Compatibility);
	if(auto* D=Cast<UVirtualSensorCaptureExportPanelWidget>(Panel))
	{
		D->SetActiveTab(Destination==ESensorToolDestination::PcdTransfer?EVirtualSensorCaptureExportTab::LiveStream:
			Destination==ESensorToolDestination::FileSave?EVirtualSensorCaptureExportTab::Capture:EVirtualSensorCaptureExportTab::ConnectionLog);
		if(Destination==ESensorToolDestination::Results)D->SetScenarioRunResultsExpanded(true);
		if(Destination==ESensorToolDestination::ServerSettings||Destination==ESensorToolDestination::Diagnostics)
			D->OpenOwnedConnectionSection(Destination==ESensorToolDestination::Diagnostics);
	}
	return true;
}

TSharedRef<SWidget> USensorToolToolbarWidget::BuildToolsMenu()
{
	auto Menu=SNew(SVerticalBox);
	// Preserve chart access until the scenario screen is reorganized in UXR-04.
	const ESensorToolDestination Destinations[]={ESensorToolDestination::SensorSettings,ESensorToolDestination::FileSave,ESensorToolDestination::ServerSettings,ESensorToolDestination::Diagnostics,ESensorToolDestination::Compatibility,ESensorToolDestination::ScenarioCharts};
	const FText Names[]={SensorToolUiText::SensorSettings(),SensorToolUiText::FileSave(),SensorToolUiText::ServerSettings(),SensorToolUiText::Diagnostics(),SensorToolUiText::Compatibility(),SensorToolUiText::ScenarioCharts()};
	for(int32 I=0;I<UE_ARRAY_COUNT(Destinations);++I)
	{
		const auto Destination=Destinations[I];
		Menu->AddSlot().AutoHeight().Padding(0,4)
		[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button())
		 .OnClicked_Lambda([this,Destination](){OpenOwnedDestination(Destination);FSlateApplication::Get().DismissAllMenus();return FReply::Handled();})
		 [SNew(STextBlock).Font_Lambda([this](){return ToolFont();})
		  .ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text(Names[I])]];
	}
	Menu->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton())
	 .OnGetMenuContent_Lambda([this](){return BuildAppearanceMenu();}).ButtonContent()[SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text(SensorToolUiText::Appearance())]];
	return Menu;
}
TSharedRef<SWidget> USensorToolToolbarWidget::BuildAppearanceMenu()
{
	auto Menu=SNew(SVerticalBox);
	for(float Scale:{.85f,1.0f,1.25f,1.5f})
		Menu->AddSlot().AutoHeight().Padding(0,4)[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button())
		 .OnClicked_Lambda([this,Scale](){if(auto* W=Workspace())W->SetOwnedPanelFontScale(Scale);FSlateApplication::Get().DismissAllMenus();return FReply::Handled();})
		 [SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text())
		  .Text(SensorToolUiText::FontPercent(Scale))]];
	Menu->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button())
	 .OnClicked_Lambda([this](){if(auto* W=Workspace())W->ResetOwnedWorkspaceLayout();FSlateApplication::Get().DismissAllMenus();return FReply::Handled();})
	 [SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted()).Text(SensorToolUiText::ResetUi())]];
	return Menu;
}
TSharedRef<SWidget> USensorToolToolbarWidget::BuildLegacyToolbar()
{
	RefreshSensors();auto Bar=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,6));
	Bar->AddSlot()[SAssignNew(SensorCombo,SComboBox<TSharedPtr<FString>>).OptionsSource(&SensorOptions).OnComboBoxOpening_Lambda([this](){RefreshSensors();if(SensorCombo.IsValid())SensorCombo->RefreshOptions();})
	.OnGenerateWidget_Lambda([](TSharedPtr<FString> Id){return SNew(STextBlock).Font(FSensorToolWorkspaceStyle::Font(16)).Text(FText::FromString(*Id));})
	.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Id,ESelectInfo::Type){if(Id)SelectSensor(*Id);})
	[SNew(STextBlock).Font(FSensorToolWorkspaceStyle::Font(16)).ColorAndOpacity(FSensorToolWorkspaceStyle::Text()).Text_Lambda([this](){return SelectedSensorText();})]];
	const FText Names[]={LOCTEXT("Monitor","모니터"),LOCTEXT("Settings","설정"),LOCTEXT("Data","데이터"),LOCTEXT("Replay","재생"),LOCTEXT("SlabCharts","Slab 차트"),LOCTEXT("SlabProgress","Slab 진행")};
	for(int32 I=0;I<6;++I){auto Role=static_cast<ESensorToolPanelRole>(I);Bar->AddSlot()[SNew(SButton).ButtonStyle(&FSensorToolWorkspaceStyle::Button()).ContentPadding(FMargin(12,7)).OnClicked_Lambda([this,Role](){if(auto* W=Workspace())W->SetPanelOpen(Role,!W->IsPanelOpen(Role));return FReply::Handled();})[SNew(STextBlock).Font(FSensorToolWorkspaceStyle::Font(16)).ColorAndOpacity_Lambda([this,Role](){auto* W=Workspace();return W&&W->IsPanelOpen(Role)?FSensorToolWorkspaceStyle::Accent():FSensorToolWorkspaceStyle::Text();}).Text(Names[I])]];}
	Bar->AddSlot()[SNew(SComboButton).OnGetMenuContent_Lambda([this](){auto Menu=SNew(SVerticalBox);for(float Scale:{.85f,1.0f,1.25f,1.5f})Menu->AddSlot().AutoHeight()[SNew(SButton).Text(FText::FromString(FString::Printf(TEXT("글자 %d%%"),FMath::RoundToInt(Scale*100)))).OnClicked_Lambda([this,Scale](){if(auto* W=Workspace())W->SetOwnedPanelFontScale(Scale);return FReply::Handled();})];Menu->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("Reset","센서 도구 UI 초기화")).OnClicked_Lambda([this](){if(auto* W=Workspace())W->ResetOwnedWorkspaceLayout();return FReply::Handled();})];return Menu;}).ButtonContent()[SNew(STextBlock).Font(FSensorToolWorkspaceStyle::Font(16)).Text(LOCTEXT("Appearance","UI 표시"))]];
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FSensorToolWorkspaceStyle::Panel()).Padding(10)[Bar];
}
#undef LOCTEXT_NAMESPACE
