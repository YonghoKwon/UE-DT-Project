#include "VirtualSensorCaptureExportPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "SlabScenarioReplayPanelWidget.h"
#include "SensorToolUiText.h"
#include "Framework/Application/SlateApplication.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "SensorToolWidgetDecl.h"
#include "SensorOwnedPresentationStyle.h"
#include "VirtualSensorMonitorPanelWidget.h"
#include "VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "EngineUtils.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#define LOCTEXT_NAMESPACE "SensorOwnedData"

int32 UVirtualSensorCaptureExportPanelWidget::ResolveOwnedTabIndex(EVirtualSensorCaptureExportTab Tab)
{ return Tab==EVirtualSensorCaptureExportTab::LiveStream?0:Tab==EVirtualSensorCaptureExportTab::ConnectionLog?3:2; }
FString UVirtualSensorCaptureExportPanelWidget::GetPcdPreflightText() const
{
    // Explicit legacy API requests still receive details, without formatting hidden Slate text.
    return IsWorkspaceOwned()&&bOwnedPreflightSnapshotReady?UVirtualSensorOutputPreflight::FormatDetails(OwnedPreflightSnapshot):CachedPcdPreflight;
}
void UVirtualSensorCaptureExportPanelWidget::InitializeOwnedEntryView(EVirtualSensorCaptureExportTab PreviousTab)
{
	if(!IsWorkspaceOwned())return;
	if(!bOwnedEntryInitialized){ActiveTab=EVirtualSensorCaptureExportTab::LiveStream;bOwnedScenarioStreamView=true;bOwnedEntryInitialized=true;}
	else ActiveTab=PreviousTab;
}

void UVirtualSensorCaptureExportPanelWidget::OpenOwnedTab(ESensorOwnedDataTab Tab)
{
    if(!IsWorkspaceOwned()) return;
    // Persist only the original enum values. Results/diagnostics have no new saved policy.
    SetActiveTab(Tab==ESensorOwnedDataTab::Transfer?EVirtualSensorCaptureExportTab::LiveStream:
        Tab==ESensorOwnedDataTab::FileSave?EVirtualSensorCaptureExportTab::Capture:EVirtualSensorCaptureExportTab::ConnectionLog);
    OwnedTab=Tab;
    if(Tab==ESensorOwnedDataTab::Transfer)if(auto* W=GetToolWorkspace())W->EnsureOwnedReplayPanel();
    bRunResultsExpanded=Tab==ESensorOwnedDataTab::Results;
    bOwnedServerExpanded=Tab==ESensorOwnedDataTab::ServerConnection;
    bOwnedReceiverExpanded=false;bOwnedTransportLogExpanded=false;
    if(WidgetTree&&WidgetTree->RootWidget)SetScenarioRunResultsExpanded(Tab==ESensorOwnedDataTab::Results);
    LastNativeStatusRefreshSeconds=-1;
    CachedRunResultsRevision=-1;
}
USlabScenarioReplayPanelWidget* UVirtualSensorCaptureExportPanelWidget::ResolveOwnedReplayPanel() const
{
    auto* W=GetToolWorkspace();
    return W?Cast<USlabScenarioReplayPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Replay)):nullptr;
}
bool UVirtualSensorCaptureExportPanelWidget::SetOwnedReplayOutputs(const FVirtualSlabSensorOutputSelection& Outputs)
{
    if(auto* Replay=ResolveOwnedReplayPanel())
    { Replay->SetReplayOutputs(Outputs);bOwnedScenarioStreamView=true;bOwnedReplayPreflight=true;LastNativeStatusRefreshSeconds=-1;return true; }
    return false;
}
bool UVirtualSensorCaptureExportPanelWidget::SetOwnedReplayExecutionOptions(const FSlabExecutionOptions& Options)
{
    if(auto* Replay=ResolveOwnedReplayPanel())
    { Replay->ExecutionOptions=Options;bOwnedScenarioStreamView=true;bOwnedReplayPreflight=true;LastNativeStatusRefreshSeconds=-1;return true; }
    return false;
}
void UVirtualSensorCaptureExportPanelWidget::OpenOwnedReplayOutputSettings()
{ FocusOwnedOutputSettings(true); }
void UVirtualSensorCaptureExportPanelWidget::FocusOwnedOutputSettings(bool bReplay)
{
    OpenOwnedTab(ESensorOwnedDataTab::Transfer);bOwnedScenarioStreamView=true;bOwnedReplayPreflight=bReplay;
    const auto Control=bReplay?OwnedReplayPcdControl:OwnedLivePcdControl;
    if(Control.IsValid())FSlateApplication::Get().SetKeyboardFocus(Control,EFocusCause::SetDirectly);
}

void UVirtualSensorCaptureExportPanelWidget::NativeConstruct()
{ Super::NativeConstruct();if(IsWorkspaceOwned()){BindFileService();if(!OwnedFileRequestId.IsEmpty())HandleFileSaveUpdated(GetActiveFileSaveStatus());} }
void UVirtualSensorCaptureExportPanelWidget::NativeDestruct()
{ if(ManualPayloadSensor.IsValid())ManualPayloadSensor->OnCompatibilityRequestCompletedNative.Remove(ManualPayloadHandle);ManualPayloadSensor.Reset();ActiveManualPayloadRequest.Reset();if(FileSaveService.IsValid())FileSaveService->OnRequestUpdated.RemoveDynamic(this,&ThisClass::HandleFileSaveUpdated);FileSaveService.Reset();Super::NativeDestruct(); }
void UVirtualSensorCaptureExportPanelWidget::BindFileService()
{
	if(!GetWorld()||FileSaveService.IsValid())return;
	FileSaveService=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>();
	if(FileSaveService.IsValid())FileSaveService->OnRequestUpdated.AddUniqueDynamic(this,&ThisClass::HandleFileSaveUpdated);
}
AVirtualSensorActorBase* UVirtualSensorCaptureExportPanelWidget::ResolveOwnedSelectedActor() const
{return SensorManager?SensorManager->GetSelectedSensorActor():nullptr;}
FVirtualSensorCaptureSelection UVirtualSensorCaptureExportPanelWidget::ResolveOwnedFileSelection() const
{auto Selection=CaptureSelection;if(OwnedFileAction==1)Selection.PointCloudFormat=SelectedPointCloudKind;return Selection;}
FString UVirtualSensorCaptureExportPanelWidget::RequestFileSave(EVirtualSensorFileSaveMode Mode)
{
	BindFileService();if(!FileSaveService.IsValid()){LastUiMessage=TEXT("파일 저장 서비스가 없습니다.");return FString();}
	auto Selection=CaptureSelection;if(Mode==EVirtualSensorFileSaveMode::CurrentFrame)Selection.PointCloudFormat=SelectedPointCloudKind;
	OwnedFileRequestId=FileSaveService->RequestSave(Mode,ResolveOwnedSelectedActor(),Selection);
	OwnedFileRequests.Add(OwnedFileRequestId);HandleFileSaveUpdated(FileSaveService->GetRequestStatus(OwnedFileRequestId));
	LastNativeStatusRefreshSeconds=-1;return OwnedFileRequestId;
}
FVirtualSensorFileSaveStatus UVirtualSensorCaptureExportPanelWidget::GetActiveFileSaveStatus() const
{return FileSaveService.IsValid()?FileSaveService->GetRequestStatus(OwnedFileRequestId):FVirtualSensorFileSaveStatus();}
void UVirtualSensorCaptureExportPanelWidget::HandleFileSaveUpdated(const FVirtualSensorFileSaveStatus& Status)
{
	bool Relevant=OwnedFileRequests.Contains(Status.RequestId);
	if(!Relevant&&!OwnedPeriodicSessionId.IsEmpty()&&GetWorld())if(auto* Periodic=GetWorld()->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>())Relevant=Periodic->GetPeriodicStatus(OwnedPeriodicSessionId).LastRequestId==Status.RequestId;
	if(!Relevant)return;
	if(!Status.IsTerminal()||DeliveredFileRequests.Contains(Status.RequestId))return;
	DeliveredFileRequests.Insert(Status.RequestId,0);if(DeliveredFileRequests.Num()>32)DeliveredFileRequests.SetNum(32);OwnedFileRequests.Remove(Status.RequestId);
	for(const auto& Result:Status.FileResults)
	{
		RecentResults.Insert(Result,0);if(RecentResults.Num()>8)RecentResults.SetNum(8);
		OnExportCompleted.Broadcast(Result); // Actual file result, never an admission acknowledgement.
	}
	if(Status.FileResults.IsEmpty())
	{
		const auto Kind=Status.SensorKind==EVirtualSensorKind::Camera?(Status.Selection.bCameraImage?EVirtualSensorExportKind::CameraJpeg:EVirtualSensorExportKind::ServerPayload):(Status.Selection.bPointCloud?Status.Selection.PointCloudFormat:EVirtualSensorExportKind::ServerPayload);
		AddResult(Kind,Status.SensorId,false,FString(),Status.Message);
	}
	// Completion/Blueprint delivery continues while hidden. Display polling stays at 5 Hz.
}
void UVirtualSensorCaptureExportPanelWidget::ToggleOwnedPeriodicSave()
{
	if(!GetWorld())return;auto* Service=GetWorld()->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>();if(!Service)return;
	if(!OwnedPeriodicSessionId.IsEmpty()&&Service->GetPeriodicStatus(OwnedPeriodicSessionId).bActive)Service->StopPeriodicSave(OwnedPeriodicSessionId);
	else OwnedPeriodicSessionId=Service->StartPeriodicSave(ResolveOwnedSelectedActor(),CaptureSelection);
	LastNativeStatusRefreshSeconds=-1;
}
bool UVirtualSensorCaptureExportPanelWidget::IsSelectedStreamScenarioControlled(EVirtualSensorStreamKind Kind) const
{
	if(!GetWorld())return false;const auto* Context=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();if(!Context)return false;
	const auto State=Context->GetSlabSensorSessionStatus().State;
	return (State==EVirtualSlabSessionState::Ready||State==EVirtualSlabSessionState::Running||State==EVirtualSlabSessionState::Paused||State==EVirtualSlabSessionState::Draining)&&Context->ControlsSensor(GetSelectedSensorIdForStream(Kind));
}
void UVirtualSensorCaptureExportPanelWidget::ToggleOwnedStream(EVirtualSensorStreamKind Kind)
{
	if(IsSelectedStreamScenarioControlled(Kind)){LastUiMessage=TEXT("시나리오가 제어하는 송신입니다. Slab 진행 패널에서 제어하세요.");return;}
	ToggleSelectedStream(Kind);LastNativeStatusRefreshSeconds=-1;
}
bool UVirtualSensorCaptureExportPanelWidget::IsOwnedCompatibilityTransport() const
{
	const auto* Publisher=SensorManager?SensorManager->StreamPublisherComponent.Get():nullptr;
	if(!Publisher||!Publisher->CanUseHighThroughputTransport())return true;
	for(const auto& S:Publisher->GetStreamStatuses())if(S.bEnabled&&S.ActiveTransportBackend==EVirtualSensorStreamTransportBackend::EngineStompCompatibility&&(S.SensorId.IsEmpty()||S.SensorId==GetSelectedSensorIdForStream(S.StreamKind)))return true;
	return false;
}
void UVirtualSensorCaptureExportPanelWidget::TickOwnedUi(double Now)
{
    BindFileService();
    if(GetVisibility()==ESlateVisibility::Collapsed||GetVisibility()==ESlateVisibility::Hidden||IsPanelCollapsed())return;
    if(LastNativeStatusRefreshSeconds>=0&&Now-LastNativeStatusRefreshSeconds<.2)return;
    LastNativeStatusRefreshSeconds=Now;
    if(LastUiMessage!=CachedOwnedMessageSource||OwnedTab!=CachedOwnedMessageTab)
    {
        CachedOwnedMessageSource=LastUiMessage;CachedOwnedMessageTab=OwnedTab;
        OwnedUiMessage=OwnedTab==ESensorOwnedDataTab::Diagnostics?LastUiMessage:SensorToolUiText::UserReason(LastUiMessage);
    }
    if((OwnedTab==ESensorOwnedDataTab::Transfer&&bOwnedDirectExpanded)||OwnedTab==ESensorOwnedDataTab::FileSave)
    {
        auto* Actor=ResolveOwnedSelectedActor();
        OwnedSelectionText=Actor?FString::Printf(TEXT("%s · %s"),Actor->GetSensorKind()==EVirtualSensorKind::Lidar?TEXT("LiDAR"):TEXT("Camera"),*Actor->GetSensorId()):TEXT("장비를 선택하세요");
        if(OwnedTab==ESensorOwnedDataTab::FileSave)
        {
            if(!OwnedFileRequestId.IsEmpty())
            {
            const auto FileStatus=GetActiveFileSaveStatus();
            if(FileStatus.Message!=CachedOwnedFileMessage||FileStatus.FrameId!=CachedOwnedFileFrame||FileStatus.SensorId!=CachedOwnedFileSensor)
            {
                CachedOwnedFileMessage=FileStatus.Message;CachedOwnedFileFrame=FileStatus.FrameId;CachedOwnedFileSensor=FileStatus.SensorId;
                const FString Message=SensorToolUiText::UserReason(FileStatus.Message);
                OwnedFileStatusText=FileStatus.FrameId>=0?FString::Printf(TEXT("%s · %s · 프레임 %lld"),*Message,*FileStatus.SensorId,FileStatus.FrameId):Message;
            }
            }
            bOwnedCanSaveCurrent=Actor&&UVirtualSensorFileSaveSubsystem::CanSaveCurrent(Actor,ResolveOwnedFileSelection());
            bOwnedSaveBusy=Actor&&FileSaveService.IsValid()&&FileSaveService->HasPendingSave(Actor);
            if(!OwnedPeriodicSessionId.IsEmpty())
            {
                const auto State=GetWorld()->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>()->GetPeriodicStatus(OwnedPeriodicSessionId);
                bOwnedPeriodicActive=State.bActive;
                OwnedPeriodicStatusText=FString::Printf(TEXT("%s · %s · %.2f초\n완료 %lld · 주기 생략 %lld · 실패 %lld · %s"),State.bActive?TEXT("주기 저장 중"):TEXT("주기 저장 중지"),*State.SensorId,State.IntervalSeconds,State.CompletedCount,State.SkippedCount,State.FailedCount,*SensorToolUiText::UserReason(State.LastMessage));
            }
        }
    }
    if(OwnedTab==ESensorOwnedDataTab::Transfer)
    {
        if(!bOwnedReplayOwnerRequested)if(auto* W=GetToolWorkspace())bOwnedReplayOwnerRequested=W->EnsureOwnedReplayPanel();
        TArray<FString> Targets;
        auto Outputs=FVirtualSlabSensorOutputSelection();
        FSlabExecutionOptions Options;
        bool MissingOwner=false;
        if(bOwnedScenarioStreamView)
        {
            if(bOwnedReplayPreflight)
            {
                if(const auto* Replay=ResolveOwnedReplayPanel()){Targets=Replay->TargetSensorIds;Outputs=Replay->GetReplayOutputs();Options=Replay->ExecutionOptions;}
                else MissingOwner=true;
            }
            else if(const auto* Slab=ResolveScenarioSlabActor()){Targets=Slab->TargetSensorIds;Outputs=Slab->GetSensorOutputs();Options=Slab->GetExecutionOptions();}
            else MissingOwner=true;
        }
        else
        {
            const auto Id=GetSelectedSensorIdForStream(EVirtualSensorStreamKind::PointCloud);
            if(!Id.IsEmpty())Targets.Add(Id);
            const auto* Publisher=SensorManager?SensorManager->StreamPublisherComponent.Get():nullptr;
            if(Publisher&&Publisher->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,Id))Outputs=FVirtualSlabSensorOutputSelection::ObservationOnly();
        }
        const auto Check=UVirtualSensorOutputPreflight::InspectForPresentation(this,Outputs,Targets,bOwnedScenarioStreamView?EVirtualSensorPreflightPurpose::ScenarioTransition:EVirtualSensorPreflightPurpose::ManualStreamStart,bOwnedPreflightExpanded);
        OwnedPreflightSnapshot=Check;bOwnedPreflightSnapshotReady=true;
        if(bOwnedPreflightExpanded)CachedPcdPreflight=Check.Summary;
        OwnedPreflightFix=0;
        if(MissingOwner){CachedPcdPreflightSummary=TEXT("시나리오 연결을 확인하세요");OwnedPreflightFix=3;}
        else if(bOwnedScenarioStreamView&&!Outputs.HasAnyOutput())
        {CachedPcdPreflightSummary=Options.Policy==ESlabExecutionPolicy::RequireData?TEXT("출력을 선택하세요 · 연결 확인 후 시작에는 출력이 필요합니다"):TEXT("준비됨 · 관찰 전용");OwnedPreflightFix=4;}
        else if(!Check.BlockingReasons.IsEmpty())
        {
            const auto& Reason=Check.BlockingReasons[0];
            CachedPcdPreflightSummary=SensorToolUiText::PreflightReason(Reason);
            OwnedPreflightFix=Reason.Contains(TEXT("없"))||Reason.Contains(TEXT("중복"))?1:Reason.Contains(TEXT("주기"))||Reason.Contains(TEXT("수집 설정"))?2:0;
        }
        else if(Check.bSessionBusy&&bOwnedScenarioStreamView){CachedPcdPreflightSummary=TEXT("현재 실행을 마친 뒤 시작하세요");OwnedPreflightFix=3;}
        else if(!Check.bTransportConnected&&Outputs.HasAnyOutput())CachedPcdPreflightSummary=TEXT("서버 연결을 확인하세요");
        else {CachedPcdPreflightSummary=SensorToolUiText::Ready().ToString();OwnedPreflightFix=-1;}
        if(bOwnedDirectExpanded)
        {
            WorkspaceStreamCards.Reset();
            for(const auto Kind:{EVirtualSensorStreamKind::PointCloud,EVirtualSensorStreamKind::CameraImage,EVirtualSensorStreamKind::LidarPayload})
                WorkspaceStreamCards.Add(Kind,TEXT("대상: ")+GetSelectedSensorIdForStream(Kind)+TEXT(" · 중지"));
            if(SensorManager&&SensorManager->StreamPublisherComponent)for(const auto& State:SensorManager->StreamPublisherComponent->GetStreamStatuses())
            {
                if(!State.SensorId.IsEmpty()&&State.SensorId!=GetSelectedSensorIdForStream(State.StreamKind))continue;
                const int64 Errors=State.OverloadCount+State.EncodeFailureCount+State.BodyLimitRejectedCount+State.DeliveryFailureCount+State.RawDeliveryFailureCount+State.ConsumerValidationFailureCount;
                WorkspaceStreamCards.Add(State.StreamKind,FString::Printf(TEXT("대상: %s · %s · 전송 %.1f건/s%s"),*GetSelectedSensorIdForStream(State.StreamKind),State.bEnabled?TEXT("전송 중"):TEXT("중지"),State.SubmittedHz,Errors?TEXT(" · 오류: 진단에서 확인하세요"):TEXT("")));
            }
        }
        if(bWorkspaceLiveDetails)CachedLiveStreamSummary=GetLiveStreamSummaryText();
    }
    if(OwnedTab==ESensorOwnedDataTab::ServerConnection)
    {
        const auto* Transport=SensorManager?SensorManager->SharedTransportComponent.Get():nullptr;
        OwnedConnectionText=Transport?TEXT("적용된 서버: ")+(Transport->TransportMode==EVirtualSensorTransportMode::HttpPost?Transport->GetTransportProfile().HttpEndpoint:Transport->GetTransportProfile().BrokerUrl):TEXT("장비 연결이 필요합니다");
    }
    if(OwnedTab==ESensorOwnedDataTab::Diagnostics)
    {
        if(bOwnedReceiverExpanded)
        {
            if(!IsValid(TopicReceiverHost)&&Now>=NextReceiverLookup)
            {
                TopicReceiverHost=nullptr;if(GetWorld())for(TActorIterator<AVirtualSensorExternalSourceHostActor> It(GetWorld());It;++It){TopicReceiverHost=*It;break;}
                NextReceiverLookup=Now+ReceiverLookupDelay;ReceiverLookupDelay=FMath::Min(5.0,ReceiverLookupDelay*2);
            }
            CachedTopicReceiverSummary=GetTopicReceiverSummaryText();
        }
        if(bOwnedTransportLogExpanded){CachedTransportLog=BuildTransportLogText();CachedLiveStreamSummary=GetLiveStreamSummaryText();}
    }
    RefreshNativeText();
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedWidget()
{
    auto Tabs=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
    const FText Names[]={SensorToolUiText::TransferTab(),SensorToolUiText::Results(),SensorToolUiText::FileSave(),SensorToolUiText::ServerTab(),SensorToolUiText::Diagnostics()};
    for(int32 I=0;I<5;++I)Tabs->AddSlot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle())
        .OnClicked_Lambda([this,I](){OpenOwnedTab(static_cast<ESensorOwnedDataTab>(I));return FReply::Handled();})
        [SNewSensorTool(STextBlock).ColorAndOpacity_Lambda([this,I](){return int32(OwnedTab)==I?GetToolAccentColor():GetToolTextColor();}).Text(Names[I])]];
    return SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).ForegroundColor(GetToolTextColor()).Padding(16)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(SensorToolUiText::Data())]
     +SVerticalBox::Slot().FillHeight(1)[SNew(SVerticalBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Tabs]
      +SVerticalBox::Slot().FillHeight(1)[SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return int32(OwnedTab);})
        +SWidgetSwitcher::Slot()[BuildOwnedLiveTab()]+SWidgetSwitcher::Slot()[BuildOwnedResultsTab()]
        +SWidgetSwitcher::Slot()[BuildOwnedFileTab()]+SWidgetSwitcher::Slot()[BuildOwnedConnectionTab()]+SWidgetSwitcher::Slot()[BuildOwnedDiagnosticsTab()]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(OwnedUiMessage);})]
  +SVerticalBox::Slot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true).Visibility_Lambda([this](){return OwnedTab==ESensorOwnedDataTab::FileSave?EVisibility::Visible:EVisibility::Collapsed;})
	   .OnAreaExpansionChanged_Lambda([this](bool Open){bOwnedStorageExpanded=Open;if(Open)RefreshNativeText();})
	   .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Results","저장 위치·최근 결과"))]
	   .BodyContent()[SNew(SBox).HeightOverride_Lambda([this](){return FMath::Clamp(GetEffectivePanelSize().Y*.30,80.0,220.0);})[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
	    +SVerticalBox::Slot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
	      +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("Root","저장 폴더")).OnClicked_Lambda([this](){OpenCaptureRootFolder();return FReply::Handled();})]
	      +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("Recent","최근 파일")).OnClicked_Lambda([this](){OpenLastResultFolder();return FReply::Handled();})]
	      +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("Copy","경로 복사")).OnClicked_Lambda([this](){CopyLastResultPath();return FReply::Handled();})]]
	    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignSensorTool(NativeStorageText,STextBlock).ColorAndOpacity(GetToolMutedColor()).AutoWrapText(true)]
	   ]]]]
	 ]]
	;
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedLiveTab()
{
    auto Column=[this](bool Replay)->TSharedRef<SWidget>
    {
        auto Content=SNew(SVerticalBox);
        Content->AddSlot().AutoHeight().Padding(0,0,0,8)[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolAccentColor()).Text(Replay?SensorToolUiText::ArchiveReplay():SensorToolUiText::NewScenario())];
        for(int32 Index=0;Index<3;++Index)
        {
            auto Control=SNew(SCheckBox)
                .IsEnabled_Lambda([this,Replay](){return Replay?ResolveOwnedReplayPanel()!=nullptr:ResolveScenarioSlabActor()!=nullptr;})
                .IsChecked_Lambda([this,Replay,Index](){auto* Panel=ResolveOwnedReplayPanel();const auto O=Replay?(Panel?Panel->GetReplayOutputs():FVirtualSlabSensorOutputSelection::ObservationOnly()):GetLiveScenarioOutputs();return (Index==0?O.bPointCloud:Index==1?O.bCameraImage:O.bLidarTelemetry)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,Replay,Index](ECheckBoxState State)
                {
                    auto* Panel=ResolveOwnedReplayPanel();auto O=Replay?(Panel?Panel->GetReplayOutputs():FVirtualSlabSensorOutputSelection::ObservationOnly()):GetLiveScenarioOutputs();
                    const bool Enabled=State==ECheckBoxState::Checked;
                    if(Index==0)O.bPointCloud=Enabled;else if(Index==1)O.bCameraImage=Enabled;else O.bLidarTelemetry=Enabled;
                    if(Replay)SetOwnedReplayOutputs(O);else SetLiveScenarioOutputs(O);
                    bOwnedScenarioStreamView=true;bOwnedReplayPreflight=Replay;LastNativeStatusRefreshSeconds=-1;
                })[SNewSensorTool(STextBlock).Text(Index==0?SensorToolUiText::PcdOutput():Index==1?SensorToolUiText::CameraOutput():SensorToolUiText::LidarOutput())];
            if(Index==0){if(Replay)OwnedReplayPcdControl=Control;else OwnedLivePcdControl=Control;}
            if(Index==1)Content->AddSlot().AutoHeight().Padding(0,8,0,0)[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text(SensorToolUiText::ExtraOutputs())];
            Content->AddSlot().AutoHeight().Padding(0,4)[Control];
        }
        Content->AddSlot().AutoHeight().Padding(0,8)[SNew(SCheckBox)
            .IsEnabled_Lambda([this,Replay](){auto* A=ResolveScenarioSlabActor();return Replay?ResolveOwnedReplayPanel()!=nullptr:A&&!A->IsSimulationActive();})
            .IsChecked_Lambda([this,Replay](){auto* P=ResolveOwnedReplayPanel();return (Replay?(P?P->ExecutionOptions.Policy:ESlabExecutionPolicy::ObservationAllowed):GetLiveScenarioExecutionOptions().Policy)==ESlabExecutionPolicy::RequireData?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
            .OnCheckStateChanged_Lambda([this,Replay](ECheckBoxState State){auto* P=ResolveOwnedReplayPanel();auto Options=Replay?(P?P->ExecutionOptions:FSlabExecutionOptions()):GetLiveScenarioExecutionOptions();Options.Policy=State==ECheckBoxState::Checked?ESlabExecutionPolicy::RequireData:ESlabExecutionPolicy::ObservationAllowed;if(Replay)SetOwnedReplayExecutionOptions(Options);else SetLiveScenarioExecutionOptions(Options);bOwnedScenarioStreamView=true;bOwnedReplayPreflight=Replay;LastNativeStatusRefreshSeconds=-1;})
            [SNewSensorTool(STextBlock).AutoWrapText(true).Text(SensorToolUiText::RequireConnection())]];
        return SNew(SBorder).BorderImage(GetToolSectionBrush()).BorderBackgroundColor(GetToolSectionColor()).Padding(8)[Content];
    };
    auto Direct=SNew(SVerticalBox);
    Direct->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(OwnedSelectionText);})];
    for(auto Kind:{EVirtualSensorStreamKind::PointCloud,EVirtualSensorStreamKind::CameraImage,EVirtualSensorStreamKind::LidarPayload})
    {
        Direct->AddSlot().AutoHeight().Padding(0,4)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(STextBlock).Text(Kind==EVirtualSensorStreamKind::PointCloud?SensorToolUiText::PcdOutput():Kind==EVirtualSensorStreamKind::CameraImage?SensorToolUiText::CameraOutput():SensorToolUiText::LidarOutput())]
                +SHorizontalBox::Slot().AutoWidth()[SNewSensorTool(SButton)
                    .IsEnabled_Lambda([this,Kind](){return !GetSelectedSensorIdForStream(Kind).IsEmpty()&&!IsSelectedStreamScenarioControlled(Kind);})
                    .Text_Lambda([this,Kind](){if(IsSelectedStreamScenarioControlled(Kind))return LOCTEXT("ScenarioControl","시나리오 연동 중");auto* P=SensorManager?SensorManager->StreamPublisherComponent.Get():nullptr;return P&&P->IsStreamEnabled(Kind,GetSelectedSensorIdForStream(Kind))?LOCTEXT("Stop","중지"):LOCTEXT("Start","시작");})
                    .OnClicked_Lambda([this,Kind](){bOwnedScenarioStreamView=false;ToggleOwnedStream(Kind);return FReply::Handled();})]]
            +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this,Kind](){const auto* Card=WorkspaceStreamCards.Find(Kind);return FText::FromString(Card?*Card:FString());})]];
    }
    auto Checks=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
    for(int32 I=0;I<3;++I)Checks->AddSlot()[SNewSensorTool(SButton).Text(I==0?SensorToolUiText::NewScenario():I==1?SensorToolUiText::ArchiveReplay():SensorToolUiText::DirectTarget())
        .ButtonColorAndOpacity_Lambda([this,I](){return (I==2?!bOwnedScenarioStreamView:bOwnedScenarioStreamView&&bOwnedReplayPreflight==(I==1))?FLinearColor(.35f,.70f,.85f,1):FLinearColor::White;})
        .ForegroundColor_Lambda([this,I](){return (I==2?!bOwnedScenarioStreamView:bOwnedScenarioStreamView&&bOwnedReplayPreflight==(I==1))?GetToolAccentColor():GetToolTextColor();})
        .OnClicked_Lambda([this,I](){bOwnedScenarioStreamView=I!=2;bOwnedReplayPreflight=I==1;LastNativeStatusRefreshSeconds=-1;return FReply::Handled();})];
    return SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNew(SExpandableArea).InitiallyCollapsed(true)
            .OnAreaExpansionChanged_Lambda([this](bool Open){bOwnedDirectExpanded=Open;LastNativeStatusRefreshSeconds=-1;})
            .HeaderContent()[SNewSensorTool(STextBlock).Text(SensorToolUiText::DirectTransfer())]
            .BodyContent()[Direct]]
        +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Column(false)]+SHorizontalBox::Slot().FillWidth(1).Padding(8,0,0,0)[Column(true)]]
        +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).AutoWrapText(true).Text(LOCTEXT("NextExecution","다음 실행부터 적용합니다 · 화면을 여는 것만으로 전송하지 않습니다"))]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).Text(SensorToolUiText::PreflightTarget())]
            +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)[Checks]]
        +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(STextBlock).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ToolTipText_Lambda([this](){return FText::FromString(CachedPcdPreflightSummary);}).Text_Lambda([this](){return FText::FromString(CachedPcdPreflightSummary);})]
            +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[SNewSensorTool(SButton).Visibility_Lambda([this](){return OwnedPreflightFix<0?EVisibility::Collapsed:EVisibility::Visible;})
                .Text_Lambda([this](){return OwnedPreflightFix==1?LOCTEXT("FixEquipment","배치 열기"):OwnedPreflightFix==2?SensorToolUiText::SensorSettings():OwnedPreflightFix==3?SensorToolUiText::Scenario():OwnedPreflightFix==4?SensorToolUiText::ChangeInTransfer():LOCTEXT("FixServer","연결 설정 열기");})
                .OnClicked_Lambda([this](){if(OwnedPreflightFix==0)OpenOwnedTab(ESensorOwnedDataTab::ServerConnection);else if(OwnedPreflightFix==4)FocusOwnedOutputSettings(bOwnedReplayPreflight);else if(auto* W=GetToolWorkspace()){const auto Role=OwnedPreflightFix==1?ESensorToolPanelRole::Placement:OwnedPreflightFix==2?ESensorToolPanelRole::Settings:ESensorToolPanelRole::SlabProgress;W->SetPanelOpen(Role,true);W->BringOwnedPanelToFront(Role);}return FReply::Handled();})]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true)
            .OnAreaExpansionChanged_Lambda([this](bool Open){bOwnedPreflightExpanded=Open;LastNativeStatusRefreshSeconds=-1;})
            .HeaderContent()[SNewSensorTool(STextBlock).Text(SensorToolUiText::PreflightDetails())]
            .BodyContent()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(CachedPcdPreflight);})]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true)
	  .OnAreaExpansionChanged_Lambda([this](bool Open){bWorkspaceLiveDetails=Open;LastNativeStatusRefreshSeconds=-1;})
	  .HeaderContent()[SNewSensorTool(STextBlock).Text(SensorToolUiText::Advanced())]
	  .BodyContent()[SNew(SVerticalBox)
	   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return IsOwnedCompatibilityTransport()?LOCTEXT("CompatPolicy","호환 전송: 간격·receipt 표본 옵션 적용"):LOCTEXT("RawPolicy","Raw TCP: 센서 완료 프레임 모두 · 매 프레임 receipt · 연결 중 FIFO");})]
	   +SVerticalBox::Slot().AutoHeight()[SNew(SComboBox<TSharedPtr<EVirtualPointCloudStreamFilterPreset>>).ComboBoxStyle(&FSensorOwnedPresentationStyle::ComboBox()).OptionsSource(&NativeStreamFilterOptions)
	    .OnGenerateWidget_Lambda([this](TSharedPtr<EVirtualPointCloudStreamFilterPreset> P){const TCHAR* N=P&&*P==EVirtualPointCloudStreamFilterPreset::TargetActorTag?TEXT("대상 물체만"):P&&*P==EVirtualPointCloudStreamFilterPreset::TagOrSemantic?TEXT("Tag·Semantic 조건"):P&&*P==EVirtualPointCloudStreamFilterPreset::SensorLocalRoi?TEXT("센서 로컬 ROI"):TEXT("전체 검출점");return SNewSensorTool(STextBlock).Text(FText::FromString(N));})
	    .OnSelectionChanged_Lambda([this](TSharedPtr<EVirtualPointCloudStreamFilterPreset> P,ESelectInfo::Type){if(P)SetPointCloudStreamFilterPreset(*P);})
	    [SNewSensorTool(STextBlock).Text_Lambda([this](){const TCHAR* N=SelectedPointCloudStreamFilterPreset==EVirtualPointCloudStreamFilterPreset::TargetActorTag?TEXT("대상 물체만"):SelectedPointCloudStreamFilterPreset==EVirtualPointCloudStreamFilterPreset::TagOrSemantic?TEXT("Tag·Semantic 조건"):SelectedPointCloudStreamFilterPreset==EVirtualPointCloudStreamFilterPreset::SensorLocalRoi?TEXT("센서 로컬 ROI"):TEXT("전체 검출점");return FText::FromString(FString(TEXT("PCD 필터: "))+N);})]]
	   +SVerticalBox::Slot().AutoHeight()[SNew(SVerticalBox).Visibility_Lambda([this](){return IsOwnedCompatibilityTransport()?EVisibility::Visible:EVisibility::Collapsed;})
	    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).Text(LOCTEXT("StrideLabel","전송 간격 / receipt 표본 (프레임)"))]
	    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
	      +SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(SEditableTextBox).Text_Lambda([this](){return FText::AsNumber(StreamFrameStride);}).OnTextCommitted_Lambda([this](const FText& T,ETextCommit::Type){StreamFrameStride=FMath::Max(1,FCString::Atoi(*T.ToString()));if(auto* P=UVirtualSensorUiPreferencesSaveGame::LoadOrCreate()){P->SensorStreamFrameStride=StreamFrameStride;UVirtualSensorUiPreferencesSaveGame::Save(P);}})]
	      +SHorizontalBox::Slot().FillWidth(1).Padding(8,0,0,0)[SNewSensorTool(SEditableTextBox).Text_Lambda([this](){return FText::AsNumber(StreamReceiptInterval);}).OnTextCommitted_Lambda([this](const FText& T,ETextCommit::Type){StreamReceiptInterval=FMath::Max(1,FCString::Atoi(*T.ToString()));if(auto* P=UVirtualSensorUiPreferencesSaveGame::LoadOrCreate()){P->SensorStreamReceiptInterval=StreamReceiptInterval;UVirtualSensorUiPreferencesSaveGame::Save(P);}})]]]
	   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(CachedLiveStreamSummary);})]
	  ]]
	];
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedFileTab()
{
	auto Modes=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
	const FText Labels[]={LOCTEXT("NewFrame","새 프레임 저장"),LOCTEXT("CurrentFrame","현재 프레임 저장"),LOCTEXT("Periodic","주기 저장")};
	for(int32 I=0;I<3;++I)Modes->AddSlot()[SNewSensorTool(SButton).Text(Labels[I]).ForegroundColor_Lambda([this,I](){return OwnedFileAction==I?GetToolAccentColor():GetToolTextColor();}).OnClicked_Lambda([this,I](){SetActiveTab(I==1?EVirtualSensorCaptureExportTab::Export:EVirtualSensorCaptureExportTab::Capture);OwnedFileAction=I;LastNativeStatusRefreshSeconds=-1;return FReply::Handled();})];
	auto IsCamera=[this](){auto* A=ResolveOwnedSelectedActor();return A&&A->GetSensorKind()==EVirtualSensorKind::Camera;};
	auto Options=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(16,8));
	auto Output=[this,Options,IsCamera](bool Camera,bool FVirtualSensorCaptureSelection::* Field,FText Name)
	{Options->AddSlot()[SNew(SCheckBox).Visibility_Lambda([IsCamera,Camera](){return IsCamera()==Camera?EVisibility::Visible:EVisibility::Collapsed;})
	 .IsChecked_Lambda([this,Field](){return CaptureSelection.*Field?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
	 .OnCheckStateChanged_Lambda([this,Field](ECheckBoxState S){auto C=CaptureSelection;C.*Field=S==ECheckBoxState::Checked;SetCaptureSelection(C);})[SNewSensorTool(STextBlock).Text(Name)]];};
	Output(true,&FVirtualSensorCaptureSelection::bCameraImage,LOCTEXT("Jpeg","JPEG 이미지"));Output(true,&FVirtualSensorCaptureSelection::bCameraPayload,LOCTEXT("CameraJson","Camera JSON"));
	Output(false,&FVirtualSensorCaptureSelection::bPointCloud,LOCTEXT("PointCloud","포인트 클라우드"));Output(false,&FVirtualSensorCaptureSelection::bLidarPayload,LOCTEXT("LidarJson","LiDAR JSON"));
	return SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
	 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Modes]
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).AutoWrapText(true).Text_Lambda([this](){return OwnedFileAction==0?LOCTEXT("NewHelp","선택 장비의 새 프레임을 비동기로 기다립니다. 요청 후 장비를 바꿔도 저장 대상은 유지됩니다."):OwnedFileAction==1?LOCTEXT("CurrentHelp","마지막으로 완료된 프레임을 저장합니다. 추가 측정을 하지 않습니다."):LOCTEXT("PeriodicHelp","시작할 때의 장비·출력·간격으로 최신 완료 프레임을 저장합니다. 서버 전송 간격과는 별개입니다.");})]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Options]
	 +SVerticalBox::Slot().AutoHeight()[SNew(SComboBox<TSharedPtr<EVirtualSensorExportKind>>).ComboBoxStyle(&FSensorOwnedPresentationStyle::ComboBox()).Visibility_Lambda([this,IsCamera](){return !IsCamera()&&CaptureSelection.bPointCloud?EVisibility::Visible:EVisibility::Collapsed;}).OptionsSource(&NativeExportKindOptions)
	  .OnGenerateWidget_Lambda([this](TSharedPtr<EVirtualSensorExportKind> K){return SNewSensorTool(STextBlock).Text(FText::FromString(K?ExportKindText(*K):FString()));})
	  .OnSelectionChanged_Lambda([this](TSharedPtr<EVirtualSensorExportKind> K,ESelectInfo::Type){if(!K)return;if(OwnedFileAction==1)SetSelectedPointCloudExportKind(*K);else{auto C=CaptureSelection;C.PointCloudFormat=*K;SetCaptureSelection(C);}})
	  [SNewSensorTool(STextBlock).Text_Lambda([this](){return FText::FromString(TEXT("파일 형식: ")+ExportKindText(ResolveOwnedFileSelection().PointCloudFormat));})]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SVerticalBox).Visibility_Lambda([this](){return OwnedFileAction==2?EVisibility::Visible:EVisibility::Collapsed;})
	  +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).Text(LOCTEXT("Interval","저장 간격 (초)"))]
	  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(SEditableTextBox).Text_Lambda([this](){return FText::AsNumber(CaptureSelection.IntervalSeconds);}).OnTextCommitted_Lambda([this](const FText& T,ETextCommit::Type){SetCaptureIntervalSeconds(FCString::Atof(*T.ToString()));})]+SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[SNewSensorTool(SButton).Text(LOCTEXT("SensorInterval","센서 주기 사용")).OnClicked_Lambda([this](){UseSelectedSensorCaptureInterval();return FReply::Handled();})]]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(SButton).Text_Lambda([this](){return OwnedFileAction==2?(bOwnedPeriodicActive?LOCTEXT("StopPeriodic","주기 저장 중지"):LOCTEXT("StartPeriodic","주기 저장 시작")):bOwnedSaveBusy?LOCTEXT("Busy","파일 처리 중…"):LOCTEXT("SaveNow","파일 저장");})
	  .IsEnabled_Lambda([this](){return OwnedFileAction==2?(bOwnedPeriodicActive||ResolveOwnedSelectedActor()!=nullptr):ResolveOwnedSelectedActor()&&!bOwnedSaveBusy&&(OwnedFileAction==0||bOwnedCanSaveCurrent);})
	  .OnClicked_Lambda([this](){if(OwnedFileAction==2)ToggleOwnedPeriodicSave();else RequestFileSave(OwnedFileAction==0?EVirtualSensorFileSaveMode::NewFrame:EVirtualSensorFileSaveMode::CurrentFrame);return FReply::Handled();})]
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){if(OwnedFileAction==2)return FText::FromString(OwnedPeriodicStatusText);if(OwnedFileAction==1&&!bOwnedCanSaveCurrent)return LOCTEXT("NoFrame","완료된 프레임이 없습니다. 측정을 시작하거나 새 프레임 저장을 선택하세요.");if(OwnedFileRequestId.IsEmpty())return SensorToolUiText::FileSaveWaiting();return FText::FromString(OwnedFileStatusText);})]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("FileTools","파일 전송 도구"))].BodyContent()[SNewSensorTool(SButton).Text(LOCTEXT("SendRecent","최근 파일 수동 전송")).OnClicked_Lambda([this](){SendLastExportToServer();return FReply::Handled();})]]
	];
}

void UVirtualSensorCaptureExportPanelWidget::OpenOwnedConnectionSection(bool Diagnostics)
{
    OpenOwnedTab(Diagnostics?ESensorOwnedDataTab::Diagnostics:ESensorOwnedDataTab::ServerConnection);
    bOwnedReceiverExpanded=Diagnostics;bOwnedTransportLogExpanded=Diagnostics;
    if(OwnedReceiverArea)OwnedReceiverArea->SetExpanded(Diagnostics);
    if(OwnedTransportLogArea)OwnedTransportLogArea->SetExpanded(Diagnostics);
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedResultsTab()
{
    return SNew(SScrollBox)+SScrollBox::Slot()[BuildRunResultsContent()];
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedConnectionTab()
{
	auto Server=SNew(SVerticalBox);
	Server->AddSlot().AutoHeight().Padding(0,8)[SNewSensorTool(SButton).Text_Lambda([this](){return bUseStompTransport?SensorToolUiText::RealtimeConnection():LOCTEXT("Http","방식: HTTP POST");}).OnClicked_Lambda([this](){bUseStompTransport=!bUseStompTransport;return FReply::Handled();})];
	auto Field=[this,Server](FText Label,FString* Value,bool Secret,bool StompOnly,bool HttpOnly)
	{Server->AddSlot().AutoHeight().Padding(0,4)[SNew(SVerticalBox).Visibility_Lambda([this,StompOnly,HttpOnly](){return (StompOnly&&!bUseStompTransport)||(HttpOnly&&bUseStompTransport)?EVisibility::Collapsed:EVisibility::Visible;})
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text(Label)]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNewSensorTool(SEditableTextBox).IsPassword(Secret).Text_Lambda([Value](){return FText::FromString(*Value);}).OnTextCommitted_Lambda([Value](const FText& T,ETextCommit::Type){*Value=T.ToString();})]];};
	Field(SensorToolUiText::ServerAddress(),&DraftBrokerUrl,false,true,false);Field(LOCTEXT("Endpoint","HTTP 서버 주소"),&DraftHttpEndpoint,false,false,true);
	Field(SensorToolUiText::PcdAddress(),&DraftExportTopic,false,true,false);Field(SensorToolUiText::CameraAddress(),&DraftCameraTopic,false,true,false);Field(SensorToolUiText::LidarAddress(),&DraftLidarTopic,false,true,false);
	Field(LOCTEXT("User","사용자명"),&DraftUserName,false,true,false);Field(LOCTEXT("Password","비밀번호 · 세션에서만 유지"),&SessionPasscode,true,true,false);Field(LOCTEXT("Bearer","Bearer token · 세션에서만 유지"),&SessionBearerToken,true,false,true);
	Field(SensorToolUiText::ConsumerAckAddress(),&DraftAckTopic,false,true,false);
	Server->AddSlot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).Text(LOCTEXT("MaxBytes","최대 메시지 크기 (bytes)"))];
	Server->AddSlot().AutoHeight()[SNewSensorTool(SEditableTextBox).Text_Lambda([this](){return FText::AsNumber(DraftMaxMessageBytes);}).OnTextCommitted_Lambda([this](const FText& T,ETextCommit::Type){DraftMaxMessageBytes=FMath::Max(1024,FCString::Atoi(*T.ToString()));})];
	Server->AddSlot().AutoHeight().Padding(0,8)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
	 +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("Apply","설정 적용")).OnClicked_Lambda([this](){ApplyTransportProfile();LastNativeStatusRefreshSeconds=-1;return FReply::Handled();})]
	 +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("Test","설정 적용·연결 시험")).OnClicked_Lambda([this](){TestServerConnection();LastNativeStatusRefreshSeconds=-1;return FReply::Handled();})]];
    return SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(OwnedConnectionText);})]
        +SVerticalBox::Slot().AutoHeight()[Server]];
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedDiagnosticsTab()
{
    return SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(SButton).Text(LOCTEXT("SelectResult","실행 선택은 결과 탭에서")).OnClicked_Lambda([this](){OpenOwnedTab(ESensorOwnedDataTab::Results);return FReply::Handled();})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true)
            .OnAreaExpansionChanged_Lambda([this](bool Open){bOwnedRunDiagnosticsExpanded=Open;CachedRunResultsRevision=-1;LastNativeStatusRefreshSeconds=-1;})
            .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("RunDiagnostics","선택 실행 · 단계별 집계·종료 snapshot·receipt"))]
            .BodyContent()[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(RunResultsText);})]
                +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(RunComparisonText);})]]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(OwnedReceiverArea,SExpandableArea).InitiallyCollapsed(!bOwnedReceiverExpanded).OnAreaExpansionChanged_Lambda([this](bool B){bOwnedReceiverExpanded=B;LastNativeStatusRefreshSeconds=-1;})
	  .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Receiver","Broker 실제 수신 검증"))].BodyContent()[SNew(SVerticalBox)
	   +SVerticalBox::Slot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
	    +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("ReceiverStop","수신 구독 끊기")).OnClicked_Lambda([this](){StopTopicReceivers();return FReply::Handled();})]
	    +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("ReceiverReconnect","수신 다시 연결")).OnClicked_Lambda([this](){NextReceiverLookup=0;ReceiverLookupDelay=.5;ReconnectTopicReceivers();return FReply::Handled();})]]
	   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(CachedTopicReceiverSummary);})]]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(OwnedTransportLogArea,SExpandableArea).InitiallyCollapsed(!bOwnedTransportLogExpanded).OnAreaExpansionChanged_Lambda([this](bool B){bOwnedTransportLogExpanded=B;LastNativeStatusRefreshSeconds=-1;})
	  .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Log","전송 기록·호환 도구"))].BodyContent()[SNew(SVerticalBox)
	   +SVerticalBox::Slot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
	    +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("DiagnosticReport","진단 보고서 저장")).OnClicked_Lambda([this](){ExportTransportDiagnosticReport();return FReply::Handled();})]
	    +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("LegacyPayload","현재 JSON Payload 수동 전송")).OnClicked_Lambda([this](){RequestSelectedPayloadSend();return FReply::Handled();})]]
	   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(CachedTransportLog+TEXT("\n")+CachedLiveStreamSummary);})]]]
	];
}
#undef LOCTEXT_NAMESPACE
