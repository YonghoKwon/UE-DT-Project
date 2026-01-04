#pragma once

#include "CoreMinimal.h"
#include "Manager/DxLevelManagerBase.h"
#include "DxLevelManagerTest.generated.h"

class UDxApiServiceTest;

UCLASS()
class M7AT10_DT_API ADxLevelManagerTest : public ADxLevelManagerBase
{
	GENERATED_BODY()

public:
	ADxLevelManagerTest();

	virtual void BeginPlay() override;

protected:
	virtual void SetupForLevel() override;
	virtual void CleanupForLevel() override;

public:
	virtual void Tick(float DeltaTime) override;

	// Function
public:
	UFUNCTION()
	void OnLevelInitApiFinished(bool bSuccess, const FString& Error);
private:
protected:

	// Variable
public:
	UPROPERTY()
	TObjectPtr<UDxApiServiceTest> DxApiService;
private:
protected:
};
