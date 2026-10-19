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
void UAgentTestSetWidget::ShowActionResult(bool Ok,const FString& Error)
{ActionErrorUntil=Ok?0:FPlatformTime::Seconds()+8;ActionError=FText::FromString(Error);SeenRevision=-1;Refresh();}
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
    const double Rate=S->ScenariosPerSecond(),Remaining=S->RemainingSeconds();
    Timing=FText::FromString(Ready?FString::Printf(TEXT("총 %.1f초"),S->TotalSeconds()):Rate>0?FString::Printf(TEXT("%.1f개/초 · 남은 시간 약 %.0f초"),Rate,FMath::Max(0.,Remaining)):TEXT("처리 속도 · 남은 시간 계산 대기"));
    if(FPlatformTime::Seconds()>ConfirmUntil)Status=FPlatformTime::Seconds()<ActionErrorUntil?ActionError:FText::FromString(S->PresentationMessage());
    const auto View=S->PreviewStatus();PreviewVisible=View.Scenario!=INDEX_NONE;
    PreviewText=FText::FromString(PreviewVisible?FString::Printf(TEXT("%s · 프레임 %d / %d · 점 %d개"),*View.Message,View.Frame+1,View.Frames,View.Points):TEXT("완료 시나리오를 선택하면 저장된 점군을 봅니다"));
    auto* Texture=S->PreviewTexture();PreviewBrush.SetResourceObject(Texture);PreviewBrush.ImageSize=Texture?FVector2D(Texture->GetSizeX(),Texture->GetSizeY()):FVector2D(512,512);
    PreviewBrush.DrawAs=Texture?ESlateBrushDrawType::Image:ESlateBrushDrawType::NoDrawType;
    if(Details)
    {
        FString Text=FString::Printf(TEXT("소재 번호 %s · 등록 %d개\n%s · %s"),*C.Trigger,S->Items().Num(),C.Pipeline?TEXT("병렬 저장"):TEXT("순차 저장"),C.CaptureCamera?TEXT("카메라 포함"):TEXT("LiDAR PCD + 라벨"));
        if(!C.AutoFillExamples)Text+=FString::Printf(TEXT("\n원본 매 %d행 저장"),C.SampleStride);
        Text+=FString::Printf(TEXT("\n정답 %s · IMU %s · 사실성 %s · 생성 후 변환 %s"),C.CameraTruth?TEXT("켬"):TEXT("끔"),C.IncludeImu?TEXT("켬"):TEXT("끔"),C.LidarFidelity?TEXT("켬"):TEXT("끔"),*C.ExportFormat);
        Text+=TEXT("\n")+S->SensorSelectionDescription()+TEXT("\n")+S->Directory();
        if(S->Items().IsValidIndex(Selected)){const auto& I=S->Items()[Selected];Text+=FString::Printf(TEXT("\n%s\n%s · %d / %d 프레임 · %.1f초"),*I.ScenarioId,*I.Message,I.CompletedFrames,I.Frames,I.Seconds);}
        DetailText=FText::FromString(Text);
    }
    if(!List||SeenRevision==S->Revision())return;SeenRevision=S->Revision();
    if(Rows.Num()!=S->Items().Num())
    {
        List->ClearChildren();Rows.Reset();RowMessages.Reset();RowStates.Reset();RowIds.Reset();
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
               .OnClicked_Lambda([this,N](){FString Error;if(auto* V=Service())ShowActionResult(V->Retry(N,Error),Error);return FReply::Handled();})
               [SNew(STextBlock).Font_Lambda([this](){return Font(13);}).Text(FText::FromString(TEXT("재시도")))]];
            List->AddSlot().AutoHeight().Padding(0,2)[Row];Rows.Add(Label);RowMessages.Add(FString());RowStates.Add(255);RowIds.Add(FString());
        }
    }
    for(int32 N=0;N<S->Items().Num();++N)
    {
        const auto& I=S->Items()[N];
        const FString Tip=I.ScenarioId+TEXT("\n")+I.Message;
        if(RowStates[N]!=uint8(I.State)||RowIds[N]!=I.ScenarioId||RowMessages[N]!=Tip)
        {
            RowStates[N]=uint8(I.State);const TCHAR* State=I.State==EAgentTestSetItemState::Waiting?TEXT("대기"):I.State==EAgentTestSetItemState::Generating?TEXT("생성 중"):I.State==EAgentTestSetItemState::Completed?TEXT("완료"):TEXT("실패");
            RowIds[N]=I.ScenarioId;
            Rows[N]->SetText(FText::FromString(FString::Printf(TEXT("%s  %03d  %s%s"),I.State==EAgentTestSetItemState::Completed?TEXT("✓"):I.State==EAgentTestSetItemState::Failed?TEXT("!"):I.State==EAgentTestSetItemState::Generating?TEXT("●"):TEXT("○"),N+1,State,I.State==EAgentTestSetItemState::Failed?*(TEXT(" · ")+I.Message):TEXT(""))));
        }
        if(RowMessages[N]!=Tip){RowMessages[N]=Tip;Rows[N]->SetToolTipText(FText::FromString(Tip));}
    }
}
TSharedRef<SWidget> UAgentTestSetWidget::RebuildWidget()
{
    auto Text=[this](TAttribute<FText> Value,int32 Size=13){return SNew(STextBlock).Font_Lambda([this,Size](){return Font(Size);}).Text(Value).AutoWrapText(true).ColorAndOpacity(GetToolTextColor());};
    auto Button=[&](const TCHAR* Name,TFunction<void()> Action){return SNew(SButton).ButtonStyle(&GetToolButtonStyle()).OnClicked_Lambda([Action](){Action();return FReply::Handled();})[SNew(STextBlock).Font_Lambda([this](){return Font(13);}).Text(FText::FromString(Name)).ColorAndOpacity(GetToolTextColor())];};
    auto Controls=SNew(SWrapBox).UseAllottedSize(true)
      +SWrapBox::Slot().Padding(0,0,8,0)
       [SNew(SButton).ButtonStyle(&GetToolButtonStyle()).Visibility_Lambda([this](){return Ready?EVisibility::Collapsed:EVisibility::Visible;}).OnClicked_Lambda([this](){if(auto* S=Service())S->SetPaused(!S->IsPaused());return FReply::Handled();})
        [Text(TAttribute<FText>::CreateLambda([this](){auto* S=Service();return FText::FromString(S&&S->IsPaused()?TEXT("재개"):TEXT("일시정지"));}))]]
      +SWrapBox::Slot().Padding(0,0,8,4)[Button(TEXT("결과 폴더"),[this](){if(auto* S=Service()){const auto Dir=S->Directory();if(!Dir.IsEmpty())FPlatformProcess::ExploreFolder(*Dir);}})]
      +SWrapBox::Slot().Padding(0,0,0,4)
       [SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton()).OnClicked_Lambda([this](){Details=!Details;Refresh();return FReply::Handled();})
        [Text(TAttribute<FText>::CreateLambda([this](){return FText::FromString(Details?TEXT("상세 접기"):TEXT("상세"));}))]];
    auto PreviewControls=SNew(SWrapBox).UseAllottedSize(true)
      +SWrapBox::Slot().Padding(0,0,8,0)[Button(TEXT("재생"),[this](){if(auto* S=Service())S->SetPreviewPaused(false);})]
      +SWrapBox::Slot().Padding(0,0,8,0)[Button(TEXT("일시정지"),[this](){if(auto* S=Service())S->SetPreviewPaused(true);})]
      +SWrapBox::Slot()[Button(TEXT("중지"),[this](){if(auto* S=Service())S->StopPreview();Selected=INDEX_NONE;Refresh();})];
    auto Contents=SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[Text(TAttribute<FText>::CreateLambda([this](){return Summary;}),16)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBox).HeightOverride(4)[SNew(SProgressBar).Style(&FSensorOwnedPresentationStyle::Progress()).FillColorAndOpacity(GetToolAccentColor()).Percent_Lambda([this](){return Progress;})]]
      +SVerticalBox::Slot().AutoHeight()[Text(TAttribute<FText>::CreateLambda([this](){return Timing;}),12)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)[Controls]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)
       [SNew(SBox).Visibility_Lambda([this](){return !Ready||FPlatformTime::Seconds()<=ConfirmUntil?EVisibility::Visible:EVisibility::Collapsed;})[Text(TAttribute<FText>::CreateLambda([this](){return Status;}))]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
       [SNew(SBox).Visibility_Lambda([this](){auto* S=Service();return S&&S->HasBusyWork()?EVisibility::Visible:EVisibility::Collapsed;})
        [Button(TEXT("사용 중인 작업 중지하고 시작"),[this](){FString Error;if(auto* S=Service())ShowActionResult(S->StopBusyWorkAndStart(Error),Error);})]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
       [SNew(SBox).Visibility_Lambda([this](){auto* S=Service();return S&&S->HasManifestFailure()?EVisibility::Visible:EVisibility::Collapsed;})
        [Button(TEXT("목록 저장 재시도"),[this](){FString Error;if(auto* S=Service())ShowActionResult(S->Retry(INDEX_NONE,Error),Error);})]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
       [SNew(SBox).Visibility_Lambda([this](){return Ready?EVisibility::Visible:EVisibility::Collapsed;})
        [Button(TEXT("점군 보기"),[this](){SelectScenario(Selected==INDEX_NONE?0:Selected);})]]
      +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(List,SVerticalBox)]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
       [SNew(SBox).Visibility_Lambda([this](){return PreviewVisible?EVisibility::Visible:EVisibility::Collapsed;})[Text(TAttribute<FText>::CreateLambda([this](){return PreviewText;}),11)]]
      +SVerticalBox::Slot().FillHeight(TAttribute<float>::CreateLambda([this](){return PreviewVisible?1.f:0.f;}))
       [SNew(SBox).Visibility_Lambda([this](){return PreviewVisible?EVisibility::Visible:EVisibility::Collapsed;})[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&PreviewBrush)]]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)
       [SNew(SBox).Visibility_Lambda([this](){return PreviewVisible?EVisibility::Visible:EVisibility::Collapsed;})[PreviewControls]]
      +SVerticalBox::Slot().FillHeight(TAttribute<float>::CreateLambda([this](){return Details?1.f:0.f;}))
       [SNew(SBox).Visibility_Lambda([this](){return Details?EVisibility::Visible:EVisibility::Collapsed;})
        [SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(TAttribute<FText>::CreateLambda([this](){return DetailText;}),11)]
          +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Button(TEXT("세트 초기화"),[this](){if(auto* S=Service())if(FPlatformTime::Seconds()>ConfirmUntil){ConfirmUntil=FPlatformTime::Seconds()+8;Status=FText::FromString(TEXT("8초 안에 다시 누르면 목록을 비웁니다 · 기존 파일 유지"));}else{FString Error;S->ResetConfirmed(Error);ConfirmUntil=0;Selected=INDEX_NONE;SeenRevision=-1;Refresh();}})]]]];
    SeenRevision=-1;Rows.Reset();RowMessages.Reset();RowStates.Reset();RowIds.Reset();Refresh();
    return BuildToolPanelFrame(SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).Padding(16)
      [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(FText::FromString(TEXT("시나리오 테스트셋")))]
        +SVerticalBox::Slot().FillHeight(1).Padding(0,12,0,0)[SNew(SBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})[Contents]]]);
}
void UAgentTestSetWidget::NativeTick(const FGeometry& Geometry,float Delta)
{Super::NativeTick(Geometry,Delta);const double Now=FPlatformTime::Seconds();if(IsVisible()&&!IsPanelCollapsed()&&Now>=NextRefresh){NextRefresh=Now+.2;Refresh();}}
void UAgentTestSetWidget::NativeDestruct()
{PreviewBrush.SetResourceObject(nullptr);Rows.Reset();List.Reset();if(auto* GI=GetGameInstance())if(auto* Dx=GI->GetSubsystem<UDxWidgetSubsystem>())Dx->UnregisterWidget(this);Super::NativeDestruct();}
