#pragma once
#include "CoreMinimal.h"
#include "InteractableActor/InteractableActor.h"
#include "Async/Future.h"
#include "ShipHatchModel.h"
#include "ShipHatchSimulation.generated.h"
class UProceduralMeshComponent;
class UVirtualLidarSurfaceResponseComponent;
class AVirtualLidarSensorActor;
class UMaterialInterface;

/** Separate hit owners keep optical material response and existing semantic rules unambiguous. */
UCLASS(NotBlueprintable)
class MA0T10_DT_API AShipHatchPart : public AInteractableActor
{
    GENERATED_BODY()
public:
    AShipHatchPart();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UVirtualLidarSurfaceResponseComponent> Optical;
    void SetColor(const FLinearColor& Color);
};

/** Explicit map-local opt-in. No subsystem, Tick, shared settings or stored UI preferences. */
UCLASS()
class MA0T10_DT_API AShipHatchSimulation : public AInteractableActor
{
    GENERATED_BODY()
public:
    AShipHatchSimulation();
    UPROPERTY(EditAnywhere,Category="Hatch|Dimensions",meta=(Units="m")) double HoldWidth=42.3;
    UPROPERTY(EditAnywhere,Category="Hatch|Dimensions",meta=(Units="m")) double HoldLength=25.8;
    UPROPERTY(EditAnywhere,Category="Hatch|Dimensions",meta=(Units="m")) double HoldDepth=22.5;
    UPROPERTY(EditAnywhere,Category="Hatch|Dimensions",meta=(Units="m")) double OpeningWidth=23;
    UPROPERTY(EditAnywhere,Category="Hatch|Dimensions",meta=(Units="m")) double OpeningLength=20;
    UPROPERTY(EditAnywhere,Category="Hatch|Dimensions",meta=(Units="m")) double CoamingHeight=2;
    UPROPERTY(EditAnywhere,Category="Hatch|Sensors",meta=(Units="m")) double SensorAboveDeck=8;
    UPROPERTY(EditAnywhere,Category="Hatch|Cargo") bool bCoal=false;
    UPROPERTY(EditAnywhere,Category="Hatch|Cargo") int32 Seed=19019;
    UPROPERTY(EditAnywhere,Category="Hatch|Cargo",meta=(ClampMin="0",ClampMax="95")) double FillMin=35;
    UPROPERTY(EditAnywhere,Category="Hatch|Cargo",meta=(ClampMin="0",ClampMax="95")) double FillMax=65;
    UPROPERTY(EditAnywhere,Category="Hatch|Sensors") bool bMLX=true;
    UPROPERTY(EditAnywhere,Category="Hatch|Sensors") bool bLivox=true;
    UPROPERTY(EditAnywhere,Category="Hatch|Sensors") bool bNativeMLX=false;
    UPROPERTY(VisibleAnywhere,Transient) TObjectPtr<AVirtualLidarSensorActor> MLX;
    UPROPERTY(VisibleAnywhere,Transient) TObjectPtr<AVirtualLidarSensorActor> Livox;
    ShipHatch::FSettings Settings() const;
    bool Regenerate(bool Randomize);
    bool SetSensorSelection(bool EnableMLX,bool EnableLivox,bool Native);
    bool IsGenerating() const { return bGenerating; }
    uint64 Revision() const { return SurfaceRevision; }
    const ShipHatch::FSurface& Surface() const { return Current; }
    FString Status;
    double LastMeshApplyMs=0;
    uint64 CompletedGenerations=0;
    void CollectObservations(TArray<ShipHatch::FObservation>& Out) const;
    bool HasFreshMeasurement() const;
    uint64 MeshScratchBytes() const { return CargoVertices.GetAllocatedSize()+CargoNormals.GetAllocatedSize()+CargoIndices.GetAllocatedSize(); }
    uint64 GenerationBufferBytes() const { return Current.Heights.GetAllocatedSize()+RecycledHeights.GetAllocatedSize()+GenerationShape.GetAllocatedSize(); }
    FSimpleMulticastDelegate OnSurfaceChanged;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<AShipHatchPart> Cargo;
    UPROPERTY(Transient) TArray<TObjectPtr<AShipHatchPart>> Structure;
    ShipHatch::FSurface Current;
    struct FGenerationBuffers { ShipHatch::FSurface Surface; TArray<double> Shape; };
    TFuture<FGenerationBuffers> Generation;
    TArray<float> RecycledHeights;
    TArray<double> GenerationShape;
    ShipHatch::FSettings Pending;
    bool bGenerating=false,bPending=false,bEnding=false;
    uint64 SurfaceRevision=0;
    int64 FrameGates[2]={-1,-1};
    FTimerHandle PollTimer;
    TArray<FVector> CargoVertices,CargoNormals;
    TArray<int32> CargoIndices;
    int32 MeshNX=0,MeshNY=0;
    void StartGeneration();
    void PollGeneration();
    void BuildStructure();
    void ApplySurface(ShipHatch::FSurface&& Surface);
    void ConfigureSensors();
};
