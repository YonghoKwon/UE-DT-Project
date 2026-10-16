#pragma once
#include "CoreMinimal.h"

/** Prepared on a worker from immutable scenario rows. No UObject access on workers. */
struct FVirtualDatasetPlannedFrame
{
    FTransform SlabPose;
    FVector SizeCm=FVector::OneVector;
    int64 FrameNo=0;
    double ElapsedSec=0;
    FString MtlNo;
};
struct FVirtualDatasetFramePlan
{
    FString Root,ScenarioId,RunId;
    TArray<FVirtualDatasetPlannedFrame> Frames;
    bool CaptureCamera=false,Pipeline=true;
};

/** Session-only settings. Never read from SaveGame or started by opening a panel. */
struct MA0T10_DT_API FVirtualDatasetRange
{
    bool Enabled=false;
    double Min=0,Max=0;
    bool IsValid() const { return FMath::IsFinite(Min)&&FMath::IsFinite(Max)&&Min<=Max; }
};
struct MA0T10_DT_API FVirtualDatasetSettings
{
    bool Enabled=false,CameraTruth=false,LidarFidelity=false,IncludeImu=false;
    FString Name=TEXT("dataset");
    // Explicit offline post-processing only. Never saved in SensorUI/Workspace or transport config.
    FString ExportFormat=TEXT("none");
    bool ExportUprightEnvelope=false;
    int32 Seed=1337,Scenes=100,SettleFrames=3;
    FVirtualDatasetRange Yaw{false,-10,10},Pitch{false,-3,3},Roll{false,-3,3};
    FVirtualDatasetRange X{false,-20,20},Y{false,-20,20},Z{false,-5,5}; // cm
    FVirtualDatasetRange Size{false,.9,1.1},Surface{false,0,4};
    FVirtualDatasetRange Roughness{false,.2,.9},Oxidation{false,0,1};
    FVirtualDatasetRange LightIntensity{false,1000,10000},LightPitch{false,-70,-20},LightYaw{false,-180,180};
    FVirtualDatasetRange SensorPosition{false,-1,1},SensorAngle{false,-.2,.2};
    bool Validate(FString& Error) const;
};
struct MA0T10_DT_API FVirtualDatasetSample
{
    FRotator SlabRotation=FRotator::ZeroRotator;
    FVector SlabOffset=FVector::ZeroVector;
    double Size=1,Roughness=0,Oxidation=0,Intensity=0;
    int32 Surface=0;
    FRotator LightRotation=FRotator::ZeroRotator;
    FVector LidarOffset=FVector::ZeroVector,CameraOffset=FVector::ZeroVector;
    FRotator LidarRotation=FRotator::ZeroRotator,CameraRotation=FRotator::ZeroRotator;
    static FVirtualDatasetSample Make(const FVirtualDatasetSettings& Settings,int32 Scene);
    FString ToJson() const;
};
