#include "Core/DxSearchSubsystem.h"
#include "DTCore.h"

void UDxSearchSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DX_LOG(GetWorld(), TEXT("DxSearchSubsystem initialized."));
}

void UDxSearchSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxSearchSubsystem::PerformSearch(uint8 InCategoryValue, const FString& SearchText)
{
	const FString TrimmedSearchText = SearchText.TrimStartAndEnd();

	if (TrimmedSearchText.IsEmpty())
	{
		DX_LOG(GetWorld(), TEXT("[DxSearchSubsystem] Search text is empty."));
		OnSearchCompleted.Broadcast(false, TArray<FSearchResult>(), TEXT("Search text is empty."));
		return;
	}

	TArray<FSearchResult> Results;
	SearchByCategory(InCategoryValue, TrimmedSearchText, Results);

	OnSearchCompleted.Broadcast(true, Results, TEXT(""));
}