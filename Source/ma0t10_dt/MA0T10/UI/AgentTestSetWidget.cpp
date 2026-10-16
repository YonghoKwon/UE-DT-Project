#include "AgentTestSetWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorToolWorkspaceStyle.h"
#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetSubsystem.h"
#include "Core/DxWidgetSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
UAgentTestSetSubsystem* UAgentTestSetWidget::Service() const{return GetWorld()?GetWorld()->GetSubsystem<UAgentTestSetSubsystem>():nullptr;}
FSlateFontInfo UAgentTestSetWidget::Font(int32 Size) const
{const auto* W=GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();return FSensorToolWorkspaceStyle::Font(Size,W?W->GetOwnedPanelFontScale():1.f);}
void UAgentTestSetWidget::Refresh()
{
    auto* S=Service();if(!S)return;const auto& C=S->Settings();const bool WasReady=Ready;Ready=S->IsComplete();if(WasReady!=Ready)SeenRevision=-1;
    ConfigText=FText::FromString(FString::Printf(TEXT("트리거 소재 번호 %s · 목표 %d개 · 매 %d행 획득 · %s · %s"),*C.Trigger,C.Target,C.SampleStride,C.Pipeline?TEXT("백그라운드 병렬 저장"):TEXT("순차 저장"),C.CaptureCamera?TEXT("카메라 포함"):TEXT("LiDAR PCD + 라벨")));
    double Seconds=0;int32 Done=0;for(const auto& I:S->Items())if(I.State==EAgentTestSetItemState::Completed){Seconds+=I.Seconds;++Done;}
    const FString Eta=Done?FString::Printf(TEXT("예상 남은 시간 %.0f초"),Seconds/Done*(C.Target-Done)):TEXT("예상 남은 시간 계산 대기");
    Summary=FText::FromString(FString::Printf(TEXT("생성 완료 %d / %d · 등록 %d개 · %s"),S->Completed(),C.Target,S->Items().Num(),*Eta));Progress=float(S->Completed())/C.Target;Status=FText::FromString(S->Message());
    if(!List||SeenRevision==S->Revision())return;SeenRevision=S->Revision();List->ClearChildren();
    const bool OptionsChanged=Options.Num()!=(Ready?S->Items().Num():0);if(OptionsChanged)Options.Reset();
    for(int32 N=0;N<S->Items().Num();++N)
    {
        const auto& I=S->Items()[N];const TCHAR* State=I.State==EAgentTestSetItemState::Waiting?TEXT("대기"):I.State==EAgentTestSetItemState::Generating?TEXT("생성 중"):I.State==EAgentTestSetItemState::Completed?TEXT("완료"):TEXT("실패");
        const auto Text=FText::FromString(FString::Printf(TEXT("%03d · %s · %s · %d/%d 프레임 · %.1f초"),N+1,*I.ScenarioId.Left(8),State,I.CompletedFrames,I.Frames,I.Seconds));
        auto Row=SNew(SHorizontalBox);Row->AddSlot().FillWidth(1)[SNew(STextBlock).Font(Font(13)).Text(Text).ToolTipText(FText::FromString(I.ScenarioId+TEXT("\n")+I.Message)).AutoWrapText(true).ColorAndOpacity(FSensorOwnedPresentationStyle::Text())];
        if(I.State==EAgentTestSetItemState::Failed)Row->AddSlot().AutoWidth()[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button()).Text(FText::FromString(TEXT("재시도"))).OnClicked_Lambda([this,N](){FString E;if(!Service()->Retry(N,E))Status=FText::FromString(E);return FReply::Handled();})];
        List->AddSlot().AutoHeight().Padding(0,4)[Row];if(Ready&&OptionsChanged)Options.Add(MakeShared<int32>(N));
    }
    if(OptionsChanged&&Select){Select->ClearSelection();Select->RefreshOptions();}
}
TSharedRef<SWidget> UAgentTestSetWidget::RebuildWidget()
{
    SelectedText=FText::FromString(TEXT("모두 생성한 뒤 시나리오 선택"));
    auto Text=[this](TAttribute<FText> Value,int32 Size=13){return SNew(STextBlock).Font_Lambda([this,Size](){return Font(Size);}).Text(Value).AutoWrapText(true).ColorAndOpacity(FSensorOwnedPresentationStyle::Text());};
    auto Controls=SNew(SHorizontalBox);
    auto Button=[&](const TCHAR* Name,TFunction<void()> Action){Controls->AddSlot().AutoWidth().Padding(0,0,8,0)[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button()).Text(FText::FromString(Name)).OnClicked_Lambda([Action](){Action();return FReply::Handled();})];};
    Button(TEXT("일시 중지"),[this](){Service()->SetPaused(true);});Button(TEXT("재개"),[this](){Service()->SetPaused(false);});Button(TEXT("결과 폴더"),[this](){const auto Dir=Service()->Directory();if(!Dir.IsEmpty())FPlatformProcess::ExploreFolder(*Dir);});
    Button(TEXT("세트 초기화"),[this](){if(FPlatformTime::Seconds()>ConfirmUntil){ConfirmUntil=FPlatformTime::Seconds()+8;Status=FText::FromString(TEXT("8초 안에 세트 초기화를 다시 누르면 목록을 비웁니다 · 기존 파일 유지"));}else {FString E;Service()->ResetConfirmed(E);ConfirmUntil=0;SeenRevision=-1;Options.Reset();SelectedText=FText::FromString(TEXT("모두 생성한 뒤 시나리오 선택"));}});
    auto ViewControls=SNew(SHorizontalBox);
    ViewControls->AddSlot().FillWidth(1)[SAssignNew(Select,SComboBox<TSharedPtr<int32>>).OptionsSource(&Options).IsEnabled_Lambda([this](){return Ready;})
      .OnGenerateWidget_Lambda([this](TSharedPtr<int32> I){const auto& Items=Service()->Items();return SNew(STextBlock).Font(Font(13)).Text(FText::FromString(I&&Items.IsValidIndex(*I)?FString::Printf(TEXT("%03d · %s"),*I+1,*Items[*I].ScenarioId):FString()));})
      .OnSelectionChanged_Lambda([this](TSharedPtr<int32> I,ESelectInfo::Type){if(!I)return;FString E;if(Service()->SelectPreview(*I,E))SelectedText=FText::FromString(FString::Printf(TEXT("시나리오 %03d · 보기 전용 · 전송 꺼짐"),*I+1));else Status=FText::FromString(E);})
      [Text(TAttribute<FText>::CreateLambda([this](){return SelectedText;}))]];
    ViewControls->AddSlot().AutoWidth().Padding(8,0,0,0)[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button()).Text(FText::FromString(TEXT("보기 중지"))).OnClicked_Lambda([this](){Service()->StopPreview();return FReply::Handled();})];
    auto Body=SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Text(FText::FromString(TEXT("AI 시나리오 테스트셋 · 임시 기능")),18)]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button()).Text(FText::FromString(TEXT("닫기"))).OnClicked_Lambda([this](){CloseWidget();return FReply::Handled();})]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Text(TAttribute<FText>::CreateLambda([this](){return ConfigText;}))]
      +SVerticalBox::Slot().AutoHeight()[Text(TAttribute<FText>::CreateLambda([this](){return Summary;}))]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SProgressBar).Percent_Lambda([this](){return Progress;})]
      +SVerticalBox::Slot().AutoHeight()[Controls]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Text(TAttribute<FText>::CreateLambda([this](){return Status;}))]
      +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(List,SVerticalBox)]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)[ViewControls]
      +SVerticalBox::Slot().AutoHeight()[Text(FText::FromString(TEXT("일반 시나리오 우선 · 화면 숨김 중에도 생성 유지 · 전송은 이후 구현 예정")),11)];
    SeenRevision=-1;Refresh();return SNew(SBorder).BorderImage(FSensorOwnedPresentationStyle::PanelBrush()).BorderBackgroundColor(FSensorOwnedPresentationStyle::Panel()).Padding(16)[Body];
}
void UAgentTestSetWidget::NativeTick(const FGeometry& Geometry,float Delta)
{Super::NativeTick(Geometry,Delta);const double Now=FPlatformTime::Seconds();if(IsVisible()&&Now>=NextRefresh){NextRefresh=Now+.2;if(Now>ConfirmUntil)Refresh();}}
void UAgentTestSetWidget::NativeDestruct()
{if(auto* GI=GetGameInstance())if(auto* Dx=GI->GetSubsystem<UDxWidgetSubsystem>()){Dx->UnregisterExternalInputBlocker(this);Dx->UnregisterWidget(this);}List.Reset();Select.Reset();Super::NativeDestruct();}
