#include "VirtualDistanceSensorComp.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "SensorDataPublisherComp.h"
#include "TimerManager.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

UVirtualDistanceSensorComp::UVirtualDistanceSensorComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualDistanceSensorComp::BeginPlay()
{
	Super::BeginPlay();

	if (ScanInterval > 0.0f && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(ScanTimerHandle, this, &UVirtualDistanceSensorComp::PerformScan, ScanInterval, true);
	}
}

void UVirtualDistanceSensorComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UVirtualDistanceSensorComp::PerformScan()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector SensorLocation = GetComponentLocation();
	const FRotator SensorRotation = GetComponentRotation();

	TArray<TSharedPtr<FJsonValue>> PointsJson;
	PointsJson.Reserve(HorizontalSamples * VerticalSamples);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VirtualLidarScan), true);
	if (bIgnoreOwner && GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	const float HorizontalHalf = HorizontalFovDeg * 0.5f;
	const float VerticalHalf = VerticalFovDeg * 0.5f;
	const int32 SafeHorizontalSamples = FMath::Max(1, HorizontalSamples);
	const int32 SafeVerticalSamples = FMath::Max(1, VerticalSamples);

	for (int32 V = 0; V < SafeVerticalSamples; ++V)
	{
		const float VerticalAlpha = (SafeVerticalSamples == 1) ? 0.5f : static_cast<float>(V) / static_cast<float>(SafeVerticalSamples - 1);
		const float PitchDeg = FMath::Lerp(-VerticalHalf, VerticalHalf, VerticalAlpha);

		for (int32 H = 0; H < SafeHorizontalSamples; ++H)
		{
			const float HorizontalAlpha = (SafeHorizontalSamples == 1) ? 0.5f : static_cast<float>(H) / static_cast<float>(SafeHorizontalSamples - 1);
			const float YawDeg = FMath::Lerp(-HorizontalHalf, HorizontalHalf, HorizontalAlpha);

			const FRotator RayRot = SensorRotation + FRotator(PitchDeg, YawDeg, 0.0f);
			const FVector End = SensorLocation + (RayRot.Vector() * MaxDistanceCm);

			FHitResult HitResult;
			const bool bHit = World->LineTraceSingleByChannel(HitResult, SensorLocation, End, CollisionChannel, QueryParams);
			const FVector Point = bHit ? HitResult.ImpactPoint : End;
			const float Distance = FVector::Dist(SensorLocation, Point);

			TSharedPtr<FJsonObject> PointObj = MakeShared<FJsonObject>();
			PointObj->SetNumberField(TEXT("x"), Point.X);
			PointObj->SetNumberField(TEXT("y"), Point.Y);
			PointObj->SetNumberField(TEXT("z"), Point.Z);
			PointObj->SetNumberField(TEXT("distance_cm"), Distance);
			PointObj->SetBoolField(TEXT("hit"), bHit);
			PointsJson.Add(MakeShared<FJsonValueObject>(PointObj));

			if (bDrawDebugPoints)
			{
				DrawDebugPoint(World, Point, 4.0f, bHit ? FColor::Green : FColor::Red, false, DebugDuration);
			}
		}
	}

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("sensor_type"), TEXT("lidar"));
	Payload->SetStringField(TEXT("sensor_name"), SensorName);
	Payload->SetStringField(TEXT("timestamp_utc"), FDateTime::UtcNow().ToIso8601());
	Payload->SetNumberField(TEXT("max_distance_cm"), MaxDistanceCm);
	Payload->SetArrayField(TEXT("points"), PointsJson);

	FString JsonPayload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
	FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);

	OnScanReady.Broadcast(JsonPayload);

	if (USensorDataPublisherComp* Publisher = ResolvePublisher())
	{
		Publisher->PublishPacket(TEXT("lidar"), SensorName, JsonPayload);
	}
}

USensorDataPublisherComp* UVirtualDistanceSensorComp::ResolvePublisher() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<USensorDataPublisherComp>() : nullptr;
}
