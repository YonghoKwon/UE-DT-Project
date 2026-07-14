#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VirtualSensorUiPreferences.generated.h"

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorPanelUiState
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "DigitalTwin|SensorUI")
    FVector2D NormalizedPosition = FVector2D::ZeroVector;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "DigitalTwin|SensorUI")
    bool bHasSavedPosition = false;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "DigitalTwin|SensorUI")
    bool bCollapsed = false;
};

UCLASS()
class MA0T10_DT_API UVirtualSensorUiPreferencesSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentVersion = 1;
    static const FString SlotName;
    static constexpr int32 UserIndex = 0;

    static UVirtualSensorUiPreferencesSaveGame* LoadOrCreate();
    static bool Save(UVirtualSensorUiPreferencesSaveGame* Preferences);
    static bool DeleteSavedPreferences();

    UPROPERTY(SaveGame)
    int32 Version = CurrentVersion;

    UPROPERTY(SaveGame)
    TMap<FName, FVirtualSensorPanelUiState> PanelStates;

    UPROPERTY(SaveGame)
    uint8 LidarViewMode = 2;

    UPROPERTY(SaveGame)
    bool bUseAdaptiveLidarDepthRange = true;

    UPROPERTY(SaveGame)
    bool bOverlayLidarMonitorGrid = true;

    UPROPERTY(SaveGame)
    bool bOverlayLidarDepthEdges = true;

    UPROPERTY(SaveGame)
    bool bMonitorDetailsExpanded = false;

    UPROPERTY(SaveGame)
    bool bSettingsKeyboardHelpExpanded = false;
};
