#include "AgentTestSetWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetSubsystem.h"
#include "Core/DxWidgetSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
UAgentTestSetSubsystem* UAgentTestSetWidget::Service() const{return GetWorld()?GetWorld()->GetSubsystem<UAgentTestSetSubsystem>():nullptr;}
FSlateFontInfo UAgentTestSetWidget::Font(int32 Size) const{return FSensorOwnedPresentationStyle::Font(Size,GetSensorToolVisualScale());}
void UAgentTestSetWidget::SelectScenario(int32 Index)
{
    if(auto* S=Service())if(S->Items().IsValidIndex(Index))
    {
        Selected=Index;FString Error;
        if(Ready&&S->Items()[Index].State==EAgentTestSetItemState::Completed)
        {if(!S->SelectPreview(Index,Error))Status=FText::FromString(Error);}
        else Status=FText::FromString(S->Items()[Index].Message);
        Refresh();
    }
}
void UAgentTestSetWidget::Refresh()
{
    auto* S=Service();if(!S)return;const auto& C=S->Settings();Ready=S->IsComplete();
    Summary=FText::FromString(FString::Printf(TEXT("완료 %d / %d"),S->Completed(),C.Target));Progress=float(S->Completed())/C.Target;
    if(FPlatformTime::Seconds()>ConfirmUntil)Status=FText::FromString(S->Message());
    const auto View=S->PreviewStatus();PreviewVisible=View.Scenario!=INDEX_NONE;
    PreviewText=FText::FromString(PreviewVisible?FString::Printf(TEXT("%s · 프레임 %d / %d · 점 %d개"),*View.Message,View.Frame+1,View.Frames,View.Points):TEXT("완료 시나리오를 선택하면 저장된 점군을 봅니다"));
    auto* Texture=S->PreviewTexture();PreviewBrush.SetResourceObject(Texture);PreviewBrush.ImageSize=Texture?FVector2D(Texture->GetSizeX(),Texture->GetSizeY()):FVector2D(512,512);
    PreviewBrush.DrawAs=Texture?ESlateBrushDrawType::Image:ESlateBrushDrawType::NoDrawType;
    if(Details)
    {
        double Seconds=0;int32 Done=0;for(const auto& I:S->Items())if(I.State==EAgentTestSetItemState::Completed){Seconds+=I.Seconds;++Done;}
        FString Text=FString::Printf(TEXT("트리거 소재 번호 %s · 등록 %d개 · 매 %d행 저장\n%s · %s\n%s"),*C.Trigger,S->Items().Num(),C.SampleStride,C.Pipeline?TEXT("병렬 저장"):TEXT("순차 저장"),C.CaptureCamera?TEXT("카메라 포함"):TEXT("LiDAR PCD + 라벨"),Done?*FString::Printf(TEXT("예상 남은 시간 %.0f초"),Seconds/Done*(C.Target-Done)):TEXT("예상 시간 계산 대기"));
        Text+=FString::Printf(TEXT("\n정답 %s · IMU %s · 사실성 %s · 생성 후 변환 %s"),C.CameraTruth?TEXT("켬"):TEXT("끔"),C.IncludeImu?TEXT("켬"):TEXT("끔"),C.LidarFidelity?TEXT("켬"):TEXT("끔"),*C.ExportFormat);
        if(S->Items().IsValidIndex(Selected)){const auto& I=S->Items()[Selected];Text+=FString::Printf(TEXT("\n%s\n%s · %d / %d 프레임 · %.1f초"),*I.ScenarioId,*I.Message,I.CompletedFrames,I.Frames,I.Seconds);}
        DetailText=FText::FromString(Text);
    }
    if(!List||SeenRevision==S->Revision())return;SeenRevision=S->Revision();
    if(Rows.Num()!=S->Items().Num())
    {
        List->ClearChildren();Rows.Reset();RowMessages.Reset();RowStates.Reset();
        for(int32 N=0;N<S->Items().Num();++N)
        {
            TSharedPtr<STextBlock> Label;
            auto Row=SNew(SHorizontalBox)
              +SHorizontalBox::Slot().FillWidth(1)
              [SNew(SButton).ButtonStyle(&GetToolButtonStyle())
               .OnClicked_Lambda([this,N](){SelectScenario(N);return FReply::Handled();})
               [SAssignNew(Label,STextBlock).Font_Lambda([this](){return Font(13);}).ColorAndOpacity_Lambda([this,N](){return N==Selected?GetToolAccentColor():GetToolTextColor();}).AutoWrapText(true)]]
              +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)
              [SNew(SButton).ButtonStyle(&GetToolButtonStyle()).Visibility_Lambda([this,N](){auto* V=Service();return V&&V->Items().IsValidIndex(N)&&V->Items()[N].State==EAgentTestSetItemState::Failed?EVisibility::Visible:EVisibility::Collapsed;})
               .OnClicked_Lambda([this,N](){FString Error;if(auto* V=Service())if(!V->Retry(N,Error))Status=FText::FromString(Error);return FReply::Handled();})
               [SNew(STextBlock).Font_Lambda([this](){return Font(13);}).Text(FText::FromString(TEXT("재시도")))]];
            List->AddSlot().AutoHeight().Padding(0,3)[Row];Rows.Add(Label);RowMessages.Add(FString());RowStates.Add(255);
        }
    }
    for(int32 N=0;N<S->Items().Num();++N)
    {
        const auto& I=S->Items()[N];
        if(RowStates[N]!=uint8(I.State))
        {
            RowStates[N]=uint8(I.State);const TCHAR* State=I.State==EAgentTestSetItemState::Waiting?TEXT("대기"):I.State==EAgentTestSetItemState::Generating?TEXT("생성 중"):I.State==EAgentTestSetItemState::Completed?TEXT("완료"):TEXT("실패");
            Rows[N]->SetText(FText::FromString(FString::Printf(TEXT("%03d · %s · %s"),N+1,*I.ScenarioId.Left(8),State)));
        }
        if(RowMessages[N]!=I.Message){RowMessages[N]=I.Message;Rows[N]->SetToolTipText(FText::FromString(I.ScenarioId+TEXT("\n")+I.Message));}
    }
}
TSharedRef<SWidget> UAgentTestSetWidget::RebuildWidget()
{
    auto Text=[this](TAttribute<FText> Value,int32 Size=13){return SNew(STextBlock).Font_Lambda([this,Size](){return Font(Size);}).Text(Value).AutoWrapText(true).ColorAndOpacity(GetToolTextColor());};
    auto Button=[&](const TCHAR* Name,TFunction<void()> Action){return SNew(SButton).ButtonStyle(&GetToolButtonStyle()).OnClicked_Lambda([Action](){Action();return FReply::Handled();})[SNew(STextBlock).Font_Lambda([this](){return Font(13);}).Text(FText::FromString(Name)).ColorAndOpacity(GetToolTextColor())];};
    auto Controls=SNew(SWrapBox).UseAllottedSize(true)
      +SWrapBox::Slot().Padding(0,0,8,0)
       [SNew(SButton).ButtonStyle(&GetToolButtonStyle()).OnClicked_Lambda([this](){if(auto* S=Service())S->SetPaused(!S->IsPaused());return FReply::Handled();})
        [Text(TAttribute<FText>::CreateLambda([this](){auto* S=Service();return FText::FromString(S&&S->IsPaused()?TEXT("재개"):TEXT("일시정지"));}))]]
      +SWrapBox::Slot()[Button(TEXT("결과 폴더"),[this](){if(auto* S=Service()){const auto Dir=S->Directory();if(!Dir.IsEmpty())FPlatformProcess::ExploreFolder(*Dir);}})];
    auto PreviewControls=SNew(SWrapBox).UseAllottedSize(true)
      +SWrapBox::Slot().Padding(0,0,8,0)[Button(TEXT("재생"),[this](){if(auto* S=Service())S->SetPreviewPaused(false);})]
      +SWrapBox::Slot().Padding(0,0,8,0)[Button(TEXT("일시정지"),[this](){if(auto* S=Service())S->SetPreviewPaused(true);})]
      +SWrapBox::Slot()[Button(TEXT("중지"),[this](){if(auto* S=Service())S->StopPreview();Selected=INDEX_NONE;Refresh();})];
    auto Contents=SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[Text(TAttribute<FText>::CreateLambda([this](){return Summary;}),16)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBox).HeightOverride(4)[SNew(SProgressBar).Style(&FSensorOwnedPresentationStyle::Progress()).FillColorAndOpacity(GetToolAccentColor()).Percent_Lambda([this](){return Progress;})]]
      +SVerticalBox::Slot().AutoHeight()[Controls]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Text(TAttribute<FText>::CreateLambda([this](){return Status;}))]
      +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
       [SNew(SBox).Visibility_Lambda([this](){auto* S=Service();return S&&S->HasBusyWork()?EVisibility::Visible:EVisibility::Collapsed;})
        [Button(TEXT("사용 중인 작업 중지하고 시작"),[this](){FString Error;if(auto* S=Service())if(!S->StopBusyWorkAndStart(Error))Status=FText::FromString(Error);})]]
      +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(List,SVerticalBox)]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[Text(TAttribute<FText>::CreateLambda([this](){return PreviewText;}),11)]
      +SVerticalBox::Slot().FillHeight(TAttribute<float>::CreateLambda([this](){return PreviewVisible?1.f:0.f;}))
       [SNew(SBox).Visibility_Lambda([this](){return PreviewVisible?EVisibility::Visible:EVisibility::Collapsed;})[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&PreviewBrush)]]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)
       [SNew(SBox).Visibility_Lambda([this](){return PreviewVisible?EVisibility::Visible:EVisibility::Collapsed;})[PreviewControls]]
      +SVerticalBox::Slot().AutoHeight()
       [SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton()).OnClicked_Lambda([this](){Details=!Details;Refresh();return FReply::Handled();})
        [Text(TAttribute<FText>::CreateLambda([this](){return FText::FromString(Details?TEXT("상세 접기"):TEXT("상세 열기"));}))]]
      +SVerticalBox::Slot().AutoHeight()
       [SNew(SBox).Visibility_Lambda([this](){return Details?EVisibility::Visible:EVisibility::Collapsed;})
        [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(TAttribute<FText>::CreateLambda([this](){return DetailText;}),11)]
          +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Button(TEXT("세트 초기화"),[this](){if(auto* S=Service())if(FPlatformTime::Seconds()>ConfirmUntil){ConfirmUntil=FPlatformTime::Seconds()+8;Status=FText::FromString(TEXT("8초 안에 다시 누르면 목록을 비웁니다 · 기존 파일 유지"));}else{FString Error;S->ResetConfirmed(Error);ConfirmUntil=0;Selected=INDEX_NONE;SeenRevision=-1;Refresh();}})]]];
    SeenRevision=-1;Rows.Reset();RowMessages.Reset();RowStates.Reset();Refresh();
    return BuildToolPanelFrame(SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).Padding(16)
      [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(FText::FromString(TEXT("AI 시나리오 테스트셋")))]
        +SVerticalBox::Slot().FillHeight(1).Padding(0,12,0,0)[SNew(SBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})[Contents]]]);
}
void UAgentTestSetWidget::NativeTick(const FGeometry& Geometry,float Delta)
{Super::NativeTick(Geometry,Delta);const double Now=FPlatformTime::Seconds();if(IsVisible()&&!IsPanelCollapsed()&&Now>=NextRefresh){NextRefresh=Now+.2;Refresh();}}
void UAgentTestSetWidget::NativeDestruct()
{PreviewBrush.SetResourceObject(nullptr);Rows.Reset();List.Reset();if(auto* GI=GetGameInstance())if(auto* Dx=GI->GetSubsystem<UDxWidgetSubsystem>())Dx->UnregisterWidget(this);Super::NativeDestruct();}
