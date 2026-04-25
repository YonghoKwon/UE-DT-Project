#include "VirtualLidarComp.h"

#include "DrawDebugHelpers.h"
#include "M7AT10/Sensor/VirtualSensorPublisherComp.h"
#include "m7at10_dt/m7at10_dt.h"

UVirtualLidarComp::UVirtualLidarComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualLidarComp::BeginPlay()
{
	Super::BeginPlay();

	if (ScanInterval > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ScanTimerHandle, this, &UVirtualLidarComp::ScanAndSend, ScanInterval, true);
	}
}

void UVirtualLidarComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UVirtualLidarComp::ScanAndSend()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 SafeHorizontalSamples = FMath::Clamp(HorizontalSamples, 1, 2048);
	const int32 SafeVerticalChannels = FMath::Clamp(VerticalChannels, 1, 128);
	const FVector Start = GetComponentLocation();
	const FRotator BaseRotation = GetComponentRotation();

	TArray<FVirtualLidarPoint> Points;
	Points.Reserve(SafeHorizontalSamples * SafeVerticalChannels);

	for (int32 ChannelIdx = 0; ChannelIdx < SafeVerticalChannels; ++ChannelIdx)
	{
		const float VerticalAlpha = SafeVerticalChannels == 1 ? 0.5f : static_cast<float>(ChannelIdx) / static_cast<float>(SafeVerticalChannels - 1);
		const float PitchOffset = FMath::Lerp(-VerticalFov * 0.5f, VerticalFov * 0.5f, VerticalAlpha);

		for (int32 SampleIdx = 0; SampleIdx < SafeHorizontalSamples; ++SampleIdx)
		{
			const float HorizontalAlpha = SafeHorizontalSamples == 1 ? 0.0f : static_cast<float>(SampleIdx) / static_cast<float>(SafeHorizontalSamples);
			const float YawOffset = FMath::Lerp(-HorizontalFov * 0.5f, HorizontalFov * 0.5f, HorizontalAlpha);

			const FRotator RayRotation = BaseRotation + FRotator(PitchOffset, YawOffset, 0.0f);
			const FVector End = Start + RayRotation.Vector() * MaxDistanceCm;

			FHitResult Hit;
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VirtualLidarScan), false, GetOwner());
			const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, QueryParams);

			FVirtualLidarPoint Point;
			Point.bHit = bHit;
			Point.Location = bHit ? Hit.ImpactPoint : End;
			Point.Distance = FVector::Distance(Start, Point.Location);
			Points.Add(Point);

			if (bDrawDebug)
			{
				DrawDebugLine(World, Start, Point.Location, bHit ? FColor::Green : FColor::Red, false, ScanInterval, 0, 0.5f);
				if (bHit)
				{
					DrawDebugPoint(World, Hit.ImpactPoint, 4.0f, FColor::Yellow, false, ScanInterval);
				}
			}
		}
	}

	const FVirtualSensorPacket Packet = BuildLidarPacket(Points);
	OnLidarPacketReady.Broadcast(Packet, Points);

	if (bPublishToHttpAutomatically)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			if (UVirtualSensorPublisherComp* Publisher = OwnerActor->FindComponentByClass<UVirtualSensorPublisherComp>())
			{
				Publisher->PublishSensorPacket(Packet);
			}
		}
	}

	UE_LOG(LogM7AT10, Verbose, TEXT("[VirtualLidarComp] Scan done. Points=%d"), Points.Num());
}

FVirtualSensorPacket UVirtualLidarComp::BuildLidarPacket(const TArray<FVirtualLidarPoint>& Points) const
{
	const int64 TimestampMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000;

	FString PointsJson;
	PointsJson.Reserve(Points.Num() * 48);
	PointsJson += TEXT("[");
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVirtualLidarPoint& Point = Points[Index];
		PointsJson += FString::Printf(
			TEXT("{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"distance\":%.2f,\"hit\":%s}"),
			Point.Location.X,
			Point.Location.Y,
			Point.Location.Z,
			Point.Distance,
			Point.bHit ? TEXT("true") : TEXT("false"));

		if (Index + 1 < Points.Num())
		{
			PointsJson += TEXT(",");
		}
	}
	PointsJson += TEXT("]");

	const FString SensorName = GetName();
	const FString PayloadJson = FString::Printf(
		TEXT("{\"sensorType\":\"lidar\",\"sensorName\":\"%s\",\"timestampUnixMs\":%lld,\"horizontalSamples\":%d,\"verticalChannels\":%d,\"maxDistanceCm\":%.2f,\"points\":%s}"),
		*SensorName,
		TimestampMs,
		HorizontalSamples,
		VerticalChannels,
		MaxDistanceCm,
		*PointsJson);

	FVirtualSensorPacket Packet;
	Packet.SensorType = TEXT("lidar");
	Packet.SensorName = SensorName;
	Packet.PayloadJson = PayloadJson;
	Packet.TimestampUnixMs = TimestampMs;
	return Packet;
}
