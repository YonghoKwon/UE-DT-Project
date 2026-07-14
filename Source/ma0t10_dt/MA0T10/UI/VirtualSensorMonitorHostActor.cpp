#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorHostActor.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorManager.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"

AVirtualSensorMonitorHostActor::AVirtualSensorMonitorHostActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AVirtualSensorMonitorHostActor::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoCreateOnBeginPlay)
    {
        CreateAndBindMonitorWidget();
        if (bAutoCreateToolPanels)
        {
            CreateAndBindToolWidgets();
        }
    }
}

void AVirtualSensorMonitorHostActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyMonitorWidget();
    Super::EndPlay(EndPlayReason);
}

UVirtualSensorMonitorWidget* AVirtualSensorMonitorHostActor::CreateAndBindMonitorWidget()
{
    if (MonitorWidget)
    {
        return MonitorWidget;
    }

    const TSubclassOf<UVirtualSensorMonitorWidget> WidgetClassToCreate = GetEffectiveMonitorWidgetClass();
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

    MonitorWidget = CreateWidget<UVirtualSensorMonitorWidget>(World, WidgetClassToCreate);
    if (!MonitorWidget)
    {
        LastStatusMessage = TEXT("Failed to create monitor widget.");
        UE_LOG(LogTemp, Warning, TEXT("[SensorMonitorHost] %s"), *LastStatusMessage);
        return nullptr;
    }

    AVirtualSensorManager* ResolvedManager = ResolveSensorManager();
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

    if (bAddToViewport)
    {
        MonitorWidget->AddToViewport(ViewportZOrder);
        MonitorWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::RightCenter, FVector2D(820.0f, 430.0f));
    }
    MonitorWidget->SetPanelPersistenceKey(TEXT("Monitor"));

    if (bConfigurePlayerInputOnCreate)
    {
        ConfigurePlayerInput();
    }

    LastStatusMessage = FString::Printf(TEXT("Monitor widget created. Manager=%s Viewport=%s"),
        ResolvedManager ? *ResolvedManager->GetName() : TEXT("None"),
        bAddToViewport ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Log, TEXT("[SensorMonitorHost] %s"), *LastStatusMessage);
    return MonitorWidget;
}

void AVirtualSensorMonitorHostActor::DestroyMonitorWidget()
{
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

TSubclassOf<UVirtualSensorMonitorWidget> AVirtualSensorMonitorHostActor::GetEffectiveMonitorWidgetClass() const
{
    if (MonitorWidgetClass)
    {
        return MonitorWidgetClass;
    }
    return bUseNativeMonitorWidgetFallback ? UVirtualSensorMonitorWidget::StaticClass() : nullptr;
}

AVirtualSensorManager* AVirtualSensorMonitorHostActor::ResolveSensorManager()
{
    if (SensorManager)
    {
        return SensorManager;
    }
    if (!bAutoFindSensorManager || !GetWorld())
    {
        return nullptr;
    }

    for (TActorIterator<AVirtualSensorManager> It(GetWorld()); It; ++It)
    {
        if (*It)
        {
            SensorManager = *It;
            return SensorManager;
        }
    }
    return nullptr;
}

void AVirtualSensorMonitorHostActor::CreateAndBindToolWidgets()
{
    UWorld* World = GetWorld();
    AVirtualSensorManager* ResolvedManager = ResolveSensorManager();
    if (!World || !ResolvedManager)
    {
        LastStatusMessage = TEXT("Tool widgets require a valid world and SensorManager.");
        return;
    }

    TSubclassOf<UVirtualSensorSettingsWidget> EffectiveSettingsClass = SettingsWidgetClass;
    if (!EffectiveSettingsClass && bUseNativeToolWidgetFallback)
    {
        EffectiveSettingsClass = UVirtualSensorSettingsWidget::StaticClass();
    }
    TSubclassOf<UVirtualSensorCaptureExportWidget> EffectiveCaptureClass = CaptureExportWidgetClass;
    if (!EffectiveCaptureClass && bUseNativeToolWidgetFallback)
    {
        EffectiveCaptureClass = UVirtualSensorCaptureExportWidget::StaticClass();
    }

    if (!SettingsWidget && EffectiveSettingsClass)
    {
        SettingsWidget = CreateWidget<UVirtualSensorSettingsWidget>(World, EffectiveSettingsClass);
        if (SettingsWidget)
        {
            SettingsWidget->SetPanelPersistenceKey(TEXT("Settings"));
            SettingsWidget->BindHostActor(this);
            SettingsWidget->BindSensorManager(ResolvedManager);
            if (bAddToViewport)
            {
                SettingsWidget->AddToViewport(SettingsViewportZOrder);
                SettingsWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::LeftCenter, FVector2D(450.0f, 640.0f));
            }
        }
    }

    if (!CaptureExportWidget && EffectiveCaptureClass)
    {
        CaptureExportWidget = CreateWidget<UVirtualSensorCaptureExportWidget>(World, EffectiveCaptureClass);
        if (CaptureExportWidget)
        {
            CaptureExportWidget->SetPanelPersistenceKey(TEXT("CaptureExport"));
            CaptureExportWidget->BindSensorManager(ResolvedManager);
            CaptureExportWidget->BindMonitorWidget(MonitorWidget);
            if (bAddToViewport)
            {
                CaptureExportWidget->AddToViewport(CaptureExportViewportZOrder);
                CaptureExportWidget->ConfigurePanelLayout(EVirtualSensorPanelPlacement::BottomCenter, FVector2D(760.0f, 280.0f));
            }
        }
    }
}

void AVirtualSensorMonitorHostActor::ResetAllPanelUiPreferences()
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

void AVirtualSensorMonitorHostActor::QueueSensorStateForMapApply(const FVirtualSensorEditableState& SensorState)
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

void AVirtualSensorMonitorHostActor::ConfigurePlayerInput()
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
