#include "VirtualLidarSensorComp.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Json.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorDataTransportComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorManager.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComp.h"

namespace
{
template <typename T> void WriteLasValue(FBufferArchive& A, const T& V) { A.Serialize(const_cast<T*>(&V), sizeof(T)); }
void WriteLasFixedString(FBufferArchive& A, const ANSICHAR* Text, int32 Len) { TArray<ANSICHAR> B; B.SetNumZeroed(Len); if (Text) { FCStringAnsi::Strncpy(B.GetData(), Text, Len); } A.Serialize(B.GetData(), Len); }
void WriteLasBytes(FBufferArchive& A, const uint8* Bytes, int32 Len) { A.Serialize(const_cast<uint8*>(Bytes), Len); }
uint16 ClampDistanceToIntensity(float Distance, float MaxDistance) { if (MaxDistance <= 0.0f) return 0; const float N = FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f); return static_cast<uint16>((1.0f - N) * 65535.0f); }
FString JoinNames(const TArray<FName>& Names) { FString R; for (int32 I = 0; I < Names.Num(); ++I) { if (I > 0) R += TEXT("|"); R += Names[I].ToString(); } return R; }
float NormalizeSignedAngleDegrees(float Angle) { while (Angle > 180.0f) Angle -= 360.0f; while (Angle < -180.0f) Angle += 360.0f; return Angle; }
float NormalizeAxisAngleDegrees(float Angle) { Angle = NormalizeSignedAngleDegrees(Angle); if (Angle > 90.0f) Angle -= 180.0f; if (Angle < -90.0f) Angle += 180.0f; return Angle; }
void AddVectorArray(TSharedRef<FJsonObject> Object, const TCHAR* FieldName, const FVector& Value)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    Array.Add(MakeShared<FJsonValueNumber>(Value.X));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
    Object->SetArrayField(FieldName, Array);
}
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

void UVirtualLidarSensorComp::SetServerPayloadPolicy(int32 InStride, int32 InMaxPoints, bool bInIncludeMissPoints)
{
    ServerPayloadStride = FMath::Clamp(InStride, 1, 100);
    MaxServerPayloadPoints = FMath::Clamp(InMaxPoints, 0, 1000000);
    bIncludeMissPointsInServerPayload = bInIncludeMissPoints;
    PayloadPointStride = ServerPayloadStride;
    MaxPayloadPoints = MaxServerPayloadPoints;
    bIncludeMissPointsInPayload = bIncludeMissPointsInServerPayload;
}

void UVirtualLidarSensorComp::SetPreviewPolicy(int32 InStride, int32 InMaxPoints, bool bInHitOnly)
{
    PreviewPointStride = FMath::Clamp(InStride, 1, 100);
    MaxPreviewPoints = FMath::Clamp(InMaxPoints, 0, 1000000);
    bPointCloudPreviewHitOnly = bInHitOnly;
    PointCloudPreviewStride = PreviewPointStride;
    MaxPointCloudPreviewInstances = MaxPreviewPoints;
    RefreshPointCloudPreview();
}

void UVirtualLidarSensorComp::SetPreviewBackend(ELidarPointCloudPreviewBackend InBackend, bool bAllowExperimentalGpuBackend)
{
    PreviewBackend = InBackend;
    bAllowExperimentalGpuPreviewBackend = bAllowExperimentalGpuBackend;
    LastPerformanceWarning = BuildPerformanceWarning();
    RefreshPointCloudPreview();
}

FString UVirtualLidarSensorComp::GetPreviewBackendName() const
{
    switch (PreviewBackend)
    {
    case ELidarPointCloudPreviewBackend::NiagaraCandidate:
        return TEXT("NiagaraCandidateCpuFallback");
    case ELidarPointCloudPreviewBackend::CustomGpuCandidate:
        return TEXT("CustomGpuCandidateCpuFallback");
    case ELidarPointCloudPreviewBackend::CpuInstancedMesh:
    default:
        return TEXT("CpuInstancedMesh");
    }
}

bool UVirtualLidarSensorComp::IsGpuPreviewBackendRequested() const
{
    return PreviewBackend == ELidarPointCloudPreviewBackend::NiagaraCandidate ||
        PreviewBackend == ELidarPointCloudPreviewBackend::CustomGpuCandidate;
}

bool UVirtualLidarSensorComp::IsGpuPreviewBackendActive() const
{
    return false;
}

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
    AddRule(TEXT("Slab"), FLinearColor(1.0f, 0.25f, 0.02f, 1.0f), {TEXT("Slab"), TEXT("slab"), TEXT("SLAB"), TEXT("SteelSlab"), TEXT("Plate")}, {}, {TEXT("Slab"), TEXT("slab"), TEXT("SLAB"), TEXT("SteelSlab"), TEXT("Plate")}, {TEXT("Slab"), TEXT("slab"), TEXT("Plate")});
    AddRule(TEXT("Roller"), FLinearColor(0.0f, 0.85f, 1.0f, 1.0f), {TEXT("Roller"), TEXT("roller"), TEXT("Rollers")}, {}, {TEXT("Roller"), TEXT("roller"), TEXT("Roll"), TEXT("roll")}, {TEXT("Roller"), TEXT("roller"), TEXT("Roll")});
    AddRule(TEXT("Conveyor"), FLinearColor(0.05f, 1.0f, 0.25f, 1.0f), {TEXT("Conveyor"), TEXT("conveyor"), TEXT("Equipment")}, {}, {TEXT("Conveyor"), TEXT("conveyor"), TEXT("Frame"), TEXT("Equipment")}, {TEXT("Conveyor"), TEXT("Frame"), TEXT("Equipment")});
    AddRule(TEXT("Floor"), FLinearColor(0.2f, 0.25f, 1.0f, 1.0f), {TEXT("Floor"), TEXT("floor"), TEXT("Ground"), TEXT("ground")}, {}, {TEXT("Floor"), TEXT("floor"), TEXT("Ground"), TEXT("ground"), TEXT("Grid")}, {TEXT("Floor"), TEXT("Ground")});
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
    if (Preset == EVirtualLidarPreset::LowDebug) { HorizontalSamples = 90; VerticalChannels = 8; ScanInterval = 1.0f; PreviewPointStride = 1; MaxPreviewPoints = 2000; PointCloudPreviewStride = PreviewPointStride; MaxPointCloudPreviewInstances = MaxPreviewPoints; PayloadPointStride = ServerPayloadStride; MaxPayloadPoints = MaxServerPayloadPoints; }
    else if (Preset == EVirtualLidarPreset::MediumPreview) { HorizontalSamples = 180; VerticalChannels = 16; ScanInterval = 0.5f; PreviewPointStride = 2; MaxPreviewPoints = 3000; PointCloudPreviewStride = PreviewPointStride; MaxPointCloudPreviewInstances = MaxPreviewPoints; PayloadPointStride = ServerPayloadStride; MaxPayloadPoints = MaxServerPayloadPoints; }
    else if (Preset == EVirtualLidarPreset::HighQuality) { HorizontalSamples = 360; VerticalChannels = 32; ScanInterval = 0.25f; PreviewPointStride = 4; MaxPreviewPoints = 5000; PointCloudPreviewStride = PreviewPointStride; MaxPointCloudPreviewInstances = MaxPreviewPoints; PayloadPointStride = ServerPayloadStride; MaxPayloadPoints = MaxServerPayloadPoints; }
}

void UVirtualLidarSensorComp::ApplyDeviceProfile(EVirtualLidarDeviceProfile NewProfile)
{
    DeviceProfile = NewProfile;
    if (DeviceProfile == EVirtualLidarDeviceProfile::LivoxMid360S)
    {
        DeviceSpec.Manufacturer = TEXT("Livox"); DeviceSpec.Model = TEXT("Mid-360S"); DeviceSpec.HorizontalFovDegrees = 360.0f; DeviceSpec.VerticalFovDegrees = 59.0f; DeviceSpec.MinRangeCm = 10.0f; DeviceSpec.TypicalRangeCm = 4000.0f; DeviceSpec.MaxRangeCm = 10000.0f; DeviceSpec.FrameRateHz = 10.0f; DeviceSpec.PointRate = 200000; DeviceSpec.Notes = TEXT("Livox Mid-360S: 40m at 10% reflectivity and 100m cutoff. Simulation quality controls runtime ray count separately.");
        HorizontalFov = DeviceSpec.HorizontalFovDegrees; MinVerticalAngle = -7.0f; MaxVerticalAngle = 52.0f; MaxDistance = DeviceSpec.TypicalRangeCm; Preset = EVirtualLidarPreset::Custom;
    }
    else { DeviceSpec.Manufacturer = TEXT("Generic"); DeviceSpec.Model = TEXT("Generic LiDAR"); }
    ApplySimulationQuality(SimulationQuality);
}

void UVirtualLidarSensorComp::ApplySimulationQuality(EVirtualSensorSimulationQuality NewQuality)
{
    SimulationQuality = NewQuality;
    if (SimulationQuality == EVirtualSensorSimulationQuality::Debug) { HorizontalSamples = 60; VerticalChannels = 8; ScanInterval = 0.5f; PreviewPointStride = 1; MaxPreviewPoints = 1000; }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::RealTimePreview) { HorizontalSamples = 120; VerticalChannels = 24; ScanInterval = 0.25f; PreviewPointStride = 2; MaxPreviewPoints = 3000; }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::Balanced) { HorizontalSamples = 240; VerticalChannels = 32; ScanInterval = 0.2f; PreviewPointStride = 3; MaxPreviewPoints = 5000; }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::FullSpec) { HorizontalSamples = 360; VerticalChannels = 60; ScanInterval = 0.1f; PreviewPointStride = 6; MaxPreviewPoints = 5000; }

    PointCloudPreviewStride = PreviewPointStride;
    MaxPointCloudPreviewInstances = MaxPreviewPoints;
    PayloadPointStride = ServerPayloadStride;
    MaxPayloadPoints = MaxServerPayloadPoints;
}

void UVirtualLidarSensorComp::ScanAndSend()
{
    ++FrameId;

    TArray<uint8> HeatmapPixels;
    ExecuteScan(LastPoints, HeatmapPixels);
    LastSlabAnalysis = AnalyzeSlabPoints(LastPoints);
    LastServerPayloadPointCount = CountServerPayloadPoints(LastPoints);
    LastPreviewPointCount = CountPreviewPoints(LastPoints);
    LastPerformanceWarning = BuildPerformanceWarning();
    UpdateLidarViewTexture(HeatmapPixels);
    RefreshPointCloudPreview();

    const FString JsonPayload = BuildJsonPayload(LastPoints);
    LastJsonPayload = JsonPayload;
    DispatchPayload(JsonPayload);
    UpdateRuntimeStatusAfterScan(JsonPayload.Len());

    if (RecorderComponent)
    {
        RecorderComponent->RecordJsonFrame(SensorId, TEXT("virtual_lidar"), FrameId, JsonPayload);
    }

    ExportEnabledPointCloudFormats();
    OnScanCompleted.Broadcast(JsonPayload, LidarViewTexture);
}

void UVirtualLidarSensorComp::InjectPointCloudFrame(const TArray<FVirtualLidarPoint>& Points, bool bSendTransport)
{
    ++FrameId;
    LastPoints = Points;
    LastSlabAnalysis = AnalyzeSlabPoints(LastPoints);
    LastServerPayloadPointCount = CountServerPayloadPoints(LastPoints);
    LastPreviewPointCount = CountPreviewPoints(LastPoints);
    LastPerformanceWarning = BuildPerformanceWarning();

    const int32 W = FMath::Max(1, HorizontalSamples);
    const int32 Hn = FMath::Max(1, VerticalChannels);
    TArray<uint8> HeatmapPixels;
    HeatmapPixels.SetNumZeroed(W * Hn * 4);
    const int32 MaxRenderablePoints = FMath::Min(LastPoints.Num(), W * Hn);
    for (int32 PointIndex = 0; PointIndex < MaxRenderablePoints; ++PointIndex)
    {
        WriteHeatmapPixel(HeatmapPixels, GetHeatmapPixelIndex(PointIndex % W, PointIndex / W, W, Hn), LastPoints[PointIndex]);
    }
    UpdateLidarViewTexture(HeatmapPixels);
    RefreshPointCloudPreview();

    const FString JsonPayload = BuildJsonPayload(LastPoints);
    LastJsonPayload = JsonPayload;
    if (bSendTransport)
    {
        DispatchPayload(JsonPayload);
    }
    UpdateRuntimeStatusAfterScan(JsonPayload.Len());

    if (RecorderComponent)
    {
        RecorderComponent->RecordJsonFrame(SensorId, TEXT("virtual_lidar_replay"), FrameId, JsonPayload);
    }

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
            FVirtualLidarPoint FirstPoint; FirstPoint.LocalDirection = GetComponentTransform().InverseTransformVectorNoScale(Dir).GetSafeNormal(); FirstPoint.Distance = MaxDistance; FirstPoint.WorldLocation = End; FirstPoint.bHit = false; FirstPoint.Row = V; FirstPoint.Col = X; FirstPoint.ReturnIndex = 0; FirstPoint.bHasGridCoord = true;
            if (bUseMultiHit)
            {
                TArray<FHitResult> Hits; World->LineTraceMultiByChannel(Hits, Origin, End, TraceChannel, Params); int32 Added = 0;
                for (const FHitResult& Hit : Hits)
                {
                    if (ShouldIgnoreHitActor(Hit.GetActor())) continue;
                    FVirtualLidarPoint P; P.LocalDirection = FirstPoint.LocalDirection; P.Row = FirstPoint.Row; P.Col = FirstPoint.Col; P.ReturnIndex = Added; P.bHasGridCoord = true; P.bHit = true; P.Distance = Hit.Distance; P.WorldLocation = Hit.ImpactPoint; PopulatePointSemanticMetadata(P, Hit); OutPoints.Add(P);
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

FVirtualLidarSlabAnalysisResult UVirtualLidarSensorComp::AnalyzeSlabPoints(const TArray<FVirtualLidarPoint>& Points) const
{
    FVirtualLidarSlabAnalysisResult Result;
    Result.ReferenceYawDegrees = ReferenceSlabYawDegrees;

    TArray<FVector> SlabPoints;
    SlabPoints.Reserve(Points.Num());
    for (const FVirtualLidarPoint& Point : Points)
    {
        if (Point.bHit && Point.SemanticLabel == SlabSemanticLabel)
        {
            SlabPoints.Add(Point.WorldLocation);
        }
    }

    Result.SlabHitPointCount = SlabPoints.Num();
    if (SlabPoints.Num() < FMath::Max(3, MinSlabPointsForAnalysis))
    {
        Result.StatusMessage = FString::Printf(TEXT("insufficient slab points: %d/%d"), SlabPoints.Num(), FMath::Max(3, MinSlabPointsForAnalysis));
        return Result;
    }

    FVector Sum = FVector::ZeroVector;
    Result.BoundsMin = FVector(FLT_MAX);
    Result.BoundsMax = FVector(-FLT_MAX);
    for (const FVector& Point : SlabPoints)
    {
        Sum += Point;
        Result.BoundsMin.X = FMath::Min(Result.BoundsMin.X, Point.X);
        Result.BoundsMin.Y = FMath::Min(Result.BoundsMin.Y, Point.Y);
        Result.BoundsMin.Z = FMath::Min(Result.BoundsMin.Z, Point.Z);
        Result.BoundsMax.X = FMath::Max(Result.BoundsMax.X, Point.X);
        Result.BoundsMax.Y = FMath::Max(Result.BoundsMax.Y, Point.Y);
        Result.BoundsMax.Z = FMath::Max(Result.BoundsMax.Z, Point.Z);
    }

    Result.Center = Sum / static_cast<float>(SlabPoints.Num());

    double XX = 0.0;
    double XY = 0.0;
    double YY = 0.0;
    for (const FVector& Point : SlabPoints)
    {
        const double DX = static_cast<double>(Point.X - Result.Center.X);
        const double DY = static_cast<double>(Point.Y - Result.Center.Y);
        XX += DX * DX;
        XY += DX * DY;
        YY += DY * DY;
    }

    const double Count = static_cast<double>(SlabPoints.Num());
    XX /= Count;
    XY /= Count;
    YY /= Count;

    const double Trace = XX + YY;
    const double Delta = FMath::Sqrt(FMath::Max(0.0, ((XX - YY) * (XX - YY) * 0.25) + (XY * XY)));
    const double LambdaMajor = Trace * 0.5 + Delta;
    const double LambdaMinor = Trace * 0.5 - Delta;

    FVector2D Axis(1.0f, 0.0f);
    if (FMath::Abs(XY) > KINDA_SMALL_NUMBER || FMath::Abs(LambdaMajor - XX) > KINDA_SMALL_NUMBER)
    {
        Axis = FVector2D(static_cast<float>(XY), static_cast<float>(LambdaMajor - XX)).GetSafeNormal();
    }

    Result.EstimatedYawDegrees = NormalizeAxisAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X)));
    Result.AngleDeviationDegrees = NormalizeAxisAngleDegrees(Result.EstimatedYawDegrees - ReferenceSlabYawDegrees);
    Result.Confidence = static_cast<float>(FMath::Clamp((LambdaMajor - LambdaMinor) / FMath::Max(1.0, LambdaMajor), 0.0, 1.0));
    Result.bValid = true;
    Result.StatusMessage = TEXT("ok");
    return Result;
}

int32 UVirtualLidarSensorComp::CountServerPayloadPoints(const TArray<FVirtualLidarPoint>& Points) const
{
    const int32 SafeStride = FMath::Max(1, ServerPayloadStride);
    const int32 SafeMax = FMath::Max(0, MaxServerPayloadPoints);
    int32 Count = 0;
    for (int32 Index = 0; Index < Points.Num(); Index += SafeStride)
    {
        if (SafeMax > 0 && Count >= SafeMax) break;
        if (!bIncludeMissPointsInServerPayload && !Points[Index].bHit) continue;
        ++Count;
    }
    return Count;
}

int32 UVirtualLidarSensorComp::CountPreviewPoints(const TArray<FVirtualLidarPoint>& Points) const
{
    const int32 SafeStride = FMath::Max(1, PreviewPointStride);
    const int32 SafeMax = FMath::Max(0, MaxPreviewPoints);
    int32 Count = 0;
    for (int32 Index = 0; Index < Points.Num(); Index += SafeStride)
    {
        if (SafeMax > 0 && Count >= SafeMax) break;
        if (bPointCloudPreviewHitOnly && !Points[Index].bHit) continue;
        ++Count;
    }
    return Count;
}

FString UVirtualLidarSensorComp::BuildPerformanceWarning() const
{
    TArray<FString> Warnings;
    if (SimulationQuality == EVirtualSensorSimulationQuality::FullSpec && bUseMultiHit)
    {
        Warnings.Add(TEXT("FullSpec+MultiHit is expensive"));
    }
    if (SimulationQuality == EVirtualSensorSimulationQuality::FullSpec && (bExportCsvOnScan || bExportJsonLinesOnScan || bExportPcdOnScan))
    {
        Warnings.Add(TEXT("FullSpec export-on-scan may stall the editor"));
    }
    if (bPointCloudPreviewEnabled && MaxPreviewPoints == 0 && PreviewPointStride <= 1)
    {
        Warnings.Add(TEXT("Preview is uncapped"));
    }
    if (MaxServerPayloadPoints == 0)
    {
        Warnings.Add(TEXT("Server payload is uncapped (MaxServerPayloadPoints=0)"));
    }
    if (IsGpuPreviewBackendRequested())
    {
        Warnings.Add(TEXT("GPU preview backend is a candidate only; CPU fallback is active"));
    }
    return FString::Join(Warnings, TEXT("; "));
}

FString UVirtualLidarSensorComp::BuildJsonPayload(const TArray<FVirtualLidarPoint>& Points) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    int32 HitPointCount = 0;
    for (const FVirtualLidarPoint& Point : Points)
    {
        if (Point.bHit)
        {
            ++HitPointCount;
        }
    }

    Root->SetStringField(TEXT("schemaVersion"), TEXT("virtual-lidar.v1"));
    Root->SetStringField(TEXT("sensorType"), TEXT("virtual_lidar"));
    Root->SetStringField(TEXT("sensorId"), SensorId);
    Root->SetStringField(TEXT("manufacturer"), DeviceSpec.Manufacturer);
    Root->SetStringField(TEXT("model"), DeviceSpec.Model);
    Root->SetNumberField(TEXT("frameId"), static_cast<double>(FrameId));
    Root->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetNumberField(TEXT("horizontalSamples"), HorizontalSamples);
    Root->SetNumberField(TEXT("verticalChannels"), VerticalChannels);
    Root->SetNumberField(TEXT("rayCount"), HorizontalSamples * VerticalChannels);
    Root->SetNumberField(TEXT("totalPointCount"), Points.Num());
    Root->SetNumberField(TEXT("hitPointCount"), HitPointCount);
    Root->SetNumberField(TEXT("maxDistance"), MaxDistance);
    Root->SetBoolField(TEXT("semanticClassification"), bEnableSemanticClassification);
    Root->SetNumberField(TEXT("serverPayloadStride"), ServerPayloadStride);
    Root->SetNumberField(TEXT("maxServerPayloadPoints"), MaxServerPayloadPoints);
    Root->SetBoolField(TEXT("includeMissPointsInServerPayload"), bIncludeMissPointsInServerPayload);
    Root->SetNumberField(TEXT("previewPointStride"), PreviewPointStride);
    Root->SetNumberField(TEXT("maxPreviewPoints"), MaxPreviewPoints);
    Root->SetStringField(TEXT("previewBackend"), GetPreviewBackendName());
    Root->SetBoolField(TEXT("gpuPreviewBackendRequested"), IsGpuPreviewBackendRequested());
    Root->SetBoolField(TEXT("gpuPreviewBackendActive"), IsGpuPreviewBackendActive());

    TSharedRef<FJsonObject> PayloadPolicyObject = MakeShared<FJsonObject>();
    PayloadPolicyObject->SetNumberField(TEXT("stride"), ServerPayloadStride);
    PayloadPolicyObject->SetNumberField(TEXT("maxPoints"), MaxServerPayloadPoints);
    PayloadPolicyObject->SetBoolField(TEXT("includeMissPoints"), bIncludeMissPointsInServerPayload);
    PayloadPolicyObject->SetStringField(TEXT("pointSelection"), bIncludeMissPointsInServerPayload ? TEXT("hit_and_miss") : TEXT("hit_only"));
    Root->SetObjectField(TEXT("payloadPolicy"), PayloadPolicyObject);

    TSharedRef<FJsonObject> PreviewPolicyObject = MakeShared<FJsonObject>();
    PreviewPolicyObject->SetNumberField(TEXT("stride"), PreviewPointStride);
    PreviewPolicyObject->SetNumberField(TEXT("maxPoints"), MaxPreviewPoints);
    PreviewPolicyObject->SetBoolField(TEXT("hitOnly"), bPointCloudPreviewHitOnly);
    PreviewPolicyObject->SetStringField(TEXT("backend"), GetPreviewBackendName());
    PreviewPolicyObject->SetBoolField(TEXT("gpuRequested"), IsGpuPreviewBackendRequested());
    PreviewPolicyObject->SetBoolField(TEXT("gpuActive"), IsGpuPreviewBackendActive());
    PreviewPolicyObject->SetBoolField(TEXT("experimentalGpuOptIn"), bAllowExperimentalGpuPreviewBackend);
    PreviewPolicyObject->SetStringField(TEXT("activePath"), IsGpuPreviewBackendActive() ? TEXT("gpu") : TEXT("cpu_instanced_mesh_fallback"));
    Root->SetObjectField(TEXT("previewPolicy"), PreviewPolicyObject);

    TSharedRef<FJsonObject> TransformObject = MakeShared<FJsonObject>();
    AddVectorArray(TransformObject, TEXT("location"), GetComponentLocation());
    const FRotator Rotation = GetComponentRotation();
    TArray<TSharedPtr<FJsonValue>> RotationJson;
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    TransformObject->SetArrayField(TEXT("rotation"), RotationJson);
    AddVectorArray(TransformObject, TEXT("forward"), GetForwardVector());
    AddVectorArray(TransformObject, TEXT("up"), GetUpVector());
    Root->SetObjectField(TEXT("sensorTransform"), TransformObject);

    if (bIncludeSlabAnalysisInPayload)
    {
        TSharedRef<FJsonObject> SlabObject = MakeShared<FJsonObject>();
        SlabObject->SetBoolField(TEXT("valid"), LastSlabAnalysis.bValid);
        SlabObject->SetNumberField(TEXT("slabHitPointCount"), LastSlabAnalysis.SlabHitPointCount);
        AddVectorArray(SlabObject, TEXT("boundsMin"), LastSlabAnalysis.BoundsMin);
        AddVectorArray(SlabObject, TEXT("boundsMax"), LastSlabAnalysis.BoundsMax);
        AddVectorArray(SlabObject, TEXT("center"), LastSlabAnalysis.Center);
        SlabObject->SetNumberField(TEXT("estimatedYawDegrees"), LastSlabAnalysis.EstimatedYawDegrees);
        SlabObject->SetNumberField(TEXT("referenceYawDegrees"), LastSlabAnalysis.ReferenceYawDegrees);
        SlabObject->SetNumberField(TEXT("angleDeviationDegrees"), LastSlabAnalysis.AngleDeviationDegrees);
        SlabObject->SetNumberField(TEXT("confidence"), LastSlabAnalysis.Confidence);
        SlabObject->SetStringField(TEXT("status"), LastSlabAnalysis.StatusMessage);
        Root->SetObjectField(TEXT("slabAnalysis"), SlabObject);
    }

    TArray<TSharedPtr<FJsonValue>> JsonPoints;
    const int32 SafeStride = FMath::Max(1, ServerPayloadStride);
    const int32 SafeMax = FMath::Max(0, MaxServerPayloadPoints);
    int32 Added = 0;
    for (int32 I = 0; I < Points.Num(); I += SafeStride)
    {
        if (SafeMax > 0 && Added >= SafeMax)
        {
            break;
        }

        const FVirtualLidarPoint& P = Points[I];
        if (!bIncludeMissPointsInServerPayload && !P.bHit)
        {
            continue;
        }

        const int32 DerivedRow = HorizontalSamples > 0 ? I / HorizontalSamples : 0;
        const int32 DerivedCol = HorizontalSamples > 0 ? I % HorizontalSamples : I;
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("pointIndex"), I);
        O->SetNumberField(TEXT("row"), P.bHasGridCoord ? P.Row : DerivedRow);
        O->SetNumberField(TEXT("col"), P.bHasGridCoord ? P.Col : DerivedCol);
        O->SetNumberField(TEXT("returnIndex"), P.ReturnIndex);
        O->SetBoolField(TEXT("gridCoordValid"), P.bHasGridCoord);
        O->SetStringField(TEXT("gridCoordSource"), P.bHasGridCoord ? TEXT("point_metadata") : TEXT("derived_from_point_index"));
        O->SetBoolField(TEXT("hit"), P.bHit);
        O->SetNumberField(TEXT("distance"), P.Distance);
        O->SetStringField(TEXT("hitActor"), P.HitActorName.ToString());
        O->SetStringField(TEXT("hitActorClass"), P.HitActorClassName.ToString());
        O->SetStringField(TEXT("semanticLabel"), P.SemanticLabel.ToString());

        TArray<TSharedPtr<FJsonValue>> Tags;
        for (const FName& T : P.HitActorTags)
        {
            Tags.Add(MakeShared<FJsonValueString>(T.ToString()));
        }
        O->SetArrayField(TEXT("hitActorTags"), Tags);
        AddVectorArray(O, TEXT("worldLocation"), P.WorldLocation);
        AddVectorArray(O, TEXT("localDirection"), P.LocalDirection);
        JsonPoints.Add(MakeShared<FJsonValueObject>(O));
        ++Added;
    }
    Root->SetNumberField(TEXT("payloadPointCount"), Added);
    Root->SetArrayField(TEXT("points"), JsonPoints);

    FString Out;
    TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root, W);
    return Out;
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
    RuntimeStatus.ServerPayloadPointCount = LastServerPayloadPointCount;
    RuntimeStatus.PreviewPointCount = LastPreviewPointCount;
    RuntimeStatus.PerformanceWarning = LastPerformanceWarning;
    RuntimeStatus.SlabAnalysis = LastSlabAnalysis;

    for (const FVirtualLidarPoint& Point : LastPoints)
    {
        if (Point.bHit)
        {
            ++RuntimeStatus.HitPointCount;
        }
    }

    RuntimeStatus.LastMessage = FString::Printf(
        TEXT("Quality=%d Rays=%d Hits=%d ServerPoints=%d ServerStride=%d MaxServer=%d Preview=%s PreviewPoints=%d PreviewStride=%d PreviewBackend=%s GpuActive=%s Slab=%s Angle=%.2f Dev=%.2f Conf=%.2f Warning=%s"),
        static_cast<int32>(SimulationQuality),
        HorizontalSamples * VerticalChannels,
        RuntimeStatus.HitPointCount,
        RuntimeStatus.ServerPayloadPointCount,
        ServerPayloadStride,
        MaxServerPayloadPoints,
        bPointCloudPreviewEnabled ? TEXT("On") : TEXT("Off"),
        RuntimeStatus.PreviewPointCount,
        PreviewPointStride,
        *GetPreviewBackendName(),
        IsGpuPreviewBackendActive() ? TEXT("true") : TEXT("false"),
        LastSlabAnalysis.bValid ? TEXT("Valid") : TEXT("Invalid"),
        LastSlabAnalysis.EstimatedYawDegrees,
        LastSlabAnalysis.AngleDeviationDegrees,
        LastSlabAnalysis.Confidence,
        LastPerformanceWarning.IsEmpty() ? TEXT("None") : *LastPerformanceWarning);
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
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("PointCloud")); IFileManager::Get().MakeDirectory(*Dir, true); const FString Prefix = FileNamePrefix.IsEmpty() ? SensorId : FileNamePrefix; const FDateTime N = FDateTime::UtcNow(); const FString Ts = FString::Printf(TEXT("%s_%03d_%lld"), *N.ToString(TEXT("%Y%m%d_%H%M%S")), N.GetMillisecond(), N.GetTicks()); LastPointCloudExportPath = FPaths::Combine(Dir, FString::Printf(TEXT("%s_%s.%s"), *Prefix, *Ts, *Extension)); return LastPointCloudExportPath;
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
    return ExportLastPointCloudLasToPath(BuildExportPath(TEXT("las"), FileNamePrefix));
}

bool UVirtualLidarSensorComp::ExportLastPointCloudLasToPath(const FString& Path) const
{
    TArray<const FVirtualLidarPoint*> Points; CollectExportPoints(Points); if (Points.Num() <= 0) return false;
    FVector Min(FLT_MAX), Max(-FLT_MAX); for (const FVirtualLidarPoint* P : Points) if (P) { Min.X = FMath::Min(Min.X, P->WorldLocation.X); Min.Y = FMath::Min(Min.Y, P->WorldLocation.Y); Min.Z = FMath::Min(Min.Z, P->WorldLocation.Z); Max.X = FMath::Max(Max.X, P->WorldLocation.X); Max.Y = FMath::Max(Max.Y, P->WorldLocation.Y); Max.Z = FMath::Max(Max.Z, P->WorldLocation.Z); }
    const double Scale = 0.001, CmToM = 0.01, OX = Min.X * CmToM, OY = Min.Y * CmToM, OZ = Min.Z * CmToM; FBufferArchive A; const uint8 Sig[4] = {'L','A','S','F'}; WriteLasBytes(A, Sig, 4); WriteLasValue<uint16>(A,0); WriteLasValue<uint16>(A,0); WriteLasValue<uint32>(A,0); WriteLasValue<uint16>(A,0); WriteLasValue<uint16>(A,0); for(int32 i=0;i<8;++i) WriteLasValue<uint8>(A,0); WriteLasValue<uint8>(A,1); WriteLasValue<uint8>(A,2); WriteLasFixedString(A,"UE-DT-Project",32); WriteLasFixedString(A,"VirtualLidar",32); const FDateTime Now = FDateTime::Now(); WriteLasValue<uint16>(A,(uint16)Now.GetDayOfYear()); WriteLasValue<uint16>(A,(uint16)Now.GetYear()); WriteLasValue<uint16>(A,227); WriteLasValue<uint32>(A,227); WriteLasValue<uint32>(A,0); WriteLasValue<uint8>(A,0); WriteLasValue<uint16>(A,20); WriteLasValue<uint32>(A,(uint32)Points.Num()); WriteLasValue<uint32>(A,(uint32)Points.Num()); for(int32 i=1;i<5;++i) WriteLasValue<uint32>(A,0); WriteLasValue<double>(A,Scale); WriteLasValue<double>(A,Scale); WriteLasValue<double>(A,Scale); WriteLasValue<double>(A,OX); WriteLasValue<double>(A,OY); WriteLasValue<double>(A,OZ); WriteLasValue<double>(A,Max.X*CmToM); WriteLasValue<double>(A,Min.X*CmToM); WriteLasValue<double>(A,Max.Y*CmToM); WriteLasValue<double>(A,Min.Y*CmToM); WriteLasValue<double>(A,Max.Z*CmToM); WriteLasValue<double>(A,Min.Z*CmToM);
    for (const FVirtualLidarPoint* P : Points) if (P) { const int32 X=(int32)FMath::RoundToDouble(((P->WorldLocation.X*CmToM)-OX)/Scale); const int32 Y=(int32)FMath::RoundToDouble(((P->WorldLocation.Y*CmToM)-OY)/Scale); const int32 Z=(int32)FMath::RoundToDouble(((P->WorldLocation.Z*CmToM)-OZ)/Scale); WriteLasValue<int32>(A,X); WriteLasValue<int32>(A,Y); WriteLasValue<int32>(A,Z); WriteLasValue<uint16>(A,ClampDistanceToIntensity(P->Distance,MaxDistance)); WriteLasValue<uint8>(A,1); WriteLasValue<uint8>(A,1); WriteLasValue<int8>(A,0); WriteLasValue<uint8>(A,0); WriteLasValue<uint16>(A,0); }
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

bool UVirtualLidarSensorComp::RunExternalLazCompressor(const FString& LasSourcePath, const FString& LazOutputPath) const
{
    bLastLazExternalCompressorAttempted = true;
    bLastLazExternalCompressorSucceeded = false;
    bLastLazExportPlaceholderOnly = false;
    LastLazLasSourcePath = FPaths::ConvertRelativePathToFull(LasSourcePath);
    LastLazOutputPath = FPaths::ConvertRelativePathToFull(LazOutputPath);

    if (!bUseExternalLazCompressor)
    {
        LastLazExportWarningText = TEXT("ExternalCompressorDisabled");
        LastLazExportStatusText = TEXT("ExternalCompressorDisabled");
        return false;
    }
    if (ExternalLazCompressorPath.IsEmpty() || !FPaths::FileExists(ExternalLazCompressorPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] External LAZ compressor is enabled but executable is missing: %s"), *SensorId, *ExternalLazCompressorPath);
        LastLazExportWarningText = FString::Printf(TEXT("External LAZ compressor executable is missing: %s"), *ExternalLazCompressorPath);
        LastLazExportStatusText = FString::Printf(TEXT("ExternalCompressorMissing: %s"), *ExternalLazCompressorPath);
        return false;
    }

    FString Arguments = ExternalLazCompressorArguments.IsEmpty()
        ? TEXT("-i {input} -o {output}")
        : ExternalLazCompressorArguments;
    if (!Arguments.Contains(TEXT("{input}")) || !Arguments.Contains(TEXT("{output}")))
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] External LAZ compressor arguments must include both {input} and {output}: %s"), *SensorId, *Arguments);
        LastLazExportWarningText = TEXT("External LAZ compressor arguments must include both {input} and {output}.");
        LastLazExportStatusText = TEXT("ExternalCompressorArgumentsMissingPlaceholders");
        return false;
    }
    FString FullLasSourcePath = FPaths::ConvertRelativePathToFull(LasSourcePath);
    FString FullLazOutputPath = FPaths::ConvertRelativePathToFull(LazOutputPath);
    FPaths::MakePlatformFilename(FullLasSourcePath);
    FPaths::MakePlatformFilename(FullLazOutputPath);
    if (FullLasSourcePath == FullLazOutputPath)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] External LAZ output path must differ from LAS source path: %s"), *SensorId, *LazOutputPath);
        LastLazExportWarningText = TEXT("External LAZ output path must differ from LAS source path.");
        LastLazExportStatusText = TEXT("ExternalCompressorOutputPathMatchesSource");
        return false;
    }
    Arguments.ReplaceInline(TEXT("{input}"), *FString::Printf(TEXT("\"%s\""), *FullLasSourcePath));
    Arguments.ReplaceInline(TEXT("{output}"), *FString::Printf(TEXT("\"%s\""), *FullLazOutputPath));

    int32 ReturnCode = INDEX_NONE;
    FString StdOut;
    FString StdErr;
    UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] Running external LAZ compressor: %s %s"), *SensorId, *ExternalLazCompressorPath, *Arguments);
    const bool bProcessRan = FPlatformProcess::ExecProcess(*ExternalLazCompressorPath, *Arguments, &ReturnCode, &StdOut, &StdErr);
    const bool bOutputExists = FPaths::FileExists(FullLazOutputPath);
    const int64 OutputSize = bOutputExists ? IFileManager::Get().FileSize(*FullLazOutputPath) : 0;
    LastLazExternalCompressorReturnCode = ReturnCode;
    LastLazOutputSizeBytes = OutputSize;
    bLastLazProducedOutputFile = bOutputExists && OutputSize > 0;
    if (!bProcessRan || ReturnCode != 0 || !bOutputExists || OutputSize <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] External LAZ compressor failed. ran=%s code=%d outputExists=%s outputSize=%lld stdout=%s stderr=%s"), *SensorId, bProcessRan ? TEXT("true") : TEXT("false"), ReturnCode, bOutputExists ? TEXT("true") : TEXT("false"), OutputSize, *StdOut, *StdErr);
        LastLazExportWarningText = FString::Printf(TEXT("External LAZ compressor failed. ran=%s code=%d outputExists=%s outputSize=%lld"),
            bProcessRan ? TEXT("true") : TEXT("false"),
            ReturnCode,
            bOutputExists ? TEXT("true") : TEXT("false"),
            OutputSize);
        LastLazExportStatusText = FString::Printf(TEXT("ExternalCompressorFailed: ran=%s code=%d outputExists=%s outputSize=%lld"),
            bProcessRan ? TEXT("true") : TEXT("false"),
            ReturnCode,
            bOutputExists ? TEXT("true") : TEXT("false"),
            OutputSize);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] LAZ saved: %s source=%s"), *SensorId, *FullLazOutputPath, *FullLasSourcePath);
    bLastLazExternalCompressorSucceeded = true;
    bLastLazProducedOutputFile = true;
    bLastLazExportSucceeded = true;
    LastLazOutputPath = FullLazOutputPath;
    LastLazLasSourcePath = FullLasSourcePath;
    LastLazExportStatusText = FString::Printf(TEXT("ExternalCompressorSucceeded: output=%s source=%s"), *FullLazOutputPath, *FullLasSourcePath);
    return true;
}

bool UVirtualLidarSensorComp::ExportLastPointCloudLaz(const FString& FileNamePrefix) const
{
    ResetLastLazExportStatus(TEXT("NotStarted"));
    bLastLazExportAttempted = true;
    bLastLazExternalCompressorRequested = bUseExternalLazCompressor;
    TArray<const FVirtualLidarPoint*> ExportPoints;
    CollectExportPoints(ExportPoints);
    LastLazExportedPointCount = ExportPoints.Num();
    const FString Prefix = FileNamePrefix.IsEmpty() ? TEXT("laz_source") : FileNamePrefix + TEXT("_laz_source");
    const FString LasSourcePath = BuildExportPath(TEXT("las"), Prefix);
    LastLazLasSourcePath = FPaths::ConvertRelativePathToFull(LasSourcePath);
    if (!ExportLastPointCloudLasToPath(LasSourcePath))
    {
        LastLazExportWarningText = TEXT("LAS source write failed before LAZ compression could run.");
        LastLazExportStatusText = TEXT("LasSourceWriteFailed");
        return false;
    }

    if (!bUseExternalLazCompressor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] LAZ compression is not integrated. Writing LAS-compatible source file with prefix '%s'."), *SensorId, *Prefix);
        bLastLazExportPlaceholderOnly = true;
        bLastLazExportSucceeded = true;
        LastLazExportWarningText = TEXT("LAZ compression is not integrated; wrote LAS-compatible source only.");
        LastLazExportStatusText = FString::Printf(TEXT("PlaceholderOnlyLasSource: %s"), *LastLazLasSourcePath);
        return true;
    }

    const FString LazOutputPath = BuildExportPath(TEXT("laz"), FileNamePrefix);
    return RunExternalLazCompressor(LasSourcePath, LazOutputPath);
}
bool UVirtualLidarSensorComp::ExportLastPointCloudCsvLasLaz(const FString& FileNamePrefix) const { const bool A = ExportLastPointCloudCsv(FileNamePrefix); const bool B = ExportLastPointCloudLas(FileNamePrefix); const bool C = ExportLastPointCloudLaz(FileNamePrefix); return A || B || C; }

void UVirtualLidarSensorComp::ResetLastLazExportStatus(const FString& StatusText) const
{
    LastLazExportStatusText = StatusText;
    LastLazExportWarningText.Reset();
    LastLazLasSourcePath.Reset();
    LastLazOutputPath.Reset();
    bLastLazExportAttempted = false;
    bLastLazExportSucceeded = false;
    bLastLazExportPlaceholderOnly = false;
    bLastLazExternalCompressorRequested = false;
    bLastLazExternalCompressorAttempted = false;
    bLastLazExternalCompressorSucceeded = false;
    bLastLazProducedOutputFile = false;
    bLastLazTrueCompressionValidated = false;
    LastLazExportedPointCount = 0;
    LastLazExternalCompressorReturnCode = INDEX_NONE;
    LastLazOutputSizeBytes = 0;
}

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
    const int32 Stride = FMath::Max(1, PreviewPointStride); int32 Added = 0;
    const int32 PreviewCapacity = MaxPreviewPoints > 0 ? FMath::Min(MaxPreviewPoints, LastPoints.Num()) : LastPoints.Num();
    TArray<FTransform> InstanceTransforms;
    InstanceTransforms.Reserve(PreviewCapacity);
    for (int32 I = 0; I < LastPoints.Num(); I += Stride)
    {
        const FVirtualLidarPoint& P = LastPoints[I]; if (bPointCloudPreviewHitOnly && !P.bHit) continue; if (MaxPreviewPoints > 0 && Added >= MaxPreviewPoints) break;
        InstanceTransforms.Add(FTransform(FRotator::ZeroRotator, P.WorldLocation, FVector(PointCloudPreviewPointScale)));
        if (bDrawPointCloudPreviewDebugPoints) DrawDebugPoint(World, P.WorldLocation, PointCloudPreviewDebugPointSize, (bUseSemanticColorInPointCloudPreview && P.bHit) ? ResolveSemanticColor(P).ToFColor(true) : PointCloudPreviewColor.ToFColor(true), false, ScanInterval);
        ++Added;
    }
    if (InstanceTransforms.Num() > 0)
    {
        Comp->AddInstances(InstanceTransforms, false, true);
        Comp->MarkRenderStateDirty();
    }
}

void UVirtualLidarSensorComp::TryAutoRegisterToManager()
{
    if (!bAutoRegisterToManager || !GetWorld()) return; for (TActorIterator<AVirtualSensorManager> It(GetWorld()); It; ++It) { if (*It) { It->RegisterLidar(this); return; } }
}
