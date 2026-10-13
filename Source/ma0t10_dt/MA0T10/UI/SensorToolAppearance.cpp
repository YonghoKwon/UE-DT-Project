#include "SensorToolAppearance.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "Kismet/GameplayStatics.h"
namespace { const FString AppearanceSlot = VirtualSensorDefaults::AppearanceSlot; }
float USensorToolAppearance::LoadScale()
{
	if (auto* Saved = Cast<USensorToolAppearance>(UGameplayStatics::LoadGameFromSlot(AppearanceSlot, 0)))
		return FMath::IsFinite(Saved->FontScale) ? FMath::Clamp(Saved->FontScale, 0.85f, 1.5f) : 1.0f;
	return 1.0f;
}
void USensorToolAppearance::SaveScale(float Scale)
{
	auto* Saved = NewObject<USensorToolAppearance>();
	Saved->FontScale = Scale;
	UGameplayStatics::SaveGameToSlot(Saved, AppearanceSlot, 0);
}
