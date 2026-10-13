#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VirtualSensorMonitorView.generated.h"

class APlayerController;
class UVirtualCameraCaptureComponent;
class UVirtualLidarScanComponent;
class URealSensorSourceComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UVirtualSensorMonitorView : public UInterface
{
    GENERATED_BODY()
};

/** Native presentation boundary; no widget types or measurement side effects. */
class MA0T10_DT_API IVirtualSensorMonitorView
{
    GENERATED_BODY()
public:
    virtual void BindMonitorCamera(UVirtualCameraCaptureComponent* Camera) = 0;
    virtual void BindMonitorLidar(UVirtualLidarScanComponent* Lidar) = 0;
    virtual void BindMonitorExternalSource(URealSensorSourceComponent* Source) = 0;
    virtual void ShowMonitorCamera() = 0;
    virtual void ShowMonitorLidar() = 0;
    virtual APlayerController* GetMonitorViewController() const = 0;
};
