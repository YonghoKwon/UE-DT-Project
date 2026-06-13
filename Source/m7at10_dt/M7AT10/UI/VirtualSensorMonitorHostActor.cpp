#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorHostActor.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"
#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.h"

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
