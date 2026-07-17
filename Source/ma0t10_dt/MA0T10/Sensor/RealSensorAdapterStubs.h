#pragma once

#include "CoreMinimal.h"
#include "RealSensorSourceComponent.h"
#include "RealSensorAdapterStubs.generated.h"

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API URos2SensorBridgeSourceComponent : public URealSensorSourceComponent
{
    GENERATED_BODY()

public:
    URos2SensorBridgeSourceComponent();

    virtual bool StartSource() override;
    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ROS2")
    FString PointCloudTopic = TEXT("/points");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ROS2")
    FString ImageTopic = TEXT("/camera/image");
};

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API ULivoxLidarSourceComponent : public URealSensorSourceComponent
{
    GENERATED_BODY()

public:
    ULivoxLidarSourceComponent();

    virtual bool StartSource() override;
    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|Livox")
    FString DeviceIp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|Livox")
    FString HostIp;
};

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API URealSenseCameraSourceComponent : public URealSensorSourceComponent
{
    GENERATED_BODY()

public:
    URealSenseCameraSourceComponent();

    virtual bool StartSource() override;
    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSense")
    FString SerialNumber;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSense")
    bool bEnableDepth = true;
};