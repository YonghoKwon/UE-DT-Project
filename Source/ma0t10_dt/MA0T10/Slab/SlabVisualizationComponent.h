#pragma once
#include "CoreMinimal.h"
#include "ActorComponent/StatusVisualizerCompBase.h"
#include "SlabScenarioTypes.h"
#include "SlabAnalysisTypes.h"
#include "SlabVisualizationComponent.generated.h"
class UStaticMeshComponent;
class UTextRenderComponent;
UCLASS(ClassGroup=(Slab),meta=(BlueprintSpawnableComponent))
class MA0T10_DT_API USlabVisualizationComponent : public UStatusVisualizerCompBase
{
	GENERATED_BODY()
public:
	USlabVisualizationComponent();
	void UpdateGeometry(const FVector& SizeCm);
	void ConfigureDisplay(const FSlabAnalysisDisplaySettings& Settings);
	void UpdateAnalysis(const FSlabScenarioRow& Row,const FSlabMetrics& Metrics,const FSlabSimulationStatus& Status,const FVector& SizeCm,const FTransform& Track,double LeftRailYcm,double RightRailYcm,bool bRailsValid,double ReferenceLengthCm=0);
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
	int32 GetOwnedHelperCount() const { return Helpers.Num()+Labels.Num()+LabelBackgrounds.Num(); }
	static constexpr int32 MaxLineHelpers=32;
	static constexpr int32 MaxTextHelpers=5;
	static constexpr int32 MaxLabelBackgrounds=5;
	/** Approximate 18-pixel glyph height, bounded for very near/far observation views. */
	static float CalculateReadableTextSize(double ViewDepthCm,double HorizontalFovDegrees,int32 ViewportHeight,double AspectRatio,bool bOrthographic=false,double OrthoWidthCm=0);
	static float CalculateScreenWorldSize(double ViewDepthCm,double HorizontalFovDegrees,int32 ViewportHeight,double AspectRatio,float Pixels,bool bOrthographic=false,double OrthoWidthCm=0);
	static FVector PullLabelTowardCamera(const FVector& Position,const FVector& CameraPosition,const FVector& CameraForward,double ViewDepthCm,double PullCm,bool bOrthographic,float& OutTextScale);
	UFUNCTION(BlueprintCallable,Category="Slab|Appearance") void SetHelpersVisible(bool bVisible);
	UFUNCTION(BlueprintPure,Category="Slab|Appearance") bool GetHelpersVisible() const { return bHelpersVisible; }
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void EnsureHelpers();
	void DrawAnalysis();
	void UpdateLabels();
	void SetLine(int32 Index,const FVector& Start,const FVector& End,float Thickness,FLinearColor Color,bool Visible=true);
	float ResolveStrokeSize() const;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Helpers;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextRenderComponent>> Labels;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> LabelBackgrounds;
	TArray<FLinearColor> AppliedLineColors;
	FSlabAnalysisDisplaySettings Display;
	FVector Size=FVector::OneVector;
	FTransform TrackTransform;
	FSlabMetrics LastMetrics;
	FString MaterialId;
	int64 FrameNo=0;
	double Elapsed=0,Duration=0;
	float Progress=0;
	double LeftRail=0,RightRail=0;
	double FixedReferenceLength=0;
	bool bRails=false;
	bool bHasAnalysis=false;
	bool bEnding=false;
	bool bHelpersVisible=true;
};
