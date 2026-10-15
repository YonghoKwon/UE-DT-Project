#include "VirtualLidarScanComponent.h"
#include "VirtualSensorSemanticLabels.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"

#include "Async/Async.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
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
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarPayloadCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "Misc/Crc.h"
#include <atomic>

namespace
{

float NormalizeSignedAngleDegrees(float Angle) { while (Angle > 180.0f) Angle -= 360.0f; while (Angle < -180.0f) Angle += 360.0f; return Angle; }
float NormalizeAxisAngleDegrees(float Angle) { Angle = NormalizeSignedAngleDegrees(Angle); if (Angle > 90.0f) Angle -= 180.0f; if (Angle < -90.0f) Angle += 180.0f; return Angle; }
}


void UVirtualLidarScanComponent::BuildBeamAngleTables(
    int32 InHorizontalSamples,
    int32 InVerticalChannels,
    TArray<float>& OutHorizontalAngles,
    TArray<float>& OutVerticalAngles) const
{
    const int32 SafeHorizontalSamples = FMath::Max(1, InHorizontalSamples);
    const int32 SafeVerticalChannels = FMath::Max(1, InVerticalChannels);
    OutHorizontalAngles.SetNumUninitialized(SafeHorizontalSamples);
    OutVerticalAngles.SetNumUninitialized(SafeVerticalChannels);

    const bool bUseHorizontalCalibration = HorizontalCalibrationAnglesDegrees.Num() == SafeHorizontalSamples;
    const bool bUseVerticalCalibration = VerticalCalibrationAnglesDegrees.Num() == SafeVerticalChannels;
    for (int32 Column = 0; Column < SafeHorizontalSamples; ++Column)
    {
        OutHorizontalAngles[Column] = bUseHorizontalCalibration
            ? HorizontalCalibrationAnglesDegrees[Column]
            : FMath::Lerp(
                -HorizontalFov * 0.5f,
                HorizontalFov * 0.5f,
                SafeHorizontalSamples == 1 ? 0.5f : static_cast<float>(Column) / static_cast<float>(SafeHorizontalSamples - 1));
    }
    for (int32 Ring = 0; Ring < SafeVerticalChannels; ++Ring)
    {
        OutVerticalAngles[Ring] = bUseVerticalCalibration
            ? VerticalCalibrationAnglesDegrees[Ring]
            : FMath::Lerp(
                MinVerticalAngle,
                MaxVerticalAngle,
                SafeVerticalChannels == 1 ? 0.5f : static_cast<float>(Ring) / static_cast<float>(SafeVerticalChannels - 1));
    }
}

float UVirtualLidarScanComponent::DeterministicUnitRandom(int32 RayIndex, int32 ReturnIndex, uint32 Salt) const
{
    uint32 Value = HashCombineFast(
        (DatasetNoiseSequence ? HashCombineFast(GetTypeHash(DatasetNoiseSequence->Key),GetTypeHash(DatasetNoiseSequence->Value)) : HashCombineFast(GetTypeHash(DeterministicNoiseSeed),GetTypeHash(FrameId+1))),
        HashCombineFast(GetTypeHash(RayIndex), HashCombineFast(GetTypeHash(ReturnIndex), Salt)));
    Value ^= Value << 13;
    Value ^= Value >> 17;
    Value ^= Value << 5;
    return static_cast<float>(Value & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float UVirtualLidarScanComponent::DeterministicGaussian(int32 RayIndex, int32 ReturnIndex) const
{
    const float U1 = FMath::Max(1.0e-6f, DeterministicUnitRandom(RayIndex, ReturnIndex, 0x4d4c5831u));
    const float U2 = DeterministicUnitRandom(RayIndex, ReturnIndex, 0x4d4c5832u);
    return FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(2.0f * PI * U2);
}

void UVirtualLidarScanComponent::FinalizeEchoMetadata(TArray<FVirtualLidarPoint>& Points, int32 StartIndex, int32 EchoCount) const
{
    for (int32 Echo = 0; Echo < EchoCount; ++Echo)
    {
        FVirtualLidarPoint& Point = Points[StartIndex + Echo];
        Point.EchoCount = EchoCount;
        Point.EchoIndex = Echo;
        Point.ReturnIndex = Echo;
        if (EchoCount <= 1)
        {
            Point.EchoType = EVirtualLidarEchoType::Single;
        }
        else if (Echo == 0)
        {
            Point.EchoType = EVirtualLidarEchoType::First;
        }
        else if (Echo == EchoCount - 1)
        {
            Point.EchoType = EVirtualLidarEchoType::Last;
        }
        else
        {
            Point.EchoType = EVirtualLidarEchoType::Strongest;
        }
    }
}

void UVirtualLidarScanComponent::SetInteractivePreviewMode(bool bEnabled, bool bSuppressDerivedOutput)
{
	bInteractivePreviewMode = bEnabled;
	bSuppressInteractiveDerivedOutput = bSuppressDerivedOutput;
}

EVirtualLidarAcquisitionBackend UVirtualLidarScanComponent::ResolveAcquisitionBackend(FString& OutReason) const
{
    OutReason.Reset();
    const bool bGpuAvailable = GpuDepthProjectionComponent.IsValid()
        && GpuDepthProjectionComponent->IsAvailable();

    if (AcquisitionBackend == EVirtualLidarAcquisitionBackend::AccurateCpuTrace)
    {
        return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
    }
    if (AcquisitionBackend == EVirtualLidarAcquisitionBackend::GpuDepthProjection)
    {
        if (bGpuAvailable) return EVirtualLidarAcquisitionBackend::GpuDepthProjection;
        OutReason = TEXT("GPU depth backend unavailable; accurate CPU trace fallback is active");
        return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
    }
    if (AcquisitionBackend == EVirtualLidarAcquisitionBackend::HardwareRayTracing)
    {
        OutReason = TEXT("hardware ray tracing backend is not implemented; accurate CPU trace fallback is active");
        return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
    }
    if (AcquisitionBackend == EVirtualLidarAcquisitionBackend::ReplayOrExternal)
    {
        OutReason = TEXT("replay/external backend has no automatic acquisition; CPU fallback is active");
        return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
    }

    const bool bDenseSolidStateProfile =
        DeviceProfile == EVirtualLidarDeviceProfile::IYOBOT_MLX80
        || DeviceProfile == EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;
    if (bDenseSolidStateProfile && bGpuAvailable && !bInteractivePreviewMode)
    {
        return EVirtualLidarAcquisitionBackend::GpuDepthProjection;
    }
    if (bDenseSolidStateProfile && !bGpuAvailable)
    {
        OutReason = TEXT("Auto requested GPU depth for ML-X but this RHI/world cannot render; accurate CPU trace fallback is active");
    }
    return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
}

bool UVirtualLidarScanComponent::IsSemanticCaptureRequested() const
{
    if(bDatasetSemanticDemand)return true;
    if(bEnableSemanticClassification&&ViewMode==EVirtualLidarViewMode::ActorClassColor)return true;
    if(!GetWorld())return false;
    for(TActorIterator<AVirtualSensorCoordinator> It(GetWorld());It;++It)
    {
        const auto* Publisher=It->StreamPublisherComponent.Get();if(!Publisher||!Publisher->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,SensorId))continue;
        const auto Filter=Publisher->GetEffectiveStreamConfig(EVirtualSensorStreamKind::PointCloud,SensorId).PointCloudFilter;
        if(Filter.IncludeActorTags.Num()||Filter.ExcludeActorTags.Num()||Filter.IncludeSemanticLabels.Num()||Filter.ExcludeSemanticLabels.Num())return true;
    }
    return false;
}

void UVirtualLidarScanComponent::RegisterWithPerformanceSubsystem()
{
    if (!GetWorld() || bRegisteredWithPerformanceSubsystem) return;
    if (UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>())
    {
        Subsystem->RegisterTask(this);
        bRegisteredWithPerformanceSubsystem = true;
    }
}

void UVirtualLidarScanComponent::UnregisterFromPerformanceSubsystem()
{
    if (!GetWorld() || !bRegisteredWithPerformanceSubsystem) return;
    if (UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>()) Subsystem->UnregisterTask(this);
    bRegisteredWithPerformanceSubsystem = false;
}

void UVirtualLidarScanComponent::SetCompatibilityOutputPolicy(EVirtualLidarCompatibilityOutputPolicy Policy)
{ CompatibilityOutputPolicy=Policy;TryStartPendingPayloadBuild(); }
FString UVirtualLidarScanComponent::RegisterCompatibilityJsonConsumer(UObject* Owner)
{
    if(!IsValid(Owner))return FString();
    for(auto It=CompatibilityConsumers.CreateIterator();It;++It)if(!It.Value().IsValid())It.RemoveCurrent();
    for(const auto& Pair:CompatibilityConsumers)if(Pair.Value.Get()==Owner)return Pair.Key;
    const FString Token=FGuid::NewGuid().ToString();CompatibilityConsumers.Add(Token,Owner);return Token;
}
void UVirtualLidarScanComponent::UnregisterCompatibilityJsonConsumer(const FString& Token)
{CompatibilityConsumers.Remove(Token);}
bool UVirtualLidarScanComponent::HasCompatibilityJsonDemand(const FVirtualSensorAcquisitionContext& Context,FString* Reason) const
{
    auto Yes=[Reason](const TCHAR* Text){if(Reason)*Reason=Text;return true;};
    if(CompatibilityOutputPolicy==EVirtualLidarCompatibilityOutputPolicy::LegacyAlways)return Yes(TEXT("기존 호환 JSON 항상 생성"));
    if((TransportComponent&&TransportComponent->TransportMode!=EVirtualSensorTransportMode::None)||OutputMode!=EVirtualLidarOutputMode::None)return Yes(TEXT("기존 JSON 출력 경로"));
    if(RecorderComponent&&RecorderComponent->IsRecording())return Yes(TEXT("JSON 녹화 중"));
    if(OnScanCompleted.IsBound())return Yes(TEXT("Blueprint JSON 완료 구독"));
    for(const auto& Pair:CompatibilityConsumers)if(Pair.Value.IsValid())return Yes(TEXT("등록된 JSON 소비자"));
    if(const auto* Actor=Cast<AVirtualLidarSensorActor>(GetOwner()))if(const auto* Output=Actor->OutputComponent.Get())
    {
        if(Output->RecorderComponent&&Output->RecorderComponent->IsRecording())return Yes(TEXT("공유 JSON 녹화 중"));
        if(Output->OnFrameRouted.IsBound())return Yes(TEXT("Blueprint 출력 완료 구독"));
        if(Output->StreamPublisherComponent&&Output->StreamPublisherComponent->NeedsCompatibilityLidarOutputForContext(SensorId,Context))return Yes(TEXT("호환 LiDAR 스트림"));
        if(!Output->StreamPublisherComponent&&Output->TransportComponent&&Output->TransportComponent->TransportMode!=EVirtualSensorTransportMode::None)return Yes(TEXT("기존 Actor JSON 전송"));
    }
    if(Reason)*Reason=TEXT("미사용 · PCD/미리보기는 정상 동작");return false;
}
void UVirtualLidarScanComponent::CompleteDerivedFrame(const FVirtualSensorFrameEnvelope& Frame)
{LastCompletedDerivedFrame=Frame;OnDerivedFrameCompletedNative.Broadcast(Frame);}
void UVirtualLidarScanComponent::SetTransportComponent(UVirtualSensorTransportComponent* InTransportComponent) { TransportComponent = InTransportComponent; }
void UVirtualLidarScanComponent::SetRecorderComponent(UVirtualSensorRecorderComponent* InRecorderComponent) { RecorderComponent = InRecorderComponent; }

void UVirtualLidarScanComponent::SetServerPayloadPolicy(int32 InStride, int32 InMaxPoints, bool bInIncludeMissPoints)
{
    ServerPayloadStride = FMath::Clamp(InStride, 1, 100);
    MaxServerPayloadPoints = FMath::Clamp(InMaxPoints, 0, 1000000);
    bIncludeMissPointsInServerPayload = bInIncludeMissPoints;
    PayloadPointStride = ServerPayloadStride;
    MaxPayloadPoints = MaxServerPayloadPoints;
    bIncludeMissPointsInPayload = bIncludeMissPointsInServerPayload;
}

void UVirtualLidarScanComponent::SetPreviewPolicyState(const FVirtualLidarPreviewPolicy& InPolicy)
{
	SetPreviewPolicy(InPolicy.PointStride, InPolicy.MaxPoints, InPolicy.bHitOnly);
}

bool UVirtualLidarScanComponent::IsGpuPreviewBackendActive() const
{
    return bGpuPreviewBackendRuntimeActive;
}

void UVirtualLidarScanComponent::SetGpuPreviewBackendRuntimeState(bool bActive, const FString& InFallbackReason)
{
    bGpuPreviewBackendRuntimeActive = bActive;
    bGpuPreviewBackendRuntimeRequested = bActive || !InFallbackReason.IsEmpty();
    GpuPreviewFallbackReason = InFallbackReason;
}

void UVirtualLidarScanComponent::ResetDefaultSemanticClassRules()
{
    SemanticClassRules = VirtualSensorSemanticLabels::Defaults();
}

FLinearColor UVirtualLidarScanComponent::GetSemanticColorForLabel(FName SemanticLabel) const
{
    for (const FVirtualLidarSemanticClassRule& R : SemanticClassRules) { if (!R.Label.IsNone() && R.Label == SemanticLabel) return R.DisplayColor; }
    return DefaultSemanticColor;
}

bool UVirtualLidarScanComponent::SemanticRuleMatches(const FVirtualLidarSemanticClassRule& Rule, const AActor* Actor, const UPrimitiveComponent* Component) const
{
    return VirtualSensorSemanticLabels::Matches(Rule, Actor, Component);
}

FName UVirtualLidarScanComponent::ResolveSemanticLabel(const FHitResult& Hit) const
{
    return ResolveSemanticLabelForComponent(Hit.GetComponent());
}

FName UVirtualLidarScanComponent::ResolveSemanticLabelForComponent(const UPrimitiveComponent* Component) const
{
    const AActor* Actor = Component ? Component->GetOwner() : nullptr;
    if (!bEnableSemanticClassification || !Actor) return DefaultSemanticLabel;
    for (const FVirtualLidarSemanticClassRule& R : SemanticClassRules) { if (SemanticRuleMatches(R, Actor, Component)) return R.Label; }
    return DefaultSemanticLabel;
}

FLinearColor UVirtualLidarScanComponent::ResolveSemanticColor(const FVirtualLidarPoint& Point) const
{
    if (!Point.bHit) return FLinearColor(0.01f, 0.01f, 0.02f, 1.0f);
    return GetSemanticColorForLabel(Point.SemanticLabel.IsNone() ? DefaultSemanticLabel : Point.SemanticLabel);
}

void UVirtualLidarScanComponent::PopulatePointSemanticMetadata(FVirtualLidarPoint& Point, const FHitResult& Hit) const
{
    const AActor* A = Hit.GetActor();
    Point.HitActorName = A ? A->GetFName() : NAME_None;
    Point.HitActorClassName = (A && A->GetClass()) ? A->GetClass()->GetFName() : NAME_None;
    Point.HitActorTags.Reset();
    if (A) Point.HitActorTags = A->Tags;
    Point.SemanticLabel = A ? ResolveSemanticLabel(Hit) : NAME_None;
}

void UVirtualLidarScanComponent::ApplyDeviceProfile(EVirtualLidarDeviceProfile NewProfile)
{
    DeviceProfile = NewProfile;
    ApplySimulationQuality(SimulationQuality);
}

FVirtualLidarProfilePreset UVirtualLidarScanComponent::ResolveProfilePreset(
    EVirtualLidarDeviceProfile Profile,
    EVirtualSensorSimulationQuality Quality)
{
    FVirtualLidarProfilePreset Result;
    Result.DeviceSpec.Manufacturer = TEXT("Generic");
    Result.DeviceSpec.Model = TEXT("Generic LiDAR");
    Result.DeviceSpec.HorizontalFovDegrees = 360.0f;
    Result.DeviceSpec.VerticalFovDegrees = 59.0f;
    Result.DeviceSpec.MinRangeCm = 10.0f;
    Result.DeviceSpec.TypicalRangeCm = 4000.0f;
    Result.DeviceSpec.MaxRangeCm = 20000.0f;
    Result.ProfileClass = EVirtualLidarProfileClass::Generic;
    Result.FidelityMode = EVirtualSensorFidelityMode::IdealTruth;
    Result.RecommendedBackend = EVirtualLidarAcquisitionBackend::Auto;
    Result.ProfileKey = TEXT("generic");
    Result.CalibrationId = TEXT("none");

    if (Profile == EVirtualLidarDeviceProfile::LivoxMid360S)
    {
        Result.DeviceSpec.Manufacturer = TEXT("Livox");
        Result.DeviceSpec.Model = TEXT("Mid-360S");
        Result.DeviceSpec.HorizontalFovDegrees = 360.0f;
        Result.DeviceSpec.VerticalFovDegrees = 59.0f;
        Result.DeviceSpec.MinRangeCm = 10.0f;
        Result.DeviceSpec.TypicalRangeCm = 4000.0f;
        Result.DeviceSpec.MaxRangeCm = 10000.0f;
        Result.DeviceSpec.FrameRateHz = 10.0f;
        Result.DeviceSpec.PointRate = 200000;
        Result.DeviceSpec.OutputFields = TEXT("XYZ");
        Result.DeviceSpec.Notes = TEXT("Livox Mid-360S: 40m at 10% reflectivity and 100m cutoff.");
        Result.ProfileKey = TEXT("livox-mid360s");
        Result.MaxDistanceCm = 4000.0f;
        Result.HorizontalFovDegrees = 360.0f;
        Result.MinVerticalAngleDegrees = -7.0f;
        Result.MaxVerticalAngleDegrees = 52.0f;
    }
    else if (Profile == EVirtualLidarDeviceProfile::IYOBOT_MLX80)
    {
        Result.DeviceSpec.Manufacturer = TEXT("IYOBOT");
        Result.DeviceSpec.Model = TEXT("ML-X(80) Integration 200");
        Result.DeviceSpec.HorizontalFovDegrees = 80.0f;
        Result.DeviceSpec.VerticalFovDegrees = 23.3f;
        Result.DeviceSpec.MinRangeCm = 10.0f;
        Result.DeviceSpec.TypicalRangeCm = 15000.0f;
        Result.DeviceSpec.MaxRangeCm = 15000.0f;
        Result.DeviceSpec.FrameRateHz = 20.0f;
        Result.DeviceSpec.PointRate = 224000;
        Result.DeviceSpec.Width = 200;
        Result.DeviceSpec.Height = 56;
        Result.DeviceSpec.HorizontalAngularResolutionDegrees = 0.4f;
        Result.DeviceSpec.VerticalAngularResolutionDegrees = 23.3f / 55.0f;
        Result.DeviceSpec.MaxEchoesPerPixel = 2;
        Result.DeviceSpec.MaxDistanceErrorMillimeters = 30.0f;
        Result.DeviceSpec.WavelengthNanometers = 940.0f;
        Result.DeviceSpec.ReferenceReflectivityPercent = 20.0f;
        Result.DeviceSpec.ReferenceAmbientLux = 100000.0f;
        Result.DeviceSpec.OutputFields = TEXT("XYZ + intensity");
        Result.DeviceSpec.Notes = TEXT("Project integration/downsample mode: 200x56 at 20Hz. Optical limits follow the public ML-X(80) specification; this is not the public native point density.");
        Result.ProfileClass = EVirtualLidarProfileClass::IntegrationDownsampled;
        Result.FidelityMode = EVirtualSensorFidelityMode::PublicSpecBased;
        Result.ProfileKey = TEXT("iyobot-mlx80-integration200");
        Result.CalibrationId = TEXT("public-spec-derived-2026");
        Result.MaxDistanceCm = 15000.0f;
        Result.HorizontalFovDegrees = 80.0f;
        Result.MinVerticalAngleDegrees = -11.65f;
        Result.MaxVerticalAngleDegrees = 11.65f;
    }
    else if (Profile == EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE)
    {
        Result.DeviceSpec.Manufacturer = TEXT("IYOBOT");
        Result.DeviceSpec.Model = TEXT("ML-X(80) Native");
        Result.DeviceSpec.HorizontalFovDegrees = 80.0f;
        Result.DeviceSpec.VerticalFovDegrees = 23.3f;
        Result.DeviceSpec.MinRangeCm = 10.0f;
        Result.DeviceSpec.TypicalRangeCm = 15000.0f;
        Result.DeviceSpec.MaxRangeCm = 15000.0f;
        Result.DeviceSpec.FrameRateHz = 20.0f;
        Result.DeviceSpec.PointRate = 645120;
        Result.DeviceSpec.Width = 576;
        Result.DeviceSpec.Height = 56;
        Result.DeviceSpec.HorizontalAngularResolutionDegrees = 0.139f;
        Result.DeviceSpec.VerticalAngularResolutionDegrees = 0.417f;
        Result.DeviceSpec.MaxEchoesPerPixel = 2;
        Result.DeviceSpec.MaxDistanceErrorMillimeters = 30.0f;
        Result.DeviceSpec.WavelengthNanometers = 940.0f;
        Result.DeviceSpec.ReferenceReflectivityPercent = 20.0f;
        Result.DeviceSpec.ReferenceAmbientLux = 100000.0f;
        Result.DeviceSpec.OutputFields = TEXT("XYZ + intensity");
        Result.DeviceSpec.Notes = TEXT("Public-spec-derived native density: approximately 576x56 at 20Hz (645,120 points/s), up to 25Hz. Exact calibration and vendor packet layout require hardware evidence.");
        Result.ProfileClass = EVirtualLidarProfileClass::PublicSpecNative;
        Result.FidelityMode = EVirtualSensorFidelityMode::PublicSpecBased;
        Result.ProfileKey = TEXT("iyobot-mlx80-native");
        Result.CalibrationId = TEXT("public-spec-derived-2026");
        Result.MaxDistanceCm = 15000.0f;
        Result.HorizontalFovDegrees = 80.0f;
        Result.MinVerticalAngleDegrees = -11.65f;
        Result.MaxVerticalAngleDegrees = 11.65f;
    }

    if (Profile == EVirtualLidarDeviceProfile::IYOBOT_MLX80)
    {
        if (Quality == EVirtualSensorSimulationQuality::Debug) { Result.HorizontalSamples = 50; Result.VerticalChannels = 14; Result.ScanIntervalSeconds = 0.2f; Result.PreviewPointStride = 1; Result.MaxPreviewPoints = 1000; }
        else if (Quality == EVirtualSensorSimulationQuality::RealTimePreview) { Result.HorizontalSamples = 100; Result.VerticalChannels = 28; Result.ScanIntervalSeconds = 0.1f; Result.PreviewPointStride = 1; Result.MaxPreviewPoints = 2800; }
        else if (Quality == EVirtualSensorSimulationQuality::Balanced) { Result.HorizontalSamples = 160; Result.VerticalChannels = 42; Result.ScanIntervalSeconds = 1.0f / 15.0f; Result.PreviewPointStride = 2; Result.MaxPreviewPoints = 5000; }
        else { Result.HorizontalSamples = 200; Result.VerticalChannels = 56; Result.ScanIntervalSeconds = 0.05f; Result.PreviewPointStride = 3; Result.MaxPreviewPoints = 5000; }
    }
    else if (Profile == EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE)
    {
        if (Quality == EVirtualSensorSimulationQuality::Debug) { Result.HorizontalSamples = 144; Result.VerticalChannels = 14; Result.ScanIntervalSeconds = 0.2f; Result.PreviewPointStride = 1; Result.MaxPreviewPoints = 2016; }
        else if (Quality == EVirtualSensorSimulationQuality::RealTimePreview) { Result.HorizontalSamples = 288; Result.VerticalChannels = 28; Result.ScanIntervalSeconds = 0.1f; Result.PreviewPointStride = 2; Result.MaxPreviewPoints = 4032; }
        else if (Quality == EVirtualSensorSimulationQuality::Balanced) { Result.HorizontalSamples = 432; Result.VerticalChannels = 42; Result.ScanIntervalSeconds = 1.0f / 15.0f; Result.PreviewPointStride = 4; Result.MaxPreviewPoints = 5000; }
        else { Result.HorizontalSamples = 576; Result.VerticalChannels = 56; Result.ScanIntervalSeconds = 0.05f; Result.PreviewPointStride = 7; Result.MaxPreviewPoints = 5000; }
    }
    else
    {
        if (Quality == EVirtualSensorSimulationQuality::Debug) { Result.HorizontalSamples = 60; Result.VerticalChannels = 8; Result.ScanIntervalSeconds = 0.5f; Result.PreviewPointStride = 1; Result.MaxPreviewPoints = 1000; }
        else if (Quality == EVirtualSensorSimulationQuality::RealTimePreview) { Result.HorizontalSamples = 120; Result.VerticalChannels = 24; Result.ScanIntervalSeconds = 0.25f; Result.PreviewPointStride = 2; Result.MaxPreviewPoints = 3000; }
        else if (Quality == EVirtualSensorSimulationQuality::Balanced) { Result.HorizontalSamples = 240; Result.VerticalChannels = 32; Result.ScanIntervalSeconds = 0.2f; Result.PreviewPointStride = 3; Result.MaxPreviewPoints = 5000; }
        else { Result.HorizontalSamples = 360; Result.VerticalChannels = 60; Result.ScanIntervalSeconds = 0.1f; Result.PreviewPointStride = 6; Result.MaxPreviewPoints = 5000; }
    }
    return Result;
}

FVirtualLidarSlabAnalysisResult UVirtualLidarScanComponent::AnalyzeSlabPoints(const TArray<FVirtualLidarPoint>& Points) const
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

int32 UVirtualLidarScanComponent::CountServerPayloadPoints(const TArray<FVirtualLidarPoint>& Points) const
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

bool UVirtualLidarScanComponent::ShouldIgnoreHitActor(const AActor* Actor) const { if (!Actor) return false; for (const FName& Tag : IgnoreActorTags) if (!Tag.IsNone() && Actor->ActorHasTag(Tag)) return true; return false; }

void UVirtualLidarScanComponent::UpdateLidarViewTexture(const TArray<uint8>& Pixels)
{
    const int32 W = FMath::Max(1, HorizontalSamples); const int32 H = FMath::Max(1, VerticalChannels); if (Pixels.Num() < W * H * 4) return;
    if (!LidarViewTexture || LidarViewTexture->GetSizeX() != W || LidarViewTexture->GetSizeY() != H)
    {
        LidarViewTexture = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
        if (LidarViewTexture)
        {
            LidarViewTexture->SRGB = true;
            LidarViewTexture->CompressionSettings = TC_VectorDisplacementmap;
            LidarViewTexture->UpdateResource();
        }
    }
    if (!LidarViewTexture || !LidarViewTexture->GetResource()) return;
    uint8* UploadData = new uint8[Pixels.Num()];
    FMemory::Memcpy(UploadData, Pixels.GetData(), Pixels.Num());
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, W, H);
    LidarViewTexture->UpdateTextureRegions(0, 1, Region, W * 4, 4, UploadData,
        [Region](uint8* Data, const FUpdateTextureRegion2D*)
        {
            delete[] Data;
            delete Region;
        });
}

void UVirtualLidarScanComponent::DispatchPayload(const FString& JsonPayload) const
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
void UVirtualLidarScanComponent::PostJson(const FString& JsonPayload) const
{
    if (HttpEndpoint.IsEmpty()) return; TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest(); Request->SetURL(HttpEndpoint); Request->SetVerb(TEXT("POST")); Request->SetHeader(TEXT("Content-Type"), TEXT("application/json")); Request->SetContentAsString(JsonPayload); Request->ProcessRequest();
}
void UVirtualLidarScanComponent::RebuildLastPointStatistics()
{
    LastHitPointCount = 0;
    LastSemanticCounts.Reset();
    for (const FVirtualLidarPoint& Point : GetLastPoints())
    {
        if (!Point.bHit) continue;
        ++LastHitPointCount;
        LastSemanticCounts.FindOrAdd(Point.SemanticLabel.IsNone() ? TEXT("Unclassified") : Point.SemanticLabel.ToString())++;
    }
}

void UVirtualLidarScanComponent::TryAutoRegisterToManager()
{
    if (!bAutoRegisterToManager || !GetWorld()) return; for (TActorIterator<AVirtualSensorCoordinator> It(GetWorld()); It; ++It) { if (*It) { It->RegisterLidar(this); return; } }
}
