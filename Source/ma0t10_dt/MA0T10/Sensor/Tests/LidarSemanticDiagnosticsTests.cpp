#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSemanticDiagnostics.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorAcquisitionBackend.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include <limits>

namespace
{
using namespace VirtualLidarSemanticDiagnostics;
FSensorRtHit Hit(uint32 Mesh, uint32 Surface, uint32 Triangle = 0, float Distance = 100)
{
    FSensorRtHit Value;
    Value.NormalAndDistance = FVector4f(1, 0, 0, Distance);
    Value.Mesh = Mesh; Value.Surface = Surface; Value.Triangle = Triangle;
    return Value;
}
TArray<FMesh> FrozenMeshes()
{
    // Sparse request slots must retain TLAS DefaultUserData, not descriptor-array order.
    TArray<FMesh> Meshes; Meshes.SetNum(3);
    auto& Mesh = Meshes[2]; Mesh.TriangleCount = 4; Mesh.Surfaces.SetNum(2);
    Mesh.Surfaces[0] = {TEXT("FrozenFloor"), TEXT("Floor"), true, true, false, false};
    Mesh.Surfaces[1] = {TEXT("FrozenSlab"), TEXT("Slab"), true, true, false, true};
    return Meshes;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarSemanticFrozenMappingTest,
    "MA0T10.LidarSemanticDiagnostics.HardwareSnapshotMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLidarSemanticFrozenMappingTest::RunTest(const FString&)
{
    auto Meshes = FrozenMeshes();
    const TArray<FSensorRtHit> Hits = {Hit(2, 1, 3), Hit(2, 0, 0), Hit(0, 0, 0, -1)};
    auto Report = BuildHardwareReport(Meshes, Hits);
    TestEqual(TEXT("mesh slot two maps both triangle-local surfaces"), Report->GetNumberField(TEXT("mapped")), 2.0);
    TestEqual(TEXT("distinct floor and slab keep their snapshot labels"), Report->GetNumberField(TEXT("mapped_slab_hits")), 1.0);
    TestEqual(TEXT("both actor names survive independent of instance slot"), Report->GetNumberField(TEXT("mapped_named_hits")), 2.0);
    TestEqual(TEXT("miss payload IDs never become a mapping failure"), Report->GetNumberField(TEXT("miss")), 1.0);
    TestEqual(TEXT("Actor Tag evidence is counted at the frozen surface"), Report->GetNumberField(TEXT("snapshot_slab_tagged_surfaces")), 1.0);
    Meshes[2].Surfaces[1].ActorName = NAME_None;
    Meshes[2].Surfaces[1].Label = NAME_None;
    TestEqual(TEXT("captured diagnostic is independent of later table mutation"), Report->GetNumberField(TEXT("mapped_slab_hits")), 1.0);
    auto Unclassified = BuildHardwareReport(Meshes, Hits);
    TestEqual(TEXT("resolved indices with absent metadata are distinguishable from invalid indices"),
        Unclassified->GetNumberField(TEXT("mapped_unclassified_hits")), 1.0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarSemanticMappingFailureTest,
    "MA0T10.LidarSemanticDiagnostics.HardwareFailureStages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLidarSemanticMappingFailureTest::RunTest(const FString&)
{
    auto Meshes = FrozenMeshes();
    const auto Expect = [this, &Meshes](const TCHAR* Name, FSensorRtHit Value, EHardwareMapping Expected)
    { TestEqual(Name, int32(ClassifyHardwareHit(Value, Meshes)), int32(Expected)); };
    Expect(TEXT("GPU miss does not inspect undefined IDs"), Hit(MAX_uint32, MAX_uint32, MAX_uint32, -1), EHardwareMapping::Miss);
    Expect(TEXT("nonfinite GPU distance is not a miss or valid mapping"), Hit(2, 1, 0, std::numeric_limits<float>::quiet_NaN()), EHardwareMapping::InvalidDistance);
    Expect(TEXT("infinite GPU distance is rejected"), Hit(2, 1, 0, std::numeric_limits<float>::infinity()), EHardwareMapping::InvalidDistance);
    Expect(TEXT("unsigned mesh sentinel cannot wrap into a valid slot"), Hit(MAX_uint32, 0), EHardwareMapping::InvalidMesh);
    Expect(TEXT("empty sparse mesh slot has no triangle"), Hit(0, 0), EHardwareMapping::InvalidTriangle);
    Expect(TEXT("triangle boundary is exclusive"), Hit(2, 1, 4), EHardwareMapping::InvalidTriangle);
    Expect(TEXT("surface boundary is exclusive"), Hit(2, 2), EHardwareMapping::InvalidSurface);
    Meshes[2].Surfaces[1].bSnapshotValid = false;
    Expect(TEXT("weak metadata absent at snapshot is explicit"), Hit(2, 1), EHardwareMapping::MissingSnapshot);
    Meshes[2].Surfaces[1].bSnapshotValid = true; Meshes[2].Surfaces[1].bComponentAlive = false;
    Expect(TEXT("expired ownership differs from absent snapshot"), Hit(2, 1), EHardwareMapping::ExpiredComponent);
    Meshes[2].Surfaces[1].bComponentAlive = true; Meshes[2].Surfaces[1].bIgnored = true;
    Expect(TEXT("explicit ignored actor is not counted as broken mapping"), Hit(2, 1), EHardwareMapping::Ignored);
    Meshes[2].Surfaces[1].bIgnored = false;
    Expect(TEXT("zero distance is still a geometric hit"), Hit(2, 1, 0, 0), EHardwareMapping::Mapped);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarSemanticDepthFailureTest,
    "MA0T10.LidarSemanticDiagnostics.DepthIdentityStages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLidarSemanticDepthFailureTest::RunTest(const FString&)
{
    FVirtualLidarDepthAcquisitionFrame Frame; Frame.Request.bRequestSemantics = true;
    Frame.ForwardDepthCentimeters = {100, 100, 100, 100, 100, 100};
    Frame.SemanticIdDepth = {FLinearColor(17, 0, 0, 100), FLinearColor(17, 0, 0, 101),
        FLinearColor(17, 0, 0, 101.01f), FLinearColor(99, 0, 0, 100),
        FLinearColor(std::numeric_limits<float>::infinity(), 0, 0, 100)};
    auto& Surface = Frame.SemanticIdentities.Add(17);
    Surface.ActorName = TEXT("FrozenSlab"); Surface.Label = TEXT("Slab"); Surface.ActorTags.Add(TEXT("Slab"));
    FVirtualLidarPoint Point; Point.bHit = true; Point.HitActorName = TEXT("FrozenSlab"); Point.SemanticLabel = TEXT("Slab");
    auto Report = BuildDepthReport(Frame, {0, 1, 2, 3, 4, 5, -1}, {Point});
    TestEqual(TEXT("original one centimetre identity boundary is preserved"), Report->GetNumberField(TEXT("mapped")), 2.0);
    TestEqual(TEXT("depth mismatch is distinct from missing ID"), Report->GetNumberField(TEXT("depth_mismatch")), 1.0);
    TestEqual(TEXT("valid depth with unknown ID is diagnosed"), Report->GetNumberField(TEXT("missing_identity")), 1.0);
    TestEqual(TEXT("nonfinite semantic buffer is diagnosed before conversion"), Report->GetNumberField(TEXT("nonfinite_semantic")), 1.0);
    TestEqual(TEXT("missing semantic readback differs from unsupported depth"), Report->GetNumberField(TEXT("no_semantic_pixel")), 1.0);
    TestEqual(TEXT("invalid pixel is bounded"), Report->GetNumberField(TEXT("no_depth")), 1.0);
    TestEqual(TEXT("converted metadata and buffer correspondence are counted separately"), Report->GetNumberField(TEXT("output_slab_hits")), 1.0);
    Frame.SemanticIdDepth.Reset(); Frame.SemanticIdentities.Reset();
    Report = BuildDepthReport(Frame, {0, 1}, {});
    TestEqual(TEXT("all Other/instance0 can be traced to absent semantic buffer"), Report->GetNumberField(TEXT("no_semantic_pixel")), 2.0);
    return !HasAnyErrors();
}
#endif
