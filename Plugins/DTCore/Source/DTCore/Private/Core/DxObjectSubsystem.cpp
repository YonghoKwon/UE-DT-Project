#include "Core/DxObjectSubsystem.h"

void UDxObjectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDxObjectSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxObjectSubsystem::Tick(float DeltaTime)
{
}

TStatId UDxObjectSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDxObjectSubsystem, STATGROUP_Tickables);
}

void UDxObjectSubsystem::RegisterObject(FName Category, const FString& Id, AActor* Actor)
{
	if (!IsValid(Actor) || Id.IsEmpty()) return;

	TMap<FString, TObjectPtr<AActor>>& CategoryMap = RegisteredObjects.FindOrAdd(Category);
	CategoryMap.Add(Id, Actor);
}

void UDxObjectSubsystem::UnregisterObject(FName Category, const FString& Id)
{
	if (TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category))
	{
		CategoryMap->Remove(Id);
	}
}

void UDxObjectSubsystem::ClearCategory(FName Category)
{
	if (TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category))
	{
		CategoryMap->Empty();
	}
}

AActor* UDxObjectSubsystem::FindObject(FName Category, const FString& Id) const
{
	const TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category);
	if (!CategoryMap) return nullptr;

	const TObjectPtr<AActor>* Found = CategoryMap->Find(Id);
	if (!Found) return nullptr;

	AActor* Actor = Found->Get();
	return IsValid(Actor) ? Actor : nullptr;
}

TArray<AActor*> UDxObjectSubsystem::GetAllObjects(FName Category) const
{
	TArray<AActor*> Result;
	const TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category);
	if (!CategoryMap) return Result;

	for (const auto& Pair : *CategoryMap)
	{
		if (AActor* Actor = Pair.Value.Get(); IsValid(Actor))
		{
			Result.Add(Actor);
		}
	}
	return Result;
}

int32 UDxObjectSubsystem::GetObjectCount(FName Category) const
{
	const TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category);
	return CategoryMap ? CategoryMap->Num() : 0;
}

const TMap<FString, TObjectPtr<AActor>>* UDxObjectSubsystem::GetCategoryMap(FName Category) const
{
	return RegisteredObjects.Find(Category);
}


