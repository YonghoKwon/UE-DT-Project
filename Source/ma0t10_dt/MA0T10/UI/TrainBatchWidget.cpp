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
#include "Widgets/Input/SCheckBox.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
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
      +SVerticalBox::Slot().AutoHeight()[Text(TEXT("학습 시나리오 배치 · 독립 워커"))]
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
      +SVerticalBox::Slot().AutoHeight().Padding(0,4)
       [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()[Text(TEXT("워커 상한 (GPU 예산 내) "))]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SSpinBox<int32>).MinValue(1).MaxValue(4).Value(1).OnValueChanged_Lambda([this](int32 N){WorkerCount=N;})]
        +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SAssignNew(RestoreId,SEditableTextBox).Font(Font).HintText(FText::FromString(TEXT("디스크 재개할 Job UUID")))]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Job 복원"),[this,Service](){FString E;if(auto* S=Service())Feedback=FText::FromString(S->RestoreJob(RestoreId->GetText().ToString(),E)?TEXT("복원 접수 · 원본/체크섬 확인 중"):E);})]]
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
        .OnGenerateRow_Lambda([Weak=TWeakObjectPtr<UTrainBatchWidget>(this)](TSharedPtr<FTrainBatchListRow> Row,const TSharedRef<STableViewBase>& Owner)->TSharedRef<ITableRow>
        {if(auto* Self=Weak.Get())return Self->GenerateRow(Row,Owner);return SNew(STableRow<TSharedPtr<FTrainBatchListRow>>,Owner);})
        .OnSelectionChanged_Lambda([this,Service](TSharedPtr<FTrainBatchListRow> Row,ESelectInfo::Type){if(Row)if(auto* S=Service()){FString E;if(!S->SelectPreview(SelectedJob,Row->Index,E))Feedback=FText::FromString(E);}})]
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font).Text_Lambda([this](){return ViewText;}).AutoWrapText(true)]
      +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(180)[SNew(SScaleBox)[SNew(SImage).Image(&PreviewBrush)]]]
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font).Text_Lambda([this](){return Feedback;}).AutoWrapText(true)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[BuildDeliverySection()]];
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
    FString E;const bool Ok=S->ConfigureTargets(Preset->GetText().ToString(),Slab,Lidar,Camera,E)&&S->SetProcessWorkers(WorkerCount,E);Feedback=FText::FromString(Ok?TEXT("명시 대상·워커 상한 적용됨"):E);
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
TSharedRef<SWidget> UTrainBatchWidget::BuildDeliverySection()
{
    const auto Font=FSensorOwnedPresentationStyle::Font(13,1);
    FTrainDeliverySettings S;FString LoadError;TrainBatchDelivery::LoadSettings(S,LoadError);Format=S.Format;
    if(!LoadError.IsEmpty())DeliveryFeedback=FText::FromString(LoadError);
    auto Text=[&](const TCHAR* Value){return SNew(STextBlock).Font(Font).Text(FText::FromString(Value));};
    auto Field=[&](TSharedPtr<SEditableTextBox>& Box,const FString& Value,const TCHAR* Hint)
    {return SAssignNew(Box,SEditableTextBox).Font(Font).Text(FText::FromString(Value)).HintText(FText::FromString(Hint));};
    auto Button=[&](const TCHAR* Name,TFunction<void()> Fn,TFunction<bool()> Enabled)
    {return SNew(SButton).IsEnabled_Lambda([Enabled](){return !Enabled||Enabled();}).OnClicked_Lambda([Fn](){Fn();return FReply::Handled();})[Text(Name)];};
    const TFunction<bool()> Idle=[this](){return !(Delivery&&Delivery->IsRunning())&&!bFetchingFingerprint;};
    return SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[Text(TEXT("결과 압축 · SFTP 전송 (tar.zst 보류)"))]
      +SVerticalBox::Slot().AutoHeight().Padding(0,4)
       [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).OnClicked_Lambda([this](){Format=Format==ETrainArchiveFormat::Zip?ETrainArchiveFormat::TarGz:ETrainArchiveFormat::Zip;return FReply::Handled();})
            [SNew(STextBlock).Font(Font).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("형식: %s"),TrainBatchArchive::Extension(Format)));})]]
        +SHorizontalBox::Slot().AutoWidth().Padding(6,0)[SNew(SBox).WidthOverride(70)[Field(Level,FString::FromInt(S.Level),TEXT("수준 0~9"))]]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
         [SNew(SCheckBox).IsChecked_Lambda([this](){return bSendSuccessOnly?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
           .OnCheckStateChanged_Lambda([this](ECheckBoxState State){bSendSuccessOnly=State==ECheckBoxState::Checked;})[Text(TEXT("실패 제외, 성공분만 보내기"))]]]
      +SVerticalBox::Slot().AutoHeight()
       [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(2)[Field(Host,S.Sftp.Host,TEXT("서버 주소"))]
        +SHorizontalBox::Slot().FillWidth(.6f)[Field(Port,FString::FromInt(S.Sftp.Port),TEXT("포트"))]
        +SHorizontalBox::Slot().FillWidth(1)[Field(User,S.Sftp.User,TEXT("계정"))]
        +SHorizontalBox::Slot().FillWidth(2)[Field(RemoteDir,S.Sftp.RemoteDirectory,TEXT("서버 폴더 (/로 시작)"))]]
      +SVerticalBox::Slot().AutoHeight()
       [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)[Field(KeyPath,S.Sftp.PrivateKeyPath,TEXT("SSH 개인키 파일 경로 (암호는 ssh-agent)"))]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("설정 저장"),[this](){SaveDelivery();},Idle)]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("서버 키 확인"),[this](){FetchFingerprint();},Idle)]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("키 등록"),[this]()
            {
                FString E;const bool Ok=TrainBatchSftp::ConfirmHostKey(ReadDeliveryFields().Sftp,PendingKnownHosts,E);
                DeliveryFeedback=FText::FromString(Ok?FString(TEXT("서버 키를 등록했습니다")):E);if(Ok)PendingFingerprint.Reset();
            },[this,Idle](){return Idle()&&!PendingFingerprint.IsEmpty();})]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,4)
       [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("압축만"),[this](){StartDelivery(false);},Idle)]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("압축 후 전송"),[this](){StartDelivery(true);},Idle)]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("전송 취소"),[this](){if(Delivery)Delivery->Cancel();},[this](){return Delivery&&Delivery->IsRunning();})]
        +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SNew(STextBlock).Font(Font).AutoWrapText(true).Text_Lambda([this](){return DeliveryStatusText();})]];
}
FTrainDeliverySettings UTrainBatchWidget::ReadDeliveryFields() const
{
    FTrainDeliverySettings S;S.Format=Format;
    S.Level=Level?FCString::Atoi(*Level->GetText().ToString()):3;
    S.Sftp.Host=Host?Host->GetText().ToString().TrimStartAndEnd():FString();
    S.Sftp.Port=Port?FCString::Atoi(*Port->GetText().ToString()):22;
    S.Sftp.User=User?User->GetText().ToString().TrimStartAndEnd():FString();
    S.Sftp.RemoteDirectory=RemoteDir?RemoteDir->GetText().ToString().TrimStartAndEnd():FString();
    S.Sftp.PrivateKeyPath=KeyPath?KeyPath->GetText().ToString().TrimStartAndEnd():FString();
    S.Sftp.KnownHostsPath=TrainBatchDelivery::KnownHostsPath();
    return S;
}
void UTrainBatchWidget::SaveDelivery()
{
    const FTrainDeliverySettings S=ReadDeliveryFields();FString E;
    if(S.Level<0||S.Level>9||(S.Format==ETrainArchiveFormat::TarGz&&S.Level==0)){DeliveryFeedback=FText::FromString(TEXT("압축 수준은 ZIP 0~9, tar.gz 1~9입니다"));return;}
    DeliveryFeedback=FText::FromString(TrainBatchDelivery::SaveSettings(S,E)?FString(TEXT("전송 설정을 저장했습니다 (키 내용과 암호는 저장하지 않음)")):E);
}
void UTrainBatchWidget::FetchFingerprint()
{
    const FTrainDeliverySettings S=ReadDeliveryFields();
    PendingKnownHosts=FPaths::GetPath(TrainBatchDelivery::KnownHostsPath())/TEXT("known_hosts.pending");
    PendingFingerprint.Reset();bFetchingFingerprint=true;DeliveryFeedback=FText::FromString(TEXT("서버에 연결해 키를 확인하는 중…"));
    TWeakObjectPtr<UTrainBatchWidget> Weak(this);const FString Pending=PendingKnownHosts;
    const FString Work=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("TrainBatch/Work"));
    Async(EAsyncExecution::Thread,[Weak,S,Pending,Work]()
    {
        FString Fingerprint,Error;const bool Ok=TrainBatchSftp::FetchHostFingerprint(S.Sftp,Pending,Work,Fingerprint,Error);
        AsyncTask(ENamedThreads::GameThread,[Weak,Ok,Fingerprint,Error]()
        {
            if(auto* W=Weak.Get())
            {
                W->bFetchingFingerprint=false;W->PendingFingerprint=Ok?Fingerprint:FString();
                W->DeliveryFeedback=FText::FromString(Ok?FString(TEXT("서버 키 지문을 서버 관리자와 대조한 뒤 '키 등록'을 누르세요:\n"))+Fingerprint:Error);
            }
        });
    });
}
void UTrainBatchWidget::StartDelivery(bool bUpload)
{
    auto* S=GetWorld()?GetWorld()->GetSubsystem<UTrainBatchSubsystem>():nullptr;if(!S)return;
    const auto* Job=S->Jobs().FindByPredicate([&](const auto& J){return J.Spec.Id==SelectedJob;});
    if(!Job){DeliveryFeedback=FText::FromString(TEXT("Job을 먼저 선택하세요"));return;}
    FTrainDeliveryRequest R;R.JobId=Job->Spec.Id;R.Settings=ReadDeliveryFields();R.bUpload=bUpload;
    TArray<FString> Excluded;FString E;
    if(!TrainBatchDelivery::BuildJobEntries(*Job,bSendSuccessOnly,R.Entries,Excluded,E)){DeliveryFeedback=FText::FromString(E);return;}
    R.OutputDir=TrainBatchDelivery::OutputDirectory(R.JobId,!Excluded.IsEmpty());
    if(!Excluded.IsEmpty())
    {
        // A success-only package states what it leaves out, inside the archive itself.
        const FString ExcludedFile=R.OutputDir/(R.JobId+TEXT(".excluded.txt"));IFileManager::Get().MakeDirectory(*R.OutputDir,true);
        if(!IFileManager::Get().FileExists(*ExcludedFile)&&!FFileHelper::SaveStringToFile(FString::Join(Excluded,TEXT("\n"))+TEXT("\n"),*ExcludedFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {DeliveryFeedback=FText::FromString(TEXT("제외 목록 저장 실패"));return;}
        FTrainArchiveEntry Note;Note.SourcePath=ExcludedFile;Note.EntryName=R.JobId+TEXT("/excluded_scenarios.txt");Note.Size=IFileManager::Get().FileSize(*ExcludedFile);R.Entries.Add(Note);
    }
    if(!Delivery)Delivery=MakeShared<FTrainBatchDelivery,ESPMode::ThreadSafe>();
    if(!Delivery->Start(R,E)){DeliveryFeedback=FText::FromString(E);return;}
    DeliveryFeedback=FText::FromString(Excluded.IsEmpty()?FString():FString::Printf(TEXT("실패/미완료 %d건 제외: %s"),Excluded.Num(),*FString::Join(Excluded,TEXT(", "))));
}
FText UTrainBatchWidget::DeliveryStatusText() const
{
    FString T=DeliveryFeedback.ToString();
    if(!Delivery)return FText::FromString(T);
    const FTrainDeliveryStatus S=Delivery->Status();
    static const TCHAR* Names[]={TEXT("대기"),TEXT("파일 수집"),TEXT("해시 계산"),TEXT("압축"),TEXT("전송"),TEXT("완료"),TEXT("실패"),TEXT("취소")};
    FString Line=Names[uint8(S.Phase)];
    if(S.Phase==ETrainDeliveryPhase::Archiving&&S.Total>0)Line+=FString::Printf(TEXT(" %.0f%%"),100.0*double(S.Done)/double(S.Total));
    if(!S.Step.IsEmpty())Line+=TEXT(" · ")+S.Step;
    if(!S.ArchivePath.IsEmpty())Line+=FString::Printf(TEXT(" · %s (%.1f MiB)%s"),*FPaths::GetCleanFilename(S.ArchivePath),double(S.ArchiveBytes)/1048576.0,S.bReusedArchive?TEXT(" 재사용"):TEXT(""));
    if(S.Phase==ETrainDeliveryPhase::Done&&!S.RemoteName.IsEmpty())Line+=TEXT(" · 서버 이름 ")+S.RemoteName;
    if(!S.Error.IsEmpty())Line+=TEXT("\n")+S.Error;
    return FText::FromString(T.IsEmpty()?Line:T+TEXT("\n")+Line);
}
