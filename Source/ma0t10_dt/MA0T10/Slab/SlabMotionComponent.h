#pragma once
#include "CoreMinimal.h"
#include "ActorComponent/MechDriverCompBase.h"
#include "SlabScenarioTypes.h"
#include "SlabMotionComponent.generated.h"

DECLARE_DELEGATE_FourParams(FSlabPoseAppliedCallback,const FSlabScenarioRow&,int32,double,bool);

/** Owns the playback clock, immutable inputs and physical pose application. */
UCLASS(ClassGroup=(Slab),meta=(BlueprintSpawnableComponent))
class MA0T10_DT_API USlabMotionComponent : public UMechDriverCompBase
{
	GENERATED_BODY()
public:
	USlabMotionComponent();
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool StartPlayback(FSlabScenarioDataPtr Data,const FTransform& Track,const FVector& SizeCm,ESlabInputUnit PositionUnit);
	void AdvancePlayback(double DeltaSeconds);
	bool SetPlaybackPaused(bool bPaused);
	void StopPlayback();
	bool IsPlaybackRunning() const { return bRunning; }
	bool IsPlaybackPaused() const { return bPaused; }
	double GetPlaybackTime() const { return Elapsed; }
	const FTransform& GetPlaybackTrack() const { return PlaybackTrack; }
	const FVector& GetSlabSizeCm() const { return SlabSizeCm; }
	ESlabInputUnit GetPlaybackPositionUnit() const { return PlaybackPositionUnit; }
	static FTransform BuildPose(const FSlabScenarioRow& Row,const FTransform& Track,const FVector& SizeCm,ESlabInputUnit PositionUnit);
	/** Runs only after SetActorTransform. The callback may stop playback safely. */
	FSlabPoseAppliedCallback OnPoseApplied;
private:
	bool ApplyCurrentPose();
	FSlabScenarioDataPtr Scenario;
	FTransform PlaybackTrack;
	FVector SlabSizeCm=FVector::OneVector;
	ESlabInputUnit PlaybackPositionUnit=ESlabInputUnit::Centimeters;
	double Elapsed=0;
	uint64 PlaybackGeneration=0;
	bool bRunning=false;
	bool bPaused=false;
};
