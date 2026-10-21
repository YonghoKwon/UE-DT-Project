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
class ADirectionalLight;
class UPointLightComponent;

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

/** Full-vessel context is a separate DTCore interactable, with real query
 * geometry and no Tick. Operational hatch framing remains independently useful. */
UCLASS(NotBlueprintable)
class MA0T10_DT_API AShipHatchVessel : public AInteractableActor
{
    GENERATED_BODY()
public:
    AShipHatchVessel();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> ScaleMarker;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UVirtualLidarSurfaceResponseComponent> Optical;
};

/** Explicit map-local opt-in. No subsystem, Tick, shared settings or stored UI preferences. */
UCLASS()
class MA0T10_DT_API AShipHatchSimulation : public AInteractableActor
{
    GENERATED_BODY()
    friend struct FHatchFeedbackProbe;
    friend struct FHatchOrientationProbe;
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
    UPROPERTY(VisibleAnywhere,Transient) TObjectPtr<AVirtualLidarSensorActor> CabRight;
    UPROPERTY(EditAnywhere,Category="Hatch|Unloader") bool bFieldLayout=true;
    UPROPERTY(EditAnywhere,Category="Hatch|Unloader") bool bShowUnloader=true;
    UPROPERTY(EditAnywhere,Category="Hatch|Vessel") bool bRealScale=true;
    UPROPERTY(EditAnywhere,Category="Hatch|Vessel",meta=(ClampMin="290",ClampMax="300",Units="m")) double VesselLength=292;
    UPROPERTY(EditAnywhere,Category="Hatch|Vessel",meta=(ClampMin="45",ClampMax="47",Units="m")) double VesselBeam=47;
    UPROPERTY(EditAnywhere,Category="Hatch|Vessel",meta=(ClampMin="24",ClampMax="25",Units="m")) double MouldedDepth=24.7;
    UPROPERTY(EditAnywhere,Category="Hatch|Vessel",meta=(ClampMin="17",ClampMax="19",Units="m")) double DesignDraft=18.2;
    UPROPERTY(EditAnywhere,Category="Hatch|Vessel",meta=(ClampMin="8",ClampMax="9")) int32 VesselHolds=9;
    UPROPERTY(EditAnywhere,Category="Hatch|Unloader",meta=(ClampMin="22",ClampMax="30",Units="m")) double RailSpan=28;
    UPROPERTY(EditAnywhere,Category="Hatch|Vessel",meta=(ClampMin="2",ClampMax="5",Units="m")) double BerthGap=3.5;
    UPROPERTY(EditAnywhere,Category="Hatch|Vessel",meta=(ClampMin="2",ClampMax="5",Units="m")) double QuayAboveWater=3.5;
    double KeelZ() const { return HoldDepth-MouldedDepth; }
    double WaterZ() const { return KeelZ()+DesignDraft; }
    double QuayZ() const { return WaterZ()+QuayAboveWater; }
    double QuayEdgeY() const { return -VesselBeam*.5-BerthGap; }
    double SeaRailY() const { return QuayEdgeY()-3; }
    double BoomTipY() const { return bRealScale?SeaRailY()+BoomLength:10; }
    bool ValidateSceneScale(FString& Error) const;
    void ShowScaleReference(bool Enabled);
    bool IsScaleReferenceVisible() const;
    void ShowWater(bool Enabled);
    bool IsWaterVisible() const { return bWaterVisible; }
    UPrimitiveComponent* GrabPrimitive() const;
    UPROPERTY(EditAnywhere,Category="Hatch|Unloader",meta=(ClampMin="35",ClampMax="45",Units="m")) double BoomLength=42;
    UPROPERTY(EditAnywhere,Category="Hatch|Unloader",meta=(ClampMin="18",ClampMax="35",Units="m")) double BoomHeight=24;
    UPROPERTY(EditAnywhere,Category="Hatch|Unloader",meta=(ClampMin="12",ClampMax="24",Units="m")) double CabHeight=16;
    UPROPERTY(EditAnywhere,Category="Hatch|Unloader",meta=(Units="m")) double CabX=0;
    UPROPERTY(EditAnywhere,Category="Hatch|Unloader",meta=(Units="m")) double CabY=-2;
    bool bCabRight=true;
    int32 FieldProfiles[3]={1,1,1}; // 0=ML-X80 Integration200, 1=Mid-360S; site models unconfirmed.
    FVector SensorOffsets[3]={FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector};
    double SensorTiltTrim[3]={0,0,0},SensorYaw[3]={0,0,0};
    double GrabAspect=1.4;
    void RefreshFieldLayout(){if(!bGenerating){ConfigureSensors();++SurfaceRevision;OnSurfaceChanged.Broadcast();}}
    AVirtualLidarSensorActor* SensorAt(int32 Index) const;
    bool SensorEnabled(int32 Index) const;
    FVector SensorOrigin(int32 Index) const;
    bool VisibleFromSensor(int32 Index,FVector Target) const;
    void SetFieldSensor(int32 Index,bool Enabled,int32 Profile);
    void ApplyUnloader();
    void ShowUnloader(bool Enabled);
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
    void HighlightSensor(int32 Index) { SelectedSensor=FMath::Clamp(Index,0,bFieldLayout?2:1);UpdateSensorPresentation(); }
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
    uint64 MeshScratchBytes() const { return CargoVertices.GetAllocatedSize()+CargoNormals.GetAllocatedSize()+CargoIndices.GetAllocatedSize()+CargoColors.GetAllocatedSize(); }
    void SetLighting(double Azimuth,double Elevation,double Intensity,bool Fill);
    double LightAzimuth=-25,LightElevation=65,LightIntensity=5;
    bool bFillLight=true;
    uint64 GenerationBufferBytes() const { return Current.Heights.GetAllocatedSize()+RecycledHeights.GetAllocatedSize()+GenerationShape.GetAllocatedSize(); }
    FSimpleMulticastDelegate OnSurfaceChanged;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<AShipHatchPart> Cargo;
    UPROPERTY(Transient) TObjectPtr<AShipHatchVessel> Vessel;
    UPROPERTY(Transient) TObjectPtr<ADirectionalLight> KeyLight;
    UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FillLight;
    FRotator OriginalLightRotation;
    float OriginalLightIntensity=0;
    uint8 OriginalLightMobility=0;
    bool bOriginalLightShadows=true;
    UPROPERTY(Transient) TArray<TObjectPtr<AShipHatchPart>> Structure;
    ShipHatch::FSurface Current;
    struct FGenerationBuffers { ShipHatch::FSurface Surface; TArray<double> Shape; };
    TFuture<FGenerationBuffers> Generation;
    TArray<float> RecycledHeights;
    TArray<double> GenerationShape;
    ShipHatch::FSettings Pending;
    bool bGenerating=false,bPending=false,bEnding=false;
    bool bWaterVisible=true;
    uint64 SurfaceRevision=0;
    uint32 SceneScaleKey=0;
    int64 FrameGates[3]={-1,-1,-1};
    double LastGeometryEdit=0;
    UPROPERTY(Transient) TArray<TObjectPtr<AShipHatchPart>> SensorPresentation;
    UPROPERTY(Transient) TObjectPtr<AShipHatchPart> UnloaderPresentation;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> GrabPresentation;
    UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> UnloaderLabel;
    UPROPERTY(Transient) TObjectPtr<ULineBatchComponent> ScanDisplay;
    FTimerHandle ScanDisplayTimer;
    bool bScanDisplay=false;
    int32 SelectedSensor=-1,ScanPhase=0;
    void UpdateSensorPresentation();
    void BuildUnloaderDetails();
    void BuildVessel();
    void BuildCorrugatedWalls(AShipHatchPart* Part);
    void RefreshScanDisplay();
    bool PresentationOccludes(FVector Origin,FVector End) const;
    FTimerHandle PollTimer;
    TArray<FVector> CargoVertices,CargoNormals;
    TArray<FLinearColor> CargoColors;
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
