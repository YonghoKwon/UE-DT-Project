#include "VirtualSensorAct.h"

#include "Components/ArrowComponent.h"
#include "VirtualLidarSensorComp.h"

AVirtualSensorAct::AVirtualSensorAct()
{
    PrimaryActorTick.bCanEverTick = false;

    LidarSensorComp = CreateDefaultSubobject<UVirtualLidarSensorComp>(TEXT("LidarSensorComp"));
    RootComponent = LidarSensorComp;

    EditorForwardArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("EditorForwardArrowComp"));
    EditorForwardArrowComp->SetupAttachment(RootComponent);
    EditorForwardArrowComp->ArrowSize = 2.0f;
    EditorForwardArrowComp->ArrowLength = 250.0f;
    EditorForwardArrowComp->bIsScreenSizeScaled = true;
}
