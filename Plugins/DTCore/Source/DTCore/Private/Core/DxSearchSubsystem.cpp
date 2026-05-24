// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/DxSearchSubsystem.h"

void UDxSearchSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("DxSearchSubsystem 초기화 완료"));
}

void UDxSearchSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxSearchSubsystem::PerformSearch(uint8 InCategoryValue, const FString& SearchText)
{
	if (SearchText.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("검색어가 비어있습니다."));
		OnSearchCompleted.Broadcast(false, TArray<FSearchResult>(), TEXT("검색어를 입력해주세요."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("검색 수행 - 카테고리: %d, 검색어: %s"),
		static_cast<int32>(InCategoryValue), *SearchText);

	TArray<FSearchResult> Results;
	SearchByCategory(InCategoryValue, SearchText, Results);

	OnSearchCompleted.Broadcast(true, Results, TEXT(""));
}
