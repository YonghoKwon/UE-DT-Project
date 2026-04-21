#include "VirtualSensorAct.h"

#include "ActorComponent/VirtualDistanceSensorComp.h"

AVirtualSensorAct::AVirtualSensorAct()
{
	PrimaryActorTick.bCanEverTick = false;

	// 컴포넌트 생성 및 루트 연결
	DistanceSensorComp = CreateDefaultSubobject<UVirtualDistanceSensorComp>(TEXT("DistanceSensorComp"));
	RootComponent = DistanceSensorComp;
}
