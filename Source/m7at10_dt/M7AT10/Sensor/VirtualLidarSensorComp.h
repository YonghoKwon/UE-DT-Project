#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VirtualLidarSensorTypes.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDeviceProfileTypes.h"
#include "VirtualLidarSensorComp.generated.h"

class UTexture2D;
class UVirtualSensorDataTransportComp;

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

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar")
    void ApplyPreset(EVirtualLidarPreset NewPreset);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|DeviceProfile")
    void ApplyDeviceProfile(EVirtualLidarDeviceProfile NewProfile);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Transport")
    void SetTransportComponent(UVirtualSensorDataTransportComp* InTransportComponent);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
    bool ExportLastPointCloudCsv(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
    bool ExportLastPointCloudJsonLines(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
    bool ExportLastPointCloudPcd(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar")
    const TArray<FVirtualLidarPoint>& GetLastPoints() const { return LastPoints; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar")
    UTexture2D* GetLidarViewTexture() const { return LidarViewTexture; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar")
    const FVirtualSensorRuntimeStatus& GetRuntimeStatus() const { return RuntimeStatus; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|DeviceProfile")
    const FVirtualSensorDeviceSpec& GetDeviceSpec() const { return DeviceSpec; }

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|VirtualLidar")
    FOnVirtualLidarScanCompleted OnScanCompleted;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    FString SensorId = TEXT("LIDAR-001");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|DeviceProfile")
    EVirtualLidarDeviceProfile DeviceProfile = EVirtualLidarDeviceProfile::LivoxMid360S;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|DeviceProfile")
    FVirtualSensorDeviceSpec DeviceSpec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    EVirtualLidarPreset Preset = EVirtualLidarPreset::Custom;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    EVirtualLidarViewMode ViewMode = EVirtualLidarViewMode::IntensityGray;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "0.033"))
    float ScanInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1.0"))
    float MaxDistance = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1", ClampMax = "1440"))
    int32 HorizontalSamples = 180;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1", ClampMax = "256"))
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
    bool bApplyDeviceProfileOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    bool bAutoRegisterToManager = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    bool bDrawDebugRays = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|View")
    bool bFlipLidarViewHorizontal = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|View")
    bool bFlipLidarViewVertical = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|MultiHit")
    bool bUseMultiHit = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|MultiHit", meta = (ClampMin = "1", ClampMax = "16"))
    int32 MaxHitsPerRay = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Export")
    bool bExportCsvOnScan = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Export")
    bool bExportJsonLinesOnScan = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Export")
    bool bExportPcdOnScan = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Filter")
    TArray<FName> IgnoreActorTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Transport")
    TObjectPtr<UVirtualSensorDataTransportComp> TransportComponent;

private:
    void ExecuteScan(TArray<FVirtualLidarPoint>& OutPoints, TArray<uint8>& OutHeatmapPixels);
    FString BuildJsonPayload(const TArray<FVirtualLidarPoint>& Points) const;
    void DispatchPayload(const FString& JsonPayload) const;
    void PostJson(const FString& JsonPayload) const;
    void SaveJsonToDisk(const FString& JsonPayload) const;
    void UpdateLidarViewTexture(const TArray<uint8>& HeatmapPixels);
    void WriteHeatmapPixel(TArray<uint8>& Pixels, int32 PixelIndex, const FVirtualLidarPoint& Point) const;
    bool ShouldIgnoreHitActor(const AActor* Actor) const;
    void TryAutoRegisterToManager();
    int32 GetHeatmapPixelIndex(int32 H, int32 V, int32 Width, int32 Height) const;
    FString BuildExportPath(const FString& Extension, const FString& FileNamePrefix) const;

private:
    FTimerHandle ScanTimerHandle;
    TArray<FVirtualLidarPoint> LastPoints;
    int64 FrameId = 0;

    UPROPERTY(Transient)
    FVirtualSensorRuntimeStatus RuntimeStatus;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> LidarViewTexture;
};
