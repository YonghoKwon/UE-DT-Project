#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VirtualLidarSensorTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorDeviceProfileTypes.h"
#include "VirtualLidarSensorComp.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UTexture2D;
class UVirtualSensorDataTransportComp;
class UVirtualSensorRecorderComp;
class UVirtualSensorPerformanceSubsystem;

UENUM(BlueprintType)
enum class ELidarPointCloudPreviewBackend : uint8
{
    CpuInstancedMesh UMETA(DisplayName = "CPU Instanced Mesh"),
    NiagaraCandidate UMETA(DisplayName = "Niagara Candidate (CPU Fallback)"),
    CustomGpuCandidate UMETA(DisplayName = "Custom GPU Candidate (CPU Fallback)")
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualLidarSemanticClassRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    FName Label = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    FLinearColor DisplayColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    TArray<FName> ActorTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    TArray<FName> ActorClassNames;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    TArray<FString> ActorNameContains;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    TArray<FString> ComponentNameContains;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVirtualLidarScanCompleted, const FString&, JsonPayload, UTexture2D*, LidarViewTexture);

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualLidarSensorComp : public USceneComponent
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

    // Automatic scan entry points used by UVirtualSensorPerformanceSubsystem.
    // ScanAndSend remains the synchronous one-shot compatibility API.
    void PrepareScheduledScan(double NowSeconds);
    int32 ProcessScheduledScanChunk(int32 MaxRays);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Replay")
    void InjectPointCloudFrame(const TArray<FVirtualLidarPoint>& Points, bool bSendTransport = true);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar")
    void ApplyPreset(EVirtualLidarPreset NewPreset);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|DeviceProfile")
    void ApplyDeviceProfile(EVirtualLidarDeviceProfile NewProfile);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Performance")
    void ApplySimulationQuality(EVirtualSensorSimulationQuality NewQuality);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Transport")
    void SetServerPayloadPolicy(int32 InStride, int32 InMaxPoints, bool bInIncludeMissPoints);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    void SetPreviewPolicy(int32 InStride, int32 InMaxPoints, bool bInHitOnly);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    void SetPreviewBackend(ELidarPointCloudPreviewBackend InBackend, bool bAllowExperimentalGpuBackend = false);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Transport")
    void SetTransportComponent(UVirtualSensorDataTransportComp* InTransportComponent);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Recorder")
    void SetRecorderComponent(UVirtualSensorRecorderComp* InRecorderComponent);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    void SetPointCloudPreviewEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    void ClearPointCloudPreview();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    bool IsPointCloudPreviewEnabled() const { return bPointCloudPreviewEnabled; }

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    void ApplyPointCloudPreviewStyle();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Semantic")
    void ResetDefaultSemanticClassRules();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Semantic")
    FLinearColor GetSemanticColorForLabel(FName SemanticLabel) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Debug")
    void LogLastPointCloud(int32 MaxPointsToLog = 100, bool bHitOnly = true) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
    bool ExportLastPointCloudCsv(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
    bool ExportLastPointCloudJsonLines(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
    bool ExportLastPointCloudPcd(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
    bool ExportLastPointCloudLas(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
    bool ExportLastPointCloudLaz(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
    bool ExportLastPointCloudCsvLasLaz(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export")
    FString GetLastPointCloudExportPath() const { return LastPointCloudExportPath; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    FString GetLastLazExportStatusText() const { return LastLazExportStatusText; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    FString GetLastLazLasSourcePath() const { return LastLazLasSourcePath; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    FString GetLastLazOutputPath() const { return LastLazOutputPath; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    bool WasLastLazExportAttempted() const { return bLastLazExportAttempted; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    bool DidLastLazExportSucceed() const { return bLastLazExportSucceeded; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    bool WasLastLazExportPlaceholderOnly() const { return bLastLazExportPlaceholderOnly; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    bool WasLastLazExternalCompressorRequested() const { return bLastLazExternalCompressorRequested; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    bool WasLastLazExternalCompressorAttempted() const { return bLastLazExternalCompressorAttempted; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    bool DidLastLazExternalCompressorSucceed() const { return bLastLazExternalCompressorSucceeded; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    bool DidLastLazProduceOutputFile() const { return bLastLazProducedOutputFile; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    bool WasLastLazTrueCompressionValidated() const { return bLastLazTrueCompressionValidated; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    int32 GetLastLazExportedPointCount() const { return LastLazExportedPointCount; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    int32 GetLastLazExternalCompressorReturnCode() const { return LastLazExternalCompressorReturnCode; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    int64 GetLastLazOutputSizeBytes() const { return LastLazOutputSizeBytes; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    FString GetLastLazExportWarningText() const { return LastLazExportWarningText; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar")
    const TArray<FVirtualLidarPoint>& GetLastPoints() const { return LastPoints; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar")
    UTexture2D* GetLidarViewTexture() const { return LidarViewTexture; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar")
    const FVirtualSensorRuntimeStatus& GetRuntimeStatus() const { return RuntimeStatus; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|SlabAnalysis")
    const FVirtualLidarSlabAnalysisResult& GetLastSlabAnalysis() const { return LastSlabAnalysis; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Transport")
    int32 GetLastServerPayloadPointCount() const { return LastServerPayloadPointCount; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Transport")
    const FString& GetLastJsonPayload() const { return LastJsonPayload; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    int32 GetLastPreviewPointCount() const { return LastPreviewPointCount; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    FString GetPreviewBackendName() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    bool IsGpuPreviewBackendRequested() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    bool IsGpuPreviewBackendActive() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Performance")
    FString GetPerformanceWarning() const { return LastPerformanceWarning; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|DeviceProfile")
    const FVirtualSensorDeviceSpec& GetDeviceSpec() const { return DeviceSpec; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Performance")
    EVirtualSensorSimulationQuality GetSimulationQuality() const { return SimulationQuality; }

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|VirtualLidar")
    FOnVirtualLidarScanCompleted OnScanCompleted;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    FString SensorId = TEXT("LIDAR-001");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|DeviceProfile")
    EVirtualLidarDeviceProfile DeviceProfile = EVirtualLidarDeviceProfile::LivoxMid360S;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|DeviceProfile")
    FVirtualSensorDeviceSpec DeviceSpec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Performance")
    EVirtualSensorSimulationQuality SimulationQuality = EVirtualSensorSimulationQuality::RealTimePreview;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Transport", meta = (ClampMin = "1", ClampMax = "100"))
    int32 ServerPayloadStride = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Transport", meta = (ClampMin = "0", ClampMax = "1000000"))
    int32 MaxServerPayloadPoints = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Transport")
    bool bIncludeMissPointsInServerPayload = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Performance", meta = (ClampMin = "1", ClampMax = "100"))
    int32 PayloadPointStride = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Performance", meta = (ClampMin = "0", ClampMax = "1000000"))
    int32 MaxPayloadPoints = 3000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Performance")
    bool bIncludeMissPointsInPayload = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    EVirtualLidarPreset Preset = EVirtualLidarPreset::Custom;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar")
    EVirtualLidarViewMode ViewMode = EVirtualLidarViewMode::DepthGradient;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "0.033"))
    float ScanInterval = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1.0"))
    float MaxDistance = 4000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1", ClampMax = "1440"))
    int32 HorizontalSamples = 120;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1", ClampMax = "256"))
    int32 VerticalChannels = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "1.0", ClampMax = "360.0"))
    float HorizontalFov = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
    float MinVerticalAngle = -7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
    float MaxVerticalAngle = 52.0f;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    bool bEnableSemanticClassification = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    bool bUseSemanticColorInPointCloudPreview = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    TArray<FVirtualLidarSemanticClassRule> SemanticClassRules;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    FName DefaultSemanticLabel = TEXT("Other");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Semantic")
    FLinearColor DefaultSemanticColor = FLinearColor(0.55f, 0.55f, 0.55f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    bool bPointCloudPreviewEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    bool bPointCloudPreviewHitOnly = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview", meta = (ClampMin = "1", ClampMax = "100"))
    int32 PointCloudPreviewStride = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview", meta = (ClampMin = "0", ClampMax = "1000000"))
    int32 MaxPointCloudPreviewInstances = 5000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview", meta = (ClampMin = "1", ClampMax = "100"))
    int32 PreviewPointStride = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview", meta = (ClampMin = "0", ClampMax = "1000000"))
    int32 MaxPreviewPoints = 5000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    ELidarPointCloudPreviewBackend PreviewBackend = ELidarPointCloudPreviewBackend::CpuInstancedMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    bool bAllowExperimentalGpuPreviewBackend = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float PointCloudPreviewPointScale = 0.035f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    FLinearColor PointCloudPreviewColor = FLinearColor(1.0f, 0.05f, 0.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    bool bDrawPointCloudPreviewDebugPoints = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview", meta = (ClampMin = "1.0", ClampMax = "50.0"))
    float PointCloudPreviewDebugPointSize = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    TObjectPtr<UStaticMesh> PointCloudPreviewMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|PointCloudPreview")
    TObjectPtr<UMaterialInterface> PointCloudPreviewMaterial;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Export")
    bool bExportHitOnlyPointCloud = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Export|LAZ")
    bool bUseExternalLazCompressor = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Export|LAZ", meta = (EditCondition = "bUseExternalLazCompressor", FilePathFilter = "exe"))
    FString ExternalLazCompressorPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Export|LAZ", meta = (EditCondition = "bUseExternalLazCompressor"))
    FString ExternalLazCompressorArguments = TEXT("-i {input} -o {output}");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Filter")
    TArray<FName> IgnoreActorTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|SlabAnalysis")
    FName SlabSemanticLabel = TEXT("Slab");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|SlabAnalysis", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
    float ReferenceSlabYawDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|SlabAnalysis", meta = (ClampMin = "3", ClampMax = "1000000"))
    int32 MinSlabPointsForAnalysis = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|SlabAnalysis")
    bool bIncludeSlabAnalysisInPayload = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Transport")
    TObjectPtr<UVirtualSensorDataTransportComp> TransportComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Recorder")
    TObjectPtr<UVirtualSensorRecorderComp> RecorderComponent;

private:
    void ExecuteScan(TArray<FVirtualLidarPoint>& OutPoints, TArray<uint8>& OutHeatmapPixels);
    FString BuildJsonPayload(const TArray<FVirtualLidarPoint>& Points) const;
    FVirtualLidarSlabAnalysisResult AnalyzeSlabPoints(const TArray<FVirtualLidarPoint>& Points) const;
    FString BuildPerformanceWarning() const;
    int32 CountServerPayloadPoints(const TArray<FVirtualLidarPoint>& Points) const;
    int32 CountPreviewPoints(const TArray<FVirtualLidarPoint>& Points) const;
    void DispatchPayload(const FString& JsonPayload) const;
    void PostJson(const FString& JsonPayload) const;
    void SaveJsonToDisk(const FString& JsonPayload) const;
    void UpdateLidarViewTexture(const TArray<uint8>& HeatmapPixels);
    void WriteHeatmapPixel(TArray<uint8>& Pixels, int32 PixelIndex, const FVirtualLidarPoint& Point) const;
    void UpdateRuntimeStatusAfterScan(int32 PayloadLength);
    void ExportEnabledPointCloudFormats() const;
    bool ShouldIgnoreHitActor(const AActor* Actor) const;
    void PopulatePointSemanticMetadata(FVirtualLidarPoint& Point, const FHitResult& Hit) const;
    FName ResolveSemanticLabel(const FHitResult& Hit) const;
    bool SemanticRuleMatches(const FVirtualLidarSemanticClassRule& Rule, const AActor* Actor, const UPrimitiveComponent* Component) const;
    FLinearColor ResolveSemanticColor(const FVirtualLidarPoint& Point) const;
    void TryAutoRegisterToManager();
    void RegisterWithPerformanceSubsystem();
    void UnregisterFromPerformanceSubsystem();
    void BeginScheduledScan(double NowSeconds);
    void CompleteScheduledScan(double NowSeconds);
    void QueueScheduledPayloadBuild(int64 CapturedFrameId, double AcquisitionStartedSeconds);
    void CompleteScheduledPayloadBuild(int64 CapturedFrameId, FString&& JsonPayload, double AcquisitionStartedSeconds);
    void QueueScheduledAutoExports();
    int32 GetHeatmapPixelIndex(int32 H, int32 V, int32 Width, int32 Height) const;
    FString BuildExportPath(const FString& Extension, const FString& FileNamePrefix) const;
    bool ExportLastPointCloudLasToPath(const FString& Path) const;
    void ResetLastLazExportStatus(const FString& StatusText = TEXT("")) const;
    bool RunExternalLazCompressor(const FString& LasSourcePath, const FString& LazOutputPath) const;
    UInstancedStaticMeshComponent* EnsurePointCloudPreviewComponent();
    void RefreshPointCloudPreview();
    void CollectExportPoints(TArray<const FVirtualLidarPoint*>& OutExportPoints) const;

private:
    FTimerHandle ScanTimerHandle;
    TArray<FVirtualLidarPoint> LastPoints;
    int64 FrameId = 0;
    int32 LastServerPayloadPointCount = 0;
    int32 LastPreviewPointCount = 0;
    FString LastPerformanceWarning;
    FString LastJsonPayload;
    FVirtualLidarSlabAnalysisResult LastSlabAnalysis;
    mutable FString LastLazExportStatusText;
    mutable FString LastLazExportWarningText;
    mutable FString LastLazLasSourcePath;
    mutable FString LastLazOutputPath;
    mutable FString LastPointCloudExportPath;
    mutable bool bLastLazExportAttempted = false;
    mutable bool bLastLazExportSucceeded = false;
    mutable bool bLastLazExportPlaceholderOnly = false;
    mutable bool bLastLazExternalCompressorRequested = false;
    mutable bool bLastLazExternalCompressorAttempted = false;
    mutable bool bLastLazExternalCompressorSucceeded = false;
    mutable bool bLastLazProducedOutputFile = false;
    mutable bool bLastLazTrueCompressionValidated = false;
    mutable int32 LastLazExportedPointCount = 0;
    mutable int32 LastLazExternalCompressorReturnCode = INDEX_NONE;
    mutable int64 LastLazOutputSizeBytes = 0;

    UPROPERTY(Transient)
    FVirtualSensorRuntimeStatus RuntimeStatus;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> LidarViewTexture;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> PointCloudPreviewComponent;

    double NextScheduledScanTime = -1.0;
    double ScheduledScanStartTime = -1.0;
    double LastScheduledCompletionTime = -1.0;
    FTransform ScheduledScanTransform = FTransform::Identity;
    int32 ScheduledScanWidth = 0;
    int32 ScheduledScanHeight = 0;
    int32 ScheduledNextRayIndex = 0;
    int32 ScheduledGeneration = 0;
    bool bRegisteredWithPerformanceSubsystem = false;
    bool bScheduledScanInProgress = false;
    bool bScheduledPayloadBuildInFlight = false;
    bool bScheduledAutoExportInFlight = false;
    bool bScheduledPayloadRefreshPending = false;
    TArray<FVirtualLidarPoint> ScheduledPoints;
    TArray<uint8> ScheduledHeatmapPixels;
};
