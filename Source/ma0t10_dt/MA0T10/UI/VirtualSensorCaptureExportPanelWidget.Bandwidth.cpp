#include "VirtualSensorCaptureExportPanelWidget.h"

#include "VirtualSensorUiPreferences.h"
#include "SensorToolWidgetDecl.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"

namespace
{
FText PcdProfileLabel(EVirtualPcdFieldProfile Profile)
{
	static const FText Labels[]={FText::FromString(TEXT("Full · 33 B/점 (기본)")),FText::FromString(TEXT("Compact · 24 B/점")),FText::FromString(TEXT("Minimal · 12 B/점")),FText::FromString(TEXT("Custom · 필드 선택"))};
	return Labels[FMath::Clamp(int32(Profile),0,3)];
}
FText PcdDivisorLabel(int32 Divisor)
{
	static const FText Labels[]={FText::FromString(TEXT("서버 전송 1/1 · 20 Hz 측정 시 20 Hz")),FText::FromString(TEXT("서버 전송 1/2 · 20 Hz 측정 시 10 Hz")),FText::FromString(TEXT("서버 전송 1/4 · 20 Hz 측정 시 5 Hz"))};
	return Labels[Divisor==4?2:Divisor==2?1:0];
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
	const auto O=ReadPcdBandwidthOptions();double Selected=0,Total=0;int32 Count=0;
	if(SensorManager&&P)for(const auto& Ref:SensorManager->GetLidarComponents())
	{
		const auto* L=Ref.Get();if(!L)continue;++Count;
		const double Rate=FVirtualPcdCodec::EstimateBytesPerSecond(P->GetPcdSendOptions(L->SensorId),L->HorizontalSamples*L->VerticalChannels,1./FMath::Max(.001f,L->ScanInterval));
		Total+=Rate;if(L==SensorManager->GetSelectedLidar())Selected=Rate;
	}
	const double Budget=P?P->PointCloudBandwidthLimitMegabytesPerSecond:80.;
	CachedPcdBandwidthEstimate=FText::FromString(FString::Printf(TEXT("%d B/점 · 선택 대당 %.2f MiB/s / %.1f Mbps\n합계 %d대 %.2f MiB/s / %.1f Mbps · %.0f MiB/s 예산 %.1f%%\n규격 점 수 기준 비압축 추정 · 압축률·검출점·헤더에 따라 실제 전송량이 달라집니다"),FVirtualPcdCodec::BytesPerPoint(O),Selected/1048576.,Selected*8/1000000.,Count,Total/1048576.,Total*8/1000000.,Budget,Budget>0?Total/1048576./Budget*100:0.));
	return CachedPcdBandwidthEstimate;
}
TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildPcdBandwidthOptions()
{
	if(PcdProfileChoices.IsEmpty())for(auto V:{EVirtualPcdFieldProfile::Full,EVirtualPcdFieldProfile::Compact,EVirtualPcdFieldProfile::Minimal,EVirtualPcdFieldProfile::Custom})PcdProfileChoices.Add(MakeShared<EVirtualPcdFieldProfile>(V));
	if(PcdDivisorChoices.IsEmpty())for(int32 V:{1,2,4})PcdDivisorChoices.Add(MakeShared<int32>(V));
	auto Fields=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12,6));
	for(int32 I=0;I<11;++I)Fields->AddSlot()[SNew(SCheckBox)
		.IsEnabled(I<3?TAttribute<bool>(false):TAttribute<bool>::CreateLambda([this](){return ReadPcdBandwidthOptions().Profile==EVirtualPcdFieldProfile::Custom;}))
		.IsChecked_Lambda([this,I](){return ReadPcdBandwidthOptions().FieldMask()&(1<<I)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
		.OnCheckStateChanged_Lambda([this,I](ECheckBoxState State){auto O=ReadPcdBandwidthOptions();if(State==ECheckBoxState::Checked)O.CustomFieldMask|=1<<I;else O.CustomFieldMask&=~(1<<I);ApplyPcdBandwidthOptions(O);})
		[SNewSensorTool(STextBlock).Text(FText::FromString(FVirtualPcdCodec::FieldName(I)))]];
	return SNew(SBorder).BorderImage(GetToolSectionBrush()).BorderBackgroundColor(GetToolSectionColor()).Padding(12)
	[SNew(SVerticalBox)
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("PCD 전송 용량 · 측정·미리보기·파일 저장은 유지")))]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SCheckBox).IsChecked_Lambda([this](){return bPcdApplyAll?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
	  .OnCheckStateChanged_Lambda([this](ECheckBoxState V){bPcdApplyAll=V==ECheckBoxState::Checked;NextPcdBandwidthEstimate=0;})
	  [SNewSensorTool(STextBlock).Text(FText::FromString(TEXT("전체 LiDAR에 적용 (끄면 선택 LiDAR)")))]]
	 +SVerticalBox::Slot().AutoHeight()[SNew(SComboBox<TSharedPtr<EVirtualPcdFieldProfile>>).ComboBoxStyle(&FSensorOwnedPresentationStyle::ComboBox()).OptionsSource(&PcdProfileChoices)
	  .OnGenerateWidget_Lambda([this](TSharedPtr<EVirtualPcdFieldProfile> V){return SNewSensorTool(STextBlock).Text(PcdProfileLabel(*V));})
	  .OnSelectionChanged_Lambda([this](TSharedPtr<EVirtualPcdFieldProfile> V,ESelectInfo::Type){if(V){auto O=ReadPcdBandwidthOptions();O.Profile=*V;ApplyPcdBandwidthOptions(O);}})
	  [SNewSensorTool(STextBlock).Text_Lambda([this](){return PcdProfileLabel(ReadPcdBandwidthOptions().Profile);})]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Fields]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SCheckBox).IsChecked_Lambda([this](){return ReadPcdBandwidthOptions().bCompressed?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
	  .OnCheckStateChanged_Lambda([this](ECheckBoxState V){auto O=ReadPcdBandwidthOptions();O.bCompressed=V==ECheckBoxState::Checked;ApplyPcdBandwidthOptions(O);})
	  [SNewSensorTool(STextBlock).Text(FText::FromString(TEXT("LZF 압축 · PCD binary_compressed")))]]
	 +SVerticalBox::Slot().AutoHeight()[SNew(SComboBox<TSharedPtr<int32>>).ComboBoxStyle(&FSensorOwnedPresentationStyle::ComboBox()).OptionsSource(&PcdDivisorChoices)
	  .OnGenerateWidget_Lambda([this](TSharedPtr<int32> V){return SNewSensorTool(STextBlock).Text(FText::FromString(FString::Printf(TEXT("전송 1/%d"),*V)));})
	  .OnSelectionChanged_Lambda([this](TSharedPtr<int32> V,ESelectInfo::Type){if(V){auto O=ReadPcdBandwidthOptions();O.SendDivisor=*V;ApplyPcdBandwidthOptions(O);}})
	  [SNewSensorTool(STextBlock).Text_Lambda([this](){return PcdDivisorLabel(ReadPcdBandwidthOptions().Divisor());})]]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return BuildPcdBandwidthEstimate();})]
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("Full 기본 · 필드 축소는 외부 수신기 합의 후 선택 · 변경 전 전송 중지·대기열 배수")))]];
}
