#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ArchitectureMonitorProbe.generated.h"

UCLASS()
class UArchitectureMonitorProbe : public UVirtualSensorMonitorPanelWidget
{
    GENERATED_BODY()
public:
    mutable TArray<FString> Calls;
    UVirtualCameraCaptureComponent* LastCamera = nullptr;
    UVirtualLidarScanComponent* LastLidar = nullptr;
    URealSensorSourceComponent* LastSource = nullptr;
    APlayerController* ViewController = nullptr;
    virtual void BindMonitorCamera(UVirtualCameraCaptureComponent* Camera) override { Calls.Add(TEXT("Camera")); LastCamera=Camera; }
    virtual void BindMonitorLidar(UVirtualLidarScanComponent* Lidar) override { Calls.Add(TEXT("Lidar")); LastLidar=Lidar; }
    virtual void BindMonitorExternalSource(URealSensorSourceComponent* Source) override { Calls.Add(TEXT("External")); LastSource=Source; }
    virtual void ShowMonitorCamera() override { Calls.Add(TEXT("ShowCamera")); }
    virtual void ShowMonitorLidar() override { Calls.Add(TEXT("ShowLidar")); }
    virtual APlayerController* GetMonitorViewController() const override { Calls.Add(TEXT("Owner")); return ViewController; }
    UFUNCTION() void ModeChanged(EVirtualSensorViewMode Mode) { Calls.Add(TEXT("Event")); }
};
