#pragma once

#include "CoreMinimal.h"
#include "SensorOwnedRayTracing.h"

class FJsonObject;
struct FVirtualLidarDepthAcquisitionFrame;
struct FVirtualLidarPoint;

/** Opt-in acquisition diagnostics only. Never supplies metadata to a measured point. */
namespace VirtualLidarSemanticDiagnostics
{
    struct FSurface
    {
        FName ActorName;
        FName Label;
        bool bSnapshotValid = false;
        bool bComponentAlive = false;
        bool bIgnored = false;
        bool bSlabTag = false;
    };

    struct FMesh
    {
        int32 TriangleCount = 0;
        TArray<FSurface> Surfaces;
    };

    enum class EHardwareMapping : uint8
    {
        Miss, InvalidDistance, InvalidMesh, InvalidTriangle, InvalidSurface,
        MissingSnapshot, ExpiredComponent, Ignored, Mapped, Count
    };

    // Pure snapshot logic: no UObject access, renderer calls or I/O.
    MA0T10_DT_API EHardwareMapping ClassifyHardwareHit(const FSensorRtHit& Hit, const TArray<FMesh>& Meshes);
    MA0T10_DT_API TSharedRef<FJsonObject> BuildHardwareReport(const TArray<FMesh>& Meshes, const TArray<FSensorRtHit>& Hits);
    MA0T10_DT_API TSharedRef<FJsonObject> BuildDepthReport(const FVirtualLidarDepthAcquisitionFrame& Frame,
        const TArray<int32>& PixelIndices, const TArray<FVirtualLidarPoint>& Points);

    /** Explicit command-line flag works in Shipping without enabling global logging. */
    bool IsEnabled();
    /** Game thread admission to the existing bounded async FileSave writer. No synchronous disk I/O. */
    void Persist(UObject* Owner, const FString& SensorId, int64 FrameId, int32 Generation,
        const TCHAR* Backend, const TCHAR* Stage, TSharedRef<FJsonObject> Report);
}
