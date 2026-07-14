#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"
#include "VirtualSensorControlTypes.generated.h"

UENUM(BlueprintType)
enum class EVirtualSensorPanelPlacement : uint8
{
    RightCenter,
    LeftCenter,
    BottomCenter
};

UENUM(BlueprintType)
enum class EVirtualSensorTargetKind : uint8
{
    Camera,
    Lidar
};

UENUM(BlueprintType)
enum class EVirtualSensorGizmoMode : uint8
{
    Translate UMETA(DisplayName = "Move"),
    Rotate UMETA(DisplayName = "Rotate")
};

UENUM(BlueprintType)
enum class EVirtualSensorCoordinateSpace : uint8
{
    Local UMETA(DisplayName = "Local"),
    World UMETA(DisplayName = "World")
};

UENUM(BlueprintType)
enum class EVirtualSensorExportKind : uint8
{
    ServerPayload,
    PointCloudCsv,
    PointCloudJsonLines,
    PointCloudPcd,
    PointCloudLas,
    PointCloudLaz,
    TimedCapture
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorEditableState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    EVirtualSensorTargetKind TargetKind = EVirtualSensorTargetKind::Camera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    FName PersistentActorTag = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    FString OriginalSensorId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    FString SensorId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    FTransform ActorTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    EVirtualSensorSimulationQuality SimulationQuality = EVirtualSensorSimulationQuality::RealTimePreview;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Camera")
    EVirtualCameraDeviceProfile CameraProfile = EVirtualCameraDeviceProfile::IntelRealSenseD455;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Camera")
    FIntPoint CameraResolution = FIntPoint(640, 360);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Camera")
    float CameraCaptureInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Camera")
    float CameraFov = 87.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Camera")
    int32 CameraJpegQuality = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Camera")
    EVirtualCameraCaptureMode CameraCaptureMode = EVirtualCameraCaptureMode::PayloadAndOutput;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    EVirtualLidarDeviceProfile LidarProfile = EVirtualLidarDeviceProfile::LivoxMid360S;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    float LidarScanInterval = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    float LidarMaxDistance = 4000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    int32 LidarHorizontalSamples = 120;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    int32 LidarVerticalChannels = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    float LidarHorizontalFov = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    float LidarMinVerticalAngle = -7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    float LidarMaxVerticalAngle = 52.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    int32 ServerPayloadStride = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    int32 MaxServerPayloadPoints = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    bool bIncludeMissPointsInServerPayload = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    int32 PreviewPointStride = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    int32 MaxPreviewPoints = 3000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    bool bPreviewHitOnly = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    bool bUseMultiHit = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    int32 MaxHitsPerRay = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    bool bExportCsvOnScan = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    bool bExportJsonLinesOnScan = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl|Lidar")
    bool bExportPcdOnScan = false;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorMapApplySnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    FString SourceMapPackage = TEXT("/Game/MA0T10/Maps/SensorTestMap");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    TArray<FVirtualSensorEditableState> SensorStates;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorExportResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport")
    EVirtualSensorExportKind Kind = EVirtualSensorExportKind::ServerPayload;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport")
    FString SensorId;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport")
    bool bSucceeded = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport")
    FString AbsolutePath;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport")
    int64 FileSizeBytes = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport")
    FString Message;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport")
    FDateTime TimestampUtc;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualSensorExportCompleted, const FVirtualSensorExportResult&, Result);
