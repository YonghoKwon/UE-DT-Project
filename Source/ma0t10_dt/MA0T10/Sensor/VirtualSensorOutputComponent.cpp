#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"

#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

UVirtualSensorOutputComponent::UVirtualSensorOutputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualSensorOutputComponent::SetSharedServices(
	UVirtualSensorTransportComponent* InTransport,
	UVirtualSensorRecorderComponent* InRecorder)
{
	TransportComponent = InTransport;
	RecorderComponent = InRecorder;
}

bool UVirtualSensorOutputComponent::RouteFrame(const FVirtualSensorFrameEnvelope& Frame)
{
	if (!Frame.HasJsonPayload() || Frame.SensorId.IsEmpty())
	{
		return false;
	}

	if (LastRoutedSensorId == Frame.SensorId && LastRoutedFrameId == Frame.FrameId)
	{
		return false;
	}

	LastRoutedSensorId = Frame.SensorId;
	LastRoutedFrameId = Frame.FrameId;
	LastJsonPayload = *Frame.JsonPayload;

	const FString SensorType = BuildSensorType(Frame.SensorKind);
	if (TransportComponent && Frame.bSendTransport)
	{
		TransportComponent->SendJson(Frame.SensorId, SensorType, LastJsonPayload);
		if (Frame.BinaryPayload.IsValid() && Frame.BinaryPayload->Num() > 0)
		{
			TArray<uint8> BinaryBytes;
			BinaryBytes.Append(Frame.BinaryPayload->GetData(), static_cast<int32>(Frame.BinaryPayload->Num()));
			TransportComponent->SendBinary(Frame.SensorId, SensorType + TEXT("_frame"), TEXT("jpg"), BinaryBytes);
		}
	}
	if (RecorderComponent && Frame.bRecord)
	{
		RecorderComponent->RecordJsonFrame(Frame.SensorId, SensorType, Frame.FrameId, LastJsonPayload);
	}

	OnFrameRouted.Broadcast(Frame.SensorId, Frame.FrameId);
	return true;
}

FString UVirtualSensorOutputComponent::BuildSensorType(EVirtualSensorKind Kind) const
{
	return Kind == EVirtualSensorKind::Camera ? TEXT("virtual_camera") : TEXT("virtual_lidar");
}
