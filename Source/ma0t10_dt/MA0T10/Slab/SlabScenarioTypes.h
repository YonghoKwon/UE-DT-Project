#pragma once

#include "CoreMinimal.h"
#include "SlabScenarioTypes.generated.h"

UENUM(BlueprintType)
enum class ESlabInputUnit : uint8 { Centimeters, Millimeters, Meters };

UENUM(BlueprintType)
enum class ESlabSimulationState : uint8 { Idle, Playing, Paused, Completed, Failed, Preparing };

/** Source values are retained verbatim in their declared input units. */
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabScenarioRow
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int64 FrameNo=0;
	UPROPERTY(BlueprintReadOnly) FString MtlNo;
	UPROPERTY(BlueprintReadOnly) double Thickness=0;
	UPROPERTY(BlueprintReadOnly) double Width=0;
	UPROPERTY(BlueprintReadOnly) double Length=0;
	UPROPERTY(BlueprintReadOnly) double Weight=0;
	UPROPERTY(BlueprintReadOnly) double LeftAngle=0;
	UPROPERTY(BlueprintReadOnly) double RightAngle=0;
	UPROPERTY(BlueprintReadOnly) double CenterX=0;
	UPROPERTY(BlueprintReadOnly) double CenterY=0;
	UPROPERTY(BlueprintReadOnly) double MovePosition=0;
	UPROPERTY(BlueprintReadOnly) double ElapsedSec=0;
	UPROPERTY(BlueprintReadOnly) bool bMeandering=false;
};

/** Immutable after parsing; shared by playback, replay archive and charts. */
struct MA0T10_DT_API FSlabScenarioData
{
	FString OriginalJson;
	FString ScenarioUUID;
	FString SourceUUID;
	FString Name;
	FString CreateTimestamp;
	bool bGeneratedArchiveId=false;
	bool bDimensionsChanged=false;
	TArray<FSlabScenarioRow> Rows;
	double SamplePeriodSec=0.05;
	double DurationSec=0;
};
using FSlabScenarioDataPtr = TSharedPtr<const FSlabScenarioData,ESPMode::ThreadSafe>;

USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabMetrics
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) double ElapsedSec=0;
	UPROPERTY(BlueprintReadOnly) int64 FrameNo=0;
	UPROPERTY(BlueprintReadOnly) double LeftAngle=0;
	UPROPERTY(BlueprintReadOnly) double RightAngle=0;
	UPROPERTY(BlueprintReadOnly) double CenterOffsetCm=0;
	UPROPERTY(BlueprintReadOnly) double MaxCornerOffsetCm=0;
	UPROPERTY(BlueprintReadOnly) double MarginLeftCm=0;
	UPROPERTY(BlueprintReadOnly) double MarginRightCm=0;
	UPROPERTY(BlueprintReadOnly) double SpeedCmPerSec=0;
	UPROPERTY(BlueprintReadOnly) bool bMarginsValid=false;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabSimulationStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ESlabSimulationState State=ESlabSimulationState::Idle;
	UPROPERTY(BlueprintReadOnly) FString ScenarioUUID;
	UPROPERTY(BlueprintReadOnly) FString RunUUID;
	UPROPERTY(BlueprintReadOnly) FString MtlNo;
	UPROPERTY(BlueprintReadOnly) int64 FrameNo=0;
	UPROPERTY(BlueprintReadOnly) int32 RowIndex=0;
	UPROPERTY(BlueprintReadOnly) int32 RowCount=0;
	UPROPERTY(BlueprintReadOnly) double ElapsedSec=0;
	UPROPERTY(BlueprintReadOnly) double DurationSec=0;
	UPROPERTY(BlueprintReadOnly) float Progress=0;
	UPROPERTY(BlueprintReadOnly) FString Message;
	UPROPERTY(BlueprintReadOnly) bool bReplay=false;
	/** Persists across pose/pause updates; distinct from movement and delivery state. */
	UPROPERTY(BlueprintReadOnly) FString TransmissionWarning;
};
