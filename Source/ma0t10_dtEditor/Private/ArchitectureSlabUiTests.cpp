#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabUiRegistry.h"
#include "ma0t10_dt/MA0T10/UI/SlabSimulationUiHostActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArchitectureSlabUiTest,"MA0T10.Architecture.SlabUiRegistry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArchitectureSlabUiTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* A=World->SpawnActor<ASlabActor>();
    auto CheckLegacy=[&](const TCHAR* Label,ASlabActor* Slab)
    {
        UWorld* CheckedWorld=Slab->GetWorld();
        int32 Count=0;for(TActorIterator<ASlabActor> It(CheckedWorld);It;++It)++Count;
        bool Old=false;for(TActorIterator<ASlabSimulationUiHostActor> It(CheckedWorld);It;++It)
            if(It->SlabActor==Slab||(!It->SlabActor&&Count==1))Old=true;
        TestEqual(Label,SlabUiRegistry::HasHostFor(Slab,Count),Old);
    };
    CheckLegacy(TEXT("no host"),A);
    auto* Host=World->SpawnActorDeferred<ASlabSimulationUiHostActor>(ASlabSimulationUiHostActor::StaticClass(),FTransform::Identity);
    TestFalse(TEXT("host is registered before BeginPlay"),Host->HasActorBegunPlay());
    CheckLegacy(TEXT("deferred unbound host before FinishSpawning"),A);
    Host->SlabActor=A;CheckLegacy(TEXT("direct property change before host initialization"),A);
    Host->FinishSpawning(FTransform::Identity);
    CheckLegacy(TEXT("editor host before BeginPlay"),A);
    auto* B=World->SpawnActor<ASlabActor>();
    Host->SlabActor=nullptr;CheckLegacy(TEXT("ambiguous unbound host A"),A);CheckLegacy(TEXT("ambiguous unbound host B"),B);
    Host->SlabActor=B;CheckLegacy(TEXT("explicit different slab A"),A);CheckLegacy(TEXT("explicit different slab B"),B);
    Host->BindSlabActor(A);CheckLegacy(TEXT("explicit API rebind A"),A);CheckLegacy(TEXT("explicit API rebind B"),B);
    ULevel* Level=Host->GetLevel();
    const bool Visible=Level->bIsVisible,Removing=Level->bIsBeingRemoved,Associating=Level->bIsAssociatingLevel,Disassociating=Level->bIsDisassociatingLevel;
    Level->bIsVisible=false;Level->bIsBeingRemoved=false;Level->bIsAssociatingLevel=false;Level->bIsDisassociatingLevel=false;
    CheckLegacy(TEXT("invisible inactive level excluded"),A);
    Level->bIsAssociatingLevel=true;CheckLegacy(TEXT("associating level included like iterator"),A);
    Level->bIsAssociatingLevel=false;Level->bIsDisassociatingLevel=true;CheckLegacy(TEXT("disassociating level included like iterator"),A);
    Level->bIsVisible=Visible;Level->bIsBeingRemoved=Removing;Level->bIsAssociatingLevel=Associating;Level->bIsDisassociatingLevel=Disassociating;
    auto* Other=UWorld::CreateWorld(EWorldType::Editor,false);
    auto* Foreign=Other->SpawnActor<ASlabSimulationUiHostActor>();Foreign->SlabActor=A;
    auto* Copy=Cast<ASlabSimulationUiHostActor>(StaticDuplicateObject(Host,Host->GetLevel(),MakeUniqueObjectName(Host->GetLevel(),Host->GetClass())));
    TestNotNull(TEXT("native duplication produces another explicit host"),Copy);
    Host->Destroy();CheckLegacy(TEXT("duplicate registers before BeginPlay after original removed"),A);
    if(Copy)Copy->Destroy();CheckLegacy(TEXT("destroy removes local binding, foreign world excluded"),A);
    Foreign->Destroy();Other->DestroyWorld(false);
    B->Destroy();CheckLegacy(TEXT("one slab again without a host"),A);
    auto* GameWorld=UWorld::CreateWorld(EWorldType::Game,false);
    auto* GameSlab=GameWorld->SpawnActor<ASlabActor>();
    auto* LateHost=GameWorld->SpawnActorDeferred<ASlabSimulationUiHostActor>(ASlabSimulationUiHostActor::StaticClass(),FTransform::Identity);
    CheckLegacy(TEXT("game world deferred unbound host before BeginPlay"),GameSlab);
    LateHost->SlabActor=GameSlab;
    CheckLegacy(TEXT("game world deferred explicit host before BeginPlay"),GameSlab);
    LateHost->FinishSpawning(FTransform::Identity);
    CheckLegacy(TEXT("game world finished host before BeginPlay"),GameSlab);
    TestFalse(TEXT("configured presence is not active UI before BeginPlay"),SlabUiRegistry::IsHostActiveForTesting(LateHost));
    LateHost->PostInitializeComponents(); // The isolated world has not routed actor initialization.
    LateHost->DispatchBeginPlay();
    TestTrue(TEXT("BeginPlay activates the existing presence"),SlabUiRegistry::IsHostActiveForTesting(LateHost));
    LateHost->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
    TestFalse(TEXT("EndPlay unregisters activation"),SlabUiRegistry::IsHostActiveForTesting(LateHost));
    CheckLegacy(TEXT("valid Actor presence during teardown retains legacy result"),GameSlab);
    LateHost->Destroy();GameSlab->Destroy();GameWorld->DestroyWorld(false);
    return true;
}
#endif
