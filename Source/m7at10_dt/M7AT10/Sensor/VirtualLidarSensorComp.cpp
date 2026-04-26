#include "VirtualLidarSensorComp.h"

#include "DrawDebugHelpers.h"
#include "Engine/Texture2D.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UVirtualLidarSensorComp::UVirtualLidarSensorComp()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualLidarSensorComp::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoStartScan)
    {
        StartScan();
    }
}

void UVirtualLidarSensorComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopScan();
    Super::EndPlay(EndPlayReason);
}

void UVirtualLidarSensorComp::StartScan()
{
    if (!GetWorld() || ScanInterval <= 0.0f)
    {
        return;
    }

    GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(ScanTimerHandle, this, &UVirtualLidarSensorComp::ScanAndSend, ScanInterval, true, 0.0f);
}

void UVirtualLidarSensorComp::StopScan()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
    }
}

void UVirtualLidarSensorComp::ScanAndSend()
{
    TArray<uint8> HeatmapPixels;
    ExecuteScan(LastPoints, HeatmapPixels);
    UpdateLidarViewTexture(HeatmapPixels);

    const FString JsonPayload = BuildJsonPayload(LastPoints);
    DispatchPayload(JsonPayload);
    OnScanCompleted.Broadcast(JsonPayload, LidarViewTexture);
}

void UVirtualLidarSensorComp::ExecuteScan(TArray<FVirtualLidarPoint>& OutPoints, TArray<uint8>& OutHeatmapPixels)
{
    OutPoints.Reset();

    const int32 SafeHorizontalSamples = FMath::Max(1, HorizontalSamples);
    const int32 SafeVerticalChannels = FMath::Max(1, VerticalChannels);
    OutPoints.Reserve(SafeHorizontalSamples * SafeVerticalChannels);
    OutHeatmapPixels.SetNumZeroed(SafeHorizontalSamples * SafeVerticalChannels * 4);

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector Origin = GetComponentLocation();
    const FRotator BaseRotation = GetComponentRotation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VirtualLidarSensor), false, GetOwner());

    for (int32 V = 0; V < SafeVerticalChannels; ++V)
    {
        const float VerticalAlpha = SafeVerticalChannels == 1 ? 0.5f : static_cast<float>(V) / static_cast<float>(SafeVerticalChannels - 1);
        const float Pitch = FMath::Lerp(MinVerticalAngle, MaxVerticalAngle, VerticalAlpha);

        for (int32 H = 0; H < SafeHorizontalSamples; ++H)
        {
            const float HorizontalAlpha = SafeHorizontalSamples == 1 ? 0.5f : static_cast<float>(H) / static_cast<float>(SafeHorizontalSamples - 1);
            const float Yaw = FMath::Lerp(-HorizontalFov * 0.5f, HorizontalFov * 0.5f, HorizontalAlpha);

            const FRotator RayRotation = BaseRotation + FRotator(Pitch, Yaw, 0.0f);
            const FVector Direction = RayRotation.Vector();
            const FVector End = Origin + Direction * MaxDistance;

            FHitResult Hit;
            const bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, TraceChannel, Params);

            FVirtualLidarPoint Point;
            Point.LocalDirection = GetComponentTransform().InverseTransformVectorNoScale(Direction).GetSafeNormal();
            Point.bHit = bHit;
            Point.Distance = bHit ? Hit.Distance : MaxDistance;
            Point.WorldLocation = bHit ? Hit.ImpactPoint : End;
            Point.HitActorName = bHit && Hit.GetActor() ? Hit.GetActor()->GetFName() : NAME_None;
            OutPoints.Add(Point);

            const float NormalizedDistance = FMath::Clamp(Point.Distance / MaxDistance, 0.0f, 1.0f);
            const uint8 Intensity = bHit ? static_cast<uint8>((1.0f - NormalizedDistance) * 255.0f) : 0;
            const int32 PixelIndex = (V * SafeHorizontalSamples + H) * 4;
            OutHeatmapPixels[PixelIndex + 0] = Intensity;
            OutHeatmapPixels[PixelIndex + 1] = Intensity;
            OutHeatmapPixels[PixelIndex + 2] = bHit ? 255 : 0;
            OutHeatmapPixels[PixelIndex + 3] = 255;

            if (bDrawDebugRays)
            {
                DrawDebugLine(World, Origin, Point.WorldLocation, bHit ? FColor::Cyan : FColor::Silver, false, ScanInterval, 0, 0.5f);
            }
        }
    }
}

FString UVirtualLidarSensorComp::BuildJsonPayload(const TArray<FVirtualLidarPoint>& Points) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("sensorType"), TEXT("virtual_lidar"));
    Root->SetStringField(TEXT("sensorId"), SensorId);
    Root->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetNumberField(TEXT("horizontalSamples"), HorizontalSamples);
    Root->SetNumberField(TEXT("verticalChannels"), VerticalChannels);
    Root->SetNumberField(TEXT("maxDistance"), MaxDistance);
    Root->SetNumberField(TEXT("horizontalFov"), HorizontalFov);
    Root->SetNumberField(TEXT("minVerticalAngle"), MinVerticalAngle);
    Root->SetNumberField(TEXT("maxVerticalAngle"), MaxVerticalAngle);

    TArray<TSharedPtr<FJsonValue>> JsonPoints;
    JsonPoints.Reserve(Points.Num());

    for (const FVirtualLidarPoint& Point : Points)
    {
        TSharedRef<FJsonObject> PointObject = MakeShared<FJsonObject>();
        PointObject->SetBoolField(TEXT("hit"), Point.bHit);
        PointObject->SetNumberField(TEXT("distance"), Point.Distance);
        PointObject->SetStringField(TEXT("hitActor"), Point.HitActorName.ToString());

        TArray<TSharedPtr<FJsonValue>> WorldLocation;
        WorldLocation.Add(MakeShared<FJsonValueNumber>(Point.WorldLocation.X));
        WorldLocation.Add(MakeShared<FJsonValueNumber>(Point.WorldLocation.Y));
        WorldLocation.Add(MakeShared<FJsonValueNumber>(Point.WorldLocation.Z));
        PointObject->SetArrayField(TEXT("worldLocation"), WorldLocation);

        JsonPoints.Add(MakeShared<FJsonValueObject>(PointObject));
    }

    Root->SetArrayField(TEXT("points"), JsonPoints);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

void UVirtualLidarSensorComp::DispatchPayload(const FString& JsonPayload) const
{
    switch (OutputMode)
    {
    case EVirtualLidarOutputMode::None:
        break;
    case EVirtualLidarOutputMode::LogOnly:
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] points=%d payloadLength=%d"), *SensorId, LastPoints.Num(), JsonPayload.Len());
        break;
    case EVirtualLidarOutputMode::SaveJson:
        SaveJsonToDisk(JsonPayload);
        break;
    case EVirtualLidarOutputMode::HttpPost:
        PostJson(JsonPayload);
        break;
    default:
        break;
    }
}

void UVirtualLidarSensorComp::PostJson(const FString& JsonPayload) const
{
    if (HttpEndpoint.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] HttpEndpoint is empty."), *SensorId);
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(HttpEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(JsonPayload);
    Request->ProcessRequest();
}

void UVirtualLidarSensorComp::SaveJsonToDisk(const FString& JsonPayload) const
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId);
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FString Timestamp = FString::Printf(TEXT("%s_%lld"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::UtcNow().GetTicks());
    const FString FileName = FString::Printf(TEXT("%s_%s.json"), *SensorId, *Timestamp);
    const FString Path = FPaths::Combine(Directory, FileName);
    FFileHelper::SaveStringToFile(JsonPayload, *Path);
}

void UVirtualLidarSensorComp::UpdateLidarViewTexture(const TArray<uint8>& HeatmapPixels)
{
    const int32 Width = FMath::Max(1, HorizontalSamples);
    const int32 Height = FMath::Max(1, VerticalChannels);
    if (HeatmapPixels.Num() != Width * Height * 4)
    {
        return;
    }

    if (!LidarViewTexture || LidarViewTexture->GetSizeX() != Width || LidarViewTexture->GetSizeY() != Height)
    {
        LidarViewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
        LidarViewTexture->SRGB = true;
        LidarViewTexture->Filter = TF_Nearest;
    }

    FTexture2DMipMap& Mip = LidarViewTexture->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Data, HeatmapPixels.GetData(), HeatmapPixels.Num());
    Mip.BulkData.Unlock();
    LidarViewTexture->UpdateResource();
}
