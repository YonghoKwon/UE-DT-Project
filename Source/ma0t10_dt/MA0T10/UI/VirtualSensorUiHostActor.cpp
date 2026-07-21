#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelHostComponent.h"

AVirtualSensorUiHostActor::AVirtualSensorUiHostActor()
{
    PrimaryActorTick.bCanEverTick = false;
    PanelHostComponent = CreateDefaultSubobject<UVirtualSensorPanelHostComponent>(TEXT("SensorPanelHostComponent"));
}

void AVirtualSensorUiHostActor::BeginPlay()
{
    Super::BeginPlay();
    bScreenDebugLogVisible = GEngine ? GEngine->bEnableOnScreenDebugMessages : true;
    if (bHideScreenDebugLogOnBeginPlay) SetScreenDebugLogVisible(false);
    if (PanelHostComponent)
    {
        PanelHostComponent->bAllowViewportFallback = bAllowViewportFallback;
    }
    if (bAutoCreateOnBeginPlay)
    {
        CreateAndBindMonitorWidget();
        if (bAutoCreateToolPanels)
        {
            CreateAndBindToolWidgets();
        }
    }
}

void AVirtualSensorUiHostActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
    if (!GEngine) return;
    if (!bHasScreenDebugLogOverride)
    {
        bSavedScreenDebugLogState = GEngine->bEnableOnScreenDebugMessages;
        bHasScreenDebugLogOverride = true;
    }
    GEngine->bEnableOnScreenDebugMessages = bVisible;
    bScreenDebugLogVisible = bVisible;
}

UVirtualSensorMonitorPanelWidget* AVirtualSensorUiHostActor::CreateAndBindMonitorWidget()
{
    if (MonitorWidget)
    {
        return MonitorWidget;
    }

    const TSubclassOf<UVirtualSensorMonitorPanelWidget> WidgetClassToCreate = GetEffectiveMonitorWidgetClass();
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

    MonitorWidget = CreateWidget<UVirtualSensorMonitorPanelWidget>(World, WidgetClassToCreate);
    if (!MonitorWidget)
    {
        LastStatusMessage = TEXT("Failed to create monitor widget.");
        UE_LOG(LogTemp, Warning, TEXT("[SensorMonitorHost] %s"), *LastStatusMessage);
        return nullptr;
    }

    AVirtualSensorCoordinator* ResolvedManager = ResolveSensorManager();
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

    MonitorWidget->SetPanelPersistenceKey(TEXT("Monitor"));
    MonitorWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::RightCenter, FVector2D(820.0f, 430.0f));
    MonitorWidget->SetPanelResizable(true);
    MonitorWidget->ResizeHandleSize = 32.0f;
    MonitorWidget->SetPanelResizeLimits(FVector2D(480.0f, 300.0f), FVector2D::ZeroVector);
    if (PanelHostComponent)
    {
        PanelHostComponent->RegisterPanel(MonitorWidget, ViewportZOrder);
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
    if (PanelHostComponent)
    {
        PanelHostComponent->UnregisterAllPanels();
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

TSubclassOf<UVirtualSensorMonitorPanelWidget> AVirtualSensorUiHostActor::GetEffectiveMonitorWidgetClass() const
{
    if (MonitorWidgetClass)
    {
        return MonitorWidgetClass;
    }
    return bUseNativeMonitorWidgetFallback ? UVirtualSensorMonitorPanelWidget::StaticClass() : nullptr;
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
    if (!World || !ResolvedManager)
    {
        LastStatusMessage = TEXT("Tool widgets require a valid world and SensorManager.");
        return;
    }

    TSubclassOf<UVirtualSensorSettingsPanelWidget> EffectiveSettingsClass = SettingsWidgetClass;
    if (!EffectiveSettingsClass && bUseNativeToolWidgetFallback)
    {
        EffectiveSettingsClass = UVirtualSensorSettingsPanelWidget::StaticClass();
    }
    TSubclassOf<UVirtualSensorCaptureExportPanelWidget> EffectiveCaptureClass = CaptureExportWidgetClass;
    if (!EffectiveCaptureClass && bUseNativeToolWidgetFallback)
    {
        EffectiveCaptureClass = UVirtualSensorCaptureExportPanelWidget::StaticClass();
    }

    if (!SettingsWidget && EffectiveSettingsClass)
    {
        SettingsWidget = CreateWidget<UVirtualSensorSettingsPanelWidget>(World, EffectiveSettingsClass);
        if (SettingsWidget)
        {
            SettingsWidget->SetPanelPersistenceKey(TEXT("Settings"));
            SettingsWidget->BindHostActor(this);
            SettingsWidget->BindSensorManager(ResolvedManager);
            SettingsWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::LeftCenter, FVector2D(450.0f, 640.0f));
            if (PanelHostComponent)
            {
                PanelHostComponent->RegisterPanel(SettingsWidget, SettingsViewportZOrder);
            }
        }
    }

    if (!CaptureExportWidget && EffectiveCaptureClass)
    {
        CaptureExportWidget = CreateWidget<UVirtualSensorCaptureExportPanelWidget>(World, EffectiveCaptureClass);
        if (CaptureExportWidget)
        {
            CaptureExportWidget->SetPanelPersistenceKey(TEXT("CaptureExport"));
            CaptureExportWidget->BindSensorManager(ResolvedManager);
            CaptureExportWidget->BindMonitorWidget(MonitorWidget);
            CaptureExportWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::BottomCenter, FVector2D(900.0f, 620.0f));
            CaptureExportWidget->SetPanelResizable(true);
            CaptureExportWidget->ResizeHandleSize = 32.0f;
            CaptureExportWidget->SetPanelResizeLimits(FVector2D(620.0f, 360.0f), FVector2D::ZeroVector);
            if (PanelHostComponent)
            {
                PanelHostComponent->RegisterPanel(CaptureExportWidget, CaptureExportViewportZOrder);
            }
        }
    }
}

void AVirtualSensorUiHostActor::ResetAllPanelUiPreferences()
{
    UVirtualSensorUiPreferencesSaveGame::DeleteSavedPreferences();
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

void AVirtualSensorUiHostActor::QueueSensorStateForMapApply(const FVirtualSensorEditableState& SensorState)
{
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
