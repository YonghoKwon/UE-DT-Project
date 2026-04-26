#include "VirtualSensorAct.h"

#include "Sensor/VirtualLidarSensorComp.h"

AVirtualSensorAct::AVirtualSensorAct()
{
    PrimaryActorTick.bCanEverTick = false;

    LidarSensorComp = CreateDefaultSubobject<UVirtualLidarSensorComp>(TEXT("LidarSensorComp"));
    RootComponent = LidarSensorComp;
}
