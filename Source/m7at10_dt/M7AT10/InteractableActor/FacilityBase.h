#pragma once

#include "CoreMinimal.h"
#include "InteractableActor.h"
#include "FacilityBase.generated.h"

UCLASS()
class DTCORE_API AFacilityBase : public AInteractableActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AFacilityBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
