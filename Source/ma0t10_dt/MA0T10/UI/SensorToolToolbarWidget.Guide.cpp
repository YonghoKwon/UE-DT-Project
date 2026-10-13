#include "SensorToolToolbarWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorPlacementInteraction.h"
#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SensorManipulationGuide"

// UXR-13: users entering 이동·회전 did not know the keys (only a small muted line in
// the placement panel). Show a hit-test-invisible guide over the 3D view they are
// already looking at, only while an owned manipulation is active. No Tick: the
// visibility lambda reads existing interaction state during paint.
bool USensorToolToolbarWidget::IsOwnedManipulationActive() const
{
	auto* W=Workspace();
	return W&&W->OwnsToolbar(this)&&W->HasPlacementInteraction()&&W->GetPlacementInteraction()->IsManipulating();
}

TSharedRef<SWidget> USensorToolToolbarWidget::BuildManipulationGuide()
{
	auto Key=[this](const FText& Label)->TSharedRef<SWidget>
	{
		return SNew(SBorder).BorderImage(FSensorOwnedPresentationStyle::PanelBrush()).BorderBackgroundColor(FSensorOwnedPresentationStyle::Selection()).Padding(FMargin(8,3))
			[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::BodySize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Text()).Text(Label)];
	};
	auto Item=[this,Key](const FText& Keys,const FText& Meaning)->TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Key(Keys)]
			+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,0,0)[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::BodySize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted()).Text(Meaning)];
	};
	auto Row=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(16,8));
	Row->AddSlot()[Item(LOCTEXT("MoveKeys","W A S D"),LOCTEXT("Move","앞뒤·좌우"))];
	Row->AddSlot()[Item(LOCTEXT("UpDownKeys","Q E"),LOCTEXT("UpDown","아래·위"))];
	Row->AddSlot()[Item(LOCTEXT("RotateKeys","← → ↑ ↓"),LOCTEXT("Rotate","회전"))];
	Row->AddSlot()[Item(LOCTEXT("RollKeys","Z C"),LOCTEXT("Roll","기울기"))];
	Row->AddSlot()[Item(LOCTEXT("SpeedKeys","Shift / Ctrl"),LOCTEXT("Speed","빠르게 / 정밀"))];
	Row->AddSlot()[Item(LOCTEXT("LookKey","마우스 오른쪽 누른 채"),LOCTEXT("Look","내 시점 이동"))];
	Row->AddSlot()[Item(LOCTEXT("EndKey","Esc"),LOCTEXT("End","끝내기"))];
	return SNew(SBorder).Visibility_Lambda([this](){return IsOwnedManipulationActive()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
		.BorderImage(FSensorOwnedPresentationStyle::SectionBrush()).BorderBackgroundColor(FSensorOwnedPresentationStyle::Panel()).Padding(16)
		[SNew(SVerticalBox)
		 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::TitleSize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Accent())
			.Text(LOCTEXT("Title","장비 이동 중 · 지금 보는 3D 화면에서 움직입니다"))]
		 +SVerticalBox::Slot().AutoHeight()[Row]
		 // A stopped device keeps its no-new-measurement contract while moved, so its
		 // preview card does not refresh. Say so instead of leaving a dark card unexplained.
		 +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::BodySize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Warning()).AutoWrapText(true)
			.Visibility_Lambda([this](){auto* W=Workspace();auto* C=W?W->GetCoordinator():nullptr;auto* A=C?C->GetSelectedSensorActor():nullptr;return A&&!A->IsSensorRunning()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
			.Text(LOCTEXT("StoppedHint","장비가 정지 상태라 미리보기 화면은 바뀌지 않습니다. 3D 화면에서 장비 위치를 보며 옮기고, 장비가 보는 화면은 측정을 시작하면 확인할 수 있습니다."))]
		 +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SNew(STextBlock).Font_Lambda([this](){return ToolFont(FSensorOwnedPresentationStyle::LabelSize);}).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted()).AutoWrapText(true)
			.Text(LOCTEXT("FocusHint","키가 듣지 않으면 3D 화면을 한 번 클릭하세요. 장비의 화살표를 마우스로 끌어도 움직입니다."))]];
}

#undef LOCTEXT_NAMESPACE
