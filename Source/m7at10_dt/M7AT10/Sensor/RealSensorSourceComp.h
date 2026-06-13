#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RealSensorSourceComp.generated.h"

class UVirtualLidarSensorComp;

UENUM(BlueprintType)
enum class ERealSensorSourceKind : uint8
{
    FileReplay UMETA(DisplayName = "File Replay"),
    Ros2Bridge UMETA(DisplayName = "ROS2 Bridge"),
    LivoxLidar UMETA(DisplayName = "Livox LiDAR"),
    RealSenseCamera UMETA(DisplayName = "RealSense Camera"),
    Custom UMETA(DisplayName = "Custom")
};

UENUM(BlueprintType)
enum class ERealSensorSourceConnectionState : uint8
{
    Stopped UMETA(DisplayName = "Stopped"),
    Starting UMETA(DisplayName = "Starting"),
    Running UMETA(DisplayName = "Running"),
    Error UMETA(DisplayName = "Error")
};

UCLASS(Abstract, BlueprintType, ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API URealSensorSourceComp : public USceneComponent
{
    GENERATED_BODY()

public:
    URealSensorSourceComp();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorSource")
    virtual bool StartSource();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorSource")
    virtual void StopSource();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorSource")
    virtual bool PushFrameOnce(bool bSendTransport = true);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorSource")
    bool IsSourceRunning() const { return ConnectionState == ERealSensorSourceConnectionState::Running; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorSource")
    ERealSensorSourceConnectionState GetConnectionState() const { return ConnectionState; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorSource")
    FString GetLastSourceMessage() const { return LastSourceMessage; }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    ERealSensorSourceKind SourceKind = ERealSensorSourceKind::Custom;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    FString SourceId = TEXT("RealSensorSource");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    TObjectPtr<UVirtualLidarSensorComp> TargetLidar;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    bool bAutoStartSource = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    bool bSendTransportByDefault = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorSource|Status")
    ERealSensorSourceConnectionState ConnectionState = ERealSensorSourceConnectionState::Stopped;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorSource|Status")
    int64 LastSourceFrameId = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorSource|Status")
    int32 LastSourcePointCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorSource|Status")
    FString LastSourceMessage;

protected:
    UVirtualLidarSensorComp* ResolveTargetLidar() const;
    void SetSourceState(ERealSensorSourceConnectionState NewState, const FString& Message);
    void MarkFramePushed(int32 PointCount, const FString& Message);
};
