#include "VirtualLidarComp.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "SensorDataRelayComp.h"
#include "TimerManager.h"
#include "m7at10_dt/m7at10_dt.h"

UVirtualLidarComp::UVirtualLidarComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualLidarComp::BeginPlay()
{
	Super::BeginPlay();
	ResolveRelayComponent();

	if (ScanInterval > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ScanTimerHandle, this, &UVirtualLidarComp::CaptureAndSendScan, ScanInterval, true);
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

void UVirtualLidarComp::CaptureAndSendScan()
{
	if (!GetWorld())
	{
		return;
	}

	const int32 SampleCount = FMath::Clamp(HorizontalSamples, 1, 4000);
	const float StartAngle = -HorizontalFovDeg * 0.5f;
	const float StepAngle = (SampleCount > 1) ? HorizontalFovDeg / (SampleCount - 1) : 0.0f;
	const FVector SensorOrigin = GetComponentLocation();
	const FQuat SensorRotation = GetComponentQuat();

	TArray<FLidarScanPoint> ScanPoints;
	ScanPoints.Reserve(SampleCount);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VirtualLidarTrace), false, GetOwner());

	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const float AngleDeg = StartAngle + StepAngle * Index;
		const FVector LocalDirection = FVector(FMath::Cos(FMath::DegreesToRadians(AngleDeg)), FMath::Sin(FMath::DegreesToRadians(AngleDeg)), 0.0f);
		const FVector WorldDirection = SensorRotation.RotateVector(LocalDirection).GetSafeNormal();
		const FVector TraceEnd = SensorOrigin + (WorldDirection * MaxDistanceCm);

		FHitResult Hit;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, SensorOrigin, TraceEnd, TraceChannel, QueryParams);

		FLidarScanPoint Point;
		Point.bHit = bHit;
		Point.Location = bHit ? Hit.Location : TraceEnd;
		Point.Distance = FVector::Dist(SensorOrigin, Point.Location);
		ScanPoints.Add(Point);

		if (bDrawDebugTrace)
		{
			DrawDebugLine(GetWorld(), SensorOrigin, Point.Location, bHit ? FColor::Green : FColor::Red, false, ScanInterval, 0, 1.0f);
		}
	}

	if (!RelayComp)
	{
		ResolveRelayComponent();
	}

	if (RelayComp)
	{
		RelayComp->PublishPayload(SensorId, TEXT("lidar"), BuildPayloadJson(ScanPoints));
	}
}

FString UVirtualLidarComp::BuildPayloadJson(const TArray<FLidarScanPoint>& ScanPoints) const
{
	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetStringField(TEXT("sensor_id"), SensorId);
	RootObj->SetStringField(TEXT("sensor_type"), TEXT("lidar"));
	RootObj->SetNumberField(TEXT("timestamp_unix_ms"), static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp()) * 1000.0);
	RootObj->SetNumberField(TEXT("sample_count"), ScanPoints.Num());
	RootObj->SetNumberField(TEXT("max_distance_cm"), MaxDistanceCm);

	TArray<TSharedPtr<FJsonValue>> PointsJson;
	PointsJson.Reserve(ScanPoints.Num());
	for (const FLidarScanPoint& Point : ScanPoints)
	{
		TSharedRef<FJsonObject> PointObj = MakeShared<FJsonObject>();
		PointObj->SetBoolField(TEXT("hit"), Point.bHit);
		PointObj->SetNumberField(TEXT("distance_cm"), Point.Distance);
		PointObj->SetNumberField(TEXT("x"), Point.Location.X);
		PointObj->SetNumberField(TEXT("y"), Point.Location.Y);
		PointObj->SetNumberField(TEXT("z"), Point.Location.Z);
		PointsJson.Add(MakeShared<FJsonValueObject>(PointObj));
	}
	RootObj->SetArrayField(TEXT("points"), PointsJson);

	FString OutJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	FJsonSerializer::Serialize(RootObj, Writer);
	return OutJson;
}

void UVirtualLidarComp::ResolveRelayComponent()
{
	RelayComp = GetOwner() ? GetOwner()->FindComponentByClass<USensorDataRelayComp>() : nullptr;
	if (!RelayComp)
	{
		UE_LOG(LogM7AT10, Verbose, TEXT("[VirtualLidarComp] SensorDataRelayComp not found on owner."));
	}
}
