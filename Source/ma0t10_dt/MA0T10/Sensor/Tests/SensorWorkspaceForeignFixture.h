#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "SensorWorkspaceForeignFixture.generated.h"
/** Deliberately only inherits the shared base, like a colleague-owned chart. */
UCLASS(Transient)
class USensorWorkspaceForeignFixture : public UVirtualSensorPanelWidgetBase
{
	GENERATED_BODY()
};
