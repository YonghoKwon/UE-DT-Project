#include "VirtualLidarSensorComp.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Json.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorRecorderComp.h"

namespace
{
template <typename TValue>
void WriteLasValue(FBufferArchive& Archive, const TValue& Value)
{
    Archive.Serialize(const_cast<TValue*>(&Value), sizeof(TValue));
}

void WriteLasFixedString(FBufferArchive& Archive, const ANSICHAR* Text, int32 Length)
{
    TArray<ANSICHAR> Buffer;
    Buffer.SetNumZeroed(Length);
    if (Text)
    {
        FCStringAnsi::Strncpy(Buffer.GetData(), Text, Length);
    }
    Archive.Serialize(Buffer.GetData(), Length);
}

void WriteLasBytes(FBufferArchive& Archive, const uint8* Bytes, int32 Length)
{
    Archive.Serialize(const_cast<uint8*>(Bytes), Length);
}

uint16 ClampDistanceToIntensity(float Distance, float MaxDistance)
{
    if (MaxDistance <= 0.0f)
    {
        return 0;
    }
    const float Normalized = FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f);
    return static_cast<uint16>((1.0f - Normalized) * 65535.0f);
}
}

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

    if (bPointCloudPreviewEnabled)
    {
        EnsurePointCloudPreviewComponent();
        ApplyPointCloudPreviewStyle();
    }

    if (bAutoStartScan)
    {
        StartScan();
    }
}

void UVirtualLidarSensorComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopScan();
    ClearPointCloudPreview();
    if (PointCloudPreviewComponent)
    {
        PointCloudPreviewComponent->DestroyComponent();
        PointCloudPreviewComponent = nullptr;
    }
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

void UVirtualLidarSensorComp::SetRecorderComponent(UVirtualSensorRecorderComp* InRecorderComponent)
{
    RecorderComponent = InRecorderComponent;
}

void UVirtualLidarSensorComp::SetPointCloudPreviewEnabled(bool bEnabled)
{
    bPointCloudPreviewEnabled = bEnabled;
    if (bPointCloudPreviewEnabled)
    {
        EnsurePointCloudPreviewComponent();
        ApplyPointCloudPreviewStyle();
        RefreshPointCloudPreview();
    }
    else
    {
        ClearPointCloudPreview();
    }
}

void UVirtualLidarSensorComp::ClearPointCloudPreview()
{
    if (PointCloudPreviewComponent)
    {
        PointCloudPreviewComponent->ClearInstances();
        PointCloudPreviewComponent->SetHiddenInGame(true);
        PointCloudPreviewComponent->SetVisibility(false, true);
    }
}

void UVirtualLidarSensorComp::ApplyPointCloudPreviewStyle()
{
    if (!PointCloudPreviewComponent)
    {
        return;
    }

    if (PointCloudPreviewMaterial)
    {
        PointCloudPreviewComponent->SetMaterial(0, PointCloudPreviewMaterial);
    }
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
    RefreshPointCloudPreview();

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
    RuntimeStatus.LastMessage = FString::Printf(TEXT("Quality=%d Rays=%d Hits=%d PayloadStride=%d PointCloudPreview=%s"),
        static_cast<int32>(SimulationQuality),
        HorizontalSamples * VerticalChannels,
        RuntimeStatus.HitPointCount,
        PayloadPointStride,
        bPointCloudPreviewEnabled ? TEXT("On") : TEXT("Off"));

    if (RecorderComponent)
    {
        RecorderComponent->RecordJsonFrame(SensorId, TEXT("virtual_lidar"), FrameId, JsonPayload);
    }

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

void UVirtualLidarSensorComp::CollectExportPoints(TArray<const FVirtualLidarPoint*>& OutExportPoints) const
{
    OutExportPoints.Reset();
    for (const FVirtualLidarPoint& Point : LastPoints)
    {
        if (bExportHitOnlyPointCloud && !Point.bHit)
        {
            continue;
        }
        OutExportPoints.Add(&Point);
    }
}

void UVirtualLidarSensorComp::LogLastPointCloud(int32 MaxPointsToLog, bool bHitOnly) const
{
    const int32 SafeMax = FMath::Max(0, MaxPointsToLog);
    int32 Logged = 0;
    int32 CandidateCount = 0;

    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] PointCloud frame=%lld total=%d hitOnly=%s maxLog=%d"), *SensorId, FrameId, LastPoints.Num(), bHitOnly ? TEXT("true") : TEXT("false"), SafeMax);

    for (int32 PointIndex = 0; PointIndex < LastPoints.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = LastPoints[PointIndex];
        if (bHitOnly && !Point.bHit)
        {
            continue;
        }
        ++CandidateCount;
        if (SafeMax > 0 && Logged >= SafeMax)
        {
            continue;
        }

        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] point[%d] x=%.3f y=%.3f z=%.3f distance=%.3f hit=%d actor=%s"),
            *SensorId,
            PointIndex,
            Point.WorldLocation.X,
            Point.WorldLocation.Y,
            Point.WorldLocation.Z,
            Point.Distance,
            Point.bHit ? 1 : 0,
            *Point.HitActorName.ToString());
        ++Logged;
    }

    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] PointCloud log complete. candidates=%d logged=%d"), *SensorId, CandidateCount, Logged);
}

bool UVirtualLidarSensorComp::ExportLastPointCloudCsv(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> ExportPoints;
    CollectExportPoints(ExportPoints);

    const FString Path = BuildExportPath(TEXT("csv"), FileNamePrefix);
    FString Text = TEXT("x,y,z,distance,hit,actor\n");
    for (const FVirtualLidarPoint* Point : ExportPoints)
    {
        if (!Point)
        {
            continue;
        }
        Text += FString::Printf(TEXT("%f,%f,%f,%f,%d,%s\n"), Point->WorldLocation.X, Point->WorldLocation.Y, Point->WorldLocation.Z, Point->Distance, Point->bHit ? 1 : 0, *Point->HitActorName.ToString());
    }
    const bool bSaved = FFileHelper::SaveStringToFile(Text, *Path);
    UE_LOG(LogTemp, bSaved ? Log : Warning, TEXT("[VirtualLidar:%s] CSV export %s: %s points=%d"), *SensorId, bSaved ? TEXT("saved") : TEXT("failed"), *Path, ExportPoints.Num());
    return bSaved;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudJsonLines(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> ExportPoints;
    CollectExportPoints(ExportPoints);

    const FString Path = BuildExportPath(TEXT("jsonl"), FileNamePrefix);
    FString Text;
    for (const FVirtualLidarPoint* Point : ExportPoints)
    {
        if (!Point)
        {
            continue;
        }
        Text += FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f,\"distance\":%f,\"hit\":%s,\"actor\":\"%s\"}\n"), Point->WorldLocation.X, Point->WorldLocation.Y, Point->WorldLocation.Z, Point->Distance, Point->bHit ? TEXT("true") : TEXT("false"), *Point->HitActorName.ToString());
    }
    const bool bSaved = FFileHelper::SaveStringToFile(Text, *Path);
    UE_LOG(LogTemp, bSaved ? Log : Warning, TEXT("[VirtualLidar:%s] JSONL export %s: %s points=%d"), *SensorId, bSaved ? TEXT("saved") : TEXT("failed"), *Path, ExportPoints.Num());
    return bSaved;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudPcd(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> ExportPoints;
    CollectExportPoints(ExportPoints);

    const FString Path = BuildExportPath(TEXT("pcd"), FileNamePrefix);
    FString Text;
    Text += TEXT("# .PCD v0.7 - Point Cloud Data file format\n");
    Text += TEXT("VERSION 0.7\nFIELDS x y z distance hit\nSIZE 4 4 4 4 4\nTYPE F F F F I\nCOUNT 1 1 1 1 1\n");
    Text += FString::Printf(TEXT("WIDTH %d\nHEIGHT 1\nPOINTS %d\nDATA ascii\n"), ExportPoints.Num(), ExportPoints.Num());
    for (const FVirtualLidarPoint* Point : ExportPoints)
    {
        if (!Point)
        {
            continue;
        }
        Text += FString::Printf(TEXT("%f %f %f %f %d\n"), Point->WorldLocation.X, Point->WorldLocation.Y, Point->WorldLocation.Z, Point->Distance, Point->bHit ? 1 : 0);
    }
    const bool bSaved = FFileHelper::SaveStringToFile(Text, *Path);
    UE_LOG(LogTemp, bSaved ? Log : Warning, TEXT("[VirtualLidar:%s] PCD export %s: %s points=%d"), *SensorId, bSaved ? TEXT("saved") : TEXT("failed"), *Path, ExportPoints.Num());
    return bSaved;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudLas(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> ExportPoints;
    CollectExportPoints(ExportPoints);
    if (ExportPoints.Num() <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] LAS export skipped: no points."), *SensorId);
        return false;
    }

    const double LasScale = 0.001;
    const double CmToMeter = 0.01;
    FVector MinPoint(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxPoint(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const FVirtualLidarPoint* Point : ExportPoints)
    {
        if (!Point)
        {
            continue;
        }
        MinPoint.X = FMath::Min(MinPoint.X, Point->WorldLocation.X);
        MinPoint.Y = FMath::Min(MinPoint.Y, Point->WorldLocation.Y);
        MinPoint.Z = FMath::Min(MinPoint.Z, Point->WorldLocation.Z);
        MaxPoint.X = FMath::Max(MaxPoint.X, Point->WorldLocation.X);
        MaxPoint.Y = FMath::Max(MaxPoint.Y, Point->WorldLocation.Y);
        MaxPoint.Z = FMath::Max(MaxPoint.Z, Point->WorldLocation.Z);
    }

    const double OffsetX = MinPoint.X * CmToMeter;
    const double OffsetY = MinPoint.Y * CmToMeter;
    const double OffsetZ = MinPoint.Z * CmToMeter;

    FBufferArchive Archive;
    const uint8 Signature[4] = {'L', 'A', 'S', 'F'};
    WriteLasBytes(Archive, Signature, 4);
    WriteLasValue<uint16>(Archive, 0);
    WriteLasValue<uint16>(Archive, 0);
    WriteLasValue<uint32>(Archive, 0);
    WriteLasValue<uint16>(Archive, 0);
    WriteLasValue<uint16>(Archive, 0);
    for (int32 Index = 0; Index < 8; ++Index)
    {
        WriteLasValue<uint8>(Archive, 0);
    }
    WriteLasValue<uint8>(Archive, 1);
    WriteLasValue<uint8>(Archive, 2);
    WriteLasFixedString(Archive, "UE-DT-Project", 32);
    WriteLasFixedString(Archive, "VirtualLidarSensorComp", 32);
    const FDateTime Now = FDateTime::Now();
    WriteLasValue<uint16>(Archive, static_cast<uint16>(Now.GetDayOfYear()));
    WriteLasValue<uint16>(Archive, static_cast<uint16>(Now.GetYear()));
    WriteLasValue<uint16>(Archive, 227);
    WriteLasValue<uint32>(Archive, 227);
    WriteLasValue<uint32>(Archive, 0);
    WriteLasValue<uint8>(Archive, 0);
    WriteLasValue<uint16>(Archive, 20);
    WriteLasValue<uint32>(Archive, static_cast<uint32>(ExportPoints.Num()));
    WriteLasValue<uint32>(Archive, static_cast<uint32>(ExportPoints.Num()));
    for (int32 ReturnIndex = 1; ReturnIndex < 5; ++ReturnIndex)
    {
        WriteLasValue<uint32>(Archive, 0);
    }
    WriteLasValue<double>(Archive, LasScale);
    WriteLasValue<double>(Archive, LasScale);
    WriteLasValue<double>(Archive, LasScale);
    WriteLasValue<double>(Archive, OffsetX);
    WriteLasValue<double>(Archive, OffsetY);
    WriteLasValue<double>(Archive, OffsetZ);
    WriteLasValue<double>(Archive, MaxPoint.X * CmToMeter);
    WriteLasValue<double>(Archive, MinPoint.X * CmToMeter);
    WriteLasValue<double>(Archive, MaxPoint.Y * CmToMeter);
    WriteLasValue<double>(Archive, MinPoint.Y * CmToMeter);
    WriteLasValue<double>(Archive, MaxPoint.Z * CmToMeter);
    WriteLasValue<double>(Archive, MinPoint.Z * CmToMeter);

    for (const FVirtualLidarPoint* Point : ExportPoints)
    {
        if (!Point)
        {
            continue;
        }
        const int32 X = FMath::RoundToInt(((Point->WorldLocation.X * CmToMeter) - OffsetX) / LasScale);
        const int32 Y = FMath::RoundToInt(((Point->WorldLocation.Y * CmToMeter) - OffsetY) / LasScale);
        const int32 Z = FMath::RoundToInt(((Point->WorldLocation.Z * CmToMeter) - OffsetZ) / LasScale);
        const uint16 Intensity = ClampDistanceToIntensity(Point->Distance, MaxDistance);
        const uint8 ReturnFlags = 1;
        const uint8 Classification = Point->bHit ? 1 : 0;
        const int8 ScanAngleRank = 0;
        const uint8 UserData = Point->bHit ? 1 : 0;
        const uint16 PointSourceId = 0;

        WriteLasValue<int32>(Archive, X);
        WriteLasValue<int32>(Archive, Y);
        WriteLasValue<int32>(Archive, Z);
        WriteLasValue<uint16>(Archive, Intensity);
        WriteLasValue<uint8>(Archive, ReturnFlags);
        WriteLasValue<uint8>(Archive, Classification);
        WriteLasValue<int8>(Archive, ScanAngleRank);
        WriteLasValue<uint8>(Archive, UserData);
        WriteLasValue<uint16>(Archive, PointSourceId);
    }

    const FString Path = BuildExportPath(TEXT("las"), FileNamePrefix);
    const bool bSaved = FFileHelper::SaveArrayToFile(Archive, *Path);
    Archive.FlushCache();
    Archive.Empty();
    UE_LOG(LogTemp, bSaved ? Log : Warning, TEXT("[VirtualLidar:%s] LAS export %s: %s points=%d"), *SensorId, bSaved ? TEXT("saved") : TEXT("failed"), *Path, ExportPoints.Num());
    return bSaved;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudLaz(const FString& FileNamePrefix) const
{
    const bool bSaved = ExportLastPointCloudLas(FileNamePrefix.IsEmpty() ? TEXT("laz_source") : FileNamePrefix + TEXT("_laz_source"));
    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] LAZ compression requires LASzip/PDAL. This build exports LAS natively; convert the generated .las to .laz with an external tool."), *SensorId);
    return bSaved;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudCsvLasLaz(const FString& FileNamePrefix) const
{
    const bool bCsv = ExportLastPointCloudCsv(FileNamePrefix);
    const bool bLas = ExportLastPointCloudLas(FileNamePrefix);
    const bool bLaz = ExportLastPointCloudLaz(FileNamePrefix);
    return bCsv && bLas && bLaz;
}

UInstancedStaticMeshComponent* UVirtualLidarSensorComp::EnsurePointCloudPreviewComponent()
{
    if (PointCloudPreviewComponent)
    {
        return PointCloudPreviewComponent;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }

    PointCloudPreviewComponent = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("LidarPointCloudPreviewComponent"));
    if (!PointCloudPreviewComponent)
    {
        return nullptr;
    }

    UStaticMesh* MeshToUse = PointCloudPreviewMesh;
    if (!MeshToUse)
    {
        MeshToUse = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    }
    if (!MeshToUse)
    {
        MeshToUse = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    }

    PointCloudPreviewComponent->SetStaticMesh(MeshToUse);
    PointCloudPreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PointCloudPreviewComponent->SetGenerateOverlapEvents(false);
    PointCloudPreviewComponent->SetCastShadow(false);
    PointCloudPreviewComponent->bSelectable = false;
    PointCloudPreviewComponent->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);
    PointCloudPreviewComponent->RegisterComponent();
    PointCloudPreviewComponent->SetHiddenInGame(!bPointCloudPreviewEnabled || bDrawPointCloudPreviewDebugPoints);
    PointCloudPreviewComponent->SetVisibility(bPointCloudPreviewEnabled && !bDrawPointCloudPreviewDebugPoints, true);
    ApplyPointCloudPreviewStyle();
    return PointCloudPreviewComponent;
}

void UVirtualLidarSensorComp::RefreshPointCloudPreview()
{
    if (!bPointCloudPreviewEnabled)
    {
        ClearPointCloudPreview();
        return;
    }

    UInstancedStaticMeshComponent* PreviewComp = EnsurePointCloudPreviewComponent();
    if (!PreviewComp && !bDrawPointCloudPreviewDebugPoints)
    {
        return;
    }

    if (PreviewComp)
    {
        PreviewComp->ClearInstances();
        PreviewComp->SetHiddenInGame(bDrawPointCloudPreviewDebugPoints);
        PreviewComp->SetVisibility(!bDrawPointCloudPreviewDebugPoints, true);
    }

    const int32 SafeStride = FMath::Max(1, PointCloudPreviewStride);
    const int32 SafeMaxInstances = FMath::Max(0, MaxPointCloudPreviewInstances);
    const float SafeScale = FMath::Max(0.001f, PointCloudPreviewPointScale);
    int32 AddedInstances = 0;
    UWorld* World = GetWorld();
    const FColor DebugColor = PointCloudPreviewColor.ToFColor(true);
    const float DebugLifetime = FMath::Max(ScanInterval * 1.5f, 0.1f);

    for (int32 PointIndex = 0; PointIndex < LastPoints.Num(); PointIndex += SafeStride)
    {
        if (SafeMaxInstances > 0 && AddedInstances >= SafeMaxInstances)
        {
            break;
        }

        const FVirtualLidarPoint& Point = LastPoints[PointIndex];
        if (bPointCloudPreviewHitOnly && !Point.bHit)
        {
            continue;
        }

        if (bDrawPointCloudPreviewDebugPoints && World)
        {
            DrawDebugPoint(World, Point.WorldLocation, PointCloudPreviewDebugPointSize, DebugColor, false, DebugLifetime);
        }
        else if (PreviewComp)
        {
            FTransform PointTransform;
            PointTransform.SetLocation(Point.WorldLocation);
            PointTransform.SetRotation(FQuat::Identity);
            PointTransform.SetScale3D(FVector(SafeScale));
            PreviewComp->AddInstance(PointTransform, true);
        }
        ++AddedInstances;
    }
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
