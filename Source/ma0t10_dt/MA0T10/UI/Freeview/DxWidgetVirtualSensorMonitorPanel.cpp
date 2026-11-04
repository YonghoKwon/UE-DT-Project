#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorMonitorPanel.h"
#include "DeviceUI/DeviceSensorMonitorBody.h"
#include "Device/Core/VirtualSensorTextureReadback.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "DeviceUI/SensorToolWidgetDecl.h"
#include "ma0t10_dt/MA0T10/UI/Common/SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorPlacementPanel.h"
#include "Widgets/SOverlay.h"

#include "Async/Async.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "Styling/CoreStyle.h"
#include "TextureResource.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Device/Sensor/VirtualLidarHeight.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Device/Camera/VirtualCameraCaptureComp.h"
#include "Device/Sensor/RealSensorSourceComp.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarSensorTypes.h"
#include "Device/Sensor/VirtualLidarVisualizationComp.h"
#include "Device/Sensor/VirtualSensorTransportComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "Device/Core/VirtualSensorSchedulerSubsystem.h"
#include "Device/Core/VirtualSensorStreamPublisherComp.h"
#include "ma0t10_dt/MA0T10/UI/Common/VirtualSensorUiPreferences.h"
#include "DeviceUI/VirtualSensorUiStyle.h"

#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"

namespace
{

const TCHAR* SensorRateStateText(EVirtualSensorRateState State)
{
	switch (State)
	{
		case EVirtualSensorRateState::Stopped:
			return TEXT("정지");
		case EVirtualSensorRateState::WarmingUp:
			return TEXT("준비 중");
		case EVirtualSensorRateState::Paused:
			return TEXT("일시정지");
		case EVirtualSensorRateState::InteractionPreview:
			return TEXT("조작 미리보기");
		case EVirtualSensorRateState::Starved:
			return TEXT("정체");
		case EVirtualSensorRateState::Failed:
			return TEXT("측정 오류");
		default:
			return TEXT("측정 중");
	}
}
}

FVirtualSensorMonitorDisplayData UDxWidgetVirtualSensorMonitorPanel::GetMonitorDisplayData() const
{
	FVirtualSensorMonitorDisplayData Data;
	Data.bShowingLidar = bShowingLidar;
	Data.TitleText = GetMonitorTitleText();
	Data.SelectedSensorText = GetSelectedSensorIdText();
	Data.FrameText = GetFrameSummaryText();
	Data.MeasurementText = GetMeasurementSummaryText();
	Data.ServerPayloadText = GetServerPayloadSummaryText();
	Data.PreviewText = GetPreviewPolicySummaryText();
	Data.SlabText = GetSlabAnalysisSummaryText();
	Data.LazExportText = GetLazExportSummaryText();
	Data.TransportText = GetTransportStatusSummaryText();
	Data.WarningText = GetTransportWarningText();
	Data.ViewModeText = GetViewModeSummaryText();
	Data.AcceptanceGateText = GetAcceptanceGateSummaryText();
	Data.RealSensorText = GetRealSensorDeploymentSummaryText();
	Data.FullStatusText = GetMonitorStatusText();
	return Data;
}

bool UDxWidgetVirtualSensorMonitorPanel::SupportsRangeOverlays(ELidarMonitorProjectionMode P)
{
	return FDeviceSensorMonitorBody::SupportsRangeOverlays(P);
}
bool UDxWidgetVirtualSensorMonitorPanel::SupportsAdaptiveDistance(ELidarColorMode C)
{
	return FDeviceSensorMonitorBody::SupportsAdaptiveDistance(C);
}
EVirtualSensorMonitorPresentation UDxWidgetVirtualSensorMonitorPanel::GetMonitorPresentation() const
{
	if (SensorManager && SensorManager->IsPointCloudOnlyModeEnabled())
		return EVirtualSensorMonitorPresentation::PointCloudOnly;
	const auto* V = GetLidarVisualizationComponent();
	return V && V->GetVisualizationSettings().bShowWorldPointCloud ? EVirtualSensorMonitorPresentation::WorldOverlay : EVirtualSensorMonitorPresentation::TwoDimensional;
}
void UDxWidgetVirtualSensorMonitorPanel::SetMonitorPresentation(EVirtualSensorMonitorPresentation P)
{
	if (!LidarComp || P > EVirtualSensorMonitorPresentation::PointCloudOnly)
		return;
	if (P == EVirtualSensorMonitorPresentation::PointCloudOnly)
	{
		if (SensorManager)
			SensorManager->SetPointCloudOnlyMode(true);
	}
	else
	{
		// Leaving point-only restores the old world/preview state before applying this explicit choice.
		if (SensorManager && SensorManager->IsPointCloudOnlyModeEnabled())
			SensorManager->SetPointCloudOnlyMode(false);
		SetLidarWorldPointCloudEnabled(P == EVirtualSensorMonitorPresentation::WorldOverlay);
	}
	RefreshNativeFallbackText();
}
AVirtualSensorActorBase* UDxWidgetVirtualSensorMonitorPanel::GetOwnedMonitoredActor() const
{
	return bShowingLidar ? (IsValid(LidarComp) ? Cast<AVirtualSensorActorBase>(LidarComp->GetOwner()) : nullptr)
	                     : (IsValid(CameraComp) ? Cast<AVirtualSensorActorBase>(CameraComp->GetOwner()) : nullptr);
}
bool UDxWidgetVirtualSensorMonitorPanel::StartOwnedMonitoredEquipment()
{
	auto* Actor = OwnedPreviewTarget.Get();
	auto* W = GetToolWorkspace();
	auto* Placement = W ? Cast<UDxWidgetVirtualSensorPlacementPanel>(W->GetOwnedPanel(ESensorToolPanelRole::Placement)) : nullptr;
	if (!IsValid(Actor) || Actor != GetOwnedMonitoredActor() || !SensorManager || !Placement || !SensorManager->GetSensorActors().Contains(Actor))
	{
		RefreshOwnedMonitorText();
		return false;
	}
	if (SensorManager->GetSelectedSensorActor() != Actor)
		SensorManager->SelectSensorActor(Actor);
	const bool Result = Placement->SetSelectedMeasuring(true);
	RefreshOwnedMonitorText();
	return Result;
}
bool UDxWidgetVirtualSensorMonitorPanel::OpenOwnedEquipmentPlacement()
{
	auto* W = GetToolWorkspace();
	if (!W || !W->GetOwnedPanel(ESensorToolPanelRole::Placement))
		return false;
	W->SetPanelOpen(ESensorToolPanelRole::Placement, true);
	W->BringOwnedPanelToFront(ESensorToolPanelRole::Placement);
	return true;
}

void UDxWidgetVirtualSensorMonitorPanel::NativeConstruct()
{
	Super::NativeConstruct();
	RestoreMonitorUiPreferences();

	if (ToggleButton)
	{
		ToggleButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleToggleButtonClicked);
		ToggleButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleToggleButtonClicked);
	}
	if (NextCameraButton)
	{
		NextCameraButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleNextCameraButtonClicked);
		NextCameraButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleNextCameraButtonClicked);
	}
	if (NextLidarButton)
	{
		NextLidarButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleNextLidarButtonClicked);
		NextLidarButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleNextLidarButtonClicked);
	}
	if (PointCloudOnlyButton)
	{
		PointCloudOnlyButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePointCloudOnlyButtonClicked);
		PointCloudOnlyButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePointCloudOnlyButtonClicked);
	}
	if (LidarViewModeButton)
	{
		LidarViewModeButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleLidarViewModeButtonClicked);
		LidarViewModeButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleLidarViewModeButtonClicked);
	}
	if (LogPointCloudButton)
	{
		LogPointCloudButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleLogPointCloudButtonClicked);
		LogPointCloudButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleLogPointCloudButtonClicked);
	}
	if (ExportPointCloudButton)
	{
		ExportPointCloudButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleExportPointCloudButtonClicked);
		ExportPointCloudButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleExportPointCloudButtonClicked);
	}
	if (LocalSensorCaptureButton)
	{
		LocalSensorCaptureButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleLocalSensorCaptureButtonClicked);
		LocalSensorCaptureButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleLocalSensorCaptureButtonClicked);
	}
	if (CaptureOnceButton)
	{
		CaptureOnceButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleCaptureOnceButtonClicked);
		CaptureOnceButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleCaptureOnceButtonClicked);
	}
	if (ExportServerPayloadButton)
	{
		ExportServerPayloadButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleExportServerPayloadButtonClicked);
		ExportServerPayloadButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleExportServerPayloadButtonClicked);
	}
	if (PreviewMoreButton)
	{
		PreviewMoreButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePreviewMoreButtonClicked);
		PreviewMoreButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePreviewMoreButtonClicked);
	}
	if (PreviewLessButton)
	{
		PreviewLessButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePreviewLessButtonClicked);
		PreviewLessButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePreviewLessButtonClicked);
	}
	if (PreviewHitOnlyButton)
	{
		PreviewHitOnlyButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePreviewHitOnlyButtonClicked);
		PreviewHitOnlyButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePreviewHitOnlyButtonClicked);
	}
	if (StartRealSensorSourcesButton)
	{
		StartRealSensorSourcesButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleStartRealSensorSourcesButtonClicked);
		StartRealSensorSourcesButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleStartRealSensorSourcesButtonClicked);
	}
	if (StopRealSensorSourcesButton)
	{
		StopRealSensorSourcesButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleStopRealSensorSourcesButtonClicked);
		StopRealSensorSourcesButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandleStopRealSensorSourcesButtonClicked);
	}
	if (PushRealSensorSourceButton)
	{
		PushRealSensorSourceButton->OnClicked.RemoveDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePushRealSensorSourceButtonClicked);
		PushRealSensorSourceButton->OnClicked.AddDynamic(this, &UDxWidgetVirtualSensorMonitorPanel::HandlePushRealSensorSourceButtonClicked);
	}

	RefreshTitle();
	RefreshImageBrush();
	RefreshStatusText();
	RefreshLocalCaptureButtonText();
	RefreshLidarViewModeButtonText();
	if (PreviewHitOnlyButtonText)
	{
		const UVirtualLidarScanComp* TargetLidar = GetTargetLidarForPreview();
		PreviewHitOnlyButtonText->SetText(FText::FromString(TargetLidar && TargetLidar->bPointCloudPreviewHitOnly ? TEXT("미리보기: 검출점만") : TEXT("미리보기: 모든 점")));
	}
}

void UDxWidgetVirtualSensorMonitorPanel::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalSensorCaptureTimerHandle);
	}
	bLocalSensorCaptureActive = false;
	PendingCameraReadbacks.Reset();
	LocalCaptureCameraAsyncWriteCount = 0;
	bLocalCaptureCameraWritePending = false;
	Super::NativeDestruct();
}

void UDxWidgetVirtualSensorMonitorPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (IsWorkspaceOwned() && (!WidgetTree || !WidgetTree->RootWidget) && (!IsVisible() || IsPanelCollapsed()))
		return;
	if (!IsWorkspaceOwned())
		PumpPendingCaptureWork();
	RefreshImageBrush();
	StatusRefreshAccumulator += InDeltaTime;
	if (StatusRefreshAccumulator >= 0.2)
	{
		StatusRefreshAccumulator = 0;
		// The default owned RGB view has no GT display or open GT controls to refresh.
		if (CameraTruthDisplayMode != 0 || bOwnedAdvancedMenuOpen || !IsWorkspaceOwned())
			RefreshCameraTruth();
		RefreshStatusText();
	}
}

void UDxWidgetVirtualSensorMonitorPanel::BindVirtualCamera(UVirtualCameraCaptureComp* InCameraComp)
{
	CameraComp = InCameraComp;
	if (CameraComp)
		PreferredPrimaryCameraId = CameraComp->SensorId;
	ResolveDualCameraSelection();
	if (GetWorld())
	{
		if (UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>())
			Subsystem->SetPreferredCamera(InCameraComp);
	}
	RefreshImageBrush();
	RefreshStatusText();
}

void UDxWidgetVirtualSensorMonitorPanel::BindSensorManager(AVirtualSensorCoordinator* InSensorManager)
{
	SensorManager = InSensorManager;
	if (SensorManager)
	{
		RestoreMonitorUiPreferences();
		RefreshCameraSelectionOptions();
		ResolveDualCameraSelection();
		SensorManager->BindMonitorWidget(this);
	}
	RefreshStatusText();
}

void UDxWidgetVirtualSensorMonitorPanel::RefreshCameraSelectionOptions()
{
	NativeCameraOptions.Reset();
	if (SensorManager)
	{
		for (int32 Index = 0; Index < SensorManager->GetCameraCount(); ++Index)
		{
			if (const UVirtualCameraCaptureComp* Camera = SensorManager->GetCameraByIndex(Index))
				NativeCameraOptions.Add(MakeShared<FString>(Camera->SensorId));
		}
	}
	if (NativePrimaryCameraCombo.IsValid())
		NativePrimaryCameraCombo->RefreshOptions();
	if (NativeSecondaryCameraCombo.IsValid())
		NativeSecondaryCameraCombo->RefreshOptions();
}

void UDxWidgetVirtualSensorMonitorPanel::ResolveDualCameraSelection()
{
	if (!SensorManager)
		return;
	if (!PreferredPrimaryCameraId.IsEmpty())
	{
		const int32 PrimaryIndex = SensorManager->FindCameraIndexBySensorId(PreferredPrimaryCameraId);
		if (PrimaryIndex != INDEX_NONE && SensorManager->GetSelectedCamera() != SensorManager->GetCameraByIndex(PrimaryIndex))
			SensorManager->SelectCameraByIndex(PrimaryIndex);
	}
	UVirtualCameraCaptureComp* Primary = SensorManager->GetSelectedCamera();
	if (Primary)
		PreferredPrimaryCameraId = Primary->SensorId;
	SecondaryCameraComp = nullptr;
	if (!PreferredSecondaryCameraId.IsEmpty())
	{
		const int32 SecondaryIndex = SensorManager->FindCameraIndexBySensorId(PreferredSecondaryCameraId);
		if (SecondaryIndex != INDEX_NONE)
			SecondaryCameraComp = SensorManager->GetCameraByIndex(SecondaryIndex);
	}
	if (!SecondaryCameraComp || SecondaryCameraComp == Primary)
	{
		for (int32 Index = 0; Index < SensorManager->GetCameraCount(); ++Index)
		{
			UVirtualCameraCaptureComp* Candidate = SensorManager->GetCameraByIndex(Index);
			if (Candidate && Candidate != Primary)
			{
				SecondaryCameraComp = Candidate;
				break;
			}
		}
	}
	PreferredSecondaryCameraId = SecondaryCameraComp ? SecondaryCameraComp->SensorId : FString();
	if (SensorManager->GetCameraCount() < 2)
		bDualCameraModeEnabled = false;
}

void UDxWidgetVirtualSensorMonitorPanel::SetDualCameraModeEnabled(bool bEnabled)
{
	bDualCameraModeEnabled = bEnabled && SensorManager && SensorManager->GetCameraCount() >= 2;
	ResolveDualCameraSelection();
	SaveMonitorUiPreferences();
	RefreshImageBrush();
}

void UDxWidgetVirtualSensorMonitorPanel::SetDualCameraSplitRatio(float Ratio)
{
	DualCameraSplitRatio = FMath::IsFinite(Ratio) ? FMath::Clamp(Ratio, .3f, .7f) : .7f;
	const int32 Primary = FMath::RoundToInt(DualCameraSplitRatio * 100);
	DualCameraSplitText = FText::Format(NSLOCTEXT("VirtualSensorMonitorPanelWidget", "CameraSplit", "비율 {0}:{1}"), FText::AsNumber(Primary), FText::AsNumber(100 - Primary));
}

bool UDxWidgetVirtualSensorMonitorPanel::SelectPrimaryCameraBySensorId(const FString& SensorId)
{
	if (!SensorManager)
		return false;
	const int32 Index = SensorManager->FindCameraIndexBySensorId(SensorId);
	if (Index == INDEX_NONE)
		return false;
	PreferredPrimaryCameraId = SensorId;
	SensorManager->SelectCameraByIndex(Index);
	ShowCameraView();
	ResolveDualCameraSelection();
	SaveMonitorUiPreferences();
	return true;
}

bool UDxWidgetVirtualSensorMonitorPanel::SelectSecondaryCameraBySensorId(const FString& SensorId)
{
	if (!SensorManager)
		return false;
	UVirtualCameraCaptureComp* Candidate = SensorManager->GetCameraByIndex(SensorManager->FindCameraIndexBySensorId(SensorId));
	if (!Candidate || Candidate == CameraComp)
		return false;
	SecondaryCameraComp = Candidate;
	PreferredSecondaryCameraId = SensorId;
	SaveMonitorUiPreferences();
	RefreshImageBrush();
	return true;
}

void UDxWidgetVirtualSensorMonitorPanel::BindRealSensorSource(URealSensorSourceComp* InRealSensorSourceComponent)
{
	RealSensorSourceComp = InRealSensorSourceComponent;
	RefreshStatusText();
}

void UDxWidgetVirtualSensorMonitorPanel::ShowCameraView()
{
	bShowingLidar = false;
	RefreshTitle();
	RefreshImageBrush();
	RefreshStatusText();
}

void UDxWidgetVirtualSensorMonitorPanel::ToggleView()
{
	bShowingLidar = !bShowingLidar;
	if (bShowingLidar && LidarComp && !LidarComp->GetLidarViewTexture())
	{
		RefreshLidarPreviewWithoutTransport();
	}
	RefreshTitle();
	RefreshImageBrush();
	RefreshStatusText();
}

int32 UDxWidgetVirtualSensorMonitorPanel::StartRealSensorSources()
{
	const int32 StartedCount = SensorManager
	                               ? SensorManager->StartAllRealSensorSources()
	                               : (RealSensorSourceComp && RealSensorSourceComp->StartSource() ? 1 : 0);
	LastRealSensorControlMessage = FString::Printf(TEXT("Real Sensor Control: started %d source(s)"), StartedCount);
	RefreshStatusText();
	return StartedCount;
}

void UDxWidgetVirtualSensorMonitorPanel::StopRealSensorSources()
{
	if (SensorManager)
	{
		SensorManager->StopAllRealSensorSources();
	}
	else if (RealSensorSourceComp)
	{
		RealSensorSourceComp->StopSource();
	}
	LastRealSensorControlMessage = TEXT("Real Sensor Control: stop requested");
	RefreshStatusText();
}

bool UDxWidgetVirtualSensorMonitorPanel::PushSelectedRealSensorSourceOnce(bool bSendTransport)
{
	const bool bPushed = SensorManager
	                         ? SensorManager->PushSelectedRealSensorSourceOnce(bSendTransport)
	                         : (RealSensorSourceComp && RealSensorSourceComp->PushFrameOnce(bSendTransport));
	LastRealSensorControlMessage = FString::Printf(TEXT("Real Sensor Control: push %s transport=%s"),
	                                               bPushed ? TEXT("succeeded") : TEXT("failed"),
	                                               bSendTransport ? TEXT("true") : TEXT("false"));
	InvalidateEnhancedLidarView();
	RefreshImageBrush();
	RefreshStatusText();
	return bPushed;
}

void UDxWidgetVirtualSensorMonitorPanel::HandleToggleButtonClicked()
{
	if (SensorManager)
	{
		SensorManager->ToggleSensorView();
		return;
	}
	ToggleView();
}

void UDxWidgetVirtualSensorMonitorPanel::HandleNextCameraButtonClicked()
{
	if (SensorManager)
	{
		SensorManager->SelectNextCamera();
		SensorManager->SetViewMode(EVirtualSensorViewMode::Camera);
	}
}

void UDxWidgetVirtualSensorMonitorPanel::HandlePointCloudOnlyButtonClicked()
{
	if (SensorManager)
	{
		SensorManager->TogglePointCloudOnlyView();
	}
}

void UDxWidgetVirtualSensorMonitorPanel::HandleStartRealSensorSourcesButtonClicked()
{
	StartRealSensorSources();
}

void UDxWidgetVirtualSensorMonitorPanel::HandleStopRealSensorSourcesButtonClicked()
{
	StopRealSensorSources();
}

void UDxWidgetVirtualSensorMonitorPanel::HandlePushRealSensorSourceButtonClicked()
{
	PushSelectedRealSensorSourceOnce(true);
}

bool UDxWidgetVirtualSensorMonitorPanel::ShouldUseNativeFallbackWidget() const
{
	return GetClass() == UDxWidgetVirtualSensorMonitorPanel::StaticClass() ||
	       !WidgetTree || !WidgetTree->RootWidget;
}

FString UDxWidgetVirtualSensorMonitorPanel::FormatOwnedSensorRate(const TCHAR* Kind, const FString& Id,
                                                                  const FVirtualSensorRuntimeStatus& Status, bool bRunning, const FVirtualSensorRateDiagnostic* Rate)
{
	const float Hz = Rate ? Rate->EffectiveHz : (bRunning ? Status.MeasuredCompletionRateHz : 0.0f);
	const auto State = Rate ? Rate->State : (bRunning ? EVirtualSensorRateState::Running : EVirtualSensorRateState::Stopped);
	return FString::Printf(TEXT("%s %s · %s · %.2f Hz"), Kind, *Id, SensorRateStateText(State), Hz);
}

const FVirtualSensorRateDiagnostic* UDxWidgetVirtualSensorMonitorPanel::GetSelectedRateDiagnostic() const
{
	// Only our registered panel consumes this current-state presentation contract.
	if (!IsWorkspaceOwned() || !GetWorld())
		return nullptr;
	const auto* Scheduler = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>();
	if (!Scheduler)
		return nullptr;
	const FString Id = bShowingLidar && LidarComp ? LidarComp->SensorId : (!bShowingLidar && CameraComp ? CameraComp->SensorId : FString());
	const FString Kind = bShowingLidar ? TEXT("LiDAR") : TEXT("Camera");
	return Scheduler->GetTelemetry().SensorRates.FindByPredicate([&](const FVirtualSensorRateDiagnostic& Rate)
	                                                             { return Rate.SensorId == Id && Rate.SensorKind == Kind; });
}

void UDxWidgetVirtualSensorMonitorPanel::RestoreMonitorUiPreferences()
{
	if (!bPersistMonitorPreferences)
		return;
	const UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
	if (!Preferences)
		return;
	bUseAdaptiveLidarDepthRange = Preferences->bUseAdaptiveLidarDepthRange;
	bOverlayLidarMonitorGrid = Preferences->bOverlayLidarMonitorGrid;
	bOverlayLidarDepthEdges = Preferences->bOverlayLidarDepthEdges;
	bMonitorDetailsExpanded = Preferences->bMonitorDetailsExpanded;
	if (!bMonitorPreferencesRestored)
	{
		bDualCameraModeEnabled = Preferences->bDualCameraModeEnabled;
		PreferredPrimaryCameraId = Preferences->PrimaryCameraSensorId;
		PreferredSecondaryCameraId = Preferences->SecondaryCameraSensorId;
		bMonitorPreferencesRestored = true;
	}
	if (LidarComp && Preferences->LidarViewMode <= static_cast<uint8>(EVirtualLidarViewMode::ActorClassColor))
	{
		LidarComp->ViewMode = static_cast<EVirtualLidarViewMode>(Preferences->LidarViewMode);
	}
	if (UVirtualLidarVisualizationComp* Visualization = GetLidarVisualizationComponent())
	{
		FVirtualLidarVisualizationSettings Settings = Visualization->GetVisualizationSettings();
		Settings.ProjectionMode = Preferences->LidarProjectionMode;
		Settings.ColorMode = Preferences->LidarColorMode;
		Settings.bShowWorldPointCloud = Preferences->bShowWorldLidarPointCloud;
		Settings.PointSize = Preferences->LidarPointSize;
		Settings.HeightReference = Preferences->LidarHeightReference;
		Settings.bAutoHeightRange = Preferences->bLidarAutoHeightRange;
		Settings.HeightMinMeters = Preferences->LidarHeightMinMeters;
		Settings.HeightMaxMeters = Preferences->LidarHeightMaxMeters;
		Settings.bUseAdaptiveDistance = bUseAdaptiveLidarDepthRange;
		Settings.bShowGrid = bOverlayLidarMonitorGrid;
		Settings.bShowDepthEdges = bOverlayLidarDepthEdges;
		Settings.bWorldTopDownAutoFit = Preferences->bWorldTopDownAutoFit;
		Visualization->SetVisualizationSettings(Settings);
	}
}

void UDxWidgetVirtualSensorMonitorPanel::SaveMonitorUiPreferences() const
{
	if (!bPersistMonitorPreferences)
		return;
	UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
	if (!Preferences)
		return;
	if (LidarComp)
		Preferences->LidarViewMode = static_cast<uint8>(LidarComp->ViewMode);
	if (const UVirtualLidarVisualizationComp* Visualization = GetLidarVisualizationComponent())
	{
		const FVirtualLidarVisualizationSettings& Settings = Visualization->GetVisualizationSettings();
		Preferences->LidarProjectionMode = Settings.ProjectionMode;
		Preferences->LidarColorMode = Settings.ColorMode;
		Preferences->bShowWorldLidarPointCloud = Settings.bShowWorldPointCloud;
		Preferences->LidarPointSize = Settings.PointSize;
		Preferences->LidarHeightReference = Settings.HeightReference;
		Preferences->bLidarAutoHeightRange = Settings.bAutoHeightRange;
		Preferences->LidarHeightMinMeters = Settings.HeightMinMeters;
		Preferences->LidarHeightMaxMeters = Settings.HeightMaxMeters;
		Preferences->bWorldTopDownAutoFit = Settings.bWorldTopDownAutoFit;
	}
	Preferences->bUseAdaptiveLidarDepthRange = bUseAdaptiveLidarDepthRange;
	Preferences->bOverlayLidarMonitorGrid = bOverlayLidarMonitorGrid;
	Preferences->bOverlayLidarDepthEdges = bOverlayLidarDepthEdges;
	Preferences->bMonitorDetailsExpanded = bMonitorDetailsExpanded;
	Preferences->bDualCameraModeEnabled = bDualCameraModeEnabled;
	Preferences->PrimaryCameraSensorId = PreferredPrimaryCameraId;
	Preferences->SecondaryCameraSensorId = PreferredSecondaryCameraId;
	UVirtualSensorUiPreferencesSaveGame::Save(Preferences);
}

void UDxWidgetVirtualSensorMonitorPanel::ResetMonitorUiPreferencesToDefault()
{
	bMonitorDetailsExpanded = false;
	bDualCameraModeEnabled = false;
	PreferredPrimaryCameraId.Reset();
	PreferredSecondaryCameraId.Reset();
	SecondaryCameraComp = nullptr;
	bUseAdaptiveLidarDepthRange = true;
	bOverlayLidarMonitorGrid = true;
	bOverlayLidarDepthEdges = true;
	if (LidarComp)
		LidarComp->ViewMode = EVirtualLidarViewMode::DepthGradient;
	if (UVirtualLidarVisualizationComp* Visualization = GetLidarVisualizationComponent())
	{
		Visualization->SetVisualizationSettings(FVirtualLidarVisualizationSettings());
	}
	InvalidateEnhancedLidarView();
	SaveMonitorUiPreferences();
	RefreshNativeFallbackText();
}

bool UDxWidgetVirtualSensorMonitorPanel::QueueCameraGpuReadbackToDisk(const FString& FramePrefix)
{
	if (!CameraComp || PendingCameraReadbacks.Num() >= FMath::Max(1, MaxPendingCameraReadbacks))
	{
		return false;
	}

	UTextureRenderTarget2D* RenderTarget = CameraComp->GetCameraRenderTarget();
	if (!RenderTarget)
	{
		return false;
	}

	if (!bLocalCaptureUseCachedSensorFrames)
	{
		CameraComp->CaptureScene();
	}

	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		return false;
	}

	FTextureRHIRef Texture = Resource->GetRenderTargetTexture();
	if (!Texture.IsValid())
	{
		return false;
	}

	const FString CameraDirectory = FPaths::Combine(EnsureLocalCaptureSessionDirectory(), TEXT("Camera"));
	IFileManager::Get().MakeDirectory(*CameraDirectory, true);

	FVirtualSensorPendingCameraReadback Pending;
	Pending.OutputPath = FPaths::Combine(CameraDirectory, FString::Printf(TEXT("%s_%s.jpg"), *FramePrefix, *CameraComp->SensorId));
	Pending.Width = RenderTarget->SizeX;
	Pending.Height = RenderTarget->SizeY;
	Pending.Readback = MakeShared<FRHIGPUTextureReadback, ESPMode::ThreadSafe>(TEXT("VirtualSensorCameraReadback"));

	TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback = Pending.Readback;
	ENQUEUE_RENDER_COMMAND(VirtualSensorQueueCameraReadback)([Readback, Texture](FRHICommandListImmediate& RHICmdList)
	                                                         {
        if (Readback.IsValid() && Texture.IsValid())
        {
            VirtualSensorTextureReadback::Enqueue(RHICmdList,*Readback,Texture);
        } });

	PendingCameraReadbacks.Add(MoveTemp(Pending));
	RefreshLocalCaptureCameraPendingState();
	return true;
}

void UDxWidgetVirtualSensorMonitorPanel::ProcessPendingCameraReadbacks()
{
	for (int32 Index = PendingCameraReadbacks.Num() - 1; Index >= 0; --Index)
	{
		FVirtualSensorPendingCameraReadback& Pending = PendingCameraReadbacks[Index];
		if (!Pending.Readback.IsValid())
		{
			PendingCameraReadbacks.RemoveAtSwap(Index);
			RefreshLocalCaptureCameraPendingState();
			continue;
		}

		if (!Pending.Readback->IsReady())
		{
			continue;
		}

		int32 RowPitchInPixels = 0;
		void* LockedData = Pending.Readback->Lock(RowPitchInPixels);
		if (!LockedData || RowPitchInPixels < Pending.Width || Pending.Width <= 0 || Pending.Height <= 0)
		{
			if (LockedData)
			{
				Pending.Readback->Unlock();
			}
			PendingCameraReadbacks.RemoveAtSwap(Index);
			RefreshLocalCaptureCameraPendingState();
			continue;
		}

		TArray<FColor> RawPixels;
		RawPixels.SetNumUninitialized(Pending.Width * Pending.Height);
		const FColor* SourcePixels = static_cast<const FColor*>(LockedData);
		for (int32 Y = 0; Y < Pending.Height; ++Y)
		{
			FMemory::Memcpy(RawPixels.GetData() + Y * Pending.Width, SourcePixels + Y * RowPitchInPixels, Pending.Width * sizeof(FColor));
		}
		Pending.Readback->Unlock();

		const FString Path = Pending.OutputPath;
		const int32 Width = Pending.Width;
		const int32 Height = Pending.Height;
		PendingCameraReadbacks.RemoveAtSwap(Index);
		RefreshLocalCaptureCameraPendingState();
		StartAsyncCameraJpegWrite(MoveTemp(RawPixels), Width, Height, Path);
	}

	RefreshLocalCaptureCameraPendingState();
}

void UDxWidgetVirtualSensorMonitorPanel::StartAsyncCameraJpegWrite(TArray<FColor>&& RawPixels, int32 Width, int32 Height, const FString& Path)
{
	IImageWrapperModule* ImageWrapperModule = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TWeakObjectPtr<UDxWidgetVirtualSensorMonitorPanel> WeakThis(this);
	++LocalCaptureCameraAsyncWriteCount;
	RefreshLocalCaptureCameraPendingState();

	Async(EAsyncExecution::ThreadPool, [WeakThis, ImageWrapperModule, RawPixels = MoveTemp(RawPixels), Path, Width, Height]() mutable
	      {
        bool bSaved = false;
        if (ImageWrapperModule)
        {
            TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::JPEG);
            if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
            {
                const TArray64<uint8>& Bytes64 = ImageWrapper->GetCompressed(80);
                if (Bytes64.Num() > 0)
                {
                    TArray<uint8> Bytes32;
                    Bytes32.Append(Bytes64.GetData(), static_cast<int32>(Bytes64.Num()));
                    bSaved = FFileHelper::SaveArrayToFile(Bytes32, *Path);
                }
            }
        }

        AsyncTask(ENamedThreads::GameThread, [WeakThis, Path, bSaved]()
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            WeakThis->LocalCaptureCameraAsyncWriteCount = FMath::Max(0, WeakThis->LocalCaptureCameraAsyncWriteCount - 1);
            WeakThis->RefreshLocalCaptureCameraPendingState();
            if (bSaved)
            {
                UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Async camera save saved: %s"), *Path);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[SensorMonitor] Async camera save failed: %s"), *Path);
            }
        }); });
}
#undef LOCTEXT_NAMESPACE
