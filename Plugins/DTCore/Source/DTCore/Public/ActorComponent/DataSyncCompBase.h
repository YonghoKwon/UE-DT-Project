#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DataSyncCompBase.generated.h"


struct FDxDataBase;
enum class EDxDataType : uint8;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DTCORE_API UDataSyncCompBase : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UDataSyncCompBase();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// Function
public:
	virtual void OnReceiveData(const TSharedPtr<FDxDataBase>& DataPtr);
private:
protected:

	// Variable
public:
private:
protected:
};
