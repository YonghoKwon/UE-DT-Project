#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

namespace
{
FColor LerpColor(const FColor& A, const FColor& B, float Alpha)
{
    return FLinearColor::LerpUsingHSV(FLinearColor(A), FLinearColor(B), FMath::Clamp(Alpha, 0.0f, 1.0f)).ToFColor(true);
}

FColor SampleStops(const TArray<FColor>& Stops, float Normalized)
{
    if (Stops.Num() == 0) return FColor::White;
    const float Position = FMath::Clamp(Normalized, 0.0f, 1.0f) * static_cast<float>(Stops.Num() - 1);
    const int32 Left = FMath::Clamp(FMath::FloorToInt(Position), 0, Stops.Num() - 1);
    const int32 Right = FMath::Min(Left + 1, Stops.Num() - 1);
    return LerpColor(Stops[Left], Stops[Right], Position - static_cast<float>(Left));
}

FColor Turbo(float Value)
{
    static const TArray<FColor> Stops = {
        FColor(48, 18, 59), FColor(50, 90, 220), FColor(35, 190, 170),
        FColor(170, 230, 50), FColor(255, 150, 20), FColor(180, 4, 38)
    };
    return SampleStops(Stops, Value);
}

FColor Viridis(float Value)
{
    static const TArray<FColor> Stops = {
        FColor(68, 1, 84), FColor(59, 82, 139), FColor(33, 145, 140),
        FColor(94, 201, 98), FColor(253, 231, 37)
    };
    return SampleStops(Stops, Value);
}

void DrawPixel(TArray<FColor>& Pixels, int32 Width, int32 Height, int32 X, int32 Y, const FColor& Color, int32 Radius = 0)
{
    for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
    {
        for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
        {
            const int32 DrawX = X + OffsetX;
            const int32 DrawY = Y + OffsetY;
            if (DrawX >= 0 && DrawX < Width && DrawY >= 0 && DrawY < Height)
            {
                Pixels[DrawY * Width + DrawX] = Color;
            }
        }
    }
}

float NormalizeValue(float Value, float MinValue, float MaxValue)
{
    return MaxValue > MinValue + KINDA_SMALL_NUMBER
        ? FMath::Clamp((Value - MinValue) / (MaxValue - MinValue), 0.0f, 1.0f)
        : 0.5f;
}

struct FLidarProjectionBuildInput
{
    TSharedPtr<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe> Points;
    FVirtualLidarVisualizationSettings Settings;
    FTransform SensorTransform;
    TMap<FName, FColor> SemanticColors;
    FColor DefaultSemanticColor = FColor(140, 140, 140);
    float MaxDistanceCm = 10000.0f;
    int32 HorizontalSamples = 1;
    int32 VerticalChannels = 1;
    bool bFlipHorizontal = false;
    bool bFlipVertical = false;
    uint64 Generation = 0;
};

struct FLidarProjectionBuildResult
{
    TArray<FColor> RangePixels;
    TArray<FColor> TopDownPixels;
    TArray<FColor> ElevationPixels;
    int32 RangeWidth = 0;
    int32 RangeHeight = 0;
    int32 TopDownWidth = 0;
    int32 TopDownHeight = 0;
    int32 ElevationWidth = 0;
    int32 ElevationHeight = 0;
    float MinDistanceCm = 0.0f;
    float MaxDistanceCm = 0.0f;
    float MinHeightCm = -100.0f;
    float MaxHeightCm = 100.0f;
    uint64 Generation = 0;
};

FColor ResolveProjectionColor(
    const FLidarProjectionBuildInput& Input,
    const FVirtualLidarPoint& Point,
    float NormalizedDistance,
    float NormalizedHeight)
{
    if (!Point.bHit) return Input.Settings.ColorMode == ELidarColorMode::HitMask ? FColor::Black : FColor(4, 8, 16, 255);
    switch (Input.Settings.ColorMode)
    {
    case ELidarColorMode::DistanceViridis: return Viridis(NormalizedDistance);
    case ELidarColorMode::RelativeHeight: return Viridis(NormalizedHeight);
    case ELidarColorMode::SemanticLabel:
        if (const FColor* Color = Input.SemanticColors.Find(Point.SemanticLabel)) return *Color;
        return Input.DefaultSemanticColor;
    case ELidarColorMode::VerticalChannel:
        return Turbo(Input.VerticalChannels > 1 ? static_cast<float>(Point.Row) / static_cast<float>(Input.VerticalChannels - 1) : 0.5f);
    case ELidarColorMode::ReturnIndex:
        return Turbo(FMath::Fmod(static_cast<float>(FMath::Max(0, Point.ReturnIndex)) * 0.381966f, 1.0f));
    case ELidarColorMode::HitMask: return FColor::White;
    case ELidarColorMode::DistanceGray:
    {
        const uint8 Gray = static_cast<uint8>((1.0f - FMath::Clamp(NormalizedDistance, 0.0f, 1.0f)) * 255.0f);
        return FColor(Gray, Gray, Gray, 255);
    }
    case ELidarColorMode::DistanceTurbo:
    default: return Turbo(NormalizedDistance);
    }
}

FIntPoint ProjectInteractivePlane(
    const FVector2D& Value,
    const FVector2D& Center,
    const FVector2D& HalfExtent,
    float RotationDegrees,
    int32 Width,
    int32 Height)
{
    const FVector2D Relative = (Value - Center).GetRotated(-RotationDegrees);
    const float NormalizedX = Relative.X / FMath::Max(1.0f, HalfExtent.X) * 0.5f + 0.5f;
    const float NormalizedY = Relative.Y / FMath::Max(1.0f, HalfExtent.Y) * 0.5f + 0.5f;
    return FIntPoint(
        FMath::RoundToInt(NormalizedX * static_cast<float>(FMath::Max(1, Width) - 1)),
        FMath::RoundToInt((1.0f - NormalizedY) * static_cast<float>(FMath::Max(1, Height) - 1)));
}

TSharedPtr<FLidarProjectionBuildResult, ESPMode::ThreadSafe> BuildProjectionFrame(const FLidarProjectionBuildInput& Input)
{
    TSharedPtr<FLidarProjectionBuildResult, ESPMode::ThreadSafe> Result = MakeShared<FLidarProjectionBuildResult, ESPMode::ThreadSafe>();
    Result->Generation = Input.Generation;
    const TArray<FVirtualLidarPoint>& Points = Input.Points.IsValid() ? *Input.Points : TArray<FVirtualLidarPoint>();

    float MinDistance = TNumericLimits<float>::Max();
    float MaxDistance = 0.0f;
    float MinHeight = TNumericLimits<float>::Max();
    float MaxHeight = -TNumericLimits<float>::Max();
    for (const FVirtualLidarPoint& Point : Points)
    {
        if (!Point.bHit) continue;
        MinDistance = FMath::Min(MinDistance, Point.Distance);
        MaxDistance = FMath::Max(MaxDistance, Point.Distance);
        const float Height = Input.SensorTransform.InverseTransformPosition(Point.WorldLocation).Z;
        MinHeight = FMath::Min(MinHeight, Height);
        MaxHeight = FMath::Max(MaxHeight, Height);
    }
    if (MinDistance == TNumericLimits<float>::Max())
    {
        MinDistance = 0.0f;
        MaxDistance = Input.MaxDistanceCm;
        MinHeight = -100.0f;
        MaxHeight = 100.0f;
    }
    Result->MinDistanceCm = MinDistance;
    Result->MaxDistanceCm = MaxDistance;
    Result->MinHeightCm = MinHeight;
    Result->MaxHeightCm = MaxHeight;

    Result->RangeWidth = FMath::Max(1, Input.HorizontalSamples);
    Result->RangeHeight = FMath::Max(1, Input.VerticalChannels);
    Result->RangePixels.Init(FColor(4, 8, 16, 255), Result->RangeWidth * Result->RangeHeight);
    TArray<float> Depths;
    Depths.Init(TNumericLimits<float>::Max(), Result->RangePixels.Num());
    for (const FVirtualLidarPoint& Point : Points)
    {
        const int32 Row = Point.bHasGridCoord ? Point.Row : 0;
        const int32 Col = Point.bHasGridCoord ? Point.Col : 0;
        if (Row < 0 || Row >= Result->RangeHeight || Col < 0 || Col >= Result->RangeWidth) continue;
        const int32 DrawCol = Input.bFlipHorizontal ? Result->RangeWidth - 1 - Col : Col;
        const int32 DrawRow = Input.bFlipVertical ? Result->RangeHeight - 1 - Row : Row;
        const int32 PixelIndex = DrawRow * Result->RangeWidth + DrawCol;
        if (Point.bHit && Point.Distance > Depths[PixelIndex]) continue;
        const float Height = Point.bHit ? Input.SensorTransform.InverseTransformPosition(Point.WorldLocation).Z : 0.0f;
        Result->RangePixels[PixelIndex] = ResolveProjectionColor(Input, Point,
            NormalizeValue(Point.Distance, Input.Settings.bUseAdaptiveDistance ? MinDistance : 0.0f,
                Input.Settings.bUseAdaptiveDistance ? MaxDistance : Input.MaxDistanceCm),
            NormalizeValue(Height, MinHeight, MaxHeight));
        Depths[PixelIndex] = Point.bHit ? Point.Distance : TNumericLimits<float>::Max();
    }
    if (Input.Settings.bShowGrid)
    {
        const int32 ColumnStep = FMath::Max(1, Result->RangeWidth / 12);
        const int32 RowStep = FMath::Max(1, Result->RangeHeight / 6);
        for (int32 Y = 0; Y < Result->RangeHeight; ++Y)
        {
            for (int32 X = 0; X < Result->RangeWidth; ++X)
            {
                if (X % ColumnStep == 0 || Y % RowStep == 0)
                {
                    FColor& Pixel = Result->RangePixels[Y * Result->RangeWidth + X];
                    Pixel = LerpColor(Pixel, FColor(210, 220, 235), 0.28f);
                }
            }
        }
    }
    if (Input.Settings.bShowDepthEdges)
    {
        TArray<FColor> EdgePixels = Result->RangePixels;
        const float Threshold = FMath::Max(10.0f, Input.MaxDistanceCm * 0.01f);
        for (int32 Y = 0; Y < Result->RangeHeight; ++Y)
        {
            for (int32 X = 0; X < Result->RangeWidth; ++X)
            {
                const int32 Index = Y * Result->RangeWidth + X;
                if (Depths[Index] == TNumericLimits<float>::Max()) continue;
                const bool bRight = X + 1 < Result->RangeWidth && Depths[Index + 1] != TNumericLimits<float>::Max() && FMath::Abs(Depths[Index] - Depths[Index + 1]) >= Threshold;
                const bool bDown = Y + 1 < Result->RangeHeight && Depths[Index + Result->RangeWidth] != TNumericLimits<float>::Max() && FMath::Abs(Depths[Index] - Depths[Index + Result->RangeWidth]) >= Threshold;
                if (bRight || bDown) EdgePixels[Index] = FColor::White;
            }
        }
        Result->RangePixels = MoveTemp(EdgePixels);
    }

    if (Input.Settings.ProjectionMode == ELidarMonitorProjectionMode::TopDown || Input.Settings.ProjectionMode == ELidarMonitorProjectionMode::Split)
    {
        Result->TopDownWidth = Result->TopDownHeight = FMath::Clamp(Input.Settings.TopDownResolution, 128, 2048);
        Result->TopDownPixels.Init(FColor(5, 10, 18, 255), Result->TopDownWidth * Result->TopDownHeight);
        const int32 CenterX = Result->TopDownWidth / 2;
        const int32 BottomY = Result->TopDownHeight - 1;
        for (int32 Ring = 1; Ring <= 4; ++Ring)
        {
            const int32 Radius = FMath::RoundToInt(static_cast<float>(Result->TopDownWidth) * 0.5f * Ring / 4.0f);
            for (int32 Degree = 0; Degree < 360; ++Degree)
            {
                const float Radians = FMath::DegreesToRadians(static_cast<float>(Degree));
                DrawPixel(Result->TopDownPixels, Result->TopDownWidth, Result->TopDownHeight,
                    CenterX + FMath::RoundToInt(FMath::Cos(Radians) * Radius),
                    BottomY - FMath::RoundToInt(FMath::Sin(Radians) * Radius), FColor(35, 50, 68));
            }
        }
        for (const FVirtualLidarPoint& Point : Points)
        {
            if (!Point.bHit) continue;
            const FVector Local = Input.SensorTransform.InverseTransformPosition(Point.WorldLocation);
            const float Zoom = FMath::Clamp(Input.Settings.TopDownZoom, 0.1f, 20.0f);
            const FVector2D Center(Input.Settings.TopDownPanCm.Y, Input.MaxDistanceCm * 0.5f + Input.Settings.TopDownPanCm.X);
            const FVector2D HalfExtent(Input.MaxDistanceCm / Zoom, Input.MaxDistanceCm * 0.5f / Zoom);
            const FIntPoint Pixel = ProjectInteractivePlane(FVector2D(Local.Y, Local.X), Center, HalfExtent,
                Input.Settings.TopDownRotationDegrees, Result->TopDownWidth, Result->TopDownHeight);
            DrawPixel(Result->TopDownPixels, Result->TopDownWidth, Result->TopDownHeight, Pixel.X, Pixel.Y,
                ResolveProjectionColor(Input, Point,
                    NormalizeValue(Point.Distance, Input.Settings.bUseAdaptiveDistance ? MinDistance : 0.0f,
                        Input.Settings.bUseAdaptiveDistance ? MaxDistance : Input.MaxDistanceCm),
                    NormalizeValue(Local.Z, MinHeight, MaxHeight)), Input.Settings.PointSize >= 3.0f ? 1 : 0);
        }
        DrawPixel(Result->TopDownPixels, Result->TopDownWidth, Result->TopDownHeight, CenterX, BottomY, FColor::White, 2);
    }

    if (Input.Settings.ProjectionMode == ELidarMonitorProjectionMode::Elevation)
    {
        Result->ElevationWidth = FMath::Clamp(Input.Settings.ElevationWidth, 128, 2048);
        Result->ElevationHeight = FMath::Clamp(Input.Settings.ElevationHeight, 64, 1024);
        Result->ElevationPixels.Init(FColor(5, 10, 18, 255), Result->ElevationWidth * Result->ElevationHeight);
        const float Padding = FMath::Max(25.0f, (MaxHeight - MinHeight) * 0.05f);
        const float DrawMinHeight = MinHeight - Padding;
        const float DrawMaxHeight = MaxHeight + Padding;
        for (const FVirtualLidarPoint& Point : Points)
        {
            if (!Point.bHit) continue;
            const FVector Local = Input.SensorTransform.InverseTransformPosition(Point.WorldLocation);
            const float Zoom = FMath::Clamp(Input.Settings.ElevationZoom, 0.1f, 20.0f);
            const float ForwardDistance = FVector2D(Local.X, Local.Y).Length();
            const FVector2D Center(Input.MaxDistanceCm * 0.5f + Input.Settings.ElevationPanCm.X,
                (DrawMinHeight + DrawMaxHeight) * 0.5f + Input.Settings.ElevationPanCm.Y);
            const FVector2D HalfExtent(Input.MaxDistanceCm * 0.5f / Zoom,
                FMath::Max(1.0f, (DrawMaxHeight - DrawMinHeight) * 0.5f / Zoom));
            const FIntPoint Pixel = ProjectInteractivePlane(FVector2D(ForwardDistance, Local.Z), Center, HalfExtent,
                Input.Settings.ElevationRotationDegrees, Result->ElevationWidth, Result->ElevationHeight);
            DrawPixel(Result->ElevationPixels, Result->ElevationWidth, Result->ElevationHeight, Pixel.X, Pixel.Y,
                ResolveProjectionColor(Input, Point,
                    NormalizeValue(Point.Distance, Input.Settings.bUseAdaptiveDistance ? MinDistance : 0.0f,
                        Input.Settings.bUseAdaptiveDistance ? MaxDistance : Input.MaxDistanceCm),
                    NormalizeValue(Local.Z, MinHeight, MaxHeight)), Input.Settings.PointSize >= 3.0f ? 1 : 0);
        }
        const int32 ZeroY = FMath::Clamp(FMath::RoundToInt((1.0f - NormalizeValue(0.0f, DrawMinHeight, DrawMaxHeight)) * (Result->ElevationHeight - 1)), 0, Result->ElevationHeight - 1);
        for (int32 X = 0; X < Result->ElevationWidth; ++X)
        {
            DrawPixel(Result->ElevationPixels, Result->ElevationWidth, Result->ElevationHeight, X, ZeroY, FColor(80, 100, 120));
        }
    }
    return Result;
}
}

UVirtualLidarVisualizationComponent::UVirtualLidarVisualizationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualLidarVisualizationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ++ProjectionGeneration;
    bProjectionBuildInFlight = false;
    if (NiagaraPointCloudComponent)
    {
        NiagaraPointCloudComponent->DeactivateImmediate();
        NiagaraPointCloudComponent->DestroyComponent();
        NiagaraPointCloudComponent = nullptr;
    }
    RangeTexture = nullptr;
    TopDownTexture = nullptr;
    ElevationTexture = nullptr;
    ScanComponent = nullptr;
    Super::EndPlay(EndPlayReason);
}

void UVirtualLidarVisualizationComponent::BindScanComponent(UVirtualLidarScanComponent* InScanComponent)
{
    ScanComponent = InScanComponent;
    if (ScanComponent)
    {
        Settings.ColorMode = MapLegacyViewMode(ScanComponent->ViewMode);
    }
}

void UVirtualLidarVisualizationComponent::RefreshLatestFrame()
{
    if (!ScanComponent) return;
    RebuildProjectionTextures();
    RefreshWorldPointCloud();
}

void UVirtualLidarVisualizationComponent::SetVisualizationSettings(const FVirtualLidarVisualizationSettings& InSettings)
{
    Settings = InSettings;
    Settings.PointSize = FMath::Clamp(Settings.PointSize, 0.25f, 12.0f);
    Settings.TopDownResolution = FMath::Clamp(Settings.TopDownResolution, 128, 2048);
    Settings.ElevationWidth = FMath::Clamp(Settings.ElevationWidth, 128, 2048);
    Settings.ElevationHeight = FMath::Clamp(Settings.ElevationHeight, 64, 1024);
    Settings.TopDownZoom = FMath::Clamp(Settings.TopDownZoom, 0.1f, 20.0f);
    Settings.ElevationZoom = FMath::Clamp(Settings.ElevationZoom, 0.1f, 20.0f);
    if (ScanComponent) ScanComponent->ViewMode = MapColorModeToLegacy(Settings.ColorMode);
    RefreshLatestFrame();
}

void UVirtualLidarVisualizationComponent::PanProjectionView(ELidarMonitorProjectionMode ProjectionMode, FVector2D PixelDelta, FVector2D ViewportSize)
{
    const FVector2D SafeSize(FMath::Max(1.0f, ViewportSize.X), FMath::Max(1.0f, ViewportSize.Y));
    const float MaxDistance = ScanComponent ? FMath::Max(1.0f, ScanComponent->MaxDistance) : 10000.0f;
    if (ProjectionMode == ELidarMonitorProjectionMode::TopDown || ProjectionMode == ELidarMonitorProjectionMode::Split)
    {
        Settings.TopDownPanCm.X += PixelDelta.Y * (MaxDistance / Settings.TopDownZoom) / SafeSize.Y;
        Settings.TopDownPanCm.Y -= PixelDelta.X * (MaxDistance * 2.0f / Settings.TopDownZoom) / SafeSize.X;
    }
    else if (ProjectionMode == ELidarMonitorProjectionMode::Elevation)
    {
        const float HeightRange = FMath::Max(200.0f, LastMaxHeightCm - LastMinHeightCm);
        Settings.ElevationPanCm.X -= PixelDelta.X * (MaxDistance / Settings.ElevationZoom) / SafeSize.X;
        Settings.ElevationPanCm.Y += PixelDelta.Y * (HeightRange / Settings.ElevationZoom) / SafeSize.Y;
    }
    RebuildProjectionTextures();
}

void UVirtualLidarVisualizationComponent::RotateProjectionView(ELidarMonitorProjectionMode ProjectionMode, float DeltaDegrees)
{
    if (ProjectionMode == ELidarMonitorProjectionMode::TopDown || ProjectionMode == ELidarMonitorProjectionMode::Split)
        Settings.TopDownRotationDegrees = FMath::UnwindDegrees(Settings.TopDownRotationDegrees + DeltaDegrees);
    else if (ProjectionMode == ELidarMonitorProjectionMode::Elevation)
        Settings.ElevationRotationDegrees = FMath::UnwindDegrees(Settings.ElevationRotationDegrees + DeltaDegrees);
    RebuildProjectionTextures();
}

void UVirtualLidarVisualizationComponent::ZoomProjectionView(ELidarMonitorProjectionMode ProjectionMode, float ZoomFactor)
{
    if (ProjectionMode == ELidarMonitorProjectionMode::TopDown || ProjectionMode == ELidarMonitorProjectionMode::Split)
        Settings.TopDownZoom = FMath::Clamp(Settings.TopDownZoom * ZoomFactor, 0.1f, 20.0f);
    else if (ProjectionMode == ELidarMonitorProjectionMode::Elevation)
        Settings.ElevationZoom = FMath::Clamp(Settings.ElevationZoom * ZoomFactor, 0.1f, 20.0f);
    RebuildProjectionTextures();
}

void UVirtualLidarVisualizationComponent::ResetProjectionView(ELidarMonitorProjectionMode ProjectionMode)
{
    if (ProjectionMode == ELidarMonitorProjectionMode::TopDown || ProjectionMode == ELidarMonitorProjectionMode::Split)
    {
        Settings.TopDownPanCm = FVector2D::ZeroVector;
        Settings.TopDownZoom = 1.0f;
        Settings.TopDownRotationDegrees = 0.0f;
    }
    if (ProjectionMode == ELidarMonitorProjectionMode::Elevation)
    {
        Settings.ElevationPanCm = FVector2D::ZeroVector;
        Settings.ElevationZoom = 1.0f;
        Settings.ElevationRotationDegrees = 0.0f;
    }
    RebuildProjectionTextures();
}

void UVirtualLidarVisualizationComponent::SetProjectionMode(ELidarMonitorProjectionMode InProjectionMode)
{
    Settings.ProjectionMode = InProjectionMode;
    RefreshLatestFrame();
}

void UVirtualLidarVisualizationComponent::SetColorMode(ELidarColorMode InColorMode)
{
    Settings.ColorMode = InColorMode;
    if (ScanComponent) ScanComponent->ViewMode = MapColorModeToLegacy(InColorMode);
    RefreshLatestFrame();
}

void UVirtualLidarVisualizationComponent::SetWorldPointCloudEnabled(bool bEnabled)
{
    Settings.bShowWorldPointCloud = bEnabled;
    RefreshWorldPointCloud();
}

void UVirtualLidarVisualizationComponent::SetPointCloudRenderPolicy(ELidarPointCloudRenderPolicy InPolicy)
{
    PointCloudRenderPolicy = InPolicy;
    RefreshWorldPointCloud();
}

ELidarPointCloudRendererState UVirtualLidarVisualizationComponent::ResolveRendererState(
    ELidarPointCloudRenderPolicy Policy,
    bool bWorldPointCloudEnabled,
    int32 HitPointCount,
    bool bNiagaraAvailable,
    bool bNiagaraReady)
{
    if (!bWorldPointCloudEnabled) return ELidarPointCloudRendererState::Disabled;
    if (HitPointCount <= 0) return ELidarPointCloudRendererState::Starting;
    if (Policy == ELidarPointCloudRenderPolicy::ForceCpu) return ELidarPointCloudRendererState::CpuFallback;
    if (bNiagaraAvailable && bNiagaraReady) return ELidarPointCloudRendererState::NiagaraActive;
    if (Policy == ELidarPointCloudRenderPolicy::ForceNiagara) return ELidarPointCloudRendererState::Error;
    return ELidarPointCloudRendererState::CpuFallback;
}

UTexture2D* UVirtualLidarVisualizationComponent::GetPreviewTexture() const
{
    switch (Settings.ProjectionMode)
    {
    case ELidarMonitorProjectionMode::TopDown: return TopDownTexture;
    case ELidarMonitorProjectionMode::Elevation: return ElevationTexture;
    case ELidarMonitorProjectionMode::Split:
    case ELidarMonitorProjectionMode::RangeImage:
    default: return RangeTexture;
    }
}

UTexture2D* UVirtualLidarVisualizationComponent::GetSecondaryPreviewTexture() const
{
    return Settings.ProjectionMode == ELidarMonitorProjectionMode::Split ? TopDownTexture : nullptr;
}

ELidarColorMode UVirtualLidarVisualizationComponent::MapLegacyViewMode(EVirtualLidarViewMode LegacyMode)
{
    switch (LegacyMode)
    {
    case EVirtualLidarViewMode::HitMask: return ELidarColorMode::HitMask;
    case EVirtualLidarViewMode::ActorClassColor: return ELidarColorMode::SemanticLabel;
    case EVirtualLidarViewMode::IntensityGray: return ELidarColorMode::DistanceGray;
    case EVirtualLidarViewMode::DepthGradient:
    default: return ELidarColorMode::DistanceTurbo;
    }
}

EVirtualLidarViewMode UVirtualLidarVisualizationComponent::MapColorModeToLegacy(ELidarColorMode ColorMode)
{
    switch (ColorMode)
    {
    case ELidarColorMode::HitMask: return EVirtualLidarViewMode::HitMask;
    case ELidarColorMode::SemanticLabel: return EVirtualLidarViewMode::ActorClassColor;
    case ELidarColorMode::DistanceGray: return EVirtualLidarViewMode::IntensityGray;
    default: return EVirtualLidarViewMode::DepthGradient;
    }
}

FColor UVirtualLidarVisualizationComponent::ResolveDisplayColor(
    const UVirtualLidarScanComponent* InScanComponent,
    ELidarColorMode ColorMode,
    const FVirtualLidarPoint& Point,
    float NormalizedDistance,
    float NormalizedHeight)
{
    if (!Point.bHit) return ColorMode == ELidarColorMode::HitMask ? FColor::Black : FColor(4, 8, 16, 255);
    switch (ColorMode)
    {
    case ELidarColorMode::DistanceViridis: return Viridis(NormalizedDistance);
    case ELidarColorMode::RelativeHeight: return Viridis(NormalizedHeight);
    case ELidarColorMode::SemanticLabel:
        return (InScanComponent ? InScanComponent->GetSemanticColorForLabel(Point.SemanticLabel) : FLinearColor(0.55f, 0.55f, 0.55f)).ToFColor(true);
    case ELidarColorMode::VerticalChannel:
        return Turbo(InScanComponent && InScanComponent->VerticalChannels > 1
            ? static_cast<float>(Point.Row) / static_cast<float>(InScanComponent->VerticalChannels - 1) : 0.5f);
    case ELidarColorMode::ReturnIndex:
        return Turbo(FMath::Fmod(static_cast<float>(FMath::Max(0, Point.ReturnIndex)) * 0.381966f, 1.0f));
    case ELidarColorMode::HitMask: return FColor::White;
    case ELidarColorMode::DistanceGray:
    {
        const uint8 Gray = static_cast<uint8>((1.0f - FMath::Clamp(NormalizedDistance, 0.0f, 1.0f)) * 255.0f);
        return FColor(Gray, Gray, Gray, 255);
    }
    case ELidarColorMode::DistanceTurbo:
    default: return Turbo(NormalizedDistance);
    }
}

FIntPoint UVirtualLidarVisualizationComponent::ProjectTopDown(const FVector& SensorLocalPoint, float MaxDistanceCm, int32 Resolution)
{
    const float SafeRange = FMath::Max(1.0f, MaxDistanceCm);
    const int32 SafeResolution = FMath::Max(1, Resolution);
    const int32 X = FMath::RoundToInt((SensorLocalPoint.Y / (SafeRange * 2.0f) + 0.5f) * static_cast<float>(SafeResolution - 1));
    const int32 Y = FMath::RoundToInt((1.0f - FMath::Clamp(SensorLocalPoint.X / SafeRange, 0.0f, 1.0f)) * static_cast<float>(SafeResolution - 1));
    return FIntPoint(X, Y);
}

FIntPoint UVirtualLidarVisualizationComponent::ProjectElevation(const FVector& SensorLocalPoint, float MaxDistanceCm, float MinHeightCm, float MaxHeightCm, int32 Width, int32 Height)
{
    const float ForwardDistance = FMath::Sqrt(FMath::Square(SensorLocalPoint.X) + FMath::Square(SensorLocalPoint.Y));
    const int32 X = FMath::RoundToInt(FMath::Clamp(ForwardDistance / FMath::Max(1.0f, MaxDistanceCm), 0.0f, 1.0f) * static_cast<float>(FMath::Max(1, Width) - 1));
    const int32 Y = FMath::RoundToInt((1.0f - NormalizeValue(SensorLocalPoint.Z, MinHeightCm, MaxHeightCm)) * static_cast<float>(FMath::Max(1, Height) - 1));
    return FIntPoint(X, Y);
}

void UVirtualLidarVisualizationComponent::RebuildProjectionTextures()
{
    ++ProjectionGeneration;
    if (!bProjectionBuildInFlight) StartProjectionBuild();
}

void UVirtualLidarVisualizationComponent::StartProjectionBuild()
{
    if (!ScanComponent || bProjectionBuildInFlight) return;
    FLidarProjectionBuildInput Input;
    Input.Points = ScanComponent->GetLastPointSnapshot();
    Input.Settings = Settings;
    Input.SensorTransform = ScanComponent->GetComponentTransform();
    Input.MaxDistanceCm = ScanComponent->MaxDistance;
    Input.HorizontalSamples = ScanComponent->HorizontalSamples;
    Input.VerticalChannels = ScanComponent->VerticalChannels;
    Input.bFlipHorizontal = ScanComponent->bFlipLidarViewHorizontal;
    Input.bFlipVertical = ScanComponent->bFlipLidarViewVertical;
    Input.DefaultSemanticColor = ScanComponent->DefaultSemanticColor.ToFColor(true);
    for (const FVirtualLidarSemanticClassRule& Rule : ScanComponent->SemanticClassRules)
    {
        Input.SemanticColors.Add(Rule.Label, Rule.DisplayColor.ToFColor(true));
    }
    Input.Generation = ProjectionGeneration;
    bProjectionBuildInFlight = true;
    TWeakObjectPtr<UVirtualLidarVisualizationComponent> WeakThis(this);
    Async(EAsyncExecution::ThreadPool, [Input = MoveTemp(Input), WeakThis]() mutable
    {
        TSharedPtr<FLidarProjectionBuildResult, ESPMode::ThreadSafe> Result = BuildProjectionFrame(Input);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, Result]()
        {
            UVirtualLidarVisualizationComponent* Self = WeakThis.Get();
            if (!Self) return;
            Self->bProjectionBuildInFlight = false;
            if (Result.IsValid() && Result->Generation == Self->ProjectionGeneration)
            {
                Self->LastMinDistanceCm = Result->MinDistanceCm;
                Self->LastMaxDistanceCm = Result->MaxDistanceCm;
                Self->LastMinHeightCm = Result->MinHeightCm;
                Self->LastMaxHeightCm = Result->MaxHeightCm;
                Self->UploadTexture(Self->RangeTexture, Result->RangePixels, Result->RangeWidth, Result->RangeHeight);
                if (Result->TopDownPixels.Num() > 0)
                {
                    Self->UploadTexture(Self->TopDownTexture, Result->TopDownPixels, Result->TopDownWidth, Result->TopDownHeight);
                }
                if (Result->ElevationPixels.Num() > 0)
                {
                    Self->UploadTexture(Self->ElevationTexture, Result->ElevationPixels, Result->ElevationWidth, Result->ElevationHeight);
                }
            }
            if (Result.IsValid() && Result->Generation != Self->ProjectionGeneration)
            {
                Self->StartProjectionBuild();
            }
        });
    });
}

void UVirtualLidarVisualizationComponent::BuildRangeImage(TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight)
{
    OutWidth = ScanComponent ? FMath::Max(1, ScanComponent->HorizontalSamples) : 1;
    OutHeight = ScanComponent ? FMath::Max(1, ScanComponent->VerticalChannels) : 1;
    OutPixels.Init(FColor(4, 8, 16, 255), OutWidth * OutHeight);
    if (!ScanComponent) return;

    const TArray<FVirtualLidarPoint>& Points = ScanComponent->GetLastPoints();
    TArray<float> Depths;
    Depths.Init(TNumericLimits<float>::Max(), OutPixels.Num());
    LastMinDistanceCm = TNumericLimits<float>::Max();
    LastMaxDistanceCm = 0.0f;
    LastMinHeightCm = TNumericLimits<float>::Max();
    LastMaxHeightCm = -TNumericLimits<float>::Max();
    const FTransform SensorTransform = ScanComponent->GetComponentTransform();

    for (const FVirtualLidarPoint& Point : Points)
    {
        if (!Point.bHit) continue;
        LastMinDistanceCm = FMath::Min(LastMinDistanceCm, Point.Distance);
        LastMaxDistanceCm = FMath::Max(LastMaxDistanceCm, Point.Distance);
        const float LocalHeight = SensorTransform.InverseTransformPosition(Point.WorldLocation).Z;
        LastMinHeightCm = FMath::Min(LastMinHeightCm, LocalHeight);
        LastMaxHeightCm = FMath::Max(LastMaxHeightCm, LocalHeight);
    }
    if (!FMath::IsFinite(LastMinDistanceCm) || LastMinDistanceCm == TNumericLimits<float>::Max())
    {
        LastMinDistanceCm = 0.0f;
        LastMaxDistanceCm = ScanComponent->MaxDistance;
        LastMinHeightCm = -100.0f;
        LastMaxHeightCm = 100.0f;
    }

    for (const FVirtualLidarPoint& Point : Points)
    {
        const int32 Row = Point.bHasGridCoord ? Point.Row : 0;
        const int32 Col = Point.bHasGridCoord ? Point.Col : 0;
        if (Row < 0 || Row >= OutHeight || Col < 0 || Col >= OutWidth) continue;
        const int32 DrawCol = ScanComponent->bFlipLidarViewHorizontal ? OutWidth - 1 - Col : Col;
        const int32 DrawRow = ScanComponent->bFlipLidarViewVertical ? OutHeight - 1 - Row : Row;
        const int32 PixelIndex = DrawRow * OutWidth + DrawCol;
        if (Point.bHit && Point.Distance > Depths[PixelIndex]) continue;
        const float DistanceMin = Settings.bUseAdaptiveDistance ? LastMinDistanceCm : 0.0f;
        const float DistanceMax = Settings.bUseAdaptiveDistance ? LastMaxDistanceCm : ScanComponent->MaxDistance;
        const float LocalHeight = Point.bHit ? SensorTransform.InverseTransformPosition(Point.WorldLocation).Z : 0.0f;
        OutPixels[PixelIndex] = ResolveDisplayColor(
            ScanComponent,
            Settings.ColorMode,
            Point,
            NormalizeValue(Point.Distance, DistanceMin, DistanceMax),
            NormalizeValue(LocalHeight, LastMinHeightCm, LastMaxHeightCm));
        Depths[PixelIndex] = Point.bHit ? Point.Distance : TNumericLimits<float>::Max();
    }

    if (Settings.bShowGrid)
    {
        const int32 ColumnStep = FMath::Max(1, OutWidth / 12);
        const int32 RowStep = FMath::Max(1, OutHeight / 6);
        for (int32 Y = 0; Y < OutHeight; ++Y)
        {
            for (int32 X = 0; X < OutWidth; ++X)
            {
                if (X % ColumnStep == 0 || Y % RowStep == 0)
                {
                    FColor& Pixel = OutPixels[Y * OutWidth + X];
                    Pixel = LerpColor(Pixel, FColor(210, 220, 235), 0.28f);
                }
            }
        }
    }

    if (Settings.bShowDepthEdges)
    {
        TArray<FColor> EdgePixels = OutPixels;
        const float EdgeThresholdCm = FMath::Max(10.0f, ScanComponent->MaxDistance * 0.01f);
        for (int32 Y = 0; Y < OutHeight; ++Y)
        {
            for (int32 X = 0; X < OutWidth; ++X)
            {
                const int32 Index = Y * OutWidth + X;
                if (Depths[Index] == TNumericLimits<float>::Max()) continue;
                const bool bRightEdge = X + 1 < OutWidth && Depths[Index + 1] != TNumericLimits<float>::Max() && FMath::Abs(Depths[Index] - Depths[Index + 1]) >= EdgeThresholdCm;
                const bool bDownEdge = Y + 1 < OutHeight && Depths[Index + OutWidth] != TNumericLimits<float>::Max() && FMath::Abs(Depths[Index] - Depths[Index + OutWidth]) >= EdgeThresholdCm;
                if (bRightEdge || bDownEdge) EdgePixels[Index] = FColor::White;
            }
        }
        OutPixels = MoveTemp(EdgePixels);
    }
}

void UVirtualLidarVisualizationComponent::BuildTopDownImage(TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight)
{
    OutWidth = OutHeight = FMath::Clamp(Settings.TopDownResolution, 128, 2048);
    OutPixels.Init(FColor(5, 10, 18, 255), OutWidth * OutHeight);
    if (!ScanComponent) return;
    const FTransform SensorTransform = ScanComponent->GetComponentTransform();
    const TArray<FVirtualLidarPoint>& Points = ScanComponent->GetLastPoints();
    const int32 CenterX = OutWidth / 2;
    const int32 BottomY = OutHeight - 1;
    for (int32 Ring = 1; Ring <= 4; ++Ring)
    {
        const int32 Radius = FMath::RoundToInt(static_cast<float>(OutWidth) * 0.5f * static_cast<float>(Ring) / 4.0f);
        for (int32 Degree = 0; Degree < 360; ++Degree)
        {
            const float Radians = FMath::DegreesToRadians(static_cast<float>(Degree));
            DrawPixel(OutPixels, OutWidth, OutHeight, CenterX + FMath::RoundToInt(FMath::Cos(Radians) * Radius), BottomY - FMath::RoundToInt(FMath::Sin(Radians) * Radius), FColor(35, 50, 68));
        }
    }
    for (const FVirtualLidarPoint& Point : Points)
    {
        if (!Point.bHit) continue;
        const FVector Local = SensorTransform.InverseTransformPosition(Point.WorldLocation);
        const FIntPoint Pixel = ProjectTopDown(Local, ScanComponent->MaxDistance, OutWidth);
        const FColor Color = ResolveDisplayColor(ScanComponent, Settings.ColorMode, Point,
            NormalizeValue(Point.Distance, Settings.bUseAdaptiveDistance ? LastMinDistanceCm : 0.0f, Settings.bUseAdaptiveDistance ? LastMaxDistanceCm : ScanComponent->MaxDistance),
            NormalizeValue(Local.Z, LastMinHeightCm, LastMaxHeightCm));
        DrawPixel(OutPixels, OutWidth, OutHeight, Pixel.X, Pixel.Y, Color, Settings.PointSize >= 3.0f ? 1 : 0);
    }
    DrawPixel(OutPixels, OutWidth, OutHeight, CenterX, BottomY, FColor::White, 2);
}

void UVirtualLidarVisualizationComponent::BuildElevationImage(TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight)
{
    OutWidth = FMath::Clamp(Settings.ElevationWidth, 128, 2048);
    OutHeight = FMath::Clamp(Settings.ElevationHeight, 64, 1024);
    OutPixels.Init(FColor(5, 10, 18, 255), OutWidth * OutHeight);
    if (!ScanComponent) return;
    const FTransform SensorTransform = ScanComponent->GetComponentTransform();
    const float HeightPadding = FMath::Max(25.0f, (LastMaxHeightCm - LastMinHeightCm) * 0.05f);
    const float MinHeight = LastMinHeightCm - HeightPadding;
    const float MaxHeight = LastMaxHeightCm + HeightPadding;
    for (const FVirtualLidarPoint& Point : ScanComponent->GetLastPoints())
    {
        if (!Point.bHit) continue;
        const FVector Local = SensorTransform.InverseTransformPosition(Point.WorldLocation);
        const FIntPoint Pixel = ProjectElevation(Local, ScanComponent->MaxDistance, MinHeight, MaxHeight, OutWidth, OutHeight);
        const FColor Color = ResolveDisplayColor(ScanComponent, Settings.ColorMode, Point,
            NormalizeValue(Point.Distance, Settings.bUseAdaptiveDistance ? LastMinDistanceCm : 0.0f, Settings.bUseAdaptiveDistance ? LastMaxDistanceCm : ScanComponent->MaxDistance),
            NormalizeValue(Local.Z, LastMinHeightCm, LastMaxHeightCm));
        DrawPixel(OutPixels, OutWidth, OutHeight, Pixel.X, Pixel.Y, Color, Settings.PointSize >= 3.0f ? 1 : 0);
    }
    const int32 ZeroY = FMath::Clamp(FMath::RoundToInt((1.0f - NormalizeValue(0.0f, MinHeight, MaxHeight)) * static_cast<float>(OutHeight - 1)), 0, OutHeight - 1);
    for (int32 X = 0; X < OutWidth; ++X) DrawPixel(OutPixels, OutWidth, OutHeight, X, ZeroY, FColor(80, 100, 120));
}

UTexture2D* UVirtualLidarVisualizationComponent::UploadTexture(TObjectPtr<UTexture2D>& Texture, const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
    if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height) return Texture;
    if (!Texture || Texture->GetSizeX() != Width || Texture->GetSizeY() != Height)
    {
        Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
        if (!Texture) return nullptr;
        Texture->SRGB = true;
        Texture->CompressionSettings = TC_VectorDisplacementmap;
        Texture->UpdateResource();
        ++TextureResourceCreateCount;
    }
    if (!Texture->GetResource()) return Texture;
    const int32 ByteCount = Pixels.Num() * sizeof(FColor);
    uint8* UploadData = new uint8[ByteCount];
    FMemory::Memcpy(UploadData, Pixels.GetData(), ByteCount);
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);
    Texture->UpdateTextureRegions(0, 1, Region, Width * sizeof(FColor), sizeof(FColor), UploadData,
        [Region](uint8* Data, const FUpdateTextureRegion2D*)
        {
            delete[] Data;
            delete Region;
        });
    return Texture;
}

void UVirtualLidarVisualizationComponent::RefreshWorldPointCloud()
{
    if (!ScanComponent) return;
    if (TryRefreshNiagaraPointCloud()) return;
    ScanComponent->SetPointCloudPreviewEnabled(Settings.bShowWorldPointCloud);
    VisiblePointCount = Settings.bShowWorldPointCloud ? ScanComponent->GetLastPreviewPointCount() : 0;
    ActiveRendererName = ScanComponent->GetPreviewBackendName();
    RendererFallbackReason = ScanComponent->IsGpuPreviewBackendRequested() && !ScanComponent->IsGpuPreviewBackendActive()
        ? TEXT("Niagara GPU 자산 또는 런타임 경로를 사용할 수 없어 CPU ISM으로 표시합니다.") : FString();
    ActiveRendererName = TEXT("CPU ISM fallback");
    if (RendererFallbackReason.IsEmpty())
    {
        RendererFallbackReason = TEXT("Niagara GPU 경로를 사용할 수 없어 CPU ISM으로 표시합니다.");
    }
}

bool UVirtualLidarVisualizationComponent::TryRefreshNiagaraPointCloud()
{
    if (!Settings.bShowWorldPointCloud)
    {
        if (NiagaraPointCloudComponent) NiagaraPointCloudComponent->SetVisibility(false, true);
        ScanComponent->SetPointCloudPreviewEnabled(false);
        VisiblePointCount = 0;
        ScanComponent->SetGpuPreviewBackendRuntimeState(false, FString());
        return true;
    }

    UNiagaraSystem* System = bForceCpuFallbackForBenchmark ? nullptr : NiagaraPointCloudSystem.LoadSynchronous();
    if (!System || !GetWorld() || GetWorld()->GetFeatureLevel() < ERHIFeatureLevel::SM5)
    {
        if (NiagaraPointCloudComponent) NiagaraPointCloudComponent->SetVisibility(false, true);
        RendererFallbackReason = bForceCpuFallbackForBenchmark
            ? TEXT("성능 비교를 위해 CPU ISM fallback을 강제로 사용합니다.")
            : System
                ? TEXT("GPU Feature Level을 사용할 수 없어 CPU ISM으로 전환했습니다.")
                : TEXT("NS_VirtualLidarPointCloud 자산이 없어 CPU ISM으로 전환했습니다.");
        ScanComponent->SetGpuPreviewBackendRuntimeState(false, RendererFallbackReason);
        return false;
    }

    if (!NiagaraPointCloudComponent)
    {
        NiagaraPointCloudComponent = NewObject<UNiagaraComponent>(GetOwner(), TEXT("VirtualLidarNiagaraPointCloud"));
        if (NiagaraPointCloudComponent)
        {
            NiagaraPointCloudComponent->SetupAttachment(GetOwner()->GetRootComponent());
            NiagaraPointCloudComponent->SetUsingAbsoluteLocation(true);
            NiagaraPointCloudComponent->SetAsset(System);
            NiagaraPointCloudComponent->RegisterComponent();
        }
    }
    if (!NiagaraPointCloudComponent)
    {
        RendererFallbackReason = TEXT("Niagara 컴포넌트를 만들 수 없어 CPU ISM으로 전환했습니다.");
        ScanComponent->SetGpuPreviewBackendRuntimeState(false, RendererFallbackReason);
        return false;
    }

    NiagaraPositions.Reset();
    NiagaraColors.Reset();
    const TArray<FVirtualLidarPoint>& Points = ScanComponent->GetLastPoints();
    NiagaraPositions.Reserve(FMath::Min(MaxNiagaraPreviewPoints, Points.Num()));
    NiagaraColors.Reserve(NiagaraPositions.Max());
    const FTransform SensorTransform = ScanComponent->GetComponentTransform();
    const int32 Stride = FMath::Max(1, FMath::DivideAndRoundUp(Points.Num(), FMath::Max(1, MaxNiagaraPreviewPoints)));
    for (int32 Index = 0; Index < Points.Num() && NiagaraPositions.Num() < MaxNiagaraPreviewPoints; Index += Stride)
    {
        const FVirtualLidarPoint& Point = Points[Index];
        if (!Point.bHit) continue;
        const FVector Local = SensorTransform.InverseTransformPosition(Point.WorldLocation);
        NiagaraPositions.Add(Point.WorldLocation);
        NiagaraColors.Add(FLinearColor(ResolveDisplayColor(ScanComponent, Settings.ColorMode, Point,
            NormalizeValue(Point.Distance, Settings.bUseAdaptiveDistance ? LastMinDistanceCm : 0.0f, Settings.bUseAdaptiveDistance ? LastMaxDistanceCm : ScanComponent->MaxDistance),
            NormalizeValue(Local.Z, LastMinHeightCm, LastMaxHeightCm))));
    }
    UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(NiagaraPointCloudComponent, TEXT("User.PointPositions"), NiagaraPositions);
    UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayColor(NiagaraPointCloudComponent, TEXT("User.PointColors"), NiagaraColors);
    NiagaraPointCloudComponent->SetVariableInt(TEXT("User.PointCount"), NiagaraPositions.Num());
    NiagaraPointCloudComponent->SetVariableFloat(TEXT("User.PointSize"), Settings.PointSize);
    NiagaraPointCloudComponent->SetVariableVec2(TEXT("User.PointSpriteSize"), FVector2D(Settings.PointSize, Settings.PointSize));
    NiagaraPointCloudComponent->SetSystemFixedBounds(FBox(FVector(-ScanComponent->MaxDistance), FVector(ScanComponent->MaxDistance)));
    NiagaraPointCloudComponent->SetVisibility(true, true);
    NiagaraPointCloudComponent->ReinitializeSystem();
    ScanComponent->SetPointCloudPreviewEnabled(false);
    VisiblePointCount = NiagaraPositions.Num();
    ActiveRendererName = TEXT("Niagara GPU sprites");
    RendererFallbackReason.Reset();
    ScanComponent->SetGpuPreviewBackendRuntimeState(true, FString());
    return true;
}

FString UVirtualLidarVisualizationComponent::GetLegendText() const
{
    switch (Settings.ColorMode)
    {
    case ELidarColorMode::DistanceTurbo: return FString::Printf(TEXT("Turbo 거리: %.1fm → %.1fm"), LastMinDistanceCm * 0.01f, LastMaxDistanceCm * 0.01f);
    case ELidarColorMode::DistanceViridis: return FString::Printf(TEXT("Viridis 거리: %.1fm → %.1fm"), LastMinDistanceCm * 0.01f, LastMaxDistanceCm * 0.01f);
    case ELidarColorMode::RelativeHeight: return FString::Printf(TEXT("센서 상대 높이: %.2fm → %.2fm"), LastMinHeightCm * 0.01f, LastMaxHeightCm * 0.01f);
    case ELidarColorMode::SemanticLabel: return TEXT("SemanticLabel 설정 색상 · 미분류=회색");
    case ELidarColorMode::VerticalChannel: return ScanComponent ? FString::Printf(TEXT("수직 채널: 0 → %d"), FMath::Max(0, ScanComponent->VerticalChannels - 1)) : TEXT("수직 채널");
    case ELidarColorMode::ReturnIndex: return TEXT("Return index: 0, 1, 2… · MultiHit에서 구분");
    case ELidarColorMode::HitMask: return TEXT("검출=흰색 · 미검출=검정");
    case ELidarColorMode::DistanceGray: return TEXT("가까움=밝음 · 멂=어두움");
    default: return FString();
    }
}
