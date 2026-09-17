#pragma once
#include "CoreMinimal.h"

enum class ELidarGeometryClass : uint8 { Unknown, Plane, Protrusion };

/** Geometry only: deliberately accepts no actor, label, material or collision metadata. */
struct MA0T10_DT_API FVirtualLidarGeometryResult
{
    bool bReliable = false;
    FVector Normal = FVector::UpVector;
    double OffsetCm = 0;
    int32 PlanePoints = 0;
    int32 ProtrusionPoints = 0;
    int32 UnknownPoints = 0;
    double ProcessingMs = 0;
    FString Reason;
    TSet<FIntVector> ProtrudingCells;

    static FIntVector Cell(const FVector& P) { return FIntVector(FMath::FloorToInt(P.X / 5), FMath::FloorToInt(P.Y / 5), 0); }
    ELidarGeometryClass Classify(const FVector& P) const;
};

namespace VirtualLidarGeometry
{
    MA0T10_DT_API FVirtualLidarGeometryResult Analyze(TConstArrayView<FVector> PointsCm, const FVirtualLidarGeometryResult* Previous = nullptr);
    inline FColor Color(ELidarGeometryClass C)
    {
        return C == ELidarGeometryClass::Plane ? FColor(51,102,255) : C == ELidarGeometryClass::Protrusion ? FColor(255,128,0) : FColor(140,140,140);
    }
}
