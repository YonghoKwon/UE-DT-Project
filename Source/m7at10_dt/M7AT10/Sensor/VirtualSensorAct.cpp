#include "VirtualSensorAct.h"

#include "Components/SceneComponent.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "m7at10_dt/M7AT10/Sensor/SensorDataRelayComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarComp.h"

AVirtualSensorAct::AVirtualSensorAct()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SensorDataRelayComp = CreateDefaultSubobject<USensorDataRelayComp>(TEXT("SensorDataRelayComp"));

	VirtualCameraComp = CreateDefaultSubobject<UVirtualCameraComp>(TEXT("VirtualCameraComp"));
	VirtualCameraComp->SetupAttachment(SceneRoot);

	VirtualLidarComp = CreateDefaultSubobject<UVirtualLidarComp>(TEXT("VirtualLidarComp"));
	VirtualLidarComp->SetupAttachment(SceneRoot);
}
