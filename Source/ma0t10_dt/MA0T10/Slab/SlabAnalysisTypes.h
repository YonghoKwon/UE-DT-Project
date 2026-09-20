#pragma once
#include "CoreMinimal.h"
#include "SlabAnalysisTypes.generated.h"
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabAnalysisDisplaySettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bOutline=true;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bCross=true;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bReferencePose=true;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bCenterline=true;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bYaw=true;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bMargins=true;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bStatus=true;
};
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabSetupValidation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bCanSimulate=false;
	UPROPERTY(BlueprintReadOnly) bool bCanSendSelectedOutputs=false;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Errors;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Warnings;
};
