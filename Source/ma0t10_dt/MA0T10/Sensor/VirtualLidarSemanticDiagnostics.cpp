#include "VirtualLidarSemanticDiagnostics.h"

#include "VirtualLidarSensorTypes.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorAcquisitionBackend.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace VirtualLidarSemanticDiagnostics
{
EHardwareMapping ClassifyHardwareHit(const FSensorRtHit& Hit, const TArray<FMesh>& Meshes)
{
    if (!FMath::IsFinite(Hit.NormalAndDistance.W)) return EHardwareMapping::InvalidDistance;
    if (Hit.NormalAndDistance.W < 0) return EHardwareMapping::Miss;
    if (Hit.Mesh >= uint32(Meshes.Num())) return EHardwareMapping::InvalidMesh;
    const auto& Mesh = Meshes[Hit.Mesh];
    if (Hit.Triangle >= uint32(FMath::Max(0, Mesh.TriangleCount))) return EHardwareMapping::InvalidTriangle;
    if (Hit.Surface >= uint32(Mesh.Surfaces.Num())) return EHardwareMapping::InvalidSurface;
    const auto& Surface = Mesh.Surfaces[Hit.Surface];
    if (!Surface.bSnapshotValid) return EHardwareMapping::MissingSnapshot;
    if (!Surface.bComponentAlive) return EHardwareMapping::ExpiredComponent;
    if (Surface.bIgnored) return EHardwareMapping::Ignored;
    return EHardwareMapping::Mapped;
}

TSharedRef<FJsonObject> BuildHardwareReport(const TArray<FMesh>& Meshes, const TArray<FSensorRtHit>& Hits)
{
    auto Report = MakeShared<FJsonObject>();
    int32 Slots = 0, Valid = 0, Named = 0, Slab = 0, SlabTag = 0, Expired = 0;
    for (const auto& Mesh : Meshes) for (const auto& Surface : Mesh.Surfaces)
    {
        ++Slots;
        Valid += Surface.bSnapshotValid;
        Expired += Surface.bSnapshotValid && !Surface.bComponentAlive;
        Named += Surface.bSnapshotValid && !Surface.ActorName.IsNone();
        Slab += Surface.bSnapshotValid && Surface.Label == TEXT("Slab");
        SlabTag += Surface.bSnapshotValid && Surface.bSlabTag;
    }
    Report->SetNumberField(TEXT("surface_count"), Slots);
    Report->SetNumberField(TEXT("snapshot_valid_surfaces"), Valid);
    Report->SetNumberField(TEXT("snapshot_expired_surfaces"), Expired);
    Report->SetNumberField(TEXT("snapshot_named_surfaces"), Named);
    Report->SetNumberField(TEXT("snapshot_slab_surfaces"), Slab);
    Report->SetNumberField(TEXT("snapshot_slab_tagged_surfaces"), SlabTag);
    Report->SetNumberField(TEXT("mesh_count"), Meshes.Num());
    Report->SetNumberField(TEXT("returned_rays"), Hits.Num());
    int32 Counts[int32(EHardwareMapping::Count)] = {};
    int32 MappedNamed = 0, MappedSlab = 0, MappedUnclassified = 0;
    for (const auto& Hit : Hits)
    {
        const auto Mapping = ClassifyHardwareHit(Hit, Meshes);
        ++Counts[int32(Mapping)];
        if (Mapping != EHardwareMapping::Mapped) continue;
        const auto& Surface = Meshes[Hit.Mesh].Surfaces[Hit.Surface];
        MappedNamed += !Surface.ActorName.IsNone();
        MappedSlab += Surface.Label == TEXT("Slab");
        MappedUnclassified += Surface.Label.IsNone();
    }
    const TCHAR* Keys[] = {TEXT("miss"), TEXT("invalid_distance"), TEXT("invalid_mesh"),
        TEXT("invalid_triangle"), TEXT("invalid_surface"), TEXT("missing_snapshot"),
        TEXT("expired_component"), TEXT("ignored"), TEXT("mapped")};
    static_assert(UE_ARRAY_COUNT(Keys) == int32(EHardwareMapping::Count), "mapping counter names");
    for (int32 I = 0; I < UE_ARRAY_COUNT(Keys); ++I) Report->SetNumberField(Keys[I], Counts[I]);
    Report->SetNumberField(TEXT("mapped_named_hits"), MappedNamed);
    Report->SetNumberField(TEXT("mapped_slab_hits"), MappedSlab);
    Report->SetNumberField(TEXT("mapped_unclassified_hits"), MappedUnclassified);
    return Report;
}

TSharedRef<FJsonObject> BuildDepthReport(const FVirtualLidarDepthAcquisitionFrame& Frame,
    const TArray<int32>& PixelIndices, const TArray<FVirtualLidarPoint>& Points)
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("semantic_requested"), Frame.Request.bRequestSemantics);
    Report->SetStringField(TEXT("semantic_status"), Frame.SemanticStatus);
    Report->SetNumberField(TEXT("identity_count"), Frame.SemanticIdentities.Num());
    Report->SetNumberField(TEXT("semantic_pixels"), Frame.SemanticIdDepth.Num());
    int32 Named = 0, Slab = 0, SlabTag = 0;
    for (const auto& Entry : Frame.SemanticIdentities)
    {
        Named += !Entry.Value.ActorName.IsNone();
        Slab += Entry.Value.Label == TEXT("Slab");
        SlabTag += Entry.Value.ActorTags.Contains(TEXT("Slab"));
    }
    Report->SetNumberField(TEXT("snapshot_named_surfaces"), Named);
    Report->SetNumberField(TEXT("snapshot_slab_surfaces"), Slab);
    Report->SetNumberField(TEXT("snapshot_slab_tagged_surfaces"), SlabTag);
    int32 NoDepth = 0, NoSemantic = 0, NonFinite = 0, Mismatch = 0, NoId = 0, Mapped = 0;
    for (int32 Pixel : PixelIndices)
    {
        if (!Frame.ForwardDepthCentimeters.IsValidIndex(Pixel) ||
            !FMath::IsFinite(Frame.ForwardDepthCentimeters[Pixel]) || Frame.ForwardDepthCentimeters[Pixel] <= 0)
        { ++NoDepth; continue; }
        if (!Frame.SemanticIdDepth.IsValidIndex(Pixel)) { ++NoSemantic; continue; }
        const auto& IdDepth = Frame.SemanticIdDepth[Pixel];
        if (!FMath::IsFinite(IdDepth.R) || !FMath::IsFinite(IdDepth.A)) { ++NonFinite; continue; }
        if (FMath::Abs(IdDepth.A - Frame.ForwardDepthCentimeters[Pixel]) > 1.0f) { ++Mismatch; continue; }
        // Check range before float->int conversion; corrupt ID pixels are diagnostic data too.
        if (IdDepth.R < 0 || double(IdDepth.R) > double(MAX_int32) ||
            !Frame.SemanticIdentities.Contains(FMath::RoundToInt(IdDepth.R))) { ++NoId; continue; }
        ++Mapped;
    }
    Report->SetNumberField(TEXT("beam_count"), PixelIndices.Num());
    Report->SetNumberField(TEXT("no_depth"), NoDepth);
    Report->SetNumberField(TEXT("no_semantic_pixel"), NoSemantic);
    Report->SetNumberField(TEXT("nonfinite_semantic"), NonFinite);
    Report->SetNumberField(TEXT("depth_mismatch"), Mismatch);
    Report->SetNumberField(TEXT("missing_identity"), NoId);
    Report->SetNumberField(TEXT("mapped"), Mapped);
    int32 OutputHits = 0, OutputNamed = 0, OutputSlab = 0;
    for (const auto& Point : Points) if (Point.bHit)
    {
        ++OutputHits;
        OutputNamed += !Point.HitActorName.IsNone();
        OutputSlab += Point.SemanticLabel == TEXT("Slab");
    }
    Report->SetNumberField(TEXT("output_hits"), OutputHits);
    Report->SetNumberField(TEXT("output_named_hits"), OutputNamed);
    Report->SetNumberField(TEXT("output_slab_hits"), OutputSlab);
    return Report;
}

bool IsEnabled()
{
    static const bool Enabled = FParse::Param(FCommandLine::Get(), TEXT("LidarSemanticDiagnostics"));
    return Enabled;
}

void Persist(UObject* Owner, const FString& SensorId, int64 FrameId, int32 Generation,
    const TCHAR* Backend, const TCHAR* Stage, TSharedRef<FJsonObject> Report)
{
    check(IsInGameThread());
    if (!IsValid(Owner) || !Owner->GetWorld()) return;
    auto* Files = Owner->GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>();
    if (!Files) return;
    Report->SetStringField(TEXT("schema"), TEXT("ma0t10.lidar-semantic-diagnostic.v1"));
    Report->SetStringField(TEXT("backend"), Backend);
    Report->SetStringField(TEXT("stage"), Stage);
    Report->SetStringField(TEXT("sensor_id"), SensorId);
    Report->SetNumberField(TEXT("frame_id"), FrameId);
    Report->SetNumberField(TEXT("generation"), Generation);
    Report->SetStringField(TEXT("diagnostic_utc"), FDateTime::UtcNow().ToIso8601());
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    const FString Id = Files->RequestAnalysisJson(Owner, Json,
        FString::Printf(TEXT("LidarSemantic_%s_%s"), Backend, *SensorId));
    (void)Id; // Shipping may compile out the logging statement below.
    // Shipping can compile out UE_LOG. The bounded FileSave JSON above is the evidence source.
    UE_LOG(LogTemp, Display, TEXT("[LidarSemantic] backend=%s stage=%s frame=%lld request=%s state=%d"),
        Backend, Stage, FrameId, *Id, int32(Files->GetRequestStatus(Id).State));
}
}
