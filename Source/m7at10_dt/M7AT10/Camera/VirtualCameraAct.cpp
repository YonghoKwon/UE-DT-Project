#include "VirtualCameraAct.h"

#include "M7AT10/Sensor/VirtualSensorPublisherComp.h"
#include "VirtualCameraComp.h"

AVirtualCameraAct::AVirtualCameraAct()
{
	PrimaryActorTick.bCanEverTick = false;

	VirtualCameraComp = CreateDefaultSubobject<UVirtualCameraComp>(TEXT("VirtualCameraComp"));
	RootComponent = VirtualCameraComp;

	SensorPublisherComp = CreateDefaultSubobject<UVirtualSensorPublisherComp>(TEXT("VirtualSensorPublisherComp"));
}
