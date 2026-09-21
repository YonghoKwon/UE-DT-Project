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
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="2",ClampMax="12")) float LineWidthPixels=4;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="18",ClampMax="56")) float TextHeightPixels=28;
	void Sanitize(){LineWidthPixels=FMath::IsFinite(LineWidthPixels)?FMath::Clamp(LineWidthPixels,2.f,12.f):4.f;TextHeightPixels=FMath::IsFinite(TextHeightPixels)?FMath::Clamp(TextHeightPixels,18.f,56.f):28.f;}
	bool HasAny() const{return bOutline||bCross||bReferencePose||bCenterline||bYaw||bMargins||bStatus;}
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
