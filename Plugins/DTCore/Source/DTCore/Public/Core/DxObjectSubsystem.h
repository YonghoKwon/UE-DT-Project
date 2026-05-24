#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxObjectSubsystem.generated.h"

/**
 *
 */
UCLASS()
class DTCORE_API UDxObjectSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// Function
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	UFUNCTION(BlueprintCallable, Category = "DxObject")
	void RegisterObject(FName Category, const FString& Id, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "DxObject")
	void UnregisterObject(FName Category, const FString& Id);

	UFUNCTION(BlueprintCallable, Category = "DxObject")
	void ClearCategory(FName Category);

	UFUNCTION(BlueprintCallable, Category = "DxObject")
	AActor* FindObject(FName Category, const FString& Id) const;

	UFUNCTION(BlueprintCallable, Category = "DxObject")
	TArray<AActor*> GetAllObjects(FName Category) const;

	UFUNCTION(BlueprintCallable, Category = "DxObject")
	int32 GetObjectCount(FName Category) const;

	template<typename T>
	T* FindObjectAs(FName Category, const FString& Id) const
	{
		return Cast<T>(FindObject(Category, Id));
	}

	const TMap<FString, TObjectPtr<AActor>>* GetCategoryMap(FName Category) const;
private:
protected:

	// Variable
public:
private:
	TMap<FName, TMap<FString, TObjectPtr<AActor>>> RegisteredObjects;
protected:
};
