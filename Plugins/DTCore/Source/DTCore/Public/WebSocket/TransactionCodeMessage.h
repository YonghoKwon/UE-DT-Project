#pragma once

#include "CoreMinimal.h"
#include "Lib/yyjson.h"

#include "TransactionCodeMessage.generated.h"

class FYyJsonParser;
/**
 *
 */
UCLASS()
class DTCORE_API UTransactionCodeMessage : public UObject
{
	GENERATED_BODY()

	// Function
public:
	virtual TSharedPtr<struct FTransactionCodeDataBase> ParseToStruct(const FString& JsonString) const
	{
		return nullptr;
	}
	virtual void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data) {}
	virtual UWorld* GetWorld() const override;
private:
protected:

	// Variable
public:
	UPROPERTY(BlueprintReadOnly, Category = "TransactionCode")
	FString TransactionCode;
private:
protected:
};
