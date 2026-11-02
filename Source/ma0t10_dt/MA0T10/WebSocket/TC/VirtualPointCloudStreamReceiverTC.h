#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Contracts/DeviceSensorStreamData.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorStreamReceiverTypes.h"
#include "VirtualPointCloudStreamReceiverTC.generated.h"



/** virtual-pointcloud.v1 Topic 본문을 검증하는 UKP형 수신 Handler입니다. */
UCLASS()
class MA0T10_DT_API UVirtualPointCloudStreamReceiverTC : public UTransactionCodeMessage
{
	GENERATED_BODY()

public:
	UVirtualPointCloudStreamReceiverTC();
	virtual TSharedPtr<FTransactionCodeDataBase> ParseToStruct(const FString& JsonString) const override;
	virtual void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data) override;
	static TSharedPtr<FTransactionCodeDataBase> ParseBinaryPcdToStruct(
		const TArray<uint8>& Body,
		const TMap<FName, FString>& Headers);

private:
	double LastOutputLogSeconds = -1.0;
};
