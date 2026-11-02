#include "DeviceSensorAssetSettings.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
UDeviceSensorAssetSettings::UDeviceSensorAssetSettings()
{
    LidarSemanticMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId")));
    LidarPointCloudSystem=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/MA0T10/Sensor/VFX/NS_VirtualLidarPointCloud.NS_VirtualLidarPointCloud")));
}
