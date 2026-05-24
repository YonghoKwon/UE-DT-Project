// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxSearchSubsystem.generated.h"

USTRUCT(BlueprintType)
struct DTCORE_API FSearchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Id;

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	uint8 CategoryValue = 0;

	UPROPERTY(BlueprintReadOnly)
	FString AdditionalInfo;
};

// 검색 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSearchCompleted,
	bool, bSuccess,
	const TArray<FSearchResult>&, Results,
	const FString&, ErrorMessasge
	);

/**
 *
 */
UCLASS(Abstract)
class DTCORE_API UDxSearchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Search")
	void PerformSearch(uint8 InCategoryValue, const FString& SearchText);

	UPROPERTY(BlueprintAssignable, Category = "Search")
	FOnSearchCompleted OnSearchCompleted;

protected:
	virtual void SearchByCategory(uint8 InCategoryValue, const FString& SearchText, TArray<FSearchResult>& OutResults) PURE_VIRTUAL(UDxSearchSubsystem::SearchByCategory, );

protected:
};
