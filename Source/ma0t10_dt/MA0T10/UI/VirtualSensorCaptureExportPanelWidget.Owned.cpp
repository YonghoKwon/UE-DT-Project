#include "VirtualSensorCaptureExportPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "SensorOwnedPresentationStyle.h"
#include "VirtualSensorMonitorPanelWidget.h"
#include "VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
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
{ return Tab==EVirtualSensorCaptureExportTab::LiveStream?0:Tab==EVirtualSensorCaptureExportTab::ConnectionLog?2:1; }

void UVirtualSensorCaptureExportPanelWidget::NativeConstruct()
{ Super::NativeConstruct();if(IsWorkspaceOwned()){BindFileService();if(!OwnedFileRequestId.IsEmpty())HandleFileSaveUpdated(GetActiveFileSaveStatus());} }
void UVirtualSensorCaptureExportPanelWidget::NativeDestruct()
{ if(FileSaveService.IsValid())FileSaveService->OnRequestUpdated.RemoveDynamic(this,&ThisClass::HandleFileSaveUpdated);FileSaveService.Reset();Super::NativeDestruct(); }
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
	OwnedFileStatusText=FString::Printf(TEXT("%s · %s · frame %lld"),*Status.Message,*Status.SensorId,Status.FrameId);
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
	LastNativeStatusRefreshSeconds=-1;RefreshNativeText();
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
	if(!IsValid(TopicReceiverHost)&&Now>=NextReceiverLookup)
	{
		TopicReceiverHost=nullptr;if(GetWorld())for(TActorIterator<AVirtualSensorExternalSourceHostActor> It(GetWorld());It;++It){TopicReceiverHost=*It;break;}
		NextReceiverLookup=Now+ReceiverLookupDelay;ReceiverLookupDelay=FMath::Min(5.0,ReceiverLookupDelay*2);
	}
	if(LastNativeStatusRefreshSeconds>=0&&Now-LastNativeStatusRefreshSeconds<.2)return;LastNativeStatusRefreshSeconds=Now;
	auto* Actor=ResolveOwnedSelectedActor();
	OwnedSelectionText=Actor?FString::Printf(TEXT("%s · %s"),Actor->GetSensorKind()==EVirtualSensorKind::Lidar?TEXT("LiDAR"):TEXT("Camera"),*Actor->GetSensorId()):TEXT("센서를 선택하세요");
	bOwnedCanSaveCurrent=Actor&&UVirtualSensorFileSaveSubsystem::CanSaveCurrent(Actor,ResolveOwnedFileSelection());
	bOwnedSaveBusy=Actor&&FileSaveService.IsValid()&&FileSaveService->HasPendingSave(Actor);
	if(ActiveTab==EVirtualSensorCaptureExportTab::LiveStream)
	{
		WorkspaceStreamCards.Reset();
		if(SensorManager&&SensorManager->StreamPublisherComponent)for(const auto& S:SensorManager->StreamPublisherComponent->GetStreamStatuses())
		{
			if(!S.SensorId.IsEmpty()&&S.SensorId!=GetSelectedSensorIdForStream(S.StreamKind))continue;
			const int64 Errors=S.OverloadCount+S.EncodeFailureCount+S.BodyLimitRejectedCount+S.DeliveryFailureCount+S.RawDeliveryFailureCount+S.ConsumerValidationFailureCount;
			WorkspaceStreamCards.Add(S.StreamKind,FString::Printf(TEXT("%s · %s · 입력 %.1f / 제출 %.1f Hz\nBroker 수락 %lld · 실제 수신 검증 %lld%s"),S.bEnabled?TEXT("송신 중"):TEXT("중지"),S.ActiveTransportBackend==EVirtualSensorStreamTransportBackend::TcpStompHighThroughput?TEXT("Raw TCP"):TEXT("호환 전송"),S.InputHz,S.SubmittedHz,S.ReceiptReceivedCount,S.ConsumerReceivedCount,Errors?TEXT(" · 오류: 상세 진단 확인"):TEXT("")));
		}
		if(bWorkspaceLiveDetails)CachedLiveStreamSummary=GetLiveStreamSummaryText();
	}
	if(ResolveOwnedTabIndex(ActiveTab)==1&&!OwnedPeriodicSessionId.IsEmpty())
	{
		const auto S=GetWorld()->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>()->GetPeriodicStatus(OwnedPeriodicSessionId);bOwnedPeriodicActive=S.bActive;
		OwnedPeriodicStatusText=FString::Printf(TEXT("%s · %s · %.2f초\n완료 프레임 %lld · 저장 주기 생략 %lld · 실패 %lld · 대기 %s\n%s"),S.bActive?TEXT("주기 저장 중"):TEXT("주기 저장 중지"),*S.SensorId,S.IntervalSeconds,S.CompletedCount,S.SkippedCount,S.FailedCount,S.bPending?TEXT("1"):TEXT("0"),*S.LastMessage);
	}
	if(ActiveTab==EVirtualSensorCaptureExportTab::ConnectionLog)
	{
		const auto* T=SensorManager?SensorManager->SharedTransportComponent.Get():nullptr;
		OwnedConnectionText=T?FString::Printf(TEXT("적용된 서버: %s\n전송 방식: %s · Broker 수락과 소비자 검증은 별도입니다."),*(T->TransportMode==EVirtualSensorTransportMode::HttpPost?T->GetTransportProfile().HttpEndpoint:T->GetTransportProfile().BrokerUrl),IsOwnedCompatibilityTransport()?TEXT("호환 전송"):TEXT("Raw TCP 고성능")):TEXT("Transport 연결 필요");
		if(bOwnedReceiverExpanded)CachedTopicReceiverSummary=GetTopicReceiverSummaryText();
		if(bOwnedTransportLogExpanded)CachedTransportLog=BuildTransportLogText();
	}
	RefreshNativeText();
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedWidget()
{
	auto Tabs=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
	const FText Names[]={LOCTEXT("Live","실시간 전송"),LOCTEXT("Files","파일 저장"),LOCTEXT("Connection","연결·진단")};
	for(int32 I=0;I<3;++I)Tabs->AddSlot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle())
	 .OnClicked_Lambda([this,I](){if(I==0)SetActiveTab(EVirtualSensorCaptureExportTab::LiveStream);else if(I==2)SetActiveTab(EVirtualSensorCaptureExportTab::ConnectionLog);else if(ResolveOwnedTabIndex(ActiveTab)!=1)SetActiveTab(EVirtualSensorCaptureExportTab::Capture);return FReply::Handled();})
	 [SNewSensorTool(STextBlock).ColorAndOpacity_Lambda([this,I](){return ResolveOwnedTabIndex(ActiveTab)==I?GetToolAccentColor():GetToolTextColor();}).Text(Names[I])]];
	return SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).ForegroundColor(GetToolTextColor()).Padding(16)
	[SNew(SVerticalBox)
	 +SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(LOCTEXT("Data","데이터"))]
	 +SVerticalBox::Slot().FillHeight(1)[SNew(SVerticalBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
	  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(OwnedSelectionText);})]
	  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Tabs]
	  +SVerticalBox::Slot().FillHeight(1)[SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return ResolveOwnedTabIndex(ActiveTab);})+SWidgetSwitcher::Slot()[BuildOwnedLiveTab()]+SWidgetSwitcher::Slot()[BuildOwnedFileTab()]+SWidgetSwitcher::Slot()[BuildOwnedConnectionTab()]]
	  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(LastUiMessage);})]
	  +SVerticalBox::Slot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true)
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
	auto Modes=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
	for(bool Scenario:{false,true})Modes->AddSlot()[SNewSensorTool(SButton).Text(Scenario?LOCTEXT("Auto","시나리오 연동"):LOCTEXT("Manual","직접 전송"))
	 .ForegroundColor_Lambda([this,Scenario](){return bOwnedScenarioStreamView==Scenario?GetToolAccentColor():GetToolTextColor();})
	 .OnClicked_Lambda([this,Scenario](){bOwnedScenarioStreamView=Scenario;return FReply::Handled();})];
	auto Auto=SNew(SVerticalBox);
	Auto->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text(LOCTEXT("AutoHelp","새 Slab 시나리오의 다음 실행에 적용합니다. 저장 목록 재생의 선택은 재생 창에서 별도로 지정합니다."))];
	auto Output=[this](int32 Index,const FText& Text)->TSharedRef<SWidget>
	{return SNew(SCheckBox).IsEnabled_Lambda([this](){return ResolveScenarioSlabActor()!=nullptr;})
	 .IsChecked_Lambda([this,Index](){const auto O=GetLiveScenarioOutputs();return (Index==0?O.bPointCloud:Index==1?O.bCameraImage:O.bLidarTelemetry)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
	 .OnCheckStateChanged_Lambda([this,Index](ECheckBoxState S){auto O=GetLiveScenarioOutputs();const bool B=S==ECheckBoxState::Checked;if(Index==0)O.bPointCloud=B;else if(Index==1)O.bCameraImage=B;else O.bLidarTelemetry=B;SetLiveScenarioOutputs(O);})[SNewSensorTool(STextBlock).Text(Text)];};
	Auto->AddSlot().AutoHeight().Padding(0,8)[Output(0,LOCTEXT("PcdAuto","PCD 포인트 클라우드 송신"))];
	Auto->AddSlot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).Text_Lambda([this](){const auto O=GetLiveScenarioOutputs();const int32 Count=(O.bCameraImage?1:0)+(O.bLidarTelemetry?1:0);return Count?FText::Format(LOCTEXT("ExtraEnabled","추가 송신 옵션 · {0}개 켜짐"),FText::AsNumber(Count)):LOCTEXT("Extra","추가 송신 옵션");})].BodyContent()[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Output(1,LOCTEXT("CameraAuto","Camera 이미지 Topic"))]+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Output(2,LOCTEXT("LidarAuto","LiDAR 정보 Topic"))]]];
	auto Manual=SNew(SVerticalBox);
	for(auto Kind:{EVirtualSensorStreamKind::PointCloud,EVirtualSensorStreamKind::CameraImage,EVirtualSensorStreamKind::LidarPayload})
	{
		const FText Label=Kind==EVirtualSensorStreamKind::PointCloud?LOCTEXT("PcdCard","포인트 클라우드 · PCD Binary"):Kind==EVirtualSensorStreamKind::CameraImage?LOCTEXT("CameraCard","Camera 이미지"):LOCTEXT("LidarCard","LiDAR 정보");
		Manual->AddSlot().AutoHeight().Padding(0,4)[SNew(SBorder).BorderImage(GetToolSectionBrush()).BorderBackgroundColor(GetToolSectionColor()).Padding(8)
		 [SNew(SVerticalBox)
		  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(STextBlock).Text(Label)]+SHorizontalBox::Slot().AutoWidth()[SNewSensorTool(SButton)
		   .IsEnabled_Lambda([this,Kind](){return !GetSelectedSensorIdForStream(Kind).IsEmpty()&&!IsSelectedStreamScenarioControlled(Kind);})
		   .Text_Lambda([this,Kind](){if(IsSelectedStreamScenarioControlled(Kind))return LOCTEXT("ScenarioControl","시나리오 연동 중");auto* P=SensorManager?SensorManager->StreamPublisherComponent.Get():nullptr;return P&&P->IsStreamEnabled(Kind,GetSelectedSensorIdForStream(Kind))?LOCTEXT("Stop","중지"):LOCTEXT("Start","시작");})
		   .OnClicked_Lambda([this,Kind](){ToggleOwnedStream(Kind);return FReply::Handled();})]]
		  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this,Kind](){return FText::FromString(TEXT("대상: ")+GetSelectedSensorIdForStream(Kind));})]
		  +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this,Kind](){const auto* S=WorkspaceStreamCards.Find(Kind);return FText::FromString(S?*S:TEXT("중지 · 시작 후 실제 송수신 상태를 표시합니다."));})]
		 ]];
	}
	return SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
	 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Modes]
	 +SVerticalBox::Slot().AutoHeight()[SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return bOwnedScenarioStreamView?1:0;})+SWidgetSwitcher::Slot()[Manual]+SWidgetSwitcher::Slot()[Auto]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true)
	  .OnAreaExpansionChanged_Lambda([this](bool Open){bWorkspaceLiveDetails=Open;LastNativeStatusRefreshSeconds=-1;})
	  .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Policy","전송 필터·상세 정책"))]
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
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).AutoWrapText(true).Text_Lambda([this](){return OwnedFileAction==0?LOCTEXT("NewHelp","선택 센서의 새 프레임을 비동기로 기다립니다. 요청 후 센서를 바꿔도 저장 대상은 유지됩니다."):OwnedFileAction==1?LOCTEXT("CurrentHelp","마지막으로 완료된 프레임을 저장합니다. 추가 측정을 하지 않습니다."):LOCTEXT("PeriodicHelp","시작할 때의 센서·출력·간격으로 최신 완료 프레임을 저장합니다. Topic 송신 간격과는 별개입니다.");})]
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
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){if(OwnedFileAction==2)return FText::FromString(OwnedPeriodicStatusText);if(OwnedFileAction==1&&!bOwnedCanSaveCurrent)return LOCTEXT("NoFrame","완료된 프레임이 없습니다. 측정을 시작하거나 새 프레임 저장을 선택하세요.");return FText::FromString(OwnedFileStatusText);})]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("FileTools","파일 전송 도구"))].BodyContent()[SNewSensorTool(SButton).Text(LOCTEXT("SendRecent","최근 파일 수동 전송")).OnClicked_Lambda([this](){SendLastExportToServer();return FReply::Handled();})]]
	];
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedConnectionTab()
{
	auto Server=SNew(SVerticalBox);
	Server->AddSlot().AutoHeight().Padding(0,8)[SNewSensorTool(SButton).Text_Lambda([this](){return bUseStompTransport?LOCTEXT("Stomp","방식: Artemis STOMP"):LOCTEXT("Http","방식: HTTP POST");}).OnClicked_Lambda([this](){bUseStompTransport=!bUseStompTransport;return FReply::Handled();})];
	auto Field=[this,Server](FText Label,FString* Value,bool Secret,bool StompOnly,bool HttpOnly)
	{Server->AddSlot().AutoHeight().Padding(0,4)[SNew(SVerticalBox).Visibility_Lambda([this,StompOnly,HttpOnly](){return (StompOnly&&!bUseStompTransport)||(HttpOnly&&bUseStompTransport)?EVisibility::Collapsed:EVisibility::Visible;})
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text(Label)]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNewSensorTool(SEditableTextBox).IsPassword(Secret).Text_Lambda([Value](){return FText::FromString(*Value);}).OnTextCommitted_Lambda([Value](const FText& T,ETextCommit::Type){*Value=T.ToString();})]];};
	Field(LOCTEXT("Broker","Broker URL"),&DraftBrokerUrl,false,true,false);Field(LOCTEXT("Endpoint","HTTP endpoint"),&DraftHttpEndpoint,false,false,true);
	Field(LOCTEXT("PointTopic","PCD Topic"),&DraftExportTopic,false,true,false);Field(LOCTEXT("CameraTopic","Camera Topic"),&DraftCameraTopic,false,true,false);Field(LOCTEXT("LidarTopic","LiDAR Topic"),&DraftLidarTopic,false,true,false);
	Field(LOCTEXT("User","사용자명"),&DraftUserName,false,true,false);Field(LOCTEXT("Password","비밀번호 · 세션에서만 유지"),&SessionPasscode,true,true,false);Field(LOCTEXT("Bearer","Bearer token · 세션에서만 유지"),&SessionBearerToken,true,false,true);
	Field(LOCTEXT("Ack","소비자 ACK Topic · 선택 사항"),&DraftAckTopic,false,true,false);
	Server->AddSlot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).Text(LOCTEXT("MaxBytes","최대 메시지 크기 (bytes)"))];
	Server->AddSlot().AutoHeight()[SNewSensorTool(SEditableTextBox).Text_Lambda([this](){return FText::AsNumber(DraftMaxMessageBytes);}).OnTextCommitted_Lambda([this](const FText& T,ETextCommit::Type){DraftMaxMessageBytes=FMath::Max(1024,FCString::Atoi(*T.ToString()));})];
	Server->AddSlot().AutoHeight().Padding(0,8)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
	 +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("Apply","설정 적용")).OnClicked_Lambda([this](){ApplyTransportProfile();LastNativeStatusRefreshSeconds=-1;return FReply::Handled();})]
	 +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("Test","설정 적용·연결 시험")).OnClicked_Lambda([this](){TestServerConnection();LastNativeStatusRefreshSeconds=-1;return FReply::Handled();})]];
	return SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
	 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(OwnedConnectionText);})]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("ServerSettings","서버 연결 설정"))].BodyContent()[Server]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true).OnAreaExpansionChanged_Lambda([this](bool B){bOwnedReceiverExpanded=B;LastNativeStatusRefreshSeconds=-1;})
	  .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Receiver","Broker 실제 수신 검증"))].BodyContent()[SNew(SVerticalBox)
	   +SVerticalBox::Slot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
	    +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("ReceiverStop","수신 구독 끊기")).OnClicked_Lambda([this](){StopTopicReceivers();return FReply::Handled();})]
	    +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("ReceiverReconnect","수신 다시 연결")).OnClicked_Lambda([this](){NextReceiverLookup=0;ReceiverLookupDelay=.5;ReconnectTopicReceivers();return FReply::Handled();})]]
	   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(CachedTopicReceiverSummary);})]]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true).OnAreaExpansionChanged_Lambda([this](bool B){bOwnedTransportLogExpanded=B;LastNativeStatusRefreshSeconds=-1;})
	  .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Log","전송 기록·호환 도구"))].BodyContent()[SNew(SVerticalBox)
	   +SVerticalBox::Slot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
	    +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("DiagnosticReport","진단 보고서 저장")).OnClicked_Lambda([this](){ExportTransportDiagnosticReport();return FReply::Handled();})]
	    +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("LegacyPayload","현재 JSON Payload 수동 전송")).OnClicked_Lambda([this](){SendSelectedPayloadToServer();return FReply::Handled();})]]
	   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return FText::FromString(CachedTransportLog);})]]]
	];
}
#undef LOCTEXT_NAMESPACE
