#include "VirtualSensorAct.h"

#include "VirtualLidarComp.h"
#include "VirtualSensorPublisherComp.h"

AVirtualSensorAct::AVirtualSensorAct()
{
	PrimaryActorTick.bCanEverTick = false;

	LidarComp = CreateDefaultSubobject<UVirtualLidarComp>(TEXT("VirtualLidarComp"));
	RootComponent = LidarComp;

	SensorPublisherComp = CreateDefaultSubobject<UVirtualSensorPublisherComp>(TEXT("VirtualSensorPublisherComp"));
}
