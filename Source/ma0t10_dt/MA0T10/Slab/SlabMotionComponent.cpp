#include "SlabMotionComponent.h"
#include "SlabScenarioCodec.h"
#include "GameFramework/Actor.h"
USlabMotionComponent::USlabMotionComponent()
{ PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=false; PrimaryComponentTick.TickGroup=TG_PrePhysics; }
void USlabMotionComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
	AdvancePlayback(DeltaTime);
}
void USlabMotionComponent::EndPlay(const EEndPlayReason::Type Reason)
{ StopPlayback(); OnPoseApplied.Unbind(); Super::EndPlay(Reason); }
bool USlabMotionComponent::StartPlayback(FSlabScenarioDataPtr Data,const FTransform& Track,const FVector& SizeCm,ESlabInputUnit PositionUnit)
{
	if(bRunning||!Data.IsValid()||Data->Rows.IsEmpty()||!FMath::IsFinite(Data->DurationSec)||Data->DurationSec<=0||
		!GetOwner()||!OnPoseApplied.IsBound()||Track.ContainsNaN()||SizeCm.ContainsNaN()||SizeCm.GetMin()<=0) return false;
	Scenario=MoveTemp(Data); PlaybackTrack=Track; PlaybackTrack.SetScale3D(FVector::OneVector);
	SlabSizeCm=SizeCm; PlaybackPositionUnit=PositionUnit; Elapsed=0; bRunning=true; bPaused=false; ++PlaybackGeneration;
	const uint64 Generation=PlaybackGeneration;
	if(!ApplyCurrentPose()) { if(Generation==PlaybackGeneration) StopPlayback(); return false; }
	SetComponentTickEnabled(true); return true;
}
FTransform USlabMotionComponent::BuildPose(const FSlabScenarioRow& Row,const FTransform& Track,const FVector& SizeCm,ESlabInputUnit PositionUnit)
{
	return FTransform(Track.GetRotation()*FRotator(0,Row.LeftAngle,0).Quaternion(),
		Track.TransformPositionNoScale(FVector(FSlabScenarioCodec::ToCm(Row.CenterX,PositionUnit),0,SizeCm.Z/2)),FVector::OneVector);
}
bool USlabMotionComponent::ApplyCurrentPose()
{
	if(!bRunning||!Scenario.IsValid()||!GetOwner()) return false;
	// Hold a reference over callbacks, which are allowed to release or replace the active run.
	const auto Data=Scenario; const uint64 Generation=PlaybackGeneration;
	FSlabScenarioRow Row; int32 Index=0;
	if(!FSlabScenarioCodec::Sample(*Data,Elapsed,Row,Index)) return false;
	GetOwner()->SetActorTransform(BuildPose(Row,PlaybackTrack,SlabSizeCm,PlaybackPositionUnit),false,nullptr,ETeleportType::TeleportPhysics);
	const bool bEnd=Elapsed>=Data->DurationSec;
	OnPoseApplied.ExecuteIfBound(Row,Index,Elapsed,bEnd);
	if(Generation!=PlaybackGeneration||!bRunning) return false;
	if(bEnd) { StopPlayback(); return false; }
	return true;
}
void USlabMotionComponent::AdvancePlayback(double DeltaSeconds)
{
	if(!bRunning||bPaused||!Scenario.IsValid()||!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<0) return;
	const uint64 Generation=PlaybackGeneration;
	Elapsed=FMath::Min(Elapsed+DeltaSeconds,Scenario->DurationSec);
	if(!ApplyCurrentPose()&&bRunning&&Generation==PlaybackGeneration) StopPlayback();
}
bool USlabMotionComponent::SetPlaybackPaused(bool Paused)
{
	if(!bRunning) return false;
	bPaused=Paused; SetComponentTickEnabled(!bPaused); return true;
}
void USlabMotionComponent::StopPlayback()
{
	SetComponentTickEnabled(false); bRunning=false; bPaused=false; ++PlaybackGeneration; Scenario.Reset();
}
