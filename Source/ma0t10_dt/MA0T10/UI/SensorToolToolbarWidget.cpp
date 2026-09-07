#include "SensorToolToolbarWidget.h"
#include "SensorToolWorkspaceStyle.h"
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
	for(auto* A:C->GetSensorActors())if(A){const bool IsCamera=A->GetSensorKind()==EVirtualSensorKind::Camera;if(A->GetSensorId()==Id){if(IsCamera){C->SelectCameraByIndex(Camera);C->SetViewMode(EVirtualSensorViewMode::Camera);}else{C->SelectLidarByIndex(Lidar);C->SetViewMode(EVirtualSensorViewMode::Lidar);}return;}if(IsCamera)++Camera;else++Lidar;}
}
FText USensorToolToolbarWidget::SelectedSensorText() const
{
	auto* W=Workspace();auto* C=W?W->GetCoordinator():nullptr;FString Id=TEXT("센서 없음");
	if(C)if(auto* A=C->GetSelectedSensorActor())Id=(A->GetSensorKind()==EVirtualSensorKind::Lidar?TEXT("LiDAR · "):TEXT("Camera · "))+A->GetSensorId();
	return FText::FromString(Id);
}
TSharedRef<SWidget> USensorToolToolbarWidget::RebuildWidget()
{
	RefreshSensors();auto Bar=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,6));
	Bar->AddSlot()[SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&SensorOptions).OnComboBoxOpening_Lambda([this](){RefreshSensors();})
	.OnGenerateWidget_Lambda([](TSharedPtr<FString> Id){return SNew(STextBlock).Font(FSensorToolWorkspaceStyle::Font(16)).Text(FText::FromString(*Id));})
	.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Id,ESelectInfo::Type){if(Id)SelectSensor(*Id);})
	[SNew(STextBlock).Font(FSensorToolWorkspaceStyle::Font(16)).ColorAndOpacity(FSensorToolWorkspaceStyle::Text()).Text_Lambda([this](){return SelectedSensorText();})]];
	const FText Names[]={LOCTEXT("Monitor","모니터"),LOCTEXT("Settings","설정"),LOCTEXT("Data","데이터"),LOCTEXT("Replay","재생")};
	for(int32 I=0;I<4;++I){auto Role=static_cast<ESensorToolPanelRole>(I);Bar->AddSlot()[SNew(SButton).ButtonStyle(&FSensorToolWorkspaceStyle::Button()).ContentPadding(FMargin(12,7)).OnClicked_Lambda([this,Role](){if(auto* W=Workspace())W->SetPanelOpen(Role,!W->IsPanelOpen(Role));return FReply::Handled();})[SNew(STextBlock).Font(FSensorToolWorkspaceStyle::Font(16)).ColorAndOpacity_Lambda([this,Role](){auto* W=Workspace();return W&&W->IsPanelOpen(Role)?FSensorToolWorkspaceStyle::Accent():FSensorToolWorkspaceStyle::Text();}).Text(Names[I])]];}
	Bar->AddSlot()[SNew(SComboButton).OnGetMenuContent_Lambda([this](){auto Menu=SNew(SVerticalBox);for(float Scale:{.85f,1.0f,1.25f,1.5f})Menu->AddSlot().AutoHeight()[SNew(SButton).Text(FText::FromString(FString::Printf(TEXT("글자 %d%%"),FMath::RoundToInt(Scale*100)))).OnClicked_Lambda([this,Scale](){if(auto* W=Workspace())W->SetOwnedPanelFontScale(Scale);return FReply::Handled();})];Menu->AddSlot().AutoHeight()[SNew(SButton).Text(LOCTEXT("Reset","센서 도구 UI 초기화")).OnClicked_Lambda([this](){if(auto* W=Workspace())W->ResetOwnedWorkspaceLayout();return FReply::Handled();})];return Menu;}).ButtonContent()[SNew(STextBlock).Font(FSensorToolWorkspaceStyle::Font(16)).Text(LOCTEXT("Appearance","UI 표시"))]];
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FSensorToolWorkspaceStyle::Panel()).Padding(10)[Bar];
}
#undef LOCTEXT_NAMESPACE
