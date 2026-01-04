#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ApiStruct.generated.h"

class UApiMessage;

UENUM(BlueprintType)
enum class EApiType : uint8
{
	Test UMETA(DisplayName = "Test")
};

UENUM(BlueprintType)
enum class EApiMethod : uint8
{
	Get UMETA(DisplayName = "GET"),
	Post UMETA(DisplayName = "POST"),
	Put UMETA(DisplayName = "PUT"),
	Delete UMETA(DisplayName = "DELETE"),
	Patch UMETA(DisplayName = "PATCH")
};

USTRUCT(BlueprintType)
struct FApiStruct : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	EApiType ApiType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	EApiMethod ApiMethod;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	FString ApiUrl;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	FString ApiResource;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	FString ApiAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	TArray<FString> QueryParameter;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	TArray<FString> PathParameter;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	FString Body;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	TSubclassOf<UApiMessage> ApiMessageClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	TArray<FString> UseLevels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "API")
	FString Comment;
};
