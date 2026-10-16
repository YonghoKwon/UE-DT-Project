#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/InteractableActor/FacilityBase.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSlabFrameContext.h"
#include "SlabScenarioTypes.h"
#include "SlabAnalysisTypes.h"
#include "SlabActor.generated.h"

class USlabDataSyncComponent;
class USlabMotionComponent;
class USlabVisualizationComponent;
class USlabMetricsComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ASlabTrackReferenceActor;
class UVirtualLidarSurfaceResponseComponent;
enum class ESlabSurfacePreset : uint8 { Default, HotRed, OxidationScale, CooledSteel, AnalysisMatte };
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSlabStateChanged);

UENUM(BlueprintType)
enum class ESlabScenarioAdmission : uint8 { None, Started, StartedWithoutTransmission, StoredOnlyBusy, Duplicate, Rejected, Preparing };
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabScenarioAdmissionStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ESlabScenarioAdmission Result=ESlabScenarioAdmission::None;
	UPROPERTY(BlueprintReadOnly) FString ScenarioUUID;
	UPROPERTY(BlueprintReadOnly) FString RunUUID;
	UPROPERTY(BlueprintReadOnly) FString Reason;
	UPROPERTY(BlueprintReadOnly) FVirtualSlabSensorOutputSelection RequestedOutputs;
	UPROPERTY(BlueprintReadOnly) FVirtualSlabSensorOutputSelection AppliedOutputs=FVirtualSlabSensorOutputSelection::ObservationOnly();
};

/** Project-owned scenario Slab. DTCore components own input, movement and diagnostics. */
UCLASS()
class MA0T10_DT_API ASlabActor : public AFacilityBase,public ISlabScenarioPlaybackAdapter
{
	GENERATED_BODY()
public:
	ASlabActor();
	/** Explicit, transient dataset geometry; refuses scenario playback/parsing. */
	bool ApplyDatasetGeometry(const FVector& SizeCm);
	void RefreshDatasetAppearance() { UpdateAppearance(); }
	virtual void Click() override;
	bool CanApplySurfacePreset(ESlabSurfacePreset Preset,FString& Reason) const;
	bool ApplySurfacePreset(ESlabSurfacePreset Preset,bool bLinkLidar,FString& Reason);
	ESlabSurfacePreset GetSurfacePreset() const { return SurfacePreset; }
	bool IsSurfaceLidarLinked() const { return bSurfaceLidarLinked; }
	static FString SurfacePresetId(ESlabSurfacePreset Preset);
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual bool StartScenarioPlayback_Implementation(const FString& Json,const FString& ScenarioUUID,const FString& RunUUID) override;
	virtual void StopScenarioPlayback_Implementation(const FString& RunUUID) override;
	UFUNCTION(BlueprintCallable,Category="Slab|Simulation") bool SubmitScenarioJson(const FString& Json);
	UFUNCTION(BlueprintCallable,Category="Slab|Simulation") bool StartSyntheticScenario();
	UFUNCTION(BlueprintCallable,Category="Slab|Simulation") bool SetSimulationPaused(bool bPaused);
	UFUNCTION(BlueprintCallable,Category="Slab|Simulation") void StopSimulation();
	UFUNCTION(BlueprintPure,Category="Slab|Simulation") bool CanResetToInitialPlacement(FString& OutReason) const;
	UFUNCTION(BlueprintCallable,Category="Slab|Simulation") bool ResetToInitialPlacement(FString& OutError);
	UFUNCTION(BlueprintPure,Category="Slab|Simulation") FSlabSimulationStatus GetSimulationStatus() const { return Status; }
	UFUNCTION(BlueprintPure,Category="Slab|Simulation") FSlabScenarioAdmissionStatus GetLastScenarioAdmissionStatus() const { return LastAdmission; }
	UFUNCTION(BlueprintPure,Category="Slab|Simulation") FSlabMetrics GetCurrentMetrics() const;
	UFUNCTION(BlueprintPure,Category="Slab|Simulation") bool IsSimulationActive() const;
	UFUNCTION(BlueprintPure,Category="Slab|Configuration") ESlabInputUnit GetDimensionUnit() const { return DimensionUnit; }
	UFUNCTION(BlueprintPure,Category="Slab|Configuration") ESlabInputUnit GetPositionUnit() const { return PositionUnit; }
	UFUNCTION(BlueprintCallable,Category="Slab|Configuration") bool SetDimensionUnit(ESlabInputUnit Value);
	UFUNCTION(BlueprintCallable,Category="Slab|Configuration") bool SetPositionUnit(ESlabInputUnit Value);
	UFUNCTION(BlueprintPure,Category="Slab|Appearance") bool GetHotAppearance() const { return bHotAppearance; }
	UFUNCTION(BlueprintCallable,Category="Slab|Appearance") void SetHotAppearance(bool bHot);
	UFUNCTION(BlueprintCallable,Category="Slab|Appearance") void SetDiagnosticHelpersVisible(bool bVisible);
	UFUNCTION(BlueprintPure,Category="Slab|Appearance") bool GetDiagnosticHelpersVisible() const;
	/** Session-only presentation selection; does not change persisted display/optical values. */
	UFUNCTION(BlueprintCallable,Category="Slab|Appearance") void SetDetailedDiagnosticLabels(bool bVisible);
	UFUNCTION(BlueprintPure,Category="Slab|Appearance") bool GetDetailedDiagnosticLabels() const;
	UFUNCTION(BlueprintPure,Category="Slab|Analysis") FSlabAnalysisDisplaySettings GetAnalysisDisplaySettings() const { return AnalysisDisplaySettings; }
	UFUNCTION(BlueprintCallable,Category="Slab|Analysis") void SetAnalysisDisplaySettings(const FSlabAnalysisDisplaySettings& Settings);
	UFUNCTION(BlueprintPure,Category="Slab|Appearance") FString GetEffectiveSurfaceMaterialPath() const;
	UFUNCTION(BlueprintCallable,Category="Slab|Setup") FSlabSetupValidation ValidateSlabSetup() const;
	static FSoftObjectPath ResolveSurfaceMaterialPath(const FSoftObjectPath& Configured);
	UFUNCTION(BlueprintPure,Category="Slab|Outputs") FVirtualSlabSensorOutputSelection GetSensorOutputs() const { return SensorOutputs; }
	UFUNCTION(BlueprintCallable,Category="Slab|Outputs") bool SetSensorOutputs(FVirtualSlabSensorOutputSelection Outputs);
	UFUNCTION(BlueprintPure,Category="Slab|Outputs") FSlabExecutionOptions GetExecutionOptions() const { return ExecutionOptions; }
	UFUNCTION(BlueprintCallable,Category="Slab|Outputs") bool SetExecutionOptions(FSlabExecutionOptions Options);
	FSlabScenarioDataPtr GetScenario() const { return Scenario; }
	FSlabMetrics CalculateMetricsForRow(const FSlabScenarioRow& Row) const;
	FVector GetTrackLocalPositionCm() const;
	uint32 GetMetricsConfigurationHash() const;
	bool ReceiveScenario(FSlabScenarioDataPtr Data);
	FTransform GetDatasetTrackTransform() const { return GetTrackTransform(); }
	bool HasPendingScenarioInput() const { return bParsing; }
	void AdvanceSimulation(double DeltaSeconds);
	/** Motion component callback: pose already applied; update metadata/metrics and session lifecycle. */
	void OnSlabPoseApplied(const FSlabScenarioRow& Row,int32 RowIndex,double Time,bool bEnd);
	USlabDataSyncComponent* GetDataSyncComponent() const { return DataSyncComponent; }
	UPROPERTY(BlueprintAssignable,Category="Slab") FSlabStateChanged OnSlabStateChanged;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Routing") FString ReceiverId=TEXT("SlabScenario.Main");
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Configuration") TObjectPtr<ASlabTrackReferenceActor> TrackReference;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Configuration") ESlabInputUnit DimensionUnit=ESlabInputUnit::Centimeters;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Configuration") ESlabInputUnit PositionUnit=ESlabInputUnit::Centimeters;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Configuration") FVector InitialDimensions=FVector(10830,1100,250);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Configuration") TArray<FString> TargetSensorIds;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Outputs") FVirtualSlabSensorOutputSelection SensorOutputs;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Outputs") FSlabExecutionOptions ExecutionOptions;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Appearance") TSoftObjectPtr<UMaterialInterface> SurfaceMaterial;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Appearance") bool bHotAppearance=false;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Analysis") FSlabAnalysisDisplaySettings AnalysisDisplaySettings;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Appearance",meta=(ClampMin="0",ClampMax="1")) float Oxidation=0.6f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Appearance",meta=(ClampMin="0",ClampMax="1")) float Roughness=0.65f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Slab|Appearance",meta=(ClampMin="0",ClampMax="20")) float EmissiveStrength=3.0f;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Slab|Components") TObjectPtr<UStaticMeshComponent> SlabMesh;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USlabDataSyncComponent> DataSyncComponent;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USlabMotionComponent> MotionComponent;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USlabVisualizationComponent> VisualizationComponent;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USlabMetricsComponent> MetricsComponent;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SurfaceInstance;
	// Retain both owned MIDs across explicit preset switches, including user MID overrides.
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> NonOxideSurfaceInstance;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> OxideSurfaceInstance;
	UPROPERTY(Transient) FSlabSimulationStatus Status;
	UPROPERTY(Transient) FSlabScenarioAdmissionStatus LastAdmission;
	FSlabScenarioDataPtr Scenario;
	FSlabScenarioDataPtr PendingScenario;
	FTransform InitialTrackTransform;
	FTransform InitialPlacementTransform;
	bool bInitialPlacementCaptured=false;
	int32 LastNotifiedIndex=INDEX_NONE;
	uint64 ParseGeneration=0;
	bool bParsing=false;
	bool bEnding=false;
	bool bRegistered=false;
	ESlabSurfacePreset SurfacePreset=ESlabSurfacePreset::Default;
	bool bSurfaceLidarLinked=false,bSurfaceBaselineCaptured=false;
	bool bHasBaselineColdColor=false,bHasBaselineMetallic=false;
	FLinearColor BaselineColdColor;
	float BaselineSurfaceMetallic=0;
	bool BaselineHot=false;
	float BaselineOxidation=0,BaselineRoughness=0,BaselineEmissive=0;
	TWeakObjectPtr<UVirtualLidarSurfaceResponseComponent> BaselineSurfaceResponse;
	float BaselineReflectivity=0;
	bool BaselineGpuOpticalOverride=false;
	UPROPERTY(Transient) TObjectPtr<UVirtualLidarSurfaceResponseComponent> OwnedPresetSurfaceResponse;
	void RecordSurfaceUsage(const FString& Run);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
	friend class FSlabInformationCardTest;
	friend class FSlabSurfaceRhiCommand;
#endif
	bool StartScenario(FSlabScenarioDataPtr Data,const FString& RunUUID,bool bReplay);
	void FinishSimulation(bool bAborted,const FString& Error=FString());
	void UpdateAppearance();
	FSoftObjectPath GetPresetSurfaceMaterialPath(ESlabSurfacePreset Preset) const;
	UMaterialInterface* GetExplicitSlotSurfaceMaterial() const;
	void UpdateDimensions();
	FTransform GetTrackTransform() const;
	FVector GetSizeCm() const;
	FTransform MakePose(const FSlabScenarioRow& Row) const;
	void ReportFailure(const FString& Message);
	void RecordAdmission(ESlabScenarioAdmission Result,const FString& ScenarioId,const FString& Reason);
};
