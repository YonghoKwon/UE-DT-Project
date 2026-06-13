#pragma once

#include "CoreMinimal.h"
#include "RealSensorSourceComp.h"
#include "RealSensorAdapterStubs.generated.h"

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API URos2SensorBridgeSourceComp : public URealSensorSourceComp
{
    GENERATED_BODY()

public:
    URos2SensorBridgeSourceComp();

    virtual bool StartSource() override;
    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ROS2")
    FString PointCloudTopic = TEXT("/points");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ROS2")
    FString ImageTopic = TEXT("/camera/image");
};

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API ULivoxLidarSourceComp : public URealSensorSourceComp
{
    GENERATED_BODY()

public:
    ULivoxLidarSourceComp();

    virtual bool StartSource() override;
    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|Livox")
    FString DeviceIp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|Livox")
    FString HostIp;
};

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API URealSenseCameraSourceComp : public URealSensorSourceComp
{
    GENERATED_BODY()

public:
    URealSenseCameraSourceComp();

    virtual bool StartSource() override;
    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSense")
    FString SerialNumber;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSense")
    bool bEnableDepth = true;
};
