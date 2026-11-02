#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DeviceSensorAssetSettings.generated.h"
class UMaterialInterface;
class UNiagaraSystem;
/** Explicit host soft paths. Defaults preserve existing project assets; no asset is moved or created. */
UCLASS(Config=Engine, DefaultConfig)
class MA0T10_DT_API UDeviceSensorAssetSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UDeviceSensorAssetSettings();
    UPROPERTY(EditAnywhere,Config,Category="Device assets") TSoftObjectPtr<UMaterialInterface> LidarSemanticMaterial;
    UPROPERTY(EditAnywhere,Config,Category="Device assets") TSoftObjectPtr<UNiagaraSystem> LidarPointCloudSystem;
};
