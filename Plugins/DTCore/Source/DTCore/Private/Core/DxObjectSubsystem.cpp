#include "Core/DxObjectSubsystem.h"
#include "DTCore.h"

void UDxObjectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDxObjectSubsystem::Deinitialize()
{
	RegisteredObjects.Empty();

	Super::Deinitialize();
}

void UDxObjectSubsystem::RegisterObject(FName Category, const FString& Id, AActor* Actor)
{
	if (Category.IsNone())
	{
		DX_LOG(GetWorld(), TEXT("[DxObjectSubsystem] RegisterObject failed. Category is none. Id=%s"), *Id);
		return;
	}

	if (Id.IsEmpty())
	{
		DX_LOG(GetWorld(), TEXT("[DxObjectSubsystem] RegisterObject failed. Id is empty. Category=%s"), *Category.ToString());
		return;
	}

	if (!IsValid(Actor))
	{
		DX_LOG(GetWorld(), TEXT("[DxObjectSubsystem] RegisterObject failed. Actor is invalid. Category=%s, Id=%s"),
			*Category.ToString(), *Id);
		return;
	}

	TMap<FString, TObjectPtr<AActor>>& CategoryMap = RegisteredObjects.FindOrAdd(Category);

	if (CategoryMap.Contains(Id))
	{
		DX_LOG(GetWorld(), TEXT("[DxObjectSubsystem] Object already registered. Overwriting. Category=%s, Id=%s"),
			*Category.ToString(), *Id);
	}

	CategoryMap.Add(Id, Actor);
}

void UDxObjectSubsystem::UnregisterObject(FName Category, const FString& Id)
{
	if (TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category))
	{
		CategoryMap->Remove(Id);

		if (CategoryMap->Num() == 0)
		{
			RegisteredObjects.Remove(Category);
		}
	}
}

void UDxObjectSubsystem::ClearCategory(FName Category)
{
	RegisteredObjects.Remove(Category);
}

void UDxObjectSubsystem::ClearAllObjects()
{
	RegisteredObjects.Empty();
}

void UDxObjectSubsystem::CompactInvalidObjects(FName Category)
{
	TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category);
	if (!CategoryMap)
	{
		return;
	}

	TArray<FString> InvalidIds;
	for (const auto& Pair : *CategoryMap)
	{
		if (!IsValid(Pair.Value.Get()))
		{
			InvalidIds.Add(Pair.Key);
		}
	}

	for (const FString& InvalidId : InvalidIds)
	{
		CategoryMap->Remove(InvalidId);
	}

	if (CategoryMap->Num() == 0)
	{
		RegisteredObjects.Remove(Category);
	}
}

void UDxObjectSubsystem::CompactAllInvalidObjects()
{
	TArray<FName> Categories;
	RegisteredObjects.GetKeys(Categories);

	for (const FName& Category : Categories)
	{
		CompactInvalidObjects(Category);
	}
}

AActor* UDxObjectSubsystem::FindObject(FName Category, const FString& Id) const
{
	const TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category);
	if (!CategoryMap)
	{
		return nullptr;
	}

	const TObjectPtr<AActor>* Found = CategoryMap->Find(Id);
	if (!Found)
	{
		return nullptr;
	}

	AActor* Actor = Found->Get();
	return IsValid(Actor) ? Actor : nullptr;
}

TArray<AActor*> UDxObjectSubsystem::GetAllObjects(FName Category) const
{
	TArray<AActor*> Result;

	const TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category);
	if (!CategoryMap)
	{
		return Result;
	}

	for (const auto& Pair : *CategoryMap)
	{
		AActor* Actor = Pair.Value.Get();
		if (IsValid(Actor))
		{
			Result.Add(Actor);
		}
	}

	return Result;
}

int32 UDxObjectSubsystem::GetObjectCount(FName Category) const
{
	return GetValidObjectCount(Category);
}

int32 UDxObjectSubsystem::GetRegisteredObjectCount(FName Category) const
{
	const TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category);
	return CategoryMap ? CategoryMap->Num() : 0;
}

int32 UDxObjectSubsystem::GetValidObjectCount(FName Category) const
{
	const TMap<FString, TObjectPtr<AActor>>* CategoryMap = RegisteredObjects.Find(Category);
	if (!CategoryMap)
	{
		return 0;
	}

	int32 Count = 0;
	for (const auto& Pair : *CategoryMap)
	{
		if (IsValid(Pair.Value.Get()))
		{
			++Count;
		}
	}

	return Count;
}

const TMap<FString, TObjectPtr<AActor>>* UDxObjectSubsystem::GetCategoryMap(FName Category) const
{
	return RegisteredObjects.Find(Category);
}