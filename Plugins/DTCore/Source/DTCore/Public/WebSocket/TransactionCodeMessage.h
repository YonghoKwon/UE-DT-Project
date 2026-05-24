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
	// ParseToStruct 함수 내부에서는 this->변수나 GetWorld() 같은 언리얼 엔진 함수를 절대로 쓰지 않는다!!!
	// 순수 Parsing을 위한 함수
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
