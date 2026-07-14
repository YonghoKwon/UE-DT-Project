#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"

#include "Kismet/GameplayStatics.h"

const FString UVirtualSensorUiPreferencesSaveGame::SlotName = TEXT("MA0T10_VirtualSensorUI_v1");

UVirtualSensorUiPreferencesSaveGame* UVirtualSensorUiPreferencesSaveGame::LoadOrCreate()
{
    if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
    {
        if (UVirtualSensorUiPreferencesSaveGame* Loaded = Cast<UVirtualSensorUiPreferencesSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex)))
        {
            if (Loaded->Version == CurrentVersion)
            {
                return Loaded;
            }
        }
    }
    return Cast<UVirtualSensorUiPreferencesSaveGame>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
}

bool UVirtualSensorUiPreferencesSaveGame::Save(UVirtualSensorUiPreferencesSaveGame* Preferences)
{
    return Preferences && UGameplayStatics::SaveGameToSlot(Preferences, SlotName, UserIndex);
}

bool UVirtualSensorUiPreferencesSaveGame::DeleteSavedPreferences()
{
    return !UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex) ||
        UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
}
