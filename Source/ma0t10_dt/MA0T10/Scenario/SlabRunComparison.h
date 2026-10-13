#pragma once
#include "CoreMinimal.h"
#include "SlabRunDeliveryTypes.h"
#include "SlabRunComparison.generated.h"

USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabRunSensorComparison
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString SensorId;
    UPROPERTY(BlueprintReadOnly) EVirtualSensorKind Kind=EVirtualSensorKind::Camera;
    UPROPERTY(BlueprintReadOnly) bool bKindKnown=true;
    UPROPERTY(BlueprintReadOnly) bool bPresentA=false;
    UPROPERTY(BlueprintReadOnly) bool bPresentB=false;
    UPROPERTY(BlueprintReadOnly) bool bPoseDeltaAvailable=false;
    UPROPERTY(BlueprintReadOnly) double TranslationDeltaCm=0;
    UPROPERTY(BlueprintReadOnly) double RotationDeltaDegrees=0;
    UPROPERTY(BlueprintReadOnly) FVector ScaleDelta=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FSlabRunSensorSnapshot A;
    UPROPERTY(BlueprintReadOnly) FSlabRunSensorSnapshot B;
    UPROPERTY(BlueprintReadOnly) TArray<FString> ChangedSettings;
};
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabRunComparison
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FSlabRunDeliverySummary A;
    UPROPERTY(BlueprintReadOnly) FSlabRunDeliverySummary B;
    UPROPERTY(BlueprintReadOnly) TArray<FSlabRunSensorComparison> Sensors;
    UPROPERTY(BlueprintReadOnly) TArray<FString> ConditionDifferences;
    UPROPERTY(BlueprintReadOnly) TArray<FString> UnknownConditions;
    UPROPERTY(BlueprintReadOnly) TArray<FString> Warnings;
    UPROPERTY(BlueprintReadOnly) bool bRecordedConditionsMatch=false;
    UPROPERTY(BlueprintReadOnly) FString Status;
};
/** Read-only: no UObject, current sensor, file or transport access. */
MA0T10_DT_API bool BuildRunComparison(const FSlabRunDeliverySummary& A,const FSlabRunDeliverySummary& B,FSlabRunComparison& Out,FString& Reason);
MA0T10_DT_API FString DescribeRunComparison(const FSlabRunComparison& Comparison);
