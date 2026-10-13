#include "SensorToolToolbarWidget.h"
#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorToolUiText.h"
#include "SensorToolIcon.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

bool USensorToolToolbarWidget::IsSidebarRouteSelected(ESensorToolDestination D) const
{
    const auto* W=Workspace();if(!W)return false;
    if(D==ESensorToolDestination::Preview)return !W->IsSidebarExpanded()&&W->IsPanelOpen(ESensorToolPanelRole::Monitor);
    return IsOwnedDestinationVisible(D);
}
TSharedRef<SWidget> USensorToolToolbarWidget::BuildSidebarShell()
{
    auto Rail=SNew(SVerticalBox);
    const ESensorToolDestination Routes[]={ESensorToolDestination::Placement,ESensorToolDestination::Preview,ESensorToolDestination::Scenario,ESensorToolDestination::PcdTransfer,ESensorToolDestination::Results};
    const FText Labels[]={SensorToolUiText::Placement(),FText::FromString(TEXT("보기")),SensorToolUiText::Scenario(),FText::FromString(TEXT("전송")),SensorToolUiText::Results()};
    for(int32 I=0;I<UE_ARRAY_COUNT(Routes);++I)
    {
        const auto Route=Routes[I];
        Rail->AddSlot().AutoHeight().Padding(0,4)[SNew(SOverlay)
          +SOverlay::Slot()[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::RailButton()).ContentPadding(FMargin(4,8)).ToolTipText(Labels[I])
            .ButtonColorAndOpacity_Lambda([this,Route](){return IsSidebarRouteSelected(Route)?FSensorOwnedPresentationStyle::Selection():FSensorOwnedPresentationStyle::Panel();})
            .OnClicked_Lambda([this,Route](){ToggleOwnedDestination(Route);return FReply::Handled();})
            [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride_Lambda([this](){return 18.f*(Workspace()?Workspace()->GetOwnedPanelFontScale()*Workspace()->GetOwnedPixelScale():1.f);}).HeightOverride_Lambda([this](){return 18.f*(Workspace()?Workspace()->GetOwnedPanelFontScale()*Workspace()->GetOwnedPixelScale():1.f);})[SNew(SSensorToolIcon).Kind(I).Color_Lambda([this,Route](){return IsSidebarRouteSelected(Route)?FSensorOwnedPresentationStyle::Accent():FSensorOwnedPresentationStyle::Muted();})]]
             +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0).HAlign(HAlign_Center)[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::LabelSize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text(Labels[I])]]]
          +SOverlay::Slot().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(3).Visibility(EVisibility::HitTestInvisible)
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(0).BorderBackgroundColor_Lambda([this,Route](){return IsSidebarRouteSelected(Route)?FSensorOwnedPresentationStyle::Primary():FLinearColor::Transparent;})]]];
    }
    Rail->AddSlot().FillHeight(1)[SNullWidget::NullWidget];
    Rail->AddSlot().AutoHeight().Padding(8)[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton()).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton()).HasDownArrow(false)
        .ToolTipText(SensorToolUiText::Tools()).OnGetMenuContent_Lambda([this](){return BuildToolsMenu();}).ButtonContent()
        [SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::BodySize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted()).Text(FText::FromString(TEXT("⋯")))]];
    auto Summary=SNew(SVerticalBox);
    Summary->AddSlot().AutoHeight()[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::BodySize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).OverflowPolicy(ETextOverflowPolicy::Ellipsis).Text_Lambda([this](){return SlabCardTitle;})];
    Summary->AddSlot().AutoHeight().Padding(0,6)[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton()).Visibility_Lambda([this](){return StatusSlab.IsValid()?EVisibility::Collapsed:EVisibility::Visible;}).OnClicked_Lambda([this](){OpenOwnedDestination(ESensorToolDestination::MapCheck);return FReply::Handled();})[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::LabelSize);}).Text(FText::FromString(TEXT("배치 방법 · 맵 점검")))]];
    Summary->AddSlot().AutoHeight().Padding(0,FSensorOwnedPresentationStyle::Gap)[SNew(SBox).HeightOverride(FSensorOwnedPresentationStyle::ProgressHeight)
        [SNew(SProgressBar).Style(&FSensorOwnedPresentationStyle::Progress()).BorderPadding(FVector2D::ZeroVector).Percent_Lambda([this](){return SlabCardProgress;}).FillColorAndOpacity(FSensorOwnedPresentationStyle::Primary())]];
    const FText Keys[]={FText::FromString(TEXT("기울기")),FText::FromString(TEXT("중심 이탈")),FText::FromString(TEXT("좌/우 여유"))};
    for(int32 I=0;I<3;++I)Summary->AddSlot().AutoHeight().Padding(0,2)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::LabelSize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted()).Text(Keys[I])]
        +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::LabelSize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text_Lambda([this,I](){return I==0?SlabCardAngle:I==1?SlabCardOffset:SlabCardMargin;})]];
    Summary->AddSlot().AutoHeight().Padding(0,FSensorOwnedPresentationStyle::Gap)[SNew(SCheckBox).Padding(FMargin(FSensorOwnedPresentationStyle::Gap,0)).IsEnabled_Lambda([this](){return StatusSlab.IsValid();})
        .IsChecked_Lambda([this](){return StatusSlab.IsValid()&&StatusSlab->GetDetailedDiagnosticLabels()?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([this](ECheckBoxState State){if(StatusSlab.IsValid())StatusSlab->SetDetailedDiagnosticLabels(State==ECheckBoxState::Checked);})
        .ToolTipText(FText::FromString(TEXT("기본은 소재번호·진행과 기울기입니다. 좌/우 여유·중심 이탈·원본 행은 상세에서 표시합니다.")))
        [SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::LabelSize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted()).Text(FText::FromString(TEXT("3D 상세 라벨")))]];
    return SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(0,0,0,56)[SNew(SBox).WidthOverride_Lambda([this](){return FSensorOwnedPresentationStyle::RailWidth*(Workspace()?Workspace()->GetOwnedPanelFontScale()*Workspace()->GetOwnedPixelScale():1);})
            [SNew(SBorder).BorderImage(FSensorOwnedPresentationStyle::PanelBrush()).BorderBackgroundColor(FSensorOwnedPresentationStyle::Panel()).Padding(0)[Rail]]]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(16)[SNew(SBox).WidthOverride_Lambda([this](){return 260*(Workspace()?Workspace()->GetOwnedPanelFontScale()*Workspace()->GetOwnedPixelScale():1);})
            [SAssignNew(SlabSummaryCard,SBorder).BorderImage(FSensorOwnedPresentationStyle::SectionBrush()).BorderBackgroundColor(FSensorOwnedPresentationStyle::Panel()).Padding(FSensorOwnedPresentationStyle::PanelPadding)[Summary]]]
        +SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)[SNew(SBox).HeightOverride_Lambda([this](){return FSensorOwnedPresentationStyle::StatusHeight*(Workspace()?Workspace()->GetOwnedPanelFontScale()*Workspace()->GetOwnedPixelScale():1);})
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FSensorOwnedPresentationStyle::Color(16,18,21)).Padding(FMargin(8,4))[BuildStatusStrip()]]];
}
