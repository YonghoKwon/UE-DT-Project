#include "VirtualSensorCaptureExportPanelWidget.h"

#include "VirtualSensorUiPreferences.h"
#include "VirtualPcdBandwidthPresentation.h"
#include "VirtualPcdOptionsPresentation.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "SensorToolWidgetDecl.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SBoxPanel.h"

namespace
{
const FText& PcdCompressionCompatibility()
{
 static const FText Text=FText::FromString(VirtualPcdOptionsPresentation::CompressionCompatibility());
 return Text;
}
FText PcdProfileLabel(EVirtualPcdFieldProfile Profile)
{
	return FText::FromString(VirtualPcdOptionsPresentation::ProfileLabel(Profile));
}
FText PcdDivisorLabel(int32 Divisor)
{
	return FText::FromString(VirtualPcdOptionsPresentation::DivisorLabel(Divisor));
}
}

FVirtualPcdSendOptions UVirtualSensorCaptureExportPanelWidget::ReadPcdBandwidthOptions() const
{
	const auto* P=SensorManager?SensorManager->StreamPublisherComponent.Get():nullptr;
	return P?P->GetPcdSendOptions(bPcdApplyAll?FString():GetSelectedSensorIdForStream(EVirtualSensorStreamKind::PointCloud)):FVirtualPcdSendOptions();
}
void UVirtualSensorCaptureExportPanelWidget::ApplyPcdBandwidthOptions(const FVirtualPcdSendOptions& O)
{
	FString Error;
	if(!SensorManager||!SensorManager->SetPcdBandwidthOptions(GetSelectedSensorIdForStream(EVirtualSensorStreamKind::PointCloud),O,bPcdApplyAll,Error))
	{LastUiMessage=Error;return;}
	if(auto* Save=UVirtualSensorUiPreferencesSaveGame::LoadOrCreate())
	{
		Save->PcdSendOptionsBySensor=SensorManager->StreamPublisherComponent->PcdSendOptionsBySensor;
		if(!UVirtualSensorUiPreferencesSaveGame::Save(Save)){LastUiMessage=TEXT("전송 옵션 적용됨 · 설정 저장 실패");return;}
	}
	LastUiMessage=TEXT("PCD 전송 옵션 적용·저장됨");NextPcdBandwidthEstimate=0;LastNativeStatusRefreshSeconds=-1;
}
FText UVirtualSensorCaptureExportPanelWidget::BuildPcdBandwidthEstimate() const
{
	// Slate may evaluate attributes while hidden. Do not scan/format then.
	if(GetVisibility()==ESlateVisibility::Hidden||GetVisibility()==ESlateVisibility::Collapsed||IsPanelCollapsed()||(IsWorkspaceOwned()&&OwnedTab!=ESensorOwnedDataTab::Transfer))return CachedPcdBandwidthEstimate;
	const double Now=FPlatformTime::Seconds();if(Now<NextPcdBandwidthEstimate)return CachedPcdBandwidthEstimate;
	NextPcdBandwidthEstimate=Now+.2;
	const auto* P=SensorManager?SensorManager->StreamPublisherComponent.Get():nullptr;
 FVirtualPcdBandwidthPresentation View;
 View.PointBytes=FVirtualPcdCodec::BytesPerPoint(ReadPcdBandwidthOptions());
 View.BudgetMiB=P?P->PointCloudBandwidthLimitMegabytesPerSecond:80.;
 const auto* Raw=GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>():nullptr;
 if(SensorManager&&P)for(const auto& Ref:SensorManager->GetLidarComponents())
 {
  const auto* L=Ref.Get();if(!L)continue;++View.SensorCount;
  const bool Selected=L==SensorManager->GetSelectedLidar();
  const double Rate=FVirtualPcdCodec::EstimateBytesPerSecond(P->GetPcdSendOptions(L->SensorId),L->HorizontalSamples*L->VerticalChannels,1./FMath::Max(.001f,L->ScanInterval));
  View.TotalMaximum+=Rate;if(Selected)View.SelectedMaximum=Rate;
  const auto Actual=Raw?Raw->GetRecentPcdBandwidth(L->SensorId,Now):FVirtualPcdBandwidthSample();
  const bool Active=P->IsStreamActiveOrAwaitingResume(EVirtualSensorStreamKind::PointCloud,L->SensorId);
  // Accepted FIFO writes can still finish after Stop; retain actuals until the window expires.
  if(!Active&&Actual.Frames==0)continue;
  ++View.ActiveCount;FVirtualSensorStreamConfig Route;
  if(Active&&(!Raw||!P->ResolveActiveStreamForSensor(EVirtualSensorStreamKind::PointCloud,L->SensorId,Route)||Route.TransportBackend!=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput||Route.PointCloudFormat!=EVirtualPointCloudStreamFormat::PCD||Route.PcdDataMode!=EVirtualPcdDataMode::Binary))
  {++View.UnmeasuredCount;continue;}
  View.Total.Add(Actual);if(Selected)View.Selected=Actual;
 }
 CachedPcdBandwidthEstimate=View.Text();CachedPcdBandwidthDetails=View.Details();
 CachedPcdFieldSummary=FText::FromString(VirtualPcdOptionsPresentation::FieldSummary(ReadPcdBandwidthOptions()));
	return CachedPcdBandwidthEstimate;
}
TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildPcdBandwidthOptions()
{
	if(PcdProfileChoices.IsEmpty())for(auto V:{EVirtualPcdFieldProfile::Full,EVirtualPcdFieldProfile::Compact,EVirtualPcdFieldProfile::Minimal,EVirtualPcdFieldProfile::Custom})PcdProfileChoices.Add(MakeShared<EVirtualPcdFieldProfile>(V));
	if(PcdDivisorChoices.IsEmpty())for(int32 V:{1,2,4})PcdDivisorChoices.Add(MakeShared<int32>(V));
	CachedPcdFieldSummary=FText::FromString(VirtualPcdOptionsPresentation::FieldSummary(ReadPcdBandwidthOptions()));
	auto Fields=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12,6));
	for(int32 I=0;I<11;++I)Fields->AddSlot()[SNew(SCheckBox)
		.IsEnabled(I<3?TAttribute<bool>(false):TAttribute<bool>::CreateLambda([this](){return ReadPcdBandwidthOptions().Profile==EVirtualPcdFieldProfile::Custom;}))
		.IsChecked_Lambda([this,I](){return ReadPcdBandwidthOptions().FieldMask()&(1<<I)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
		.OnCheckStateChanged_Lambda([this,I](ECheckBoxState State){auto O=ReadPcdBandwidthOptions();if(State==ECheckBoxState::Checked)O.CustomFieldMask|=1<<I;else O.CustomFieldMask&=~(1<<I);ApplyPcdBandwidthOptions(O);})
		.ToolTipText(FText::FromString(VirtualPcdOptionsPresentation::FieldTooltip(I)))
		[SNewSensorTool(STextBlock).Text(FText::FromString(VirtualPcdOptionsPresentation::FieldLabel(I))).AutoWrapText(true)]];
	return SNew(SBorder).BorderImage(GetToolSectionBrush()).BorderBackgroundColor(GetToolSectionColor()).Padding(12)
	[SNew(SVerticalBox)
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).Text(FText::FromString(TEXT("PCD 전송 설정")))]
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("측정·미리보기·파일 저장 유지")))]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SCheckBox).IsChecked_Lambda([this](){return bPcdApplyAll?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
	  .OnCheckStateChanged_Lambda([this](ECheckBoxState V){bPcdApplyAll=V==ECheckBoxState::Checked;NextPcdBandwidthEstimate=0;})
	  [SNewSensorTool(STextBlock).Text(FText::FromString(TEXT("전체 LiDAR에 적용"))).ToolTipText(FText::FromString(TEXT("끄면 선택 LiDAR에만 적용합니다")))]]
	 +SVerticalBox::Slot().AutoHeight()[SNew(SComboBox<TSharedPtr<EVirtualPcdFieldProfile>>).ComboBoxStyle(&FSensorOwnedPresentationStyle::ComboBox()).OptionsSource(&PcdProfileChoices)
	  .OnGenerateWidget_Lambda([this](TSharedPtr<EVirtualPcdFieldProfile> V){return SNewSensorTool(STextBlock).Text(PcdProfileLabel(*V)).AutoWrapText(true);})
	  .OnSelectionChanged_Lambda([this](TSharedPtr<EVirtualPcdFieldProfile> V,ESelectInfo::Type){if(V){auto O=ReadPcdBandwidthOptions();O.Profile=*V;ApplyPcdBandwidthOptions(O);}})
	  [SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return PcdProfileLabel(ReadPcdBandwidthOptions().Profile);})]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return CachedPcdFieldSummary;})]
	 +SVerticalBox::Slot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true)
	  .HeaderContent()[SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("필드 상세 · 사용자 지정에서 편집")))]
	  .BodyContent()[SNew(SVerticalBox)
	   +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("필드 편집: 사용자 지정에서만 가능 · XYZ 필수")))]
	   +SVerticalBox::Slot().AutoHeight()[Fields]]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SCheckBox).ToolTipText(PcdCompressionCompatibility()).IsChecked_Lambda([this](){return ReadPcdBandwidthOptions().bCompressed?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
	  .OnCheckStateChanged_Lambda([this](ECheckBoxState V){auto O=ReadPcdBandwidthOptions();O.bCompressed=V==ECheckBoxState::Checked;ApplyPcdBandwidthOptions(O);})
	  [SNewSensorTool(STextBlock).Text(FText::FromString(TEXT("LZF 압축")))]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("서버의 압축 해제 지원을 확인하세요"))).ToolTipText(PcdCompressionCompatibility())]
	 +SVerticalBox::Slot().AutoHeight()[SNew(SComboBox<TSharedPtr<int32>>).ComboBoxStyle(&FSensorOwnedPresentationStyle::ComboBox()).OptionsSource(&PcdDivisorChoices)
	  .ToolTipText(FText::FromString(VirtualPcdOptionsPresentation::DivisorTooltip()))
	  .OnGenerateWidget_Lambda([this](TSharedPtr<int32> V){return SNewSensorTool(STextBlock).Text(PcdDivisorLabel(*V));})
	  .OnSelectionChanged_Lambda([this](TSharedPtr<int32> V,ESelectInfo::Type){if(V){auto O=ReadPcdBandwidthOptions();O.SendDivisor=*V;ApplyPcdBandwidthOptions(O);}})
	  [SNewSensorTool(STextBlock).Text_Lambda([this](){return PcdDivisorLabel(ReadPcdBandwidthOptions().Divisor());})]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return BuildPcdBandwidthEstimate();}).ToolTipText_Lambda([this](){return CachedPcdBandwidthDetails;})]
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("전체가 기본 · 축소 전 수신기 합의\n변경 전 전송 중지·대기열 배수")))]];
}
