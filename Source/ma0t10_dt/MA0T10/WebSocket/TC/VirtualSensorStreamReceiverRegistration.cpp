#include "VirtualSensorStreamReceiverRegistration.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorStreamReceiverRegistry.h"
#include "VirtualCameraStreamReceiverTC.h"
#include "VirtualLidarStreamReceiverTC.h"
#include "VirtualPointCloudStreamReceiverTC.h"

namespace
{
	class FVirtualSensorTcReceiverCodec final : public IVirtualSensorStreamReceiverCodec
	{
	public:
		virtual TSharedPtr<FVirtualSensorTopicReceivedDataBase> CreateResult(EVirtualSensorTopicReceiveKind Kind) const override
		{
			if (Kind == EVirtualSensorTopicReceiveKind::PointCloud) return MakeShared<FVirtualPointCloudStreamReceiverData>();
			if (Kind == EVirtualSensorTopicReceiveKind::Camera) return MakeShared<FVirtualCameraStreamReceiverData>();
			return MakeShared<FVirtualLidarStreamReceiverData>();
		}

		virtual TSharedPtr<FVirtualSensorTopicReceivedDataBase> ParseBinaryPcd(
			const TArray<uint8>& Body, const TMap<FName, FString>& Headers) const override
		{
			return StaticCastSharedPtr<FVirtualSensorTopicReceivedDataBase>(UVirtualPointCloudStreamReceiverTC::ParseBinaryPcdToStruct(Body, Headers));
		}
	};
}

void RegisterVirtualSensorStreamReceiverCodec()
{
	VirtualSensorStreamReceiverRegistry::Register(MakeShared<FVirtualSensorTcReceiverCodec, ESPMode::ThreadSafe>());
}

void UnregisterVirtualSensorStreamReceiverCodec()
{
	VirtualSensorStreamReceiverRegistry::Unregister();
}
