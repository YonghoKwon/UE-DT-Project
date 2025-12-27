#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DxLevelManagerBase.generated.h"

enum class EDxViewMode : uint8;

UCLASS()
class M7AT10_DT_API ADxLevelManagerBase : public AActor
{
	GENERATED_BODY()

public:
	ADxLevelManagerBase();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupForLevel();
	virtual void CleanupForLevel();
public:
	virtual void Tick(float DeltaTime) override;

	// Function
public:
private:
protected:

	// Variable
public:
private:
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	EDxViewMode ViewMode;
};
