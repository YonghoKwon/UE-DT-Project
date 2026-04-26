#include "VirtualSensorAct.h"

#include "VirtualLidarSensorComp.h"

AVirtualSensorAct::AVirtualSensorAct()
{
    PrimaryActorTick.bCanEverTick = false;

    LidarSensorComp = CreateDefaultSubobject<UVirtualLidarSensorComp>(TEXT("LidarSensorComp"));
    // RootComponent = LidarSensorComp;
}
