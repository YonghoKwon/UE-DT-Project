#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorHostActor.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorManager.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorWidget.h"

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
    }

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
