#pragma once

#include "CoreMinimal.h"
#include "TransactionCodeStruct.generated.h"

class UTransactionCodeMessage;

USTRUCT(BlueprintType)
struct FTransactionCodeStruct : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString TransactionCodeName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString TransactionCodeInfo;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UTransactionCodeMessage> TransactionCodeMessageClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowedClasses = "World"))
	TArray<TSoftClassPtr<UWorld>> UseLevels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Comment;
};
