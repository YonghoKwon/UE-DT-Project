#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/FilePath.h"
#include "CsvPointCloudPreviewActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

UCLASS(BlueprintType)
class M7AT10_DT_API ACsvPointCloudPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    ACsvPointCloudPreviewActor();

#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
#endif

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "DigitalTwin|CSV PointCloud")
    bool LoadCsvPointCloud();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "DigitalTwin|CSV PointCloud")
    void ClearPointCloudPreview();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|CSV PointCloud")
    int32 GetLoadedPointCount() const { return LoadedPointCount; }

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|CSV PointCloud")
    TObjectPtr<UInstancedStaticMeshComponent> PointCloudComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud", meta = (FilePathFilter = "csv"))
    FFilePath CsvFilePath;

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
    float PointScale = 0.035f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud")
    FLinearColor PointColor = FLinearColor(1.0f, 0.05f, 0.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|CSV PointCloud")
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

private:
    FString ResolveCsvFilePath() const;
    bool ParseCsvPointLine(const FString& Line, int32& OutRow, int32& OutCol, FVector& OutPoint) const;
    void ApplyPreviewStyle();
    void ResetStatus();

private:
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> DynamicPointMaterial;
};
