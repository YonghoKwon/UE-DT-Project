#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"

#include "Kismet/GameplayStatics.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

const FString UVirtualSensorUiPreferencesSaveGame::SlotName = TEXT("MA0T10_VirtualSensorUI_v6");
const FString UVirtualSensorUiPreferencesSaveGame::Version5SlotName = TEXT("MA0T10_VirtualSensorUI_v5");
const FString UVirtualSensorUiPreferencesSaveGame::Version4SlotName = TEXT("MA0T10_VirtualSensorUI_v4");
const FString UVirtualSensorUiPreferencesSaveGame::Version3SlotName = TEXT("MA0T10_VirtualSensorUI_v3");
const FString UVirtualSensorUiPreferencesSaveGame::Version2SlotName = TEXT("MA0T10_VirtualSensorUI_v2");
const FString UVirtualSensorUiPreferencesSaveGame::LegacySlotName = TEXT("MA0T10_VirtualSensorUI_v1");

UVirtualSensorUiPreferencesSaveGame* UVirtualSensorUiPreferencesSaveGame::LoadOrCreate()
{
    const auto LoadSlot = [](const FString& Name) -> UVirtualSensorUiPreferencesSaveGame*
    {
        return UGameplayStatics::DoesSaveGameExist(Name, UserIndex)
            ? Cast<UVirtualSensorUiPreferencesSaveGame>(UGameplayStatics::LoadGameFromSlot(Name, UserIndex))
            : nullptr;
    };

    if (UVirtualSensorUiPreferencesSaveGame* Loaded = LoadSlot(SlotName))
    {
        if (Loaded->Version == CurrentVersion) return Loaded;
    }

    if (UVirtualSensorUiPreferencesSaveGame* Version5 = LoadSlot(Version5SlotName))
    {
        if (Version5->Version == 5)
        {
            Version5->SelectedPointCloudStreamFormat = static_cast<uint8>(EVirtualPointCloudStreamFormat::CSV);
			Version5->LocalCaptureIntervalSeconds = 1.0f;
			Version5->bLocalCaptureUseSensorInterval = false;
			Version5->bLocalCaptureCameraImage = true;
			Version5->bLocalCaptureCameraPayload = false;
			Version5->bLocalCaptureLidarPayload = true;
			Version5->bLocalCapturePointCloud = true;
			Version5->LocalCapturePointCloudFormat = 1;
            Version5->Version = CurrentVersion;
            Save(Version5);
            return Version5;
        }
    }

    if (UVirtualSensorUiPreferencesSaveGame* Version4 = LoadSlot(Version4SlotName))
    {
        if (Version4->Version == 4)
        {
            Version4->CaptureExportActiveTab = 0;
            Version4->SensorStreamFrameStride = 1;
            Version4->SensorStreamReceiptInterval = 10;
            Version4->Version = CurrentVersion;
            Save(Version4);
            return Version4;
        }
    }

    if (UVirtualSensorUiPreferencesSaveGame* Version3 = LoadSlot(Version3SlotName))
    {
        if (Version3->Version == 3)
        {
            Version3->bWorldTopDownAutoFit = true;
            Version3->Version = CurrentVersion;
            Save(Version3);
            return Version3;
        }
    }

    if (UVirtualSensorUiPreferencesSaveGame* Version2 = LoadSlot(Version2SlotName))
    {
        if (Version2->Version == 2)
        {
            Version2->Version = CurrentVersion;
            Save(Version2);
            return Version2;
        }
    }

    if (UVirtualSensorUiPreferencesSaveGame* Legacy = LoadSlot(LegacySlotName))
    {
        if (Legacy->Version == 1)
        {
            Legacy->LidarProjectionMode = ELidarMonitorProjectionMode::RangeImage;
            Legacy->LidarColorMode = UVirtualLidarVisualizationComponent::MapLegacyViewMode(
                static_cast<EVirtualLidarViewMode>(FMath::Clamp<int32>(Legacy->LidarViewMode, 0, 3)));
            Legacy->bShowWorldLidarPointCloud = true;
            Legacy->LidarPointSize = 2.0f;
            Legacy->Version = CurrentVersion;
            Save(Legacy);
            return Legacy;
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
    const bool bDeletedCurrent = !UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex) ||
        UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
    const bool bDeletedVersion5 = !UGameplayStatics::DoesSaveGameExist(Version5SlotName, UserIndex) ||
        UGameplayStatics::DeleteGameInSlot(Version5SlotName, UserIndex);
    const bool bDeletedVersion4 = !UGameplayStatics::DoesSaveGameExist(Version4SlotName, UserIndex) ||
        UGameplayStatics::DeleteGameInSlot(Version4SlotName, UserIndex);
    const bool bDeletedVersion3 = !UGameplayStatics::DoesSaveGameExist(Version3SlotName, UserIndex) ||
        UGameplayStatics::DeleteGameInSlot(Version3SlotName, UserIndex);
    const bool bDeletedVersion2 = !UGameplayStatics::DoesSaveGameExist(Version2SlotName, UserIndex) ||
        UGameplayStatics::DeleteGameInSlot(Version2SlotName, UserIndex);
    const bool bDeletedLegacy = !UGameplayStatics::DoesSaveGameExist(LegacySlotName, UserIndex) ||
        UGameplayStatics::DeleteGameInSlot(LegacySlotName, UserIndex);
    return bDeletedCurrent && bDeletedVersion5 && bDeletedVersion4 && bDeletedVersion3 && bDeletedVersion2 && bDeletedLegacy;
}
