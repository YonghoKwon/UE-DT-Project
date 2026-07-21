#pragma once

#include "CoreMinimal.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualSensorStreamReceiverTypes.h"
#include "VirtualLidarStreamReceiverTC.generated.h"

struct FVirtualLidarStreamReceiverData : FVirtualSensorTopicReceivedDataBase
{
	int32 HorizontalSamples = 0;
	int32 VerticalChannels = 0;
	int32 RayCount = 0;
	int32 TotalPointCount = 0;
	int32 HitPointCount = 0;
	int32 PayloadPointCount = 0;
};

/** virtual-lidar.v1 Topic 본문을 검증하는 UKP형 수신 Handler입니다. */
UCLASS()
class MA0T10_DT_API UVirtualLidarStreamReceiverTC : public UTransactionCodeMessage
{
	GENERATED_BODY()

public:
	UVirtualLidarStreamReceiverTC();
	virtual TSharedPtr<FTransactionCodeDataBase> ParseToStruct(const FString& JsonString) const override;
	virtual void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data) override;

private:
	double LastOutputLogSeconds = -1.0;
};
