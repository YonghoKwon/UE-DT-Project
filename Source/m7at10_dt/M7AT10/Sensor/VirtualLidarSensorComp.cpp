#include "VirtualLidarSensorComp.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
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
template <typename T> void WriteLasValue(FBufferArchive& A, const T& V) { A.Serialize(const_cast<T*>(&V), sizeof(T)); }
void WriteLasFixedString(FBufferArchive& A, const ANSICHAR* Text, int32 Len) { TArray<ANSICHAR> B; B.SetNumZeroed(Len); if (Text) { FCStringAnsi::Strncpy(B.GetData(), Text, Len); } A.Serialize(B.GetData(), Len); }
void WriteLasBytes(FBufferArchive& A, const uint8* Bytes, int32 Len) { A.Serialize(const_cast<uint8*>(Bytes), Len); }
uint16 ClampDistanceToIntensity(float Distance, float MaxDistance) { if (MaxDistance <= 0.0f) return 0; const float N = FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f); return static_cast<uint16>((1.0f - N) * 65535.0f); }
FString JoinNames(const TArray<FName>& Names) { FString R; for (int32 I = 0; I < Names.Num(); ++I) { if (I > 0) R += TEXT("|"); R += Names[I].ToString(); } return R; }
}

UVirtualLidarSensorComp::UVirtualLidarSensorComp()
{
    PrimaryComponentTick.bCanEverTick = false;
    ResetDefaultSemanticClassRules();
}

void UVirtualLidarSensorComp::BeginPlay()
{
    Super::BeginPlay();
    if (SemanticClassRules.Num() <= 0) { ResetDefaultSemanticClassRules(); }
    if (bApplyDeviceProfileOnBeginPlay) { ApplyDeviceProfile(DeviceProfile); }
    if (Preset != EVirtualLidarPreset::Custom) { ApplyPreset(Preset); }
    TryAutoRegisterToManager();
    if (bPointCloudPreviewEnabled) { EnsurePointCloudPreviewComponent(); ApplyPointCloudPreviewStyle(); }
    if (bAutoStartScan) { StartScan(); }
}

void UVirtualLidarSensorComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopScan();
    ClearPointCloudPreview();
    if (PointCloudPreviewComponent) { PointCloudPreviewComponent->DestroyComponent(); PointCloudPreviewComponent = nullptr; }
    Super::EndPlay(EndPlayReason);
}

void UVirtualLidarSensorComp::StartScan()
{
    if (!GetWorld() || ScanInterval <= 0.0f) return;
    GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(ScanTimerHandle, this, &UVirtualLidarSensorComp::ScanAndSend, ScanInterval, true, 0.0f);
}
void UVirtualLidarSensorComp::StopScan() { if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle); }
void UVirtualLidarSensorComp::SetTransportComponent(UVirtualSensorDataTransportComp* InTransportComponent) { TransportComponent = InTransportComponent; }
void UVirtualLidarSensorComp::SetRecorderComponent(UVirtualSensorRecorderComp* InRecorderComponent) { RecorderComponent = InRecorderComponent; }

void UVirtualLidarSensorComp::SetPointCloudPreviewEnabled(bool bEnabled)
{
    bPointCloudPreviewEnabled = bEnabled;
    if (bPointCloudPreviewEnabled) { EnsurePointCloudPreviewComponent(); ApplyPointCloudPreviewStyle(); RefreshPointCloudPreview(); }
    else { ClearPointCloudPreview(); }
}
void UVirtualLidarSensorComp::ClearPointCloudPreview()
{
    if (PointCloudPreviewComponent) { PointCloudPreviewComponent->ClearInstances(); PointCloudPreviewComponent->SetHiddenInGame(true); PointCloudPreviewComponent->SetVisibility(false, true); }
}
void UVirtualLidarSensorComp::ApplyPointCloudPreviewStyle()
{
    if (PointCloudPreviewComponent && PointCloudPreviewMaterial) PointCloudPreviewComponent->SetMaterial(0, PointCloudPreviewMaterial);
}

void UVirtualLidarSensorComp::ResetDefaultSemanticClassRules()
{
    SemanticClassRules.Reset();
    auto AddRule = [this](FName Label, const FLinearColor& Color, TArray<FName> Tags, TArray<FName> Classes, TArray<FString> Names, TArray<FString> Comps)
    {
        FVirtualLidarSemanticClassRule R; R.Label = Label; R.DisplayColor = Color; R.ActorTags = Tags; R.ActorClassNames = Classes; R.ActorNameContains = Names; R.ComponentNameContains = Comps; SemanticClassRules.Add(R);
    };
    AddRule(TEXT("Slab"), FLinearColor(1.0f, 0.25f, 0.02f, 1.0f), {TEXT("Slab"), TEXT("slab"), TEXT("SLAB"), TEXT("슬라브"), TEXT("Blade")}, {}, {TEXT("Slab"), TEXT("slab"), TEXT("SLAB"), TEXT("슬라브"), TEXT("Blade"), TEXT("blade")}, {TEXT("Slab"), TEXT("slab"), TEXT("Blade")});
    AddRule(TEXT("Roller"), FLinearColor(0.0f, 0.85f, 1.0f, 1.0f), {TEXT("Roller"), TEXT("roller"), TEXT("Rollers"), TEXT("롤러")}, {}, {TEXT("Roller"), TEXT("roller"), TEXT("Roll"), TEXT("roll"), TEXT("롤러")}, {TEXT("Roller"), TEXT("roller"), TEXT("Roll")});
    AddRule(TEXT("Conveyor"), FLinearColor(0.05f, 1.0f, 0.25f, 1.0f), {TEXT("Conveyor"), TEXT("conveyor"), TEXT("Equipment"), TEXT("설비")}, {}, {TEXT("Conveyor"), TEXT("conveyor"), TEXT("Frame"), TEXT("Equipment"), TEXT("설비")}, {TEXT("Conveyor"), TEXT("Frame"), TEXT("Equipment")});
    AddRule(TEXT("Floor"), FLinearColor(0.2f, 0.25f, 1.0f, 1.0f), {TEXT("Floor"), TEXT("floor"), TEXT("Ground"), TEXT("ground"), TEXT("바닥")}, {}, {TEXT("Floor"), TEXT("floor"), TEXT("Ground"), TEXT("ground"), TEXT("Grid")}, {TEXT("Floor"), TEXT("Ground")});
    AddRule(TEXT("Sensor"), FLinearColor(0.75f, 0.15f, 1.0f, 1.0f), {TEXT("Sensor"), TEXT("Lidar"), TEXT("LiDAR"), TEXT("Camera")}, {}, {TEXT("Sensor"), TEXT("Lidar"), TEXT("LiDAR"), TEXT("Camera")}, {TEXT("Sensor"), TEXT("Lidar"), TEXT("Camera")});
}

FLinearColor UVirtualLidarSensorComp::GetSemanticColorForLabel(FName SemanticLabel) const
{
    for (const FVirtualLidarSemanticClassRule& R : SemanticClassRules) { if (!R.Label.IsNone() && R.Label == SemanticLabel) return R.DisplayColor; }
    return DefaultSemanticColor;
}

bool UVirtualLidarSensorComp::SemanticRuleMatches(const FVirtualLidarSemanticClassRule& Rule, const AActor* Actor, const UPrimitiveComponent* Component) const
{
    if (!Actor) return false;
    for (const FName& Tag : Rule.ActorTags) { if (!Tag.IsNone() && Actor->ActorHasTag(Tag)) return true; }
    const FString ActorName = Actor->GetName();
    for (const FString& Pattern : Rule.ActorNameContains) { if (!Pattern.IsEmpty() && ActorName.Contains(Pattern, ESearchCase::IgnoreCase)) return true; }
    const FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
    for (const FName& ClassRule : Rule.ActorClassNames) { const FString Pattern = ClassRule.ToString(); if (!Pattern.IsEmpty() && (ClassName.Equals(Pattern, ESearchCase::IgnoreCase) || ClassName.Contains(Pattern, ESearchCase::IgnoreCase))) return true; }
    if (Component) { const FString CompName = Component->GetName(); for (const FString& Pattern : Rule.ComponentNameContains) { if (!Pattern.IsEmpty() && CompName.Contains(Pattern, ESearchCase::IgnoreCase)) return true; } }
    return false;
}

FName UVirtualLidarSensorComp::ResolveSemanticLabel(const FHitResult& Hit) const
{
    if (!bEnableSemanticClassification || !Hit.GetActor()) return DefaultSemanticLabel;
    for (const FVirtualLidarSemanticClassRule& R : SemanticClassRules) { if (SemanticRuleMatches(R, Hit.GetActor(), Hit.GetComponent())) return R.Label; }
    return DefaultSemanticLabel;
}

FLinearColor UVirtualLidarSensorComp::ResolveSemanticColor(const FVirtualLidarPoint& Point) const
{
    if (!Point.bHit) return FLinearColor(0.01f, 0.01f, 0.02f, 1.0f);
    return GetSemanticColorForLabel(Point.SemanticLabel.IsNone() ? DefaultSemanticLabel : Point.SemanticLabel);
}

void UVirtualLidarSensorComp::PopulatePointSemanticMetadata(FVirtualLidarPoint& Point, const FHitResult& Hit) const
{
    const AActor* A = Hit.GetActor();
    Point.HitActorName = A ? A->GetFName() : NAME_None;
    Point.HitActorClassName = (A && A->GetClass()) ? A->GetClass()->GetFName() : NAME_None;
    Point.HitActorTags.Reset();
    if (A) Point.HitActorTags = A->Tags;
    Point.SemanticLabel = A ? ResolveSemanticLabel(Hit) : NAME_None;
}

void UVirtualLidarSensorComp::ApplyPreset(EVirtualLidarPreset NewPreset)
{
    Preset = NewPreset;
    if (Preset == EVirtualLidarPreset::LowDebug) { HorizontalSamples = 90; VerticalChannels = 8; ScanInterval = 1.0f; PayloadPointStride = 2; MaxPayloadPoints = 2000; }
    else if (Preset == EVirtualLidarPreset::MediumPreview) { HorizontalSamples = 180; VerticalChannels = 16; ScanInterval = 0.5f; PayloadPointStride = 3; MaxPayloadPoints = 5000; }
    else if (Preset == EVirtualLidarPreset::HighQuality) { HorizontalSamples = 360; VerticalChannels = 32; ScanInterval = 0.25f; PayloadPointStride = 4; MaxPayloadPoints = 12000; }
}

void UVirtualLidarSensorComp::ApplyDeviceProfile(EVirtualLidarDeviceProfile NewProfile)
{
    DeviceProfile = NewProfile;
    if (DeviceProfile == EVirtualLidarDeviceProfile::LivoxMid360S)
    {
        DeviceSpec.Manufacturer = TEXT("Livox"); DeviceSpec.Model = TEXT("Mid-360S"); DeviceSpec.HorizontalFovDegrees = 360.0f; DeviceSpec.VerticalFovDegrees = 59.0f; DeviceSpec.MinRangeCm = 10.0f; DeviceSpec.TypicalRangeCm = 4000.0f; DeviceSpec.MaxRangeCm = 10000.0f; DeviceSpec.FrameRateHz = 10.0f; DeviceSpec.PointRate = 200000; DeviceSpec.Notes = TEXT("Livox Mid-360S metadata profile. Simulation quality controls runtime ray count separately.");
        HorizontalFov = DeviceSpec.HorizontalFovDegrees; MinVerticalAngle = -7.0f; MaxVerticalAngle = 52.0f; MaxDistance = DeviceSpec.TypicalRangeCm; Preset = EVirtualLidarPreset::Custom;
    }
    else { DeviceSpec.Manufacturer = TEXT("Generic"); DeviceSpec.Model = TEXT("Generic LiDAR"); }
    ApplySimulationQuality(SimulationQuality);
}

void UVirtualLidarSensorComp::ApplySimulationQuality(EVirtualSensorSimulationQuality NewQuality)
{
    SimulationQuality = NewQuality;
    if (SimulationQuality == EVirtualSensorSimulationQuality::Debug) { HorizontalSamples = 60; VerticalChannels = 8; ScanInterval = 0.5f; PayloadPointStride = 1; MaxPayloadPoints = 1000; }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::RealTimePreview) { HorizontalSamples = 120; VerticalChannels = 24; ScanInterval = 0.25f; PayloadPointStride = 4; MaxPayloadPoints = 3000; }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::Balanced) { HorizontalSamples = 240; VerticalChannels = 32; ScanInterval = 0.2f; PayloadPointStride = 4; MaxPayloadPoints = 6000; }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::FullSpec) { HorizontalSamples = 360; VerticalChannels = 60; ScanInterval = 0.1f; PayloadPointStride = 8; MaxPayloadPoints = 10000; }
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
    UpdateRuntimeStatusAfterScan(JsonPayload.Len());

    if (RecorderComponent)
    {
        RecorderComponent->RecordJsonFrame(SensorId, TEXT("virtual_lidar"), FrameId, JsonPayload);
    }

    ExportEnabledPointCloudFormats();
    OnScanCompleted.Broadcast(JsonPayload, LidarViewTexture);
}

void UVirtualLidarSensorComp::ExecuteScan(TArray<FVirtualLidarPoint>& OutPoints, TArray<uint8>& OutHeatmapPixels)
{
    OutPoints.Reset();
    const int32 W = FMath::Max(1, HorizontalSamples); const int32 Hn = FMath::Max(1, VerticalChannels);
    OutPoints.Reserve(W * Hn * (bUseMultiHit ? FMath::Max(1, MaxHitsPerRay) : 1)); OutHeatmapPixels.SetNumZeroed(W * Hn * 4);
    UWorld* World = GetWorld(); if (!World) return;
    const FVector Origin = GetComponentLocation(); const FRotator BaseRotation = GetComponentRotation(); FCollisionQueryParams Params(SCENE_QUERY_STAT(VirtualLidarSensor), false, GetOwner());
    for (int32 V = 0; V < Hn; ++V)
    {
        const float Pitch = FMath::Lerp(MinVerticalAngle, MaxVerticalAngle, Hn == 1 ? 0.5f : (float)V / (float)(Hn - 1));
        for (int32 X = 0; X < W; ++X)
        {
            const float Yaw = FMath::Lerp(-HorizontalFov * 0.5f, HorizontalFov * 0.5f, W == 1 ? 0.5f : (float)X / (float)(W - 1));
            const FRotator RayRotation = BaseRotation + FRotator(Pitch, Yaw, 0.0f); const FVector Dir = RayRotation.Vector(); const FVector End = Origin + Dir * MaxDistance;
            FVirtualLidarPoint FirstPoint; FirstPoint.LocalDirection = GetComponentTransform().InverseTransformVectorNoScale(Dir).GetSafeNormal(); FirstPoint.Distance = MaxDistance; FirstPoint.WorldLocation = End; FirstPoint.bHit = false;
            if (bUseMultiHit)
            {
                TArray<FHitResult> Hits; World->LineTraceMultiByChannel(Hits, Origin, End, TraceChannel, Params); int32 Added = 0;
                for (const FHitResult& Hit : Hits)
                {
                    if (ShouldIgnoreHitActor(Hit.GetActor())) continue;
                    FVirtualLidarPoint P; P.LocalDirection = FirstPoint.LocalDirection; P.bHit = true; P.Distance = Hit.Distance; P.WorldLocation = Hit.ImpactPoint; PopulatePointSemanticMetadata(P, Hit); OutPoints.Add(P);
                    if (!FirstPoint.bHit) FirstPoint = P;
                    if (++Added >= FMath::Max(1, MaxHitsPerRay)) break;
                }
                if (Added == 0) OutPoints.Add(FirstPoint);
            }
            else
            {
                FHitResult Hit; bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, TraceChannel, Params); if (bHit && ShouldIgnoreHitActor(Hit.GetActor())) bHit = false;
                FirstPoint.bHit = bHit; FirstPoint.Distance = bHit ? Hit.Distance : MaxDistance; FirstPoint.WorldLocation = bHit ? Hit.ImpactPoint : End; if (bHit) PopulatePointSemanticMetadata(FirstPoint, Hit); OutPoints.Add(FirstPoint);
            }
            WriteHeatmapPixel(OutHeatmapPixels, GetHeatmapPixelIndex(X, V, W, Hn), FirstPoint);
            if (bDrawDebugRays) DrawDebugLine(World, Origin, FirstPoint.WorldLocation, FirstPoint.bHit ? ResolveSemanticColor(FirstPoint).ToFColor(true) : FColor::Silver, false, ScanInterval, 0, 0.5f);
        }
    }
}

FString UVirtualLidarSensorComp::BuildJsonPayload(const TArray<FVirtualLidarPoint>& Points) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("sensorType"), TEXT("virtual_lidar")); Root->SetStringField(TEXT("sensorId"), SensorId); Root->SetStringField(TEXT("manufacturer"), DeviceSpec.Manufacturer); Root->SetStringField(TEXT("model"), DeviceSpec.Model); Root->SetNumberField(TEXT("frameId"), (double)FrameId); Root->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601()); Root->SetNumberField(TEXT("horizontalSamples"), HorizontalSamples); Root->SetNumberField(TEXT("verticalChannels"), VerticalChannels); Root->SetNumberField(TEXT("rayCount"), HorizontalSamples * VerticalChannels); Root->SetNumberField(TEXT("maxDistance"), MaxDistance); Root->SetBoolField(TEXT("semanticClassification"), bEnableSemanticClassification);
    TArray<TSharedPtr<FJsonValue>> JsonPoints; const int32 SafeStride = FMath::Max(1, PayloadPointStride); const int32 SafeMax = FMath::Max(0, MaxPayloadPoints); int32 Added = 0;
    for (int32 I = 0; I < Points.Num(); I += SafeStride)
    {
        if (SafeMax > 0 && Added >= SafeMax) break; const FVirtualLidarPoint& P = Points[I]; if (!bIncludeMissPointsInPayload && !P.bHit) continue;
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("hit"), P.bHit); O->SetNumberField(TEXT("distance"), P.Distance); O->SetStringField(TEXT("hitActor"), P.HitActorName.ToString()); O->SetStringField(TEXT("hitActorClass"), P.HitActorClassName.ToString()); O->SetStringField(TEXT("semanticLabel"), P.SemanticLabel.ToString());
        TArray<TSharedPtr<FJsonValue>> Tags; for (const FName& T : P.HitActorTags) Tags.Add(MakeShared<FJsonValueString>(T.ToString())); O->SetArrayField(TEXT("hitActorTags"), Tags);
        TArray<TSharedPtr<FJsonValue>> WL; WL.Add(MakeShared<FJsonValueNumber>(P.WorldLocation.X)); WL.Add(MakeShared<FJsonValueNumber>(P.WorldLocation.Y)); WL.Add(MakeShared<FJsonValueNumber>(P.WorldLocation.Z)); O->SetArrayField(TEXT("worldLocation"), WL);
        JsonPoints.Add(MakeShared<FJsonValueObject>(O)); ++Added;
    }
    Root->SetNumberField(TEXT("payloadPointCount"), Added); Root->SetArrayField(TEXT("points"), JsonPoints); FString Out; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out); FJsonSerializer::Serialize(Root, W); return Out;
}

void UVirtualLidarSensorComp::WriteHeatmapPixel(TArray<uint8>& Pixels, int32 PixelIndex, const FVirtualLidarPoint& Point) const
{
    if (!Pixels.IsValidIndex(PixelIndex + 3)) return;
    const float N = FMath::Clamp(Point.Distance / FMath::Max(1.0f, MaxDistance), 0.0f, 1.0f); const uint8 I = Point.bHit ? (uint8)((1.0f - N) * 255.0f) : 0; FColor C;
    if (ViewMode == EVirtualLidarViewMode::HitMask) C = Point.bHit ? FColor::White : FColor::Black;
    else if (ViewMode == EVirtualLidarViewMode::DepthGradient) C = Point.bHit ? FColor(255, I, (uint8)(N * 255.0f), 255) : FColor::Black;
    else if (ViewMode == EVirtualLidarViewMode::ActorClassColor) C = Point.bHit ? ResolveSemanticColor(Point).ToFColor(true) : FColor(3, 8, 10, 255);
    else C = FColor(I, I, I, 255);
    Pixels[PixelIndex + 0] = C.B; Pixels[PixelIndex + 1] = C.G; Pixels[PixelIndex + 2] = C.R; Pixels[PixelIndex + 3] = 255;
}

bool UVirtualLidarSensorComp::ShouldIgnoreHitActor(const AActor* Actor) const { if (!Actor) return false; for (const FName& Tag : IgnoreActorTags) if (!Tag.IsNone() && Actor->ActorHasTag(Tag)) return true; return false; }
int32 UVirtualLidarSensorComp::GetHeatmapPixelIndex(int32 H, int32 V, int32 W, int32 Ht) const { const int32 DH = bFlipLidarViewHorizontal ? W - 1 - H : H; const int32 DV = bFlipLidarViewVertical ? Ht - 1 - V : V; return (DV * W + DH) * 4; }

void UVirtualLidarSensorComp::UpdateLidarViewTexture(const TArray<uint8>& Pixels)
{
    const int32 W = FMath::Max(1, HorizontalSamples); const int32 H = FMath::Max(1, VerticalChannels); if (Pixels.Num() < W * H * 4) return;
    if (!LidarViewTexture || LidarViewTexture->GetSizeX() != W || LidarViewTexture->GetSizeY() != H) { LidarViewTexture = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8); if (LidarViewTexture) { LidarViewTexture->SRGB = true; LidarViewTexture->CompressionSettings = TC_VectorDisplacementmap; } }
    if (!LidarViewTexture || !LidarViewTexture->GetPlatformData() || LidarViewTexture->GetPlatformData()->Mips.Num() <= 0) return;
    FTexture2DMipMap& Mip = LidarViewTexture->GetPlatformData()->Mips[0]; void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE); if (Data) FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num()); Mip.BulkData.Unlock(); LidarViewTexture->UpdateResource();
}

void UVirtualLidarSensorComp::UpdateRuntimeStatusAfterScan(int32 PayloadLength)
{
    RuntimeStatus.SensorId = SensorId;
    RuntimeStatus.SensorType = TEXT("virtual_lidar");
    RuntimeStatus.FrameId = FrameId;
    RuntimeStatus.LastUpdateUtc = FDateTime::UtcNow();
    RuntimeStatus.LastPayloadLength = PayloadLength;
    RuntimeStatus.TotalPointCount = LastPoints.Num();
    RuntimeStatus.HitPointCount = 0;

    for (const FVirtualLidarPoint& Point : LastPoints)
    {
        if (Point.bHit)
        {
            ++RuntimeStatus.HitPointCount;
        }
    }

    RuntimeStatus.LastMessage = FString::Printf(
        TEXT("Quality=%d Rays=%d Hits=%d PayloadStride=%d MaxPayload=%d Preview=%s PreviewStride=%d Semantic=%s Rules=%d"),
        static_cast<int32>(SimulationQuality),
        HorizontalSamples * VerticalChannels,
        RuntimeStatus.HitPointCount,
        PayloadPointStride,
        MaxPayloadPoints,
        bPointCloudPreviewEnabled ? TEXT("On") : TEXT("Off"),
        PointCloudPreviewStride,
        bEnableSemanticClassification ? TEXT("On") : TEXT("Off"),
        SemanticClassRules.Num());
}

void UVirtualLidarSensorComp::ExportEnabledPointCloudFormats() const
{
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
}

void UVirtualLidarSensorComp::DispatchPayload(const FString& JsonPayload) const
{
    if (TransportComponent)
    {
        TransportComponent->SendJson(SensorId, TEXT("virtual_lidar"), JsonPayload);
        return;
    }

    if (OutputMode == EVirtualLidarOutputMode::LogOnly)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] Payload length=%d"), *SensorId, JsonPayload.Len());
    }
    else if (OutputMode == EVirtualLidarOutputMode::SaveJson)
    {
        SaveJsonToDisk(JsonPayload);
    }
    else if (OutputMode == EVirtualLidarOutputMode::HttpPost)
    {
        PostJson(JsonPayload);
    }
}
void UVirtualLidarSensorComp::PostJson(const FString& JsonPayload) const
{
    if (HttpEndpoint.IsEmpty()) return; TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest(); Request->SetURL(HttpEndpoint); Request->SetVerb(TEXT("POST")); Request->SetHeader(TEXT("Content-Type"), TEXT("application/json")); Request->SetContentAsString(JsonPayload); Request->ProcessRequest();
}
void UVirtualLidarSensorComp::SaveJsonToDisk(const FString& JsonPayload) const { const FString Path = BuildExportPath(TEXT("json"), SensorId); FFileHelper::SaveStringToFile(JsonPayload, *Path); }

FString UVirtualLidarSensorComp::BuildExportPath(const FString& Extension, const FString& FileNamePrefix) const
{
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("PointCloud")); IFileManager::Get().MakeDirectory(*Dir, true); const FString Prefix = FileNamePrefix.IsEmpty() ? SensorId : FileNamePrefix; const FDateTime N = FDateTime::UtcNow(); const FString Ts = FString::Printf(TEXT("%s_%03d_%lld"), *N.ToString(TEXT("%Y%m%d_%H%M%S")), N.GetMillisecond(), N.GetTicks()); return FPaths::Combine(Dir, FString::Printf(TEXT("%s_%s.%s"), *Prefix, *Ts, *Extension));
}
void UVirtualLidarSensorComp::CollectExportPoints(TArray<const FVirtualLidarPoint*>& Out) const { Out.Reset(); for (const FVirtualLidarPoint& P : LastPoints) if (!bExportHitOnlyPointCloud || P.bHit) Out.Add(&P); }

void UVirtualLidarSensorComp::LogLastPointCloud(int32 MaxPointsToLog, bool bHitOnly) const
{
    int32 Logged = 0, Candidates = 0; UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] PointCloud frame=%lld total=%d"), *SensorId, FrameId, LastPoints.Num());
    for (int32 Idx = 0; Idx < LastPoints.Num(); ++Idx) { const FVirtualLidarPoint& P = LastPoints[Idx]; if (bHitOnly && !P.bHit) continue; ++Candidates; if (MaxPointsToLog > 0 && Logged >= MaxPointsToLog) continue; UE_LOG(LogTemp, Warning, TEXT("[%d] x=%.3f y=%.3f z=%.3f d=%.3f hit=%d actor=%s class=%s label=%s tags=%s"), Idx, P.WorldLocation.X, P.WorldLocation.Y, P.WorldLocation.Z, P.Distance, P.bHit ? 1 : 0, *P.HitActorName.ToString(), *P.HitActorClassName.ToString(), *P.SemanticLabel.ToString(), *JoinNames(P.HitActorTags)); ++Logged; }
    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] PointCloud log complete. candidates=%d logged=%d"), *SensorId, Candidates, Logged);
}

bool UVirtualLidarSensorComp::ExportLastPointCloudCsv(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> Points; CollectExportPoints(Points); FString Text = TEXT("x,y,z,distance,hit,actor,actor_class,semantic_label,tags\n");
    for (const FVirtualLidarPoint* P : Points) if (P) Text += FString::Printf(TEXT("%f,%f,%f,%f,%d,%s,%s,%s,%s\n"), P->WorldLocation.X, P->WorldLocation.Y, P->WorldLocation.Z, P->Distance, P->bHit ? 1 : 0, *P->HitActorName.ToString(), *P->HitActorClassName.ToString(), *P->SemanticLabel.ToString(), *JoinNames(P->HitActorTags));
    const FString Path = BuildExportPath(TEXT("csv"), FileNamePrefix);
    const bool bFileSaved = FFileHelper::SaveStringToFile(Text, *Path);
    if (bFileSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] CSV saved: %s points=%d"), *SensorId, *Path, Points.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] CSV failed: %s"), *SensorId, *Path);
    }
    return bFileSaved;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudJsonLines(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> Points; CollectExportPoints(Points); FString Text;
    for (const FVirtualLidarPoint* P : Points) if (P) Text += FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f,\"distance\":%f,\"hit\":%s,\"actor\":\"%s\",\"actorClass\":\"%s\",\"semanticLabel\":\"%s\",\"tags\":\"%s\"}\n"), P->WorldLocation.X, P->WorldLocation.Y, P->WorldLocation.Z, P->Distance, P->bHit ? TEXT("true") : TEXT("false"), *P->HitActorName.ToString(), *P->HitActorClassName.ToString(), *P->SemanticLabel.ToString(), *JoinNames(P->HitActorTags));
    const FString Path = BuildExportPath(TEXT("jsonl"), FileNamePrefix);
    const bool bFileSaved = FFileHelper::SaveStringToFile(Text, *Path);
    if (bFileSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] JSONL saved: %s points=%d"), *SensorId, *Path, Points.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] JSONL failed: %s"), *SensorId, *Path);
    }
    return bFileSaved;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudPcd(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> Points; CollectExportPoints(Points); FString Text = FString::Printf(TEXT("# .PCD v0.7\nVERSION 0.7\nFIELDS x y z distance hit\nSIZE 4 4 4 4 4\nTYPE F F F F I\nCOUNT 1 1 1 1 1\nWIDTH %d\nHEIGHT 1\nPOINTS %d\nDATA ascii\n"), Points.Num(), Points.Num());
    for (const FVirtualLidarPoint* P : Points) if (P) Text += FString::Printf(TEXT("%f %f %f %f %d\n"), P->WorldLocation.X, P->WorldLocation.Y, P->WorldLocation.Z, P->Distance, P->bHit ? 1 : 0);
    const FString Path = BuildExportPath(TEXT("pcd"), FileNamePrefix);
    const bool bFileSaved = FFileHelper::SaveStringToFile(Text, *Path);
    if (bFileSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] PCD saved: %s points=%d"), *SensorId, *Path, Points.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] PCD failed: %s"), *SensorId, *Path);
    }
    return bFileSaved;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudLas(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> Points; CollectExportPoints(Points); if (Points.Num() <= 0) return false;
    FVector Min(FLT_MAX), Max(-FLT_MAX); for (const FVirtualLidarPoint* P : Points) if (P) { Min.X = FMath::Min(Min.X, P->WorldLocation.X); Min.Y = FMath::Min(Min.Y, P->WorldLocation.Y); Min.Z = FMath::Min(Min.Z, P->WorldLocation.Z); Max.X = FMath::Max(Max.X, P->WorldLocation.X); Max.Y = FMath::Max(Max.Y, P->WorldLocation.Y); Max.Z = FMath::Max(Max.Z, P->WorldLocation.Z); }
    const double Scale = 0.001, CmToM = 0.01, OX = Min.X * CmToM, OY = Min.Y * CmToM, OZ = Min.Z * CmToM; FBufferArchive A; const uint8 Sig[4] = {'L','A','S','F'}; WriteLasBytes(A, Sig, 4); WriteLasValue<uint16>(A,0); WriteLasValue<uint16>(A,0); WriteLasValue<uint32>(A,0); WriteLasValue<uint16>(A,0); WriteLasValue<uint16>(A,0); for(int32 i=0;i<8;++i) WriteLasValue<uint8>(A,0); WriteLasValue<uint8>(A,1); WriteLasValue<uint8>(A,2); WriteLasFixedString(A,"UE-DT-Project",32); WriteLasFixedString(A,"VirtualLidar",32); const FDateTime Now = FDateTime::Now(); WriteLasValue<uint16>(A,(uint16)Now.GetDayOfYear()); WriteLasValue<uint16>(A,(uint16)Now.GetYear()); WriteLasValue<uint16>(A,227); WriteLasValue<uint32>(A,227); WriteLasValue<uint32>(A,0); WriteLasValue<uint8>(A,0); WriteLasValue<uint16>(A,20); WriteLasValue<uint32>(A,(uint32)Points.Num()); WriteLasValue<uint32>(A,(uint32)Points.Num()); for(int32 i=1;i<5;++i) WriteLasValue<uint32>(A,0); WriteLasValue<double>(A,Scale); WriteLasValue<double>(A,Scale); WriteLasValue<double>(A,Scale); WriteLasValue<double>(A,OX); WriteLasValue<double>(A,OY); WriteLasValue<double>(A,OZ); WriteLasValue<double>(A,Max.X*CmToM); WriteLasValue<double>(A,Min.X*CmToM); WriteLasValue<double>(A,Max.Y*CmToM); WriteLasValue<double>(A,Min.Y*CmToM); WriteLasValue<double>(A,Max.Z*CmToM); WriteLasValue<double>(A,Min.Z*CmToM);
    for (const FVirtualLidarPoint* P : Points) if (P) { const int32 X=(int32)FMath::RoundToDouble(((P->WorldLocation.X*CmToM)-OX)/Scale); const int32 Y=(int32)FMath::RoundToDouble(((P->WorldLocation.Y*CmToM)-OY)/Scale); const int32 Z=(int32)FMath::RoundToDouble(((P->WorldLocation.Z*CmToM)-OZ)/Scale); WriteLasValue<int32>(A,X); WriteLasValue<int32>(A,Y); WriteLasValue<int32>(A,Z); WriteLasValue<uint16>(A,ClampDistanceToIntensity(P->Distance,MaxDistance)); WriteLasValue<uint8>(A,1); WriteLasValue<uint8>(A,1); WriteLasValue<int8>(A,0); WriteLasValue<uint8>(A,0); WriteLasValue<uint16>(A,0); }
    const FString Path = BuildExportPath(TEXT("las"), FileNamePrefix);
    const bool bFileSaved = FFileHelper::SaveArrayToFile(A, *Path);
    if (bFileSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] LAS saved: %s points=%d"), *SensorId, *Path, Points.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] LAS failed: %s"), *SensorId, *Path);
    }
    return bFileSaved;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudLaz(const FString& FileNamePrefix) const { const FString Prefix = FileNamePrefix.IsEmpty() ? TEXT("laz_source") : FileNamePrefix + TEXT("_laz_source"); return ExportLastPointCloudLas(Prefix); }
bool UVirtualLidarSensorComp::ExportLastPointCloudCsvLasLaz(const FString& FileNamePrefix) const { const bool A = ExportLastPointCloudCsv(FileNamePrefix); const bool B = ExportLastPointCloudLas(FileNamePrefix); const bool C = ExportLastPointCloudLaz(FileNamePrefix); return A || B || C; }

UInstancedStaticMeshComponent* UVirtualLidarSensorComp::EnsurePointCloudPreviewComponent()
{
    if (!PointCloudPreviewComponent)
    {
        UObject* PreviewOuter = GetOwner() ? Cast<UObject>(GetOwner()) : Cast<UObject>(this);
        PointCloudPreviewComponent = NewObject<UInstancedStaticMeshComponent>(PreviewOuter, TEXT("VirtualLidarPointCloudPreview"));
        if (PointCloudPreviewComponent) { PointCloudPreviewComponent->SetupAttachment(this); PointCloudPreviewComponent->RegisterComponent(); if (!PointCloudPreviewMesh) PointCloudPreviewMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"))); if (PointCloudPreviewMesh) PointCloudPreviewComponent->SetStaticMesh(PointCloudPreviewMesh); }
    }
    return PointCloudPreviewComponent;
}

void UVirtualLidarSensorComp::RefreshPointCloudPreview()
{
    UWorld* World = GetWorld(); if (!bPointCloudPreviewEnabled || !World) { ClearPointCloudPreview(); return; }
    UInstancedStaticMeshComponent* Comp = EnsurePointCloudPreviewComponent(); if (!Comp) return; Comp->ClearInstances(); Comp->SetHiddenInGame(false); Comp->SetVisibility(true, true);
    const int32 Stride = FMath::Max(1, PointCloudPreviewStride); int32 Added = 0;
    for (int32 I = 0; I < LastPoints.Num(); I += Stride)
    {
        const FVirtualLidarPoint& P = LastPoints[I]; if (bPointCloudPreviewHitOnly && !P.bHit) continue; if (MaxPointCloudPreviewInstances > 0 && Added >= MaxPointCloudPreviewInstances) break;
        const FTransform T(FRotator::ZeroRotator, P.WorldLocation, FVector(PointCloudPreviewPointScale)); Comp->AddInstance(T, true);
        if (bDrawPointCloudPreviewDebugPoints) DrawDebugPoint(World, P.WorldLocation, PointCloudPreviewDebugPointSize, (bUseSemanticColorInPointCloudPreview && P.bHit) ? ResolveSemanticColor(P).ToFColor(true) : PointCloudPreviewColor.ToFColor(true), false, ScanInterval);
        ++Added;
    }
}

void UVirtualLidarSensorComp::TryAutoRegisterToManager()
{
    if (!bAutoRegisterToManager || !GetWorld()) return; for (TActorIterator<AVirtualSensorManager> It(GetWorld()); It; ++It) { if (*It) { It->RegisterLidar(this); return; } }
}
