#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorMonitorPanel.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "DxWidgetArchitectureMonitorProbe.generated.h"

UCLASS()
class UDxWidgetArchitectureMonitorProbe : public UDxWidgetVirtualSensorMonitorPanel
{
    GENERATED_BODY()
public:
    mutable TArray<FString> Calls;
    UVirtualCameraCaptureComp* LastCamera = nullptr;
    UVirtualLidarScanComp* LastLidar = nullptr;
    URealSensorSourceComp* LastSource = nullptr;
    APlayerController* ViewController = nullptr;
    virtual void BindMonitorCamera(UVirtualCameraCaptureComp* Camera) override { Calls.Add(TEXT("Camera")); LastCamera=Camera; }
    virtual void BindMonitorLidar(UVirtualLidarScanComp* Lidar) override { Calls.Add(TEXT("Lidar")); LastLidar=Lidar; }
    virtual void BindMonitorExternalSource(URealSensorSourceComp* Source) override { Calls.Add(TEXT("External")); LastSource=Source; }
    virtual void ShowMonitorCamera() override { Calls.Add(TEXT("ShowCamera")); }
    virtual void ShowMonitorLidar() override { Calls.Add(TEXT("ShowLidar")); }
    virtual APlayerController* GetMonitorViewController() const override { Calls.Add(TEXT("Owner")); return ViewController; }
    UFUNCTION() void ModeChanged(EVirtualSensorViewMode Mode) { Calls.Add(TEXT("Event")); }
};
