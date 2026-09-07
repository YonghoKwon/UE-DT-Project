#pragma once
#include "CoreMinimal.h"
struct FVirtualSensorTopicReceivedDataBase;
struct MA0T10_DT_API FVirtualSensorPcdReceiveContract
{
	static bool Normalize(const TMap<FName,FString>& Input,TMap<FName,FString>& Output,FVirtualSensorTopicReceivedDataBase& Data);
};
