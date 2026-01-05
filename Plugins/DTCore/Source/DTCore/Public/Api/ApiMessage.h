#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ApiMessage.generated.h"

class FYyJsonParser;
/**
 *
 */
UCLASS()
class DTCORE_API UApiMessage : public UObject
{
	GENERATED_BODY()

	// Function
public:
	virtual TSharedPtr<struct FApiDataBase> ParseToStruct(const FString& JsonString)
	{
		return nullptr;
	}
	virtual void ProcessStructData(const TSharedPtr<FApiDataBase>& Data) {}
	virtual UWorld* GetWorld() const override;
private:
protected:

	// Variable
public:
	UPROPERTY(BlueprintReadOnly, Category = "Api")
	FString ResourceAndAction;
private:
protected:
};
