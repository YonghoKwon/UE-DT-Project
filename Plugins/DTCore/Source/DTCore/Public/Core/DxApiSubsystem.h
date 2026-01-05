#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Api/ApiStruct.h"
#include "DxApiSubsystem.generated.h"

class FHttpModule;

DECLARE_DYNAMIC_DELEGATE_ThreeParams(FDxApiCallback, bool, bSuccess, int32, ResponseCode, const FString&, Content);

/**
 *
 */
UCLASS()
class DTCORE_API UDxApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// Function
public:
	UDxApiSubsystem();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "DxApi")
	void DxHttpCall(const FString& FullUrl, const FString& Verb, const FString& ContentString, const TMap<FString, FString>& Headers, FDxApiCallback Callback);
	UFUNCTION(BlueprintCallable, Category = "DxApi")
	void DxRequestApi(const FName& RowName, FDxApiCallback Callback);
	UFUNCTION(BlueprintCallable, Category = "DxApi")
	void DxRequestApiWithParameter(const FName& RowName, FDxApiCallback Callback, const TArray<FString>& Parameters);
private:
	void InternalOnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FDxApiCallback Callback);
	FString GetServerUrl(EApiType ApiType);
	FString GetHttpStr(EApiMethod ApiMethod);
protected:

	// Variable
public:
	FHttpModule* HttpModule;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DxApi")
	UDataTable* DT_Api;
private:
protected:
};
