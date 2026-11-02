#include "TrainBatchWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "Core/DxWidgetSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/SBoxPanel.h"

UTrainBatchWidget* UTrainBatchWidget::OpenTrainingPanel(UObject* Context)
{
    UWorld* W=Context?Context->GetWorld():nullptr;if(!W||!W->GetGameInstance())return nullptr;
    auto* Dx=W->GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>();if(!Dx)return nullptr;
    for(const auto& Open:Dx->GetOpenWidgets())if(auto* Existing=Cast<UTrainBatchWidget>(Open.Get()))if(Existing->GetWorld()==W)
    {Dx->BringToFront(Existing);return Existing;}
    auto* Widget=CreateWidget<UTrainBatchWidget>(W,StaticClass());if(!Widget)return nullptr;
    Widget->AddToViewport();Widget->SetDesiredSizeInViewport(FVector2D(880,720));
    Dx->RegisterWidget(Widget);Dx->RegisterExternalInputBlocker(Widget);Dx->BringToFront(Widget);return Widget;
}
TSharedRef<SWidget> UTrainBatchWidget::RebuildWidget()
{
    const auto Font=FSensorOwnedPresentationStyle::Font(13,1);
    auto Text=[&](const TCHAR* Value){return SNew(STextBlock).Font(Font).Text(FText::FromString(Value));};
    auto Button=[&](const TCHAR* Name,TFunction<void()> Fn)
    {return SNew(SButton).OnClicked_Lambda([Fn](){Fn();return FReply::Handled();})[Text(Name)];};
    auto Service=[this](){return GetWorld()?GetWorld()->GetSubsystem<UTrainBatchSubsystem>():nullptr;};
    return SNew(SBorder).Padding(12)
    [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[Text(TEXT("학습 시나리오 배치 · 1A 순차 생성"))]
      +SVerticalBox::Slot().AutoHeight().Padding(0,6)
       [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("켜기 / 끄기"),[this,Service](){FString E;if(auto* S=Service()){if(!S->SetEnabled(!S->IsEnabled(),E))Feedback=FText::FromString(E);else Feedback=FText::GetEmpty();SeenRevision=-1;Refresh();}})]
        +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SNew(STextBlock).Font(Font).Text_Lambda([this](){return Summary;}).AutoWrapText(true)]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("닫기"),[this](){CloseWidget();})]]
      +SVerticalBox::Slot().AutoHeight()[Text(TEXT("대상: preset ID / Slab Actor 이름 / LiDAR ID / Camera ID(선택)"))]
      +SVerticalBox::Slot().AutoHeight().Padding(0,4)
       [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Preset,SEditableTextBox).Font(Font).Text(FText::FromString(TEXT("slab-train-v1")))]
        +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(SlabName,SEditableTextBox).Font(Font).HintText(FText::FromString(TEXT("Slab Actor 이름")))]
        +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(LidarId,SEditableTextBox).Font(Font).HintText(FText::FromString(TEXT("LiDAR SensorId")))]
        +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(CameraId,SEditableTextBox).Font(Font).HintText(FText::FromString(TEXT("Camera SensorId")))]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("대상 적용"),[this](){ApplyTargets();})]]
      +SVerticalBox::Slot().AutoHeight()
       [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(ManifestPath,SEditableTextBox).Font(Font).HintText(FText::FromString(TEXT("로컬 Job manifest 절대 경로")))]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("가져오기"),[this,Service](){FString E;if(auto* S=Service()){const bool Accepted=S->ImportManifest(ManifestPath->GetText().ToString(),E);Feedback=FText::FromString(Accepted?TEXT("가져오기 접수 · 결과는 상태 확인"):E);}})]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,6)
       [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)
         [SAssignNew(JobPicker,SComboBox<TSharedPtr<FString>>).OptionsSource(&JobIds)
          .OnGenerateWidget_Lambda([Font](TSharedPtr<FString> Id){return SNew(STextBlock).Font(Font).Text(FText::FromString(*Id));})
          .OnSelectionChanged_Lambda([this,Service](TSharedPtr<FString> Id,ESelectInfo::Type){if(auto* S=Service())S->StopPreview();SelectedJob=Id?*Id:FString();SeenRevision=-1;Refresh();})
          [SNew(STextBlock).Font(Font).Text_Lambda([this](){return FText::FromString(SelectedJob.IsEmpty()?TEXT("Job 선택"):SelectedJob);})]]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("시작"),[this,Service](){FString E;if(auto* S=Service()){if(!S->StartJob(SelectedJob,E))Feedback=FText::FromString(E);else Feedback=FText::GetEmpty();}})]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("중단"),[Service](){if(auto* S=Service())S->CancelJob();})]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("보기 중지"),[Service](){if(auto* S=Service())S->StopPreview();})]]
      +SVerticalBox::Slot().FillHeight(1)
       [SAssignNew(List,SListView<TSharedPtr<FTrainBatchListRow>>).ListItemsSource(&Rows).ItemHeight(30)
        .OnGenerateRow(this,&UTrainBatchWidget::GenerateRow)
        .OnSelectionChanged_Lambda([this,Service](TSharedPtr<FTrainBatchListRow> Row,ESelectInfo::Type){if(Row)if(auto* S=Service()){FString E;if(!S->SelectPreview(SelectedJob,Row->Index,E))Feedback=FText::FromString(E);}})]
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font).Text_Lambda([this](){return ViewText;}).AutoWrapText(true)]
      +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(180)[SNew(SScaleBox)[SNew(SImage).Image(&PreviewBrush)]]]
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font).Text_Lambda([this](){return Feedback;}).AutoWrapText(true)]
      +SVerticalBox::Slot().AutoHeight()[Text(TEXT("압축 ZIP/tar.gz · SFTP 전송은 1C 예정 / tar.zst 보류(미지원)"))]];
}
TSharedRef<ITableRow> UTrainBatchWidget::GenerateRow(TSharedPtr<FTrainBatchListRow> Row,const TSharedRef<STableViewBase>& Owner)
{return SNew(STableRow<TSharedPtr<FTrainBatchListRow>>,Owner)[SNew(STextBlock).Font(FSensorOwnedPresentationStyle::Font(13,1)).Text_Lambda([Row](){return Row->Text;})];}
void UTrainBatchWidget::ApplyTargets()
{
    auto* S=GetWorld()?GetWorld()->GetSubsystem<UTrainBatchSubsystem>():nullptr;if(!S)return;
    ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;AVirtualCameraSensorActor* Camera=nullptr;int32 Slabs=0,Lidars=0,Cameras=0;
    const FString Name=SlabName->GetText().ToString(),Lid=LidarId->GetText().ToString(),Cam=CameraId->GetText().ToString();
    for(TActorIterator<ASlabActor> It(GetWorld());It;++It)if(It->GetName()==Name){Slab=*It;++Slabs;}
    for(TActorIterator<AVirtualLidarSensorActor> It(GetWorld());It;++It)if(It->GetSensorId()==Lid){Lidar=*It;++Lidars;}
    if(!Cam.IsEmpty())for(TActorIterator<AVirtualCameraSensorActor> It(GetWorld());It;++It)if(It->GetSensorId()==Cam){Camera=*It;++Cameras;}
    if(Slabs!=1||Lidars!=1||(!Cam.IsEmpty()&&Cameras!=1)){Feedback=FText::FromString(TEXT("각 대상은 같은 World에서 정확히 1개여야 합니다"));return;}
    FString E;const bool Ok=S->ConfigureTargets(Preset->GetText().ToString(),Slab,Lidar,Camera,E);Feedback=FText::FromString(Ok?TEXT("명시 대상 적용됨"):E);
}
void UTrainBatchWidget::Refresh()
{
    auto* S=GetWorld()?GetWorld()->GetSubsystem<UTrainBatchSubsystem>():nullptr;if(!S)return;
    Summary=FText::FromString(S->GetStatus());ViewText=FText::FromString(S->PreviewStatus());
    auto* Texture=S->PreviewTexture();PreviewBrush.SetResourceObject(Texture);PreviewBrush.ImageSize=Texture?FVector2D(Texture->GetSizeX(),Texture->GetSizeY()):FVector2D(1,1);PreviewBrush.DrawAs=Texture?ESlateBrushDrawType::Image:ESlateBrushDrawType::NoDrawType;
    if(SeenRevision==S->Revision())return;SeenRevision=S->Revision();
    if(JobIds.Num()!=S->Jobs().Num())
    {JobIds.Reset();for(const auto& J:S->Jobs())JobIds.Add(MakeShared<FString>(J.Spec.Id));if(JobPicker)JobPicker->RefreshOptions();}
    if(SelectedJob.IsEmpty()&&!JobIds.IsEmpty())SelectedJob=*JobIds[0];
    const auto* Job=S->Jobs().FindByPredicate([&](const auto& J){return J.Spec.Id==SelectedJob;});
    if(!Job){Rows.Reset();if(List)List->RequestListRefresh();return;}
    const bool Rebuild=Rows.Num()!=Job->Items.Num();if(Rebuild){Rows.Reset();for(int32 I=0;I<Job->Items.Num();++I){auto Row=MakeShared<FTrainBatchListRow>();Row->Index=I;Rows.Add(Row);}}
    for(int32 I=0;I<Job->Items.Num();++I)
    {
        const auto& Item=Job->Items[I];const TCHAR* Status=Item.State==ETrainBatchItemState::Completed?TEXT("완료"):Item.State==ETrainBatchItemState::Failed?TEXT("실패"):Item.State==ETrainBatchItemState::Running?TEXT("실행"):Item.State==ETrainBatchItemState::Waiting?TEXT("대기"):TEXT("수신 중");
        Rows[I]->Text=FText::FromString(FString::Printf(TEXT("%s · %s · %d 프레임 · %s"),*Item.Spec.Id,Status,Item.CompletedFrames,*Item.Message));
    }
    if(Rebuild&&List)List->RequestListRefresh();
}
void UTrainBatchWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{Super::NativeTick(Geometry,DeltaTime);if(!IsVisible())return;const double Now=FPlatformTime::Seconds();if(Now<NextRefresh)return;NextRefresh=Now+.2;Refresh();}
void UTrainBatchWidget::NativeDestruct()
{
    if(auto* W=GetWorld())if(auto* S=W->GetSubsystem<UTrainBatchSubsystem>())S->StopPreview();
    if(auto* GI=GetGameInstance())if(auto* Dx=GI->GetSubsystem<UDxWidgetSubsystem>()){Dx->UnregisterExternalInputBlocker(this);Dx->UnregisterWidget(this);}
    PreviewBrush.SetResourceObject(nullptr);Super::NativeDestruct();
}
