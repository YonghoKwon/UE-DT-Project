#pragma once
#include "CoreMinimal.h"
#include "InteractableActor/InteractableActor.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/GameModeBase.h"
#include "CoilLoadingModel.h"
#include "CoilLoadingSimulation.generated.h"
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UPhysicalMaterial;
class AVirtualLidarSensorActor;
class IVirtualSensorRayQuery;
class ULineBatchComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UTextRenderComponent;

/** This scenario has an explicit view camera and UI; no unowned pawn collision. */
UCLASS()
class MA0T10_DT_API ACoilLoadingGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACoilLoadingGameMode();
};

UCLASS()
class MA0T10_DT_API UCoilLoadingSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) FString PlanJson;
};
/** One measured identity per coil; settled rendering is batched by the scene owner. */
UCLASS(NotBlueprintable)
class MA0T10_DT_API ACoilLoadingBody : public AInteractableActor
{
    GENERATED_BODY()
public:
    ACoilLoadingBody();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Detail;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(Transient) TObjectPtr<UPhysicalMaterial> ContactMaterial;
    void Configure(UStaticMesh* Mesh,UMaterialInterface* Material,const CoilLoading::FSpec& Spec);
};
enum class ECoilLoadingPhase:uint8 { Idle,Lifting,Traversing,Lowering,Settling,MotionTest };
struct FCoilPhysicsObservation
{
    FString State=TEXT("미실행");
    double MaxDisplacement=0,MaxRotationDegrees=0,SettlingSeconds=0,MaxSpeed=0;
    bool bAnomaly=false;
};
/** Only runs in the explicitly created CoilLoadingTestMap. No global policy or map mutation. */
UCLASS()
class MA0T10_DT_API ACoilLoadingSimulation : public AInteractableActor
{
    GENERATED_BODY()
public:
    ACoilLoadingSimulation();
    UPROPERTY(EditAnywhere,Category="Coil|Assets") TObjectPtr<UMaterialInterface> CoilSurfaceAsset;
    UPROPERTY(EditAnywhere,Category="Coil|Assets") TObjectPtr<UMaterialInterface> StructureMaterialAsset;
    UPROPERTY(EditAnywhere,Category="Coil|Assets") TObjectPtr<UStaticMesh> StructureCubeAsset;
    CoilLoading::FSettings Settings;
    CoilLoading::FSpec NextSpec;
    int32 NextTier=1,NextRow=1,NextColumn=1;
    bool bNextDunnage=true,bNextChocks=true,bNextLashing=false,bNextForeAft=true;
    double NextChockKN=0,NextLashingKN=0,NextStiffness=0,NextOvalityLimit=0,NextRollingResistanceM=0;
    double RollDegrees=5,PitchDegrees=2,MotionPeriod=10,MotionDuration=20;
    UPROPERTY(Transient) TObjectPtr<AVirtualLidarSensorActor> Lidar;
    UPROPERTY(Transient) TObjectPtr<ACoilLoadingBody> Ghost;
    UPROPERTY(Transient) TObjectPtr<ACoilLoadingBody> Suspended;
    UPROPERTY(Transient) TObjectPtr<ACoilLoadingBody> WaitingCoil;
    const TArray<CoilLoading::FPlacement>& Plan() const { return Placements; }
    const CoilLoading::FResult& Analysis() const { return PreviewAnalysis; }
    const FCoilPhysicsObservation& Physics() const { return PhysicsResult; }
    uint64 Revision() const { return PlanRevision; }
    ECoilLoadingPhase Phase() const { return CurrentPhase; }
    CoilLoading::FPlacement PreviewPlacement() const;
    void RefreshPreview();
    bool PlaceNext(bool Animate=true);
    bool AddTier();
    bool Undo();
    bool StartMotionTest();
    void CancelOperation();
    bool ImportCoils(const FString& Text,bool Json);
    bool RequestPlanSave();
    bool IsPlanSavePending() const { return !PlanSaveRequest.IsEmpty(); }
    FString LastPlanSavePath;
    bool LoadPlan(const FString& Path);
    bool IsPlanLoadPending() const { return bPlanLoading; }
    bool IsSlotSavePending() const { return bSlotSaving; }
    bool SaveSlot(const FString& Slot=TEXT("CoilLoading_v1"));
    bool LoadSlot(const FString& Slot=TEXT("CoilLoading_v1"));
    void SetView(int32 View); // 0 context,1 interior,2 section,3 top,4 points
    int32 View() const { return ActiveView; }
    FString Status=TEXT("배치 전 해석 · 미확인 입력을 확인하세요");
    double LidarDeviationP95=0;
    double LidarFitResidualP95=0;
    int32 FittedCoilCount=0,UnmatchedCoilIdPoints=0;
    int32 LidarCoilHits=0;
    int64 MeasuredFrame=-1;
    bool bLidarCompared=false;
    FString QueryDiagnostic;
    double RailX=-8;
    void HighlightCoils(const TArray<int32>& Indices);
    bool IsHighlighted(int32 Index) const { return Highlighted.Contains(Index); }
    const TArray<int32>& HighlightedCoils() const { return Highlighted; }
    double QuayZ() const { return Settings.Draft-Settings.KeelToTanktop+3.5; }
    double QuayEdgeY() const { return Settings.QuayEdgeY(); }
    double QuayOffset(double Setback) const { return QuayEdgeY()+Settings.QuaySide*Setback; }
    double ServiceLaneWidth() const { return 12; }
    double ShedCenterY() const { return QuayOffset(Settings.RailSetback+Settings.RailGauge+5+ServiceLaneWidth()+5); }
    FVector PickupPosition() const { return FVector(-8,QuayOffset(8),QuayZ()+1+NextSpec.OuterDiameter()*.5); }
    FVector CraneTop() const { return FVector(RailX,Settings.RailCenterY(),QuayZ()+28); }
    FVector HookPosition() const { return CurrentHook; }
    TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> CaptureQuery(AActor* Ignore,ECollisionChannel Channel);
    FSimpleMulticastDelegate OnChanged;
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<ACoilLoadingBody>> Bodies;
    UPROPERTY(Transient) TArray<TObjectPtr<ACoilLoadingBody>> Stock;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PlanSupports;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> CraneParts;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> TrolleyDetails;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> HookDetails;
    FVector HookVisualOrigin=FVector::ZeroVector;
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> PresentationBatches;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> PresentationMeshes;
    TArray<FVector> CraneStarts;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Structure;
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Batches;
    UPROPERTY(Transient) TMap<FString,TObjectPtr<UStaticMesh>> SharedMeshes;
    UPROPERTY(Transient) TMap<FString,TObjectPtr<UStaticMesh>> SharedDetailMeshes;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> CoilSurfaceMaterials;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> DetailMaterials;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> Materials;
    UPROPERTY(Transient) TObjectPtr<ULineBatchComponent> RigLines;
    UPROPERTY(Transient) TObjectPtr<USkyLightComponent> Ambient;
    UPROPERTY(Transient) TObjectPtr<USkyAtmosphereComponent> Sky;
    TArray<CoilLoading::FPlacement> Placements;
    TArray<CoilLoading::FSpec> Manifest;
    TArray<FString> History;
    CoilLoading::FResult PreviewAnalysis;
    CoilLoading::FSettings BuiltSceneSettings;
    CoilLoading::FPlacement Pending;
    FCoilPhysicsObservation PhysicsResult;
    ECoilLoadingPhase CurrentPhase=ECoilLoadingPhase::Idle;
    double PhaseTime=0,QuietTime=0,SimulationTime=0;
    FVector PhaseStart=FVector::ZeroVector,CurrentHook=FVector::ZeroVector;
    TArray<FTransform> MotionStarts;
    int32 ActiveView=0,ManifestIndex=0;
    uint64 PlanRevision=0,GeometryRevision=0,CachedGeometryRevision=MAX_uint64;
    int32 SupportedPlacementCount=0;
    TArray<int32> Highlighted;
    TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> CachedQuery;
    FTimerHandle PresentationTimer;
    bool bEnding=false;
    bool bSlotSaving=false;
    bool bPlanLoading=false;
    uint64 PlanLoadGeneration=0;
    FString PlanSaveRequest;
    UStaticMesh* CoilMesh(const CoilLoading::FSpec& Spec);
    UStaticMesh* CoilDetailMesh(const CoilLoading::FSpec& Spec);
    void ApplyCoilAppearance(ACoilLoadingBody* Body,const CoilLoading::FSpec& Spec);
    UMaterialInterface* Material(FLinearColor Color);
    ACoilLoadingBody* SpawnCoil(const CoilLoading::FSpec& Spec,FVector Center);
    UStaticMeshComponent* Box(FVector Center,FVector Size,int32 Color,bool Collision=true,FRotator Rotation=FRotator::ZeroRotator);
    void BuildScene();
    void BuildQuayPresentation();
    void UpdateTrolley();
    void RestoreSceneGeometry();
    void ConfigureLidar();
    void RebuildBatches();
    void RebuildPlanBodies();
    void BuildSupports();
    void CommitPlacement(ACoilLoadingBody* Body);
    void DrawRig();
    void Observe();
    void PushUndo();
    void ParseLoadedPlan(FString Json,uint64 Generation,uint64 Revision);
    void FinishPlanLoad(uint64 Generation,uint64 Revision,CoilLoading::FSettings Settings,TArray<CoilLoading::FPlacement> Plan,FString Error);
    void FinishPhysics();
    int32 NominalRowCapacity() const;
};
