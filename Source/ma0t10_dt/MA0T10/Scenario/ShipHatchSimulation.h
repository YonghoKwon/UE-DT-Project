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
class ULineBatchComponent;
class UTextRenderComponent;

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
    double EffectiveMLXHeight() const;
    FVector SensorLocalOrigin(bool IsMLX) const;
    bool VisibleFromSelected(FVector Target) const;
    void Coverage(double& Opening,double& VisibleFloor,double& FloorArea) const;
    bool SelectSensorAt(FVector Origin,FVector Direction);
    void SetScanDisplay(bool Enabled);
    bool IsScanDisplayEnabled() const { return bScanDisplay; }
    static constexpr int32 DisplayRayLimit=96;
    int32 DisplayRayCount() const;
    void HighlightSensor(int32 Index) { SelectedSensor=FMath::Clamp(Index,0,1);UpdateSensorPresentation(); }
    int32 HighlightedSensor() const { return SelectedSensor; }
    bool PickCargo(FVector WorldOrigin,FVector WorldDirection,FVector& LocalHitMeters) const;
    bool Dig(FVector WorldOrigin,FVector WorldDirection,double Radius=2,double Depth=1.5);
    bool UndoDig();
    bool RestoreCargo();
    static constexpr int32 UndoLimit=16;
    int32 UndoCount() const { return HistoryCount; }
    double LastRemovedVolume() const { return LastRemoved; }
    double TotalRemovedVolume() const { return TotalRemoved; }
    uint64 DigBufferBytes() const;
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
    double LastGeometryEdit=0;
    UPROPERTY(Transient) TArray<TObjectPtr<AShipHatchPart>> SensorPresentation;
    UPROPERTY(Transient) TObjectPtr<ULineBatchComponent> ScanDisplay;
    FTimerHandle ScanDisplayTimer;
    bool bScanDisplay=false;
    int32 SelectedSensor=-1,ScanPhase=0;
    void UpdateSensorPresentation();
    void RefreshScanDisplay();
    bool PresentationOccludes(FVector Origin,FVector End) const;
    FTimerHandle PollTimer;
    TArray<FVector> CargoVertices,CargoNormals;
    TArray<int32> CargoIndices;
    int32 MeshNX=0,MeshNY=0;
    struct FDigState { TArray<float> Heights; double Volume=0,Fill=0,Last=0,Total=0; };
    FDigState Original,History[UndoLimit];
    TArray<float> DigScratch;
    int32 HistoryHead=0,HistoryCount=0;
    double LastRemoved=0,TotalRemoved=0;
    void UpdateCargoMesh(bool UpdateAppearance=true);
    void FinishCargoEdit();
    void StartGeneration();
    void PollGeneration();
    void BuildStructure();
    void ApplySurface(ShipHatch::FSurface&& Surface);
    void ConfigureSensors();
};
