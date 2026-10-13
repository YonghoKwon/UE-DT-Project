#include "VirtualSensorAcquisitionContext.h"
#include "Engine/World.h"

TMap<FString,FString> FVirtualSensorAcquisitionContext::ToHeaders() const
{
    TMap<FString,FString> Result;
    if (!bEligible) return Result;
    Result.Add(TEXT("x-run-uuid"), RunId);
    if (!ScenarioUUID.IsEmpty()) Result.Add(TEXT("x-scenario-uuid"), ScenarioUUID);
    Result.Add(TEXT("x-mtl-no"), MtlNo);
    Result.Add(TEXT("x-slab-frame-no"), LexToString(SlabFrameNo));
    Result.Add(TEXT("x-slab-elapsed-sec"), FString::Printf(TEXT("%.6f"), ElapsedSec));
    Result.Add(TEXT("x-session-segment"), LexToString(Segment));
    return Result;
}
namespace
{
struct FProviderEntry
{
    TWeakObjectPtr<UObject> Owner;
    IVirtualSensorAcquisitionContextProvider* Provider = nullptr;
};
TMap<TWeakObjectPtr<const UWorld>, FProviderEntry> Providers;
void PruneProviders()
{
    for (auto It = Providers.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || !It.Value().Owner.IsValid()) It.RemoveCurrent();
}
}
bool FVirtualSensorAcquisitionContextProviders::Register(UWorld* World, UObject* Owner, IVirtualSensorAcquisitionContextProvider* Provider)
{
    check(IsInGameThread());
    if (!IsValid(World) || !IsValid(Owner) || !Provider) return false;
    PruneProviders();
    const auto* Existing = Providers.Find(World);
    if (Existing && Existing->Provider != Provider) return false;
    Providers.Add(World, {Owner, Provider});
    return true;
}
void FVirtualSensorAcquisitionContextProviders::Unregister(UWorld* World, const IVirtualSensorAcquisitionContextProvider* Provider)
{
    check(IsInGameThread());
    const auto* Existing = Providers.Find(World);
    if (Existing && Existing->Provider == Provider) Providers.Remove(World);
    PruneProviders();
}
IVirtualSensorAcquisitionContextProvider* FVirtualSensorAcquisitionContextProviders::Get(const UWorld* World)
{
    check(IsInGameThread());
    const auto* Existing = World ? Providers.Find(World) : nullptr;
    return Existing && Existing->Owner.IsValid() ? Existing->Provider : nullptr;
}
