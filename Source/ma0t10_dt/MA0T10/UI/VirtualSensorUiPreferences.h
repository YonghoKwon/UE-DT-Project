#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
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

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "DigitalTwin|SensorUI")
    FVector2D ExpandedSize = FVector2D::ZeroVector;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "DigitalTwin|SensorUI")
    bool bHasSavedSize = false;
};

UCLASS()
class MA0T10_DT_API UVirtualSensorUiPreferencesSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentVersion = 3;
    static const FString SlotName;
    static const FString Version2SlotName;
    static const FString LegacySlotName;
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
    ELidarMonitorProjectionMode LidarProjectionMode = ELidarMonitorProjectionMode::RangeImage;

    UPROPERTY(SaveGame)
    ELidarColorMode LidarColorMode = ELidarColorMode::DistanceTurbo;

    UPROPERTY(SaveGame)
    bool bShowWorldLidarPointCloud = true;

    UPROPERTY(SaveGame)
    float LidarPointSize = 2.0f;

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

	UPROPERTY(SaveGame)
    uint8 SelectedPointCloudExportKind = 1;

	UPROPERTY(SaveGame) FString StompBrokerUrl = TEXT("ws://127.0.0.1:61616");
	UPROPERTY(SaveGame) FString StompCameraTopic = TEXT("topic.virtual.sensor.camera.0");
	UPROPERTY(SaveGame) FString StompLidarTopic = TEXT("topic.virtual.sensor.lidar.0");
	UPROPERTY(SaveGame) FString StompExportTopic = TEXT("topic.virtual.sensor.export.0");
	UPROPERTY(SaveGame) FString StompUserName = TEXT("artemis");
	UPROPERTY(SaveGame) FString StompAckTopic;
	UPROPERTY(SaveGame) int32 OutboundMaxMessageBytes = 8388608;
	UPROPERTY(SaveGame) FString OutboundHttpEndpoint;
};
