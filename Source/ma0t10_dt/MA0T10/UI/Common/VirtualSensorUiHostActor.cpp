#include "ma0t10_dt/MA0T10/UI/Common/VirtualSensorUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/Common/SensorToolAppearance.h"
#include "ma0t10_dt/MA0T10/UI/Common/VirtualSensorToolWorkspaceSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Device/Core/VirtualSensorLayoutSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorMonitorPanel.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorSettingsPanel.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorPlacementPanel.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorCaptureExportPanel.h"
#include "ma0t10_dt/MA0T10/UI/Common/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/VirtualSensorPanelHostComp.h"

AVirtualSensorUiHostActor::AVirtualSensorUiHostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PanelHostComp = CreateDefaultSubobject<UVirtualSensorPanelHostComp>(TEXT("SensorPanelHostComponent"));
}

void AVirtualSensorUiHostActor::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Workspace = GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>())
		if (!Workspace->RegisterPlacementHost(this))
			LastStatusMessage = TEXT("센서 UI Host가 중복되었습니다. 기존 Host를 확인하세요.");
	bScreenDebugLogVisible = GEngine ? GEngine->bEnableOnScreenDebugMessages : true;
	if (bHideScreenDebugLogOnBeginPlay)
		SetScreenDebugLogVisible(false);
	if (PanelHostComp)
	{
		PanelHostComp->bAllowViewportFallback = bAllowViewportFallback;
	}
	if (bAutoCreateOnBeginPlay)
	{
		CreateAndBindMonitorWidget();
		if (bAutoCreateToolPanels)
		{
			CreateAndBindToolWidgets();
			if (!PlacementWidget)
				SchedulePlacementRetry();
		}
	}
}

void AVirtualSensorUiHostActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bHostEnding = true;
	GetWorldTimerManager().ClearTimer(PlacementRetryTimer);
	if (auto* Workspace = GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>())
		Workspace->UnregisterPlacementHost(this);
	DestroyMonitorWidget();
	if (bHasScreenDebugLogOverride && GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = bSavedScreenDebugLogState;
		bScreenDebugLogVisible = bSavedScreenDebugLogState;
		bHasScreenDebugLogOverride = false;
	}
	Super::EndPlay(EndPlayReason);
}

void AVirtualSensorUiHostActor::SetScreenDebugLogVisible(bool bVisible)
{
	if (!GEngine)
		return;
	if (!bHasScreenDebugLogOverride)
	{
		bSavedScreenDebugLogState = GEngine->bEnableOnScreenDebugMessages;
		bHasScreenDebugLogOverride = true;
	}
	GEngine->bEnableOnScreenDebugMessages = bVisible;
	bScreenDebugLogVisible = bVisible;
}

UDxWidgetVirtualSensorMonitorPanel* AVirtualSensorUiHostActor::CreateAndBindMonitorWidget()
{
	if (MonitorWidget)
	{
		return MonitorWidget;
	}

	const TSubclassOf<UDxWidgetVirtualSensorMonitorPanel> WidgetClassToCreate = GetEffectiveMonitorWidgetClass();
	if (!WidgetClassToCreate)
	{
		LastStatusMessage = TEXT("MonitorWidgetClass is not set and native fallback is disabled.");
		UE_LOG(LogTemp, Warning, TEXT("[SensorMonitorHost] %s"), *LastStatusMessage);
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LastStatusMessage = TEXT("World is not available.");
		UE_LOG(LogTemp, Warning, TEXT("[SensorMonitorHost] %s"), *LastStatusMessage);
		return nullptr;
	}

	MonitorWidget = CreateWidget<UDxWidgetVirtualSensorMonitorPanel>(World, WidgetClassToCreate);
	if (!MonitorWidget)
	{
		LastStatusMessage = TEXT("Failed to create monitor widget.");
		UE_LOG(LogTemp, Warning, TEXT("[SensorMonitorHost] %s"), *LastStatusMessage);
		return nullptr;
	}

	AVirtualSensorCoordinator* ResolvedManager = ResolveSensorManager();
	auto* Workspace = World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	Workspace->SetCoordinator(ResolvedManager);
	Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Monitor, MonitorWidget);
	if (ResolvedManager)
	{
		MonitorWidget->BindSensorManager(ResolvedManager);
		if (bEnablePointCloudOnlyOnStart)
		{
			ResolvedManager->SetPointCloudOnlyMode(true);
		}
	}

	if (bShowLidarViewOnStart)
	{
		MonitorWidget->ShowLidarView();
	}

	SensorToolFontScale = USensorToolAppearance::LoadScale();
	MonitorWidget->SetSensorAppearanceOwner(this);
	MonitorWidget->SetPanelPersistenceKey(TEXT("Monitor"));
	MonitorWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::RightCenter, FVector2D(820.0f, 430.0f));
	MonitorWidget->SetPanelResizable(true);
	MonitorWidget->ResizeHandleSize = 32.0f;
	MonitorWidget->SetPanelResizeLimits(FVector2D(480.0f, 300.0f), FVector2D::ZeroVector);
	if (PanelHostComp)
	{
		PanelHostComp->RegisterPanel(MonitorWidget, ViewportZOrder);
	}

	if (bConfigurePlayerInputOnCreate)
	{
		ConfigurePlayerInput();
	}

	LastStatusMessage = FString::Printf(TEXT("Monitor widget created. Manager=%s Viewport=%s"),
	                                    ResolvedManager ? *ResolvedManager->GetName() : TEXT("None"),
	                                    bAllowViewportFallback ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("[SensorMonitorHost] %s"), *LastStatusMessage);
	return MonitorWidget;
}

void AVirtualSensorUiHostActor::DestroyMonitorWidget()
{
	if (PlacementWidget)
	{
		PlacementWidget->EndPlacement();
		PlacementWidget->RemoveFromParent();
		PlacementWidget = nullptr;
	}
	if (PanelHostComp)
	{
		PanelHostComp->UnregisterAllPanels();
	}
	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}
	if (CaptureExportWidget)
	{
		CaptureExportWidget->RemoveFromParent();
		CaptureExportWidget = nullptr;
	}
	if (MonitorWidget)
	{
		MonitorWidget->RemoveFromParent();
		MonitorWidget = nullptr;
		LastStatusMessage = TEXT("Monitor widget destroyed.");
	}
}

TSubclassOf<UDxWidgetVirtualSensorMonitorPanel> AVirtualSensorUiHostActor::GetEffectiveMonitorWidgetClass() const
{
	if (MonitorWidgetClass)
	{
		return MonitorWidgetClass;
	}
	return bUseNativeMonitorWidgetFallback ? UDxWidgetVirtualSensorMonitorPanel::StaticClass() : nullptr;
}

AVirtualSensorCoordinator* AVirtualSensorUiHostActor::ResolveSensorManager()
{
	if (SensorManager)
	{
		return SensorManager;
	}
	if (!bAutoFindSensorManager || !GetWorld())
	{
		return nullptr;
	}

	for (TActorIterator<AVirtualSensorCoordinator> It(GetWorld()); It; ++It)
	{
		if (*It)
		{
			SensorManager = *It;
			return SensorManager;
		}
	}
	return nullptr;
}

void AVirtualSensorUiHostActor::CreateAndBindToolWidgets()
{
	UWorld* World = GetWorld();
	AVirtualSensorCoordinator* ResolvedManager = ResolveSensorManager();
	if (!World || !World->GetFirstPlayerController() || !ResolvedManager)
	{
		LastStatusMessage = TEXT("배치 준비 대기: PlayerController와 센서 Coordinator가 필요합니다.");
		return;
	}
	if (World->GetSubsystem<UVirtualSensorLayoutSubsystem>()->Coordinator() != ResolvedManager)
	{
		LastStatusMessage = TEXT("배치 준비 실패: 같은 World에 Coordinator가 정확히 하나 필요합니다.");
		return;
	}
	auto* Workspace = World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	if (!Workspace->RegisterPlacementHost(this))
	{
		LastStatusMessage = TEXT("배치 준비 실패: 다른 센서 UI Host가 이미 등록되어 있습니다.");
		return;
	}
	Workspace->SetCoordinator(ResolvedManager);
	if (MonitorWidget)
		MonitorWidget->BindSensorManager(ResolvedManager);

	TSubclassOf<UDxWidgetVirtualSensorSettingsPanel> EffectiveSettingsClass = SettingsWidgetClass;
	if (!EffectiveSettingsClass && bUseNativeToolWidgetFallback)
	{
		EffectiveSettingsClass = UDxWidgetVirtualSensorSettingsPanel::StaticClass();
	}
	TSubclassOf<UDxWidgetVirtualSensorCaptureExportPanel> EffectiveCaptureClass = CaptureExportWidgetClass;
	if (!EffectiveCaptureClass && bUseNativeToolWidgetFallback)
	{
		EffectiveCaptureClass = UDxWidgetVirtualSensorCaptureExportPanel::StaticClass();
	}

	if (!SettingsWidget && EffectiveSettingsClass)
	{
		SettingsWidget = CreateWidget<UDxWidgetVirtualSensorSettingsPanel>(World, EffectiveSettingsClass);
		if (SettingsWidget)
		{
			if (!Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Settings, SettingsWidget))
			{
				SettingsWidget = nullptr;
				LastStatusMessage = TEXT("배치 준비 실패: 설정 패널 등록이 충돌합니다.");
				return;
			}
			SettingsWidget->SetPanelPersistenceKey(TEXT("Settings"));
			SettingsWidget->SetSensorAppearanceOwner(this);
			SettingsWidget->BindHostActor(this);
			SettingsWidget->BindSensorManager(ResolvedManager);
			SettingsWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::LeftCenter, FVector2D(450.0f, 640.0f));
			if (PanelHostComp)
			{
				PanelHostComp->RegisterPanel(SettingsWidget, SettingsViewportZOrder);
			}
		}
	}

	if (!PlacementWidget && (PlacementWidgetClass || bUseNativeToolWidgetFallback))
	{
		UClass* Class = PlacementWidgetClass ? PlacementWidgetClass.Get() : UDxWidgetVirtualSensorPlacementPanel::StaticClass();
		PlacementWidget = CreateWidget<UDxWidgetVirtualSensorPlacementPanel>(World, Class);
		if (PlacementWidget)
		{
			if (!Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Placement, PlacementWidget))
			{
				PlacementWidget = nullptr;
				LastStatusMessage = TEXT("배치 준비 실패: 배치 패널 등록이 충돌합니다.");
				return;
			}
			PlacementWidget->BindPlacementControls(SettingsWidget);
			PlacementWidget->SetPanelPersistenceKey(TEXT("Placement"));
			PlacementWidget->SetSensorAppearanceOwner(this);
			PlacementWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::LeftCenter, FVector2D(600, 760));
			if (PanelHostComp)
				PanelHostComp->RegisterPanel(PlacementWidget, 25);
		}
	}
	if (!CaptureExportWidget && EffectiveCaptureClass)
	{
		CaptureExportWidget = CreateWidget<UDxWidgetVirtualSensorCaptureExportPanel>(World, EffectiveCaptureClass);
		if (CaptureExportWidget)
		{
			World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->RegisterOwnedPanel(ESensorToolPanelRole::Data, CaptureExportWidget);
			CaptureExportWidget->SetPanelPersistenceKey(TEXT("CaptureExport"));
			CaptureExportWidget->SetSensorAppearanceOwner(this);
			CaptureExportWidget->BindSensorManager(ResolvedManager);
			CaptureExportWidget->BindMonitorWidget(MonitorWidget);
			CaptureExportWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::BottomCenter, FVector2D(900.0f, 620.0f));
			CaptureExportWidget->SetPanelResizable(true);
			CaptureExportWidget->ResizeHandleSize = 32.0f;
			CaptureExportWidget->SetPanelResizeLimits(FVector2D(620.0f, 360.0f), FVector2D::ZeroVector);
			if (PanelHostComp)
			{
				PanelHostComp->RegisterPanel(CaptureExportWidget, CaptureExportViewportZOrder);
			}
		}
	}
}

bool AVirtualSensorUiHostActor::EnsurePlacementPanel(FString& OutReason)
{
	OutReason.Reset();
	if (bHostEnding || IsActorBeingDestroyed())
	{
		OutReason = TEXT("센서 UI Host가 종료 중입니다.");
		return false;
	}
	auto* Workspace = GetWorld() ? GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>() : nullptr;
	if (PlacementWidget && Workspace && Workspace->GetOwnedPanel(ESensorToolPanelRole::Placement) == PlacementWidget)
		return true;
	if (PlacementWidget)
	{
		if (!Workspace || !Workspace->RegisterPlacementHost(this) || !Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Placement, PlacementWidget))
		{
			OutReason = TEXT("배치 패널 등록이 충돌합니다. 기존 Host 연결을 확인하세요.");
			return false;
		}
		if (PanelHostComp)
			PanelHostComp->RegisterPanel(PlacementWidget, 25);
	}
	else
		CreateAndBindToolWidgets();
	if (PlacementWidget && Workspace && Workspace->GetOwnedPanel(ESensorToolPanelRole::Placement) == PlacementWidget)
	{
		GetWorldTimerManager().ClearTimer(PlacementRetryTimer);
		return true;
	}
	if (PlacementWidget)
	{
		OutReason = TEXT("배치 화면 연결 대기: 게임 viewport가 준비되면 다시 여세요.");
		LastStatusMessage = OutReason;
		return false;
	}
	if (!SettingsWidget && !SettingsWidgetClass && !bUseNativeToolWidgetFallback)
		LastStatusMessage = TEXT("배치 준비 실패: SettingsWidgetClass를 지정하거나 native fallback을 켜세요.");
	else if (SettingsWidget && !PlacementWidgetClass && !bUseNativeToolWidgetFallback)
		LastStatusMessage = TEXT("배치 준비 실패: PlacementWidgetClass를 지정하거나 native fallback을 켜세요.");
	OutReason = LastStatusMessage.IsEmpty() ? TEXT("배치 패널을 생성하지 못했습니다. Widget 클래스와 Host 연결을 확인하세요.") : LastStatusMessage;
	return false;
}

bool AVirtualSensorUiHostActor::ShowPlacementPanel(FString& OutReason)
{
	bPlacementOpenRequested = true;
	if (!EnsurePlacementPanel(OutReason))
	{
		PlacementRetryAttempts = 0;
		SchedulePlacementRetry();
		return false;
	}
	GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->SetPanelOpen(ESensorToolPanelRole::Placement, true);
	bPlacementOpenRequested = false;
	return true;
}

void AVirtualSensorUiHostActor::SchedulePlacementRetry()
{
	if (!bHostEnding && PlacementRetryAttempts < 20 && !GetWorldTimerManager().IsTimerActive(PlacementRetryTimer))
		GetWorldTimerManager().SetTimer(PlacementRetryTimer, this, &AVirtualSensorUiHostActor::RetryPlacementInitialization, .5f, false);
}

void AVirtualSensorUiHostActor::RetryPlacementInitialization()
{
	++PlacementRetryAttempts;
	FString Reason;
	if (EnsurePlacementPanel(Reason))
	{
		if (bPlacementOpenRequested)
			GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->SetPanelOpen(ESensorToolPanelRole::Placement, true);
		bPlacementOpenRequested = false;
	}
	else
		SchedulePlacementRetry();
}

void AVirtualSensorUiHostActor::ResetAllPanelUiPreferences()
{
	if (GetWorld())
		if (auto* Workspace = GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>())
		{
			Workspace->ResetOwnedWorkspaceLayout();
			return;
		}
	ResetSensorToolAppearance();
	if (MonitorWidget)
	{
		MonitorWidget->ResetMonitorUiPreferencesToDefault();
		MonitorWidget->ResetPanelUiStateToDefault();
	}
	if (SettingsWidget)
	{
		SettingsWidget->ResetSettingsUiPreferencesToDefault();
		SettingsWidget->ResetPanelUiStateToDefault();
	}
	if (CaptureExportWidget)
	{
		CaptureExportWidget->ResetPanelUiStateToDefault();
	}
	LastStatusMessage = TEXT("센서 UI 배치와 표시 설정을 기본값으로 초기화했습니다.");
}

void AVirtualSensorUiHostActor::ResetSensorToolAppearance()
{
	SetSensorToolFontScale(1.0f);
}
float AVirtualSensorUiHostActor::GetSensorToolFontScale() const
{
	if (GetWorld())
		if (auto* W = GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>())
			if (MonitorWidget && MonitorWidget->GetToolWorkspace() == W)
				return W->GetOwnedPanelFontScale();
	return SensorToolFontScale;
}

void AVirtualSensorUiHostActor::SetSensorToolFontScale(float Scale)
{
	if (GetWorld())
		if (auto* Workspace = GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>())
			if (Workspace->GetOwnedPanel(ESensorToolPanelRole::Monitor))
			{
				Workspace->SetOwnedPanelFontScale(Scale);
				SensorToolFontScale = Workspace->GetOwnedPanelFontScale();
				return;
			}
	if (!FMath::IsFinite(Scale))
		return;
	SensorToolFontScale = FMath::Clamp(Scale, 0.85f, 1.5f);
	for (UDxWidgetVirtualSensorPanelBase* Panel : {static_cast<UDxWidgetVirtualSensorPanelBase*>(MonitorWidget), static_cast<UDxWidgetVirtualSensorPanelBase*>(SettingsWidget), static_cast<UDxWidgetVirtualSensorPanelBase*>(CaptureExportWidget)})
		if (Panel)
			Panel->ApplySensorToolFontScale(SensorToolFontScale);
	USensorToolAppearance::SaveScale(SensorToolFontScale);
}

void AVirtualSensorUiHostActor::QueueSensorStateForMapApply(const FVirtualSensorEditableState& SensorState)
{
	if (!CanQueueMapApply())
	{
		LastStatusMessage = TEXT("이 맵에서는 맵 저장 예약을 사용할 수 없습니다 · 저장 위치 또는 전체 배치를 사용하세요.");
		return;
	}
	PendingMapApplySnapshot.SourceMapPackage = TEXT("/Game/MA0T10/Maps/SensorTestMap");
	for (FVirtualSensorEditableState& Existing : PendingMapApplySnapshot.SensorStates)
	{
		if (Existing.PersistentActorTag == SensorState.PersistentActorTag)
		{
			Existing = SensorState;
			LastStatusMessage = FString::Printf(TEXT("Updated queued map state for %s. Stop PIE to save."), *SensorState.SensorId);
			return;
		}
	}
	PendingMapApplySnapshot.SensorStates.Add(SensorState);
	LastStatusMessage = FString::Printf(TEXT("Queued map state for %s. Stop PIE to save."), *SensorState.SensorId);
}

bool AVirtualSensorUiHostActor::CanQueueMapApply() const
{
	if (!GetWorld())
		return false;
	FString Package = GetWorld()->GetOutermost()->GetName();
	if (!GetWorld()->StreamingLevelsPrefix.IsEmpty())
		Package = Package.Replace(*GetWorld()->StreamingLevelsPrefix, TEXT(""));
	return Package == TEXT("/Game/MA0T10/Maps/SensorTestMap");
}

void AVirtualSensorUiHostActor::ConfigurePlayerInput()
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController || !MonitorWidget)
	{
		LastStatusMessage = TEXT("Monitor widget created, but player input could not be configured.");
		UE_LOG(LogTemp, Warning, TEXT("[SensorMonitorHost] %s"), *LastStatusMessage);
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(MonitorWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = bShowMouseCursorOnCreate;
	PlayerController->bEnableClickEvents = true;
	PlayerController->bEnableMouseOverEvents = true;
}
