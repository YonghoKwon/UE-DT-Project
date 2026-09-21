#include "SensorToolToolbarWidget.h"
#include "SensorToolWorkspaceStyle.h"
#include "SensorOwnedPresentationStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
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
	int32 Camera=0,Lidar=0;
	for(auto* A:C->GetSensorActors())if(A){const bool IsCamera=A->GetSensorKind()==EVirtualSensorKind::Camera;if(A->GetSensorId()==Id){if(IsCamera){C->SelectCameraByIndex(Camera);C->SetViewMode(EVirtualSensorViewMode::Camera);}else{C->SelectLidarByIndex(Lidar);C->SetViewMode(EVirtualSensorViewMode::Lidar);}W->SynchronizeOwnedSelection();return;}if(IsCamera)++Camera;else++Lidar;}
}
FText USensorToolToolbarWidget::SelectedSensorText() const
{
	auto* W=Workspace();auto* C=W?W->GetCoordinator():nullptr;FString Id=TEXT("센서 없음");
	if(C)if(auto* A=C->GetSelectedSensorActor())Id=(A->GetSensorKind()==EVirtualSensorKind::Lidar?TEXT("LiDAR · "):TEXT("Camera · "))+A->GetSensorId();
	return FText::FromString(Id);
}
TSharedRef<SWidget> USensorToolToolbarWidget::RebuildWidget()
{
	auto* W=Workspace();
	if(!W||!W->OwnsToolbar(this)) return BuildLegacyToolbar();
	RefreshSensors();
	auto Bar=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
	Bar->AddSlot()
	[SAssignNew(SensorCombo,SComboBox<TSharedPtr<FString>>).ComboBoxStyle(&FSensorOwnedPresentationStyle::ComboBox())
	 .OptionsSource(&SensorOptions).ForegroundColor(FSensorOwnedPresentationStyle::Text())
	 .OnComboBoxOpening_Lambda([this](){RefreshSensors();if(SensorCombo)SensorCombo->RefreshOptions();})
	 .OnGenerateWidget_Lambda([this](TSharedPtr<FString> Id){return SNew(STextBlock).Font(ToolFont()).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text(FText::FromString(Id?*Id:FString()));})
	 .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Id,ESelectInfo::Type){if(Id)SelectSensor(*Id);})
	 [SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text_Lambda([this](){return SelectedSensorText();})]];
	for(bool Slab:{false,true})
		Bar->AddSlot()[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton())
		 .OnGetMenuContent_Lambda([this,Slab](){return BuildPanelMenu(Slab);})
		 .ButtonContent()[SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text(Slab?LOCTEXT("SlabTools","Slab 도구"):LOCTEXT("SensorTools","센서 도구"))]];
	Bar->AddSlot()[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton())
	 .OnGetMenuContent_Lambda([this](){return BuildAppearanceMenu();})
	 .ButtonContent()[SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted()).Text(LOCTEXT("Appearance","UI 표시"))]];
	return SNew(SBorder).BorderImage(FSensorOwnedPresentationStyle::PanelBrush()).BorderBackgroundColor(FSensorOwnedPresentationStyle::Panel()).Padding(8)[Bar];
}
FSlateFontInfo USensorToolToolbarWidget::ToolFont(int32 Size) const
{
	const auto* W=Workspace();return FSensorToolWorkspaceStyle::Font(Size,W&&W->OwnsToolbar(this)?W->GetOwnedPanelFontScale():1.0f);
}
TSharedRef<SWidget> USensorToolToolbarWidget::BuildPanelMenu(bool Slab)
{
	auto Menu=SNew(SVerticalBox);
	const ESensorToolPanelRole Roles[]={Slab?ESensorToolPanelRole::SlabProgress:ESensorToolPanelRole::Monitor,Slab?ESensorToolPanelRole::SlabCharts:ESensorToolPanelRole::Settings,Slab?ESensorToolPanelRole::Replay:ESensorToolPanelRole::Data};
	const FText Names[]={Slab?LOCTEXT("SlabProgress","진행"):LOCTEXT("Monitor","모니터"),Slab?LOCTEXT("SlabCharts","차트"):LOCTEXT("Settings","설정"),Slab?LOCTEXT("Replay","재생 목록"):LOCTEXT("Data","데이터")};
	for(int32 I=0;I<3;++I)
	{
		const auto Role=Roles[I];const FText Label=Names[I];
		Menu->AddSlot().AutoHeight().Padding(0,4)
		[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button())
		 .OnClicked_Lambda([this,Role](){if(auto* W=Workspace())W->SetPanelOpen(Role,!W->IsPanelOpen(Role));FSlateApplication::Get().DismissAllMenus();return FReply::Handled();})
		 [SNew(STextBlock).Font_Lambda([this](){return ToolFont();})
		  .ColorAndOpacity_Lambda([this,Role](){const auto* W=Workspace();return W&&W->IsPanelOpen(Role)?FSensorOwnedPresentationStyle::Accent():FSensorOwnedPresentationStyle::Text();})
		  .Text_Lambda([this,Role,Label](){const auto* W=Workspace();return FText::Format(LOCTEXT("PanelMenuItem","{0}  {1}"),FText::FromString(W&&W->IsPanelOpen(Role)?TEXT("●"):TEXT("○")),Label);})]];
	}
	return Menu;
}
TSharedRef<SWidget> USensorToolToolbarWidget::BuildAppearanceMenu()
{
	auto Menu=SNew(SVerticalBox);
	for(float Scale:{.85f,1.0f,1.25f,1.5f})
		Menu->AddSlot().AutoHeight().Padding(0,4)[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button())
		 .OnClicked_Lambda([this,Scale](){if(auto* W=Workspace())W->SetOwnedPanelFontScale(Scale);FSlateApplication::Get().DismissAllMenus();return FReply::Handled();})
		 [SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text())
		  .Text(FText::Format(LOCTEXT("FontPercent","글자 {0}%"),FText::AsNumber(FMath::RoundToInt(Scale*100))))]];
	Menu->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button())
	 .OnClicked_Lambda([this](){if(auto* W=Workspace())W->ResetOwnedWorkspaceLayout();FSlateApplication::Get().DismissAllMenus();return FReply::Handled();})
	 [SNew(STextBlock).Font_Lambda([this](){return ToolFont();}).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted()).Text(LOCTEXT("Reset","센서 도구 UI 초기화"))]];
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
