#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SensorToolAppearance.generated.h"

UCLASS()
class MA0T10_DT_API USensorToolAppearance : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) float FontScale = 1.0f;
	static float LoadScale();
	static void SaveScale(float Scale);
};
