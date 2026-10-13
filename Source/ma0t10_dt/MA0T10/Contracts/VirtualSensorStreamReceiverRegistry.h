#pragma once

#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorStreamReceiverTypes.h"

/** Stateless receiver codec supplied by the host TC layer. Calls may run on workers. */
class MA0T10_DT_API IVirtualSensorStreamReceiverCodec
{
public:
	virtual ~IVirtualSensorStreamReceiverCodec() = default;
	virtual TSharedPtr<FVirtualSensorTopicReceivedDataBase> CreateResult(EVirtualSensorTopicReceiveKind Kind) const = 0;
	virtual TSharedPtr<FVirtualSensorTopicReceivedDataBase> ParseBinaryPcd(
		const TArray<uint8>& Body, const TMap<FName, FString>& Headers) const = 0;
};

/** Module-owned registration; snapshots retain the stateless codec during worker calls. */
namespace VirtualSensorStreamReceiverRegistry
{
	MA0T10_DT_API void Register(const TSharedRef<const IVirtualSensorStreamReceiverCodec, ESPMode::ThreadSafe>& Codec);
	MA0T10_DT_API void Unregister();
	MA0T10_DT_API TSharedPtr<const IVirtualSensorStreamReceiverCodec, ESPMode::ThreadSafe> Get();
}
