#include "VirtualCameraAct.h"

#include "Sensor/SensorDataPublisherComp.h"
#include "VirtualCameraComp.h"

AVirtualCameraAct::AVirtualCameraAct()
{
	PrimaryActorTick.bCanEverTick = false;

	VirtualCameraComp = CreateDefaultSubobject<UVirtualCameraComp>(TEXT("VirtualCameraComp"));
	RootComponent = VirtualCameraComp;

	SensorPublisherComp = CreateDefaultSubobject<USensorDataPublisherComp>(TEXT("SensorPublisherComp"));
}

void AVirtualCameraAct::BeginPlay()
{
	Super::BeginPlay();
}

void AVirtualCameraAct::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
