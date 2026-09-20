#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlabTrackReferenceActor.generated.h"
class UStaticMeshComponent;

/** Local X is travel, Y is right. Rail values describe inner faces, not mesh centres. */
UCLASS()
class MA0T10_DT_API ASlabTrackReferenceActor : public AActor
{
	GENERATED_BODY()
public:
	ASlabTrackReferenceActor();
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Slab|Track") double LeftRailYcm=-900;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Slab|Track") double RightRailYcm=900;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Slab|Track") double RailLengthCm=16000;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Slab|Track") double RailHeightCm=150;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Slab|Track") bool bShowReference=true;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Slab|Track") bool bRailsConfigured=true;
	UFUNCTION(BlueprintPure,Category="Slab|Track") bool HasValidRails() const;
private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftRail;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RightRail;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> CenterLine;
};
