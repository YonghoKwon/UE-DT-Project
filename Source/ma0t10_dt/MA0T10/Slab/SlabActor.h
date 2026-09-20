#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/InteractableActor/FacilityBase.h"
#include "ma0t10_dt/MA0T10/Core/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSlabFrameContext.h"
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSlabStateChanged);

/** Project-owned scenario Slab. DTCore components own input, movement and diagnostics. */
UCLASS()
class MA0T10_DT_API ASlabActor : public AFacilityBase,public ISlabScenarioPlaybackAdapter
{
	GENERATED_BODY()
public:
	ASlabActor();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual bool StartScenarioPlayback_Implementation(const FString& Json,const FString& ScenarioUUID,const FString& RunUUID) override;
	virtual void StopScenarioPlayback_Implementation(const FString& RunUUID) override;
	UFUNCTION(BlueprintCallable,Category="Slab|Simulation") bool SubmitScenarioJson(const FString& Json);
	UFUNCTION(BlueprintCallable,Category="Slab|Simulation") bool StartSyntheticScenario();
	UFUNCTION(BlueprintCallable,Category="Slab|Simulation") bool SetSimulationPaused(bool bPaused);
	UFUNCTION(BlueprintCallable,Category="Slab|Simulation") void StopSimulation();
	UFUNCTION(BlueprintPure,Category="Slab|Simulation") FSlabSimulationStatus GetSimulationStatus() const { return Status; }
	UFUNCTION(BlueprintPure,Category="Slab|Simulation") FSlabMetrics GetCurrentMetrics() const;
	UFUNCTION(BlueprintPure,Category="Slab|Simulation") bool IsSimulationActive() const;
	UFUNCTION(BlueprintPure,Category="Slab|Configuration") ESlabInputUnit GetDimensionUnit() const { return DimensionUnit; }
	UFUNCTION(BlueprintPure,Category="Slab|Configuration") ESlabInputUnit GetPositionUnit() const { return PositionUnit; }
	UFUNCTION(BlueprintCallable,Category="Slab|Configuration") bool SetDimensionUnit(ESlabInputUnit Value);
	UFUNCTION(BlueprintCallable,Category="Slab|Configuration") bool SetPositionUnit(ESlabInputUnit Value);
	UFUNCTION(BlueprintPure,Category="Slab|Appearance") bool GetHotAppearance() const { return bHotAppearance; }
	UFUNCTION(BlueprintCallable,Category="Slab|Appearance") void SetHotAppearance(bool bHot);
	UFUNCTION(BlueprintCallable,Category="Slab|Appearance") void SetDiagnosticHelpersVisible(bool bVisible);
	UFUNCTION(BlueprintPure,Category="Slab|Analysis") FSlabAnalysisDisplaySettings GetAnalysisDisplaySettings() const { return AnalysisDisplaySettings; }
	UFUNCTION(BlueprintCallable,Category="Slab|Analysis") void SetAnalysisDisplaySettings(const FSlabAnalysisDisplaySettings& Settings);
	UFUNCTION(BlueprintPure,Category="Slab|Appearance") FString GetEffectiveSurfaceMaterialPath() const;
	UFUNCTION(BlueprintCallable,Category="Slab|Setup") FSlabSetupValidation ValidateSlabSetup() const;
	static FSoftObjectPath ResolveSurfaceMaterialPath(const FSoftObjectPath& Configured);
	UFUNCTION(BlueprintPure,Category="Slab|Outputs") FVirtualSlabSensorOutputSelection GetSensorOutputs() const { return SensorOutputs; }
	UFUNCTION(BlueprintCallable,Category="Slab|Outputs") bool SetSensorOutputs(FVirtualSlabSensorOutputSelection Outputs);
	FSlabScenarioDataPtr GetScenario() const { return Scenario; }
	FSlabMetrics CalculateMetricsForRow(const FSlabScenarioRow& Row) const;
	uint32 GetMetricsConfigurationHash() const;
	bool ReceiveScenario(FSlabScenarioDataPtr Data);
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
	UPROPERTY(Transient) FSlabSimulationStatus Status;
	FSlabScenarioDataPtr Scenario;
	FTransform InitialTrackTransform;
	int32 LastNotifiedIndex=INDEX_NONE;
	uint64 ParseGeneration=0;
	bool bParsing=false;
	bool bEnding=false;
	bool bRegistered=false;
	bool StartScenario(FSlabScenarioDataPtr Data,const FString& RunUUID,bool bReplay);
	void FinishSimulation(bool bAborted,const FString& Error=FString());
	void UpdateAppearance();
	UMaterialInterface* GetExplicitSlotSurfaceMaterial() const;
	void UpdateDimensions();
	FTransform GetTrackTransform() const;
	FVector GetSizeCm() const;
	FTransform MakePose(const FSlabScenarioRow& Row) const;
	void ReportFailure(const FString& Message);
};
