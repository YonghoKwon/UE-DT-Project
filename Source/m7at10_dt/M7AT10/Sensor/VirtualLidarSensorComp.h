#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VirtualLidarSensorTypes.h"
#include "VirtualLidarSensorComp.generated.h"

class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVirtualLidarScanCompleted, const FString&, JsonPayload, UTexture2D*, LidarViewTexture);

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API UVirtualLidarSensorComp : public USceneComponent
{
    GENERATED_BODY()

public:
    UVirtualLidarSensorComp();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar")
    void StartScan();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar")
    void StopScan();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar")
    void ScanAndSend();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar")
    const TArray<FVirtualLidarPoint>& GetLastPoints() const { return LastPoints; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar")
    UTexture2D* GetLidarViewTexture() const { return LidarViewTexture; }

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|VirtualLidar")
    FOnVirtualLidarScanCompleted OnScanCompleted;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    FString SensorId = TEXT("LIDAR-001");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "0.033"))
    float ScanInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1.0"))
    float MaxDistance = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1", ClampMax = "720"))
    int32 HorizontalSamples = 180;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1", ClampMax = "128"))
    int32 VerticalChannels = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1.0", ClampMax = "360.0"))
    float HorizontalFov = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
    float MinVerticalAngle = -15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
    float MaxVerticalAngle = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    EVirtualLidarOutputMode OutputMode = EVirtualLidarOutputMode::LogOnly;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (EditCondition = "OutputMode == EVirtualLidarOutputMode::HttpPost"))
    FString HttpEndpoint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    bool bAutoStartScan = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    bool bDrawDebugRays = false;

private:
    void ExecuteScan(TArray<FVirtualLidarPoint>& OutPoints, TArray<uint8>& OutHeatmapPixels);
    FString BuildJsonPayload(const TArray<FVirtualLidarPoint>& Points) const;
    void DispatchPayload(const FString& JsonPayload) const;
    void PostJson(const FString& JsonPayload) const;
    void SaveJsonToDisk(const FString& JsonPayload) const;
    void UpdateLidarViewTexture(const TArray<uint8>& HeatmapPixels);

private:
    FTimerHandle ScanTimerHandle;
    TArray<FVirtualLidarPoint> LastPoints;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> LidarViewTexture;
};
