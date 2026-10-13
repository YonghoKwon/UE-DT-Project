#include "SlabUiRegistry.h"
#include "SlabActor.h"
#include "Engine/Level.h"
#include "Engine/World.h"

namespace
{
struct FHostBinding
{
    TWeakObjectPtr<AActor> Owner;
    TFunction<ASlabActor*()> ResolveTarget;
    bool bActive=false;
    TFunction<bool(ASlabActor*)> Activate;
};
TArray<FHostBinding> Hosts;
void PruneHosts()
{
    Hosts.RemoveAll([](const FHostBinding& Binding) { return !Binding.Owner.IsValid(); });
}
bool IsActiveHostLevel(const AActor* Owner)
{
    const ULevel* Level=Owner->GetLevel();
    if (!Level) return false;
    // Match UE 5.3 TActorIterator's default OnlyActiveLevels policy, including
    // association/disassociation and static level collections.
    const bool Visible=(Level->bIsVisible&&!Level->bIsBeingRemoved)||Level->bIsAssociatingLevel||Level->bIsDisassociatingLevel;
    const FLevelCollection* Collection=Level->GetCachedLevelCollection();
    const FLevelCollection* Active=Level->OwningWorld?Level->OwningWorld->GetActiveLevelCollection():nullptr;
    return Visible&&(!Active||Collection==Active||!Collection||Collection->GetType()==ELevelCollectionType::StaticLevels);
}
}

void SlabUiRegistry::RegisterHost(AActor* Owner, TFunction<ASlabActor*()> ResolveTarget, bool bActive)
{
    check(IsInGameThread());
    if (!IsValid(Owner) || Owner->IsTemplate() || !ResolveTarget) return;
    PruneHosts();
    for (FHostBinding& Binding : Hosts)
    {
        if (Binding.Owner.Get() == Owner) { Binding.ResolveTarget = MoveTemp(ResolveTarget); Binding.bActive=bActive; return; }
    }
    Hosts.Add({Owner, MoveTemp(ResolveTarget), bActive});
}

void SlabUiRegistry::UnregisterHost(AActor* Owner)
{
    check(IsInGameThread());
    PruneHosts();
    for (FHostBinding& Binding : Hosts) if (Binding.Owner.Get()==Owner) Binding.bActive=false;
}
void SlabUiRegistry::RegisterActivation(AActor* Owner,TFunction<bool(ASlabActor*)> Callback)
{check(IsInGameThread());PruneHosts();for(auto& B:Hosts)if(B.Owner.Get()==Owner){B.Activate=MoveTemp(Callback);return;}}
bool SlabUiRegistry::Activate(ASlabActor* Slab)
{
    check(IsInGameThread());if(!IsValid(Slab)||Slab->IsActorBeingDestroyed())return false;PruneHosts();
    // An explicit 3D selection may retarget the existing owned information panel.
    for(bool Exact:{true,false})for(auto& B:Hosts)
        if(B.bActive&&B.Activate&&B.Owner->GetWorld()==Slab->GetWorld()&&IsActiveHostLevel(B.Owner.Get())&&((B.ResolveTarget()==Slab)==Exact))
            if(B.Activate(Slab))return true;
    return false;
}
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
bool SlabUiRegistry::IsHostActiveForTesting(const AActor* Owner)
{
    check(IsInGameThread());
    PruneHosts();
    for (const FHostBinding& Binding : Hosts) if (Binding.Owner.Get()==Owner) return Binding.bActive;
    return false;
}
#endif

bool SlabUiRegistry::HasHostFor(const ASlabActor* Slab, int32 SlabCount)
{
    check(IsInGameThread());
    if (!Slab || !Slab->GetWorld()) return false;
    PruneHosts();
    for (const FHostBinding& Binding : Hosts)
    {
        if (Binding.Owner->GetWorld() != Slab->GetWorld() || !IsActiveHostLevel(Binding.Owner.Get())) continue;
        ASlabActor* Target = Binding.ResolveTarget();
        if (Target == Slab || (!Target && SlabCount == 1)) return true;
    }
    return false;
}
