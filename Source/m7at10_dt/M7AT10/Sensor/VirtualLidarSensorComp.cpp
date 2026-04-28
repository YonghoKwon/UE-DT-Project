#include "VirtualLidarSensorComp.h"

#include "DrawDebugHelpers.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"

UVirtualLidarSensorComp::UVirtualLidarSensorComp()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualLidarSensorComp::BeginPlay()
{
    Super::BeginPlay();

    if (bApplyDeviceProfileOnBeginPlay)
    {
        ApplyDeviceProfile(DeviceProfile);
    }

    if (Preset != EVirtualLidarPreset::Custom)
    {
        ApplyPreset(Preset);
    }

    TryAutoRegisterToManager();

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

void UVirtualLidarSensorComp::SetTransportComponent(UVirtualSensorDataTransportComp* InTransportComponent)
{
    TransportComponent = InTransportComponent;
}

void UVirtualLidarSensorComp::ApplyPreset(EVirtualLidarPreset NewPreset)
{
    Preset = NewPreset;

    if (Preset == EVirtualLidarPreset::LowDebug)
    {
        HorizontalSamples = 90;
        VerticalChannels = 8;
        ScanInterval = 1.0f;
        PayloadPointStride = 2;
        MaxPayloadPoints = 2000;
    }
    else if (Preset == EVirtualLidarPreset::MediumPreview)
    {
        HorizontalSamples = 180;
        VerticalChannels = 16;
        ScanInterval = 0.5f;
        PayloadPointStride = 3;
        MaxPayloadPoints = 5000;
    }
    else if (Preset == EVirtualLidarPreset::HighQuality)
    {
        HorizontalSamples = 360;
        VerticalChannels = 32;
        ScanInterval = 0.25f;
        PayloadPointStride = 4;
        MaxPayloadPoints = 12000;
    }
}

void UVirtualLidarSensorComp::ApplyDeviceProfile(EVirtualLidarDeviceProfile NewProfile)
{
    DeviceProfile = NewProfile;

    if (DeviceProfile == EVirtualLidarDeviceProfile::LivoxMid360S)
    {
        DeviceSpec.Manufacturer = TEXT("Livox");
        DeviceSpec.Model = TEXT("Mid-360S");
        DeviceSpec.HorizontalFovDegrees = 360.0f;
        DeviceSpec.VerticalFovDegrees = 59.0f;
        DeviceSpec.MinRangeCm = 10.0f;
        DeviceSpec.TypicalRangeCm = 4000.0f;
        DeviceSpec.MaxRangeCm = 10000.0f;
        DeviceSpec.FrameRateHz = 10.0f;
        DeviceSpec.PointRate = 200000;
        DeviceSpec.Notes = TEXT("Livox Mid-360S metadata profile. Simulation quality controls runtime ray count separately.");

        HorizontalFov = DeviceSpec.HorizontalFovDegrees;
        MinVerticalAngle = -7.0f;
        MaxVerticalAngle = 52.0f;
        MaxDistance = DeviceSpec.TypicalRangeCm;
        Preset = EVirtualLidarPreset::Custom;
        ApplySimulationQuality(SimulationQuality);
    }
    else
    {
        DeviceSpec.Manufacturer = TEXT("Generic");
        DeviceSpec.Model = TEXT("Generic LiDAR");
        ApplySimulationQuality(SimulationQuality);
    }
}

void UVirtualLidarSensorComp::ApplySimulationQuality(EVirtualSensorSimulationQuality NewQuality)
{
    SimulationQuality = NewQuality;

    if (SimulationQuality == EVirtualSensorSimulationQuality::Debug)
    {
        HorizontalSamples = 60;
        VerticalChannels = 8;
        ScanInterval = 0.5f;
        PayloadPointStride = 1;
        MaxPayloadPoints = 1000;
    }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::RealTimePreview)
    {
        HorizontalSamples = 120;
        VerticalChannels = 24;
        ScanInterval = 0.25f;
        PayloadPointStride = 4;
        MaxPayloadPoints = 3000;
    }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::Balanced)
    {
        HorizontalSamples = 240;
        VerticalChannels = 32;
        ScanInterval = 0.2f;
        PayloadPointStride = 4;
        MaxPayloadPoints = 6000;
    }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::FullSpec)
    {
        HorizontalSamples = 360;
        VerticalChannels = 60;
        ScanInterval = 0.1f;
        PayloadPointStride = 8;
        MaxPayloadPoints = 10000;
    }
}

void UVirtualLidarSensorComp::ScanAndSend()
{
    ++FrameId;

    TArray<uint8> HeatmapPixels;
    ExecuteScan(LastPoints, HeatmapPixels);
    UpdateLidarViewTexture(HeatmapPixels);

    const FString JsonPayload = BuildJsonPayload(LastPoints);
    DispatchPayload(JsonPayload);

    RuntimeStatus.SensorId = SensorId;
    RuntimeStatus.SensorType = TEXT("virtual_lidar");
    RuntimeStatus.FrameId = FrameId;
    RuntimeStatus.LastUpdateUtc = FDateTime::UtcNow();
    RuntimeStatus.LastPayloadLength = JsonPayload.Len();
    RuntimeStatus.TotalPointCount = LastPoints.Num();
    RuntimeStatus.HitPointCount = 0;
    for (const FVirtualLidarPoint& Point : LastPoints)
    {
        if (Point.bHit)
        {
            ++RuntimeStatus.HitPointCount;
        }
    }
    RuntimeStatus.LastMessage = FString::Printf(TEXT("Quality=%d Rays=%d PayloadStride=%d"),
        static_cast<int32>(SimulationQuality),
        HorizontalSamples * VerticalChannels,
        PayloadPointStride);

    if (bExportCsvOnScan)
    {
        ExportLastPointCloudCsv();
    }
    if (bExportJsonLinesOnScan)
    {
        ExportLastPointCloudJsonLines();
    }
    if (bExportPcdOnScan)
    {
        ExportLastPointCloudPcd();
    }

    OnScanCompleted.Broadcast(JsonPayload, LidarViewTexture);
}

void UVirtualLidarSensorComp::ExecuteScan(TArray<FVirtualLidarPoint>& OutPoints, TArray<uint8>& OutHeatmapPixels)
{
    OutPoints.Reset();

    const int32 SafeHorizontalSamples = FMath::Max(1, HorizontalSamples);
    const int32 SafeVerticalChannels = FMath::Max(1, VerticalChannels);
    OutPoints.Reserve(SafeHorizontalSamples * SafeVerticalChannels * (bUseMultiHit ? FMath::Max(1, MaxHitsPerRay) : 1));
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

            FVirtualLidarPoint FirstPoint;
            FirstPoint.LocalDirection = GetComponentTransform().InverseTransformVectorNoScale(Direction).GetSafeNormal();
            FirstPoint.Distance = MaxDistance;
            FirstPoint.WorldLocation = End;
            FirstPoint.bHit = false;
            FirstPoint.HitActorName = NAME_None;

            if (bUseMultiHit)
            {
                TArray<FHitResult> Hits;
                World->LineTraceMultiByChannel(Hits, Origin, End, TraceChannel, Params);

                int32 AddedHits = 0;
                for (const FHitResult& Hit : Hits)
                {
                    if (ShouldIgnoreHitActor(Hit.GetActor()))
                    {
                        continue;
                    }

                    FVirtualLidarPoint Point;
                    Point.LocalDirection = FirstPoint.LocalDirection;
                    Point.bHit = true;
                    Point.Distance = Hit.Distance;
                    Point.WorldLocation = Hit.ImpactPoint;
                    Point.HitActorName = Hit.GetActor() ? Hit.GetActor()->GetFName() : NAME_None;
                    OutPoints.Add(Point);

                    if (!FirstPoint.bHit)
                    {
                        FirstPoint = Point;
                    }

                    ++AddedHits;
                    if (AddedHits >= FMath::Max(1, MaxHitsPerRay))
                    {
                        break;
                    }
                }

                if (AddedHits == 0)
                {
                    OutPoints.Add(FirstPoint);
                }
            }
            else
            {
                FHitResult Hit;
                bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, TraceChannel, Params);
                if (bHit && ShouldIgnoreHitActor(Hit.GetActor()))
                {
                    bHit = false;
                }

                FirstPoint.bHit = bHit;
                FirstPoint.Distance = bHit ? Hit.Distance : MaxDistance;
                FirstPoint.WorldLocation = bHit ? Hit.ImpactPoint : End;
                FirstPoint.HitActorName = bHit && Hit.GetActor() ? Hit.GetActor()->GetFName() : NAME_None;
                OutPoints.Add(FirstPoint);
            }

            const int32 PixelIndex = GetHeatmapPixelIndex(H, V, SafeHorizontalSamples, SafeVerticalChannels);
            WriteHeatmapPixel(OutHeatmapPixels, PixelIndex, FirstPoint);

            if (bDrawDebugRays)
            {
                DrawDebugLine(World, Origin, FirstPoint.WorldLocation, FirstPoint.bHit ? FColor::Cyan : FColor::Silver, false, ScanInterval, 0, 0.5f);
            }
        }
    }
}

FString UVirtualLidarSensorComp::BuildJsonPayload(const TArray<FVirtualLidarPoint>& Points) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("sensorType"), TEXT("virtual_lidar"));
    Root->SetStringField(TEXT("sensorId"), SensorId);
    Root->SetStringField(TEXT("manufacturer"), DeviceSpec.Manufacturer);
    Root->SetStringField(TEXT("model"), DeviceSpec.Model);
    Root->SetNumberField(TEXT("frameId"), static_cast<double>(FrameId));
    Root->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetNumberField(TEXT("horizontalSamples"), HorizontalSamples);
    Root->SetNumberField(TEXT("verticalChannels"), VerticalChannels);
    Root->SetNumberField(TEXT("rayCount"), HorizontalSamples * VerticalChannels);
    Root->SetNumberField(TEXT("maxDistance"), MaxDistance);
    Root->SetNumberField(TEXT("horizontalFov"), HorizontalFov);
    Root->SetNumberField(TEXT("minVerticalAngle"), MinVerticalAngle);
    Root->SetNumberField(TEXT("maxVerticalAngle"), MaxVerticalAngle);
    Root->SetBoolField(TEXT("multiHit"), bUseMultiHit);
    Root->SetNumberField(TEXT("maxHitsPerRay"), MaxHitsPerRay);
    Root->SetNumberField(TEXT("simulationQuality"), static_cast<int32>(SimulationQuality));
    Root->SetNumberField(TEXT("payloadPointStride"), PayloadPointStride);
    Root->SetNumberField(TEXT("maxPayloadPoints"), MaxPayloadPoints);
    Root->SetBoolField(TEXT("includeMissPointsInPayload"), bIncludeMissPointsInPayload);

    TSharedRef<FJsonObject> TransformObject = MakeShared<FJsonObject>();
    const FVector ComponentLocation = GetComponentLocation();
    const FRotator ComponentRotation = GetComponentRotation();
    const FVector Forward = GetForwardVector();
    const FVector Up = GetUpVector();

    TArray<TSharedPtr<FJsonValue>> Location;
    Location.Add(MakeShared<FJsonValueNumber>(ComponentLocation.X));
    Location.Add(MakeShared<FJsonValueNumber>(ComponentLocation.Y));
    Location.Add(MakeShared<FJsonValueNumber>(ComponentLocation.Z));
    TransformObject->SetArrayField(TEXT("location"), Location);

    TArray<TSharedPtr<FJsonValue>> Rotation;
    Rotation.Add(MakeShared<FJsonValueNumber>(ComponentRotation.Pitch));
    Rotation.Add(MakeShared<FJsonValueNumber>(ComponentRotation.Yaw));
    Rotation.Add(MakeShared<FJsonValueNumber>(ComponentRotation.Roll));
    TransformObject->SetArrayField(TEXT("rotation"), Rotation);

    TArray<TSharedPtr<FJsonValue>> ForwardJson;
    ForwardJson.Add(MakeShared<FJsonValueNumber>(Forward.X));
    ForwardJson.Add(MakeShared<FJsonValueNumber>(Forward.Y));
    ForwardJson.Add(MakeShared<FJsonValueNumber>(Forward.Z));
    TransformObject->SetArrayField(TEXT("forward"), ForwardJson);

    TArray<TSharedPtr<FJsonValue>> UpJson;
    UpJson.Add(MakeShared<FJsonValueNumber>(Up.X));
    UpJson.Add(MakeShared<FJsonValueNumber>(Up.Y));
    UpJson.Add(MakeShared<FJsonValueNumber>(Up.Z));
    TransformObject->SetArrayField(TEXT("up"), UpJson);

    Root->SetObjectField(TEXT("sensorTransform"), TransformObject);
    Root->SetArrayField(TEXT("sensorWorldLocation"), Location);

    TArray<TSharedPtr<FJsonValue>> JsonPoints;
    const int32 SafeStride = FMath::Max(1, PayloadPointStride);
    const int32 SafeMaxPayloadPoints = FMath::Max(0, MaxPayloadPoints);
    int32 AddedPayloadPoints = 0;

    for (int32 PointIndex = 0; PointIndex < Points.Num(); PointIndex += SafeStride)
    {
        if (SafeMaxPayloadPoints > 0 && AddedPayloadPoints >= SafeMaxPayloadPoints)
        {
            break;
        }

        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (!bIncludeMissPointsInPayload && !Point.bHit)
        {
            continue;
        }

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
        ++AddedPayloadPoints;
    }

    Root->SetNumberField(TEXT("payloadPointCount"), AddedPayloadPoints);
    Root->SetArrayField(TEXT("points"), JsonPoints);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

void UVirtualLidarSensorComp::WriteHeatmapPixel(TArray<uint8>& Pixels, int32 PixelIndex, const FVirtualLidarPoint& Point) const
{
    const float NormalizedDistance = FMath::Clamp(Point.Distance / MaxDistance, 0.0f, 1.0f);
    const uint8 Intensity = Point.bHit ? static_cast<uint8>((1.0f - NormalizedDistance) * 255.0f) : 0;

    if (!Pixels.IsValidIndex(PixelIndex + 3))
    {
        return;
    }

    if (ViewMode == EVirtualLidarViewMode::HitMask)
    {
        Pixels[PixelIndex + 0] = Point.bHit ? 255 : 0;
        Pixels[PixelIndex + 1] = Point.bHit ? 255 : 0;
        Pixels[PixelIndex + 2] = Point.bHit ? 255 : 0;
    }
    else if (ViewMode == EVirtualLidarViewMode::DepthGradient)
    {
        Pixels[PixelIndex + 0] = Point.bHit ? static_cast<uint8>(NormalizedDistance * 255.0f) : 0;
        Pixels[PixelIndex + 1] = Point.bHit ? Intensity : 0;
        Pixels[PixelIndex + 2] = Point.bHit ? 255 : 0;
    }
    else if (ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        Pixels[PixelIndex + 0] = Point.bHit ? 64 : 0;
        Pixels[PixelIndex + 1] = Point.bHit ? 220 : 0;
        Pixels[PixelIndex + 2] = Point.bHit ? 64 : 0;
    }
    else
    {
        Pixels[PixelIndex + 0] = Intensity;
        Pixels[PixelIndex + 1] = Intensity;
        Pixels[PixelIndex + 2] = Intensity;
    }
    Pixels[PixelIndex + 3] = 255;
}

bool UVirtualLidarSensorComp::ShouldIgnoreHitActor(const AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }
    for (const FName& Tag : IgnoreActorTags)
    {
        if (!Tag.IsNone() && Actor->ActorHasTag(Tag))
        {
            return true;
        }
    }
    return false;
}

int32 UVirtualLidarSensorComp::GetHeatmapPixelIndex(int32 H, int32 V, int32 Width, int32 Height) const
{
    const int32 DrawH = bFlipLidarViewHorizontal ? Width - 1 - H : H;
    const int32 DrawV = bFlipLidarViewVertical ? Height - 1 - V : V;
    return (DrawV * Width + DrawH) * 4;
}

FString UVirtualLidarSensorComp::BuildExportPath(const FString& Extension, const FString& FileNamePrefix) const
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("PointCloud"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FString Prefix = FileNamePrefix.IsEmpty() ? SensorId : FileNamePrefix;
    const FString Timestamp = FString::Printf(TEXT("%s_%lld"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::UtcNow().GetTicks());
    return FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s.%s"), *Prefix, *Timestamp, *Extension));
}

bool UVirtualLidarSensorComp::ExportLastPointCloudCsv(const FString& FileNamePrefix) const
{
    FString Text = TEXT("x,y,z,distance,hit,actor\n");
    for (const FVirtualLidarPoint& Point : LastPoints)
    {
        Text += FString::Printf(TEXT("%f,%f,%f,%f,%d,%s\n"), Point.WorldLocation.X, Point.WorldLocation.Y, Point.WorldLocation.Z, Point.Distance, Point.bHit ? 1 : 0, *Point.HitActorName.ToString());
    }
    return FFileHelper::SaveStringToFile(Text, *BuildExportPath(TEXT("csv"), FileNamePrefix));
}

bool UVirtualLidarSensorComp::ExportLastPointCloudJsonLines(const FString& FileNamePrefix) const
{
    FString Text;
    for (const FVirtualLidarPoint& Point : LastPoints)
    {
        Text += FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f,\"distance\":%f,\"hit\":%s,\"actor\":\"%s\"}\n"), Point.WorldLocation.X, Point.WorldLocation.Y, Point.WorldLocation.Z, Point.Distance, Point.bHit ? TEXT("true") : TEXT("false"), *Point.HitActorName.ToString());
    }
    return FFileHelper::SaveStringToFile(Text, *BuildExportPath(TEXT("jsonl"), FileNamePrefix));
}

bool UVirtualLidarSensorComp::ExportLastPointCloudPcd(const FString& FileNamePrefix) const
{
    FString Text;
    Text += TEXT("# .PCD v0.7 - Point Cloud Data file format\n");
    Text += TEXT("VERSION 0.7\nFIELDS x y z distance hit\nSIZE 4 4 4 4 4\nTYPE F F F F I\nCOUNT 1 1 1 1 1\n");
    Text += FString::Printf(TEXT("WIDTH %d\nHEIGHT 1\nPOINTS %d\nDATA ascii\n"), LastPoints.Num(), LastPoints.Num());
    for (const FVirtualLidarPoint& Point : LastPoints)
    {
        Text += FString::Printf(TEXT("%f %f %f %f %d\n"), Point.WorldLocation.X, Point.WorldLocation.Y, Point.WorldLocation.Z, Point.Distance, Point.bHit ? 1 : 0);
    }
    return FFileHelper::SaveStringToFile(Text, *BuildExportPath(TEXT("pcd"), FileNamePrefix));
}

void UVirtualLidarSensorComp::DispatchPayload(const FString& JsonPayload) const
{
    if (TransportComponent)
    {
        TransportComponent->SendJson(SensorId, TEXT("virtual_lidar"), JsonPayload);
        return;
    }

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

void UVirtualLidarSensorComp::TryAutoRegisterToManager()
{
    if (!bAutoRegisterToManager || !GetWorld())
    {
        return;
    }

    for (TActorIterator<AVirtualSensorManager> It(GetWorld()); It; ++It)
    {
        It->RegisterLidar(this);
        break;
    }
}
