#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Async/Async.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSlabContextCompatibility.h"

namespace
{
class FContextProviderStub : public IVirtualSensorAcquisitionContextProvider
{
public:
    FVirtualSensorAcquisitionContext CaptureAcquisitionContext(const FString&, int64) override { return {}; }
    void CompleteAcquisition(const FString&, int64, bool) override {}
    bool BeginContextDerivation(const FString&, int64, const FVirtualSensorAcquisitionContext&) override { return false; }
    void EndCompatibilityDerivation(const FString&, const FString&, int64, bool) override {}
    bool AllowsAcquisitionFrame(const FString&, const FVirtualSensorAcquisitionContext&) const override { return true; }
    bool AllowsOutputFrame(const FString&, EVirtualSensorAcquisitionOutput, const FVirtualSensorAcquisitionContext&) const override { return true; }
    bool AllowsOutputDemand(const FString&, EVirtualSensorAcquisitionOutput) const override { return true; }
    bool IsSensorConfigurationLocked(const FString&) const override { return false; }
    bool IsSensorPlacementLocked(const FString&) const override { return false; }
    bool IsAcquisitionSessionBusy() const override { return false; }
    void RecordAcquisitionOutputFailure(const FString&, const FString&, EVirtualSensorAcquisitionOutput, int64, const FString&) override {}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcquisitionProviderLifetimeTest, "MA0T10.Architecture.ContextProviderLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAcquisitionProviderLifetimeTest::RunTest(const FString&)
{
    auto* WorldA = NewObject<UWorld>(); auto* WorldB = NewObject<UWorld>();
    auto* OwnerA = NewObject<USceneComponent>(); auto* OwnerB = NewObject<USceneComponent>();
    FContextProviderStub A, B;
    using Registry = FVirtualSensorAcquisitionContextProviders;
    TestNull(TEXT("no host is supported"), Registry::Get(WorldA));
    TestTrue(TEXT("register A"), Registry::Register(WorldA, OwnerA, &A));
    TestTrue(TEXT("register B"), Registry::Register(WorldB, OwnerB, &B));
    TestTrue(TEXT("world isolation"), Registry::Get(WorldA) == &A && Registry::Get(WorldB) == &B);
    TestFalse(TEXT("cannot replace active host"), Registry::Register(WorldA, OwnerB, &B));
    Registry::Unregister(WorldA, &B);
    TestTrue(TEXT("wrong owner cannot unregister"), Registry::Get(WorldA) == &A);
    OwnerA->MarkAsGarbage();
    TestNull(TEXT("dead owner never dereferenced"), Registry::Get(WorldA));
    TestTrue(TEXT("new owner replaces expired entry"), Registry::Register(WorldA, OwnerB, &B));
    Registry::Unregister(WorldA, &B); Registry::Unregister(WorldB, &B);
    TestNull(TEXT("unregistered world"), Registry::Get(WorldA));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcquisitionContextValueTest, "MA0T10.Architecture.ContextSnapshotValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAcquisitionContextValueTest::RunTest(const FString&)
{
    FVirtualSlabFrameContext Legacy;
    Legacy.RunId = TEXT("run-old"); Legacy.ScenarioUUID = TEXT("scenario-old"); Legacy.MtlNo = TEXT("0000001");
    Legacy.SlabFrameNo = 9007199254740995LL; Legacy.ElapsedSec = 1.23456789;
    Legacy.Generation = 17; Legacy.Segment = 3; Legacy.bEligible = true; Legacy.AcquisitionUtcTicks = 123456789012345;
    const FVirtualSensorAcquisitionContext Captured = Legacy;
    auto Headers = Async(EAsyncExecution::ThreadPool, [Captured]() { return Captured.ToHeaders(); });
    Legacy.RunId = TEXT("new-run"); Legacy.SlabFrameNo = 99;
    const FVirtualSlabFrameContext RoundTrip = Captured;
    TestEqual(TEXT("immutable run"), RoundTrip.RunId, FString(TEXT("run-old")));
    TestEqual(TEXT("integer remains exact"), RoundTrip.SlabFrameNo, 9007199254740995LL);
    TestEqual(TEXT("generation"), RoundTrip.Generation, 17);
    TestEqual(TEXT("acquisition UTC"), RoundTrip.AcquisitionUtcTicks, 123456789012345LL);
    const auto H = Headers.Get();
    TestEqual(TEXT("worker snapshot run"), H.FindRef(TEXT("x-run-uuid")), RoundTrip.RunId);
    TestEqual(TEXT("worker snapshot integer"), H.FindRef(TEXT("x-slab-frame-no")), FString(TEXT("9007199254740995")));
    TestEqual(TEXT("six decimal places"), H.FindRef(TEXT("x-slab-elapsed-sec")), FString(TEXT("1.234568")));
    return true;
}
#endif
