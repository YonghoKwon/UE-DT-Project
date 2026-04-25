#include "VirtualSensorAct.h"

#include "SensorDataPublisherComp.h"
#include "VirtualDistanceSensorComp.h"

AVirtualSensorAct::AVirtualSensorAct()
{
	PrimaryActorTick.bCanEverTick = false;

	// 센서 컴포넌트 생성 및 루트 연결
	DistanceSensorComp = CreateDefaultSubobject<UVirtualDistanceSensorComp>(TEXT("DistanceSensorComp"));
	RootComponent = DistanceSensorComp;

	// 데이터 전송용 컴포넌트
	SensorPublisherComp = CreateDefaultSubobject<USensorDataPublisherComp>(TEXT("SensorPublisherComp"));
}
