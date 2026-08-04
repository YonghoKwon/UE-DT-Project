#pragma once

#include "CoreMinimal.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualSensorStreamReceiverTypes.h"
#include "VirtualPointCloudStreamReceiverTC.generated.h"

struct FVirtualPointCloudStreamReceiverData : FVirtualSensorTopicReceivedDataBase
{
	FString Format;
	FString Encoding;
	int32 PointCount = 0;
	int32 DeclaredByteCount = 0;
	int32 DecodedByteCount = 0;
	int32 SourcePointCount = 0;
	int32 FilterRevision = 0;
	FString ChecksumSha1;
	FString ProfileKey;
};

/** virtual-pointcloud.v1 Topic 본문을 검증하는 UKP형 수신 Handler입니다. */
UCLASS()
class MA0T10_DT_API UVirtualPointCloudStreamReceiverTC : public UTransactionCodeMessage
{
	GENERATED_BODY()

public:
	UVirtualPointCloudStreamReceiverTC();
	virtual TSharedPtr<FTransactionCodeDataBase> ParseToStruct(const FString& JsonString) const override;
	virtual void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data) override;
	TSharedPtr<FTransactionCodeDataBase> ParseBinaryPcdToStruct(
		const TArray<uint8>& Body,
		const TMap<FName, FString>& Headers) const;

private:
	double LastOutputLogSeconds = -1.0;
};
