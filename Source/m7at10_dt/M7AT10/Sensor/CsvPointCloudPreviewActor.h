#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CsvPointCloudPreviewActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class ECsvPointCloudPreviewRenderMode : uint8
{
    ProceduralMesh UMETA(DisplayName = "Procedural Mesh Fast Preview"),
    InstancedMesh UMETA(DisplayName = "Instanced Mesh Legacy Preview")
};

UCLASS(BlueprintType)
class MA0T10_DT_API ACsvPointCloudPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    ACsvPointCloudPreviewActor();

#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
#endif

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|CSV PointCloud")
    bool LoadCsvPointCloud();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|CSV PointCloud")
    void LoadCsvPointCloudInEditor();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|CSV PointCloud")
    void SelectCsvFileAndLoadInEditor();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|CSV PointCloud")
    void ClearPointCloudPreview();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|CSV PointCloud")
    void ClearPointCloudPreviewInEditor();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|CSV PointCloud")
    int32 GetLoadedPointCount() const { return LoadedPointCount; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|CSV PointCloud|Status")
    int32 GetProceduralPreviewSectionCount() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|CSV PointCloud|Status")
    int32 GetInstancedPreviewInstanceCount() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|CSV PointCloud|Status")
    FString GetLastPreviewTelemetryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|CSV PointCloud|Status")
    bool WasLastRenderModeAutoPromoted() const { return bLastRenderModeAutoPromoted; }

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud")
    TObjectPtr<UProceduralMeshComponent> ProceduralPointCloudComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud")
    TObjectPtr<UInstancedStaticMeshComponent> PointCloudComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud", meta = (FilePathFilter = "csv", RelativeToGameDir))
    FString CsvFilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud")
    ECsvPointCloudPreviewRenderMode RenderMode = ECsvPointCloudPreviewRenderMode::ProceduralMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud|Safety")
    bool bAutoPromoteLargeInstancedPreviewToProcedural = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud|Safety", meta = (ClampMin = "0", ClampMax = "10000000", EditCondition = "bAutoPromoteLargeInstancedPreviewToProcedural"))
    int32 AutoPromoteInstancedToProceduralPointThreshold = 50000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud")
    bool bAutoLoadOnConstruction = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud")
    bool bTreatCsvCoordinatesAsWorldSpace = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud")
    bool bSkipZeroVectorPoints = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud", meta = (ClampMin = "0.001", ClampMax = "1000.0"))
    float CoordinateScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud", meta = (ClampMin = "1", ClampMax = "1000"))
    int32 PointStride = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud", meta = (ClampMin = "0", ClampMax = "10000000"))
    int32 MaxPointsToLoad = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float PointScale = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud")
    FLinearColor PointColor = FLinearColor(1.0f, 0.05f, 0.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud|Procedural", meta = (ClampMin = "1", ClampMax = "100000"))
    int32 ProceduralBatchSize = 50000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud|Instanced")
    bool bUseLowCostCubeWhenPointMeshIsEmpty = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud|Instanced")
    TObjectPtr<UStaticMesh> PointMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud")
    TObjectPtr<UMaterialInterface> PointMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status")
    int32 LoadedPointCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status")
    FIntPoint RowRange = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status")
    FIntPoint ColRange = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status")
    FVector MinBounds = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status")
    FVector MaxBounds = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status")
    FString LastLoadedPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    int32 LastInputLineCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    int32 LastAcceptedPointCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    int32 LastPreviewSectionCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    int32 LastPreviewInstanceCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    float LastParseDurationMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    float LastBuildDurationMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    float LastLoadDurationMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    FString LastRenderModeName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    FString LastRequestedRenderModeName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    FString LastPreviewStatus;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    bool bLastRenderModeAutoPromoted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud|Status|Telemetry")
    FString LastRenderModeAutoPromotionReason;

private:
    FString ResolveCsvFilePath() const;
    bool ParseCsvPointLine(const FString& Line, int32& OutRow, int32& OutCol, FVector& OutPoint) const;
    void ApplyPreviewStyle();
    UStaticMesh* ResolvePointMesh() const;
    void ResetStatus();
    ECsvPointCloudPreviewRenderMode ResolveEffectiveRenderMode(int32 AcceptedPointCount);
    FString GetRenderModeName(ECsvPointCloudPreviewRenderMode InRenderMode) const;
    void BuildProceduralPointCloud(const TArray<FVector>& Points);
    void BuildInstancedPointCloud(const TArray<FVector>& Points);
#if WITH_EDITOR
    bool OpenCsvFileDialog(FString& OutFilePath) const;
#endif

private:
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> DynamicPointMaterial;
};
