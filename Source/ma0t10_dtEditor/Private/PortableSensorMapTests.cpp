#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolRig.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/SlabSimulationUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorMapCheckWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "Core/DxWidgetSubsystem.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
const FString ReportRoot=FPaths::ProjectSavedDir()/TEXT("Reports/MAP/MAP-01");
UWorld* PieWorld(){for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)return C.World();return nullptr;}
class FPortableFlow : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;bool HasSlab;double Started=FPlatformTime::Seconds(),ScenarioAt=0;int32 Phase=0;
    FString Request;TWeakObjectPtr<AVirtualSensorActorBase> Selected;FTransform Pose;
public:
    FPortableFlow(FAutomationTestBase* T,bool S):Test(T),HasSlab(S){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>50){Test->AddError(TEXT("Portable flow timed out"));return true;}
        auto* World=PieWorld();if(!FinalAcceptanceViewportReady(World))return false;
        AVirtualSensorToolRig* Rig=nullptr;for(TActorIterator<AVirtualSensorToolRig> It(World);It;++It)Rig=*It;
        if(!Rig||!Rig->IsSetupReady())return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=Rig->Coordinator.Get();
        auto* P=Rig->SensorUiHost->GetPlacementWidget();auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());
        auto* Library=World->GetSubsystem<UVirtualSensorPlacementSubsystem>();if(!P||!Bar)return false;
        if(Phase==0)
        {
            Test->TestEqual(TEXT("only composition actors created"),Rig->GetOwnedActorCount(),HasSlab?4:2);
            Test->TestEqual(TEXT("Slab presence preserved"),IsValid(Rig->TargetSlab),HasSlab);
            Library->SetAutomationStorageRoot(ReportRoot/TEXT("poses")/FGuid::NewGuid().ToString());
            Test->TestTrue(TEXT("add LiDAR through existing placement panel"),P->AddSensor(EVirtualSensorKind::Lidar));P->EndPlacement();
            Test->TestTrue(TEXT("add Camera through existing placement panel"),P->AddSensor(EVirtualSensorKind::Camera));P->EndPlacement();
            Test->TestEqual(TEXT("both equipment registered"),C->GetSensorActors().Num(),2);
            Selected=C->GetSelectedSensorActor();if(!Selected.IsValid()){Test->AddError(TEXT("No selected equipment"));return true;}
            Test->TestTrue(TEXT("panel selection"),P->SelectEquipment(Selected.Get()));
            Test->TestTrue(TEXT("enter manipulation"),P->SetSelectedManipulating(true));Pose=Selected->GetActorTransform();Pose.AddToTranslation(FVector(30,50,100));
            Test->TestTrue(TEXT("interaction moves selected equipment"),Selected->UpdateInteractiveTransform(Pose));P->EndPlacement();
            Request=P->SaveCurrentPose(TEXT("portable pose"));Test->TestFalse(TEXT("pose save started"),Request.IsEmpty());
            Test->TestFalse(TEXT("new map cannot queue automatic map write"),Rig->SensorUiHost->CanQueueMapApply());
            Test->TestFalse(TEXT("compatibility API rejects before changing pending state"),Rig->SensorUiHost->GetSettingsWidget()->QueuePendingStateForSensorTestMap());
            Rig->SensorUiHost->QueueSensorStateForMapApply(FVirtualSensorEditableState());Test->TestFalse(TEXT("host API cannot bypass map guard"),Rig->SensorUiHost->HasPendingMapApplySnapshot());
            ++Phase;return false;
        }
        if(Phase==1)
        {
            const auto Save=Library->GetPoseSaveStatus(Request);if(Save.State==EVirtualSensorPoseSaveState::Writing)return false;
            Test->TestEqual(TEXT("asynchronous pose file saved"),Save.State,EVirtualSensorPoseSaveState::Saved);
            FString Error;Selected->ApplyEditableTransform(FTransform(FVector(900,900,900)),Error);
            Test->TestTrue(TEXT("saved pose recalled"),P->MoveToBookmark(Save.BookmarkId));
            Test->TestTrue(TEXT("pose restored"),UVirtualSensorPlacementSubsystem::SamePose(Pose,Selected->GetActorTransform()));
            Test->TestTrue(TEXT("PCD screen accessible"),Bar->OpenOwnedDestination(ESensorToolDestination::PcdTransfer));
            Test->TestEqual(TEXT("PCD transfer tab selected"),Rig->SensorUiHost->GetCaptureExportWidget()->GetOwnedTab(),ESensorOwnedDataTab::Transfer);
            const auto Check=UVirtualSensorOutputPreflight::InspectForPurpose(C,FVirtualSlabSensorOutputSelection(),{},EVirtualSensorPreflightPurpose::ManualStreamStart);
            Test->TestTrue(TEXT("read-only preflight gives streams or actionable reasons"),!Check.Streams.IsEmpty()||!Check.BlockingReasons.IsEmpty());
            Test->TestTrue(TEXT("results accessible"),Bar->OpenOwnedDestination(ESensorToolDestination::Results));
            Test->TestTrue(TEXT("map check opens through toolbar"),Bar->OpenOwnedDestination(ESensorToolDestination::MapCheck));
            auto* MapCheck=W->GetMapCheckWidget();Test->TestNotNull(TEXT("map check exists"),MapCheck);
            if(MapCheck){MapCheck->RefreshChecks();Test->TestEqual(TEXT("all configuration checks recorded"),MapCheck->GetRows().Num(),11);MapCheck->CloseWidget();}
            if(HasSlab)
            {
                Test->TestTrue(TEXT("scenario injected through Actor API"),Rig->TargetSlab->SubmitScenarioJson(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower))));
                Test->TestTrue(TEXT("scenario card reachable"),Bar->OpenOwnedDestination(ESensorToolDestination::Scenario));
                Test->TestEqual(TEXT("card bound to explicit Slab"),Rig->SlabUiHost->ProgressWidget->GetBoundSlabActor(),Rig->TargetSlab.Get());ScenarioAt=FPlatformTime::Seconds();
            }
            else Test->TestTrue(TEXT("no-Slab scenario opens placement guidance"),Bar->OpenOwnedDestination(ESensorToolDestination::Scenario));
            ++Phase;return false;
        }
        if(HasSlab&&FPlatformTime::Seconds()-ScenarioAt<2)return false;
        if(HasSlab){Test->TestTrue(TEXT("scenario progressed"),Rig->TargetSlab->GetSimulationStatus().Progress>0);Rig->TargetSlab->StopSimulation();}
        TArray<TWeakObjectPtr<AActor>> Owned;for(TActorIterator<AActor> It(World);It;++It)if(It->GetOwner()==Rig)Owned.Add(*It);
        TArray<TPair<TWeakObjectPtr<AActor>,FTransform>> Existing;for(TActorIterator<AActor> It(World);It;++It)if(It->GetOwner()!=Rig&&*It!=Rig&&*It!=Rig->TargetSlab)Existing.Add({*It,It->GetActorTransform()});
        Rig->Destroy();for(const auto& A:Owned)Test->TestTrue(TEXT("created helper destroyed by Rig EndPlay"),!A.IsValid()||A->IsActorBeingDestroyed());
        Test->TestEqual(TEXT("zero owned helper actors remain"),Rig->GetOwnedActorCount(),0);
        for(const auto& A:Existing){Test->TestTrue(TEXT("existing actors survive Rig cleanup"),A.Key.IsValid()&&!A.Key->IsActorBeingDestroyed());if(A.Key.IsValid())Test->TestTrue(TEXT("existing transforms unchanged"),A.Key->GetActorTransform().Equals(A.Value));}
        return true;
    }
};
class FPortableRender : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;FString MapName;int32 Phase=0;double Started=FPlatformTime::Seconds(),ReadyAt=0;FString Path;FDateTime Previous;bool Requested=false;
    TUniquePtr<TGuardValue<bool>> Debug;
public:
    FPortableRender(FAutomationTestBase* T,const FString& Map):Test(T),MapName(Map){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>60){Test->AddError(TEXT("Portable capture timeout"));return true;}
        auto* World=PieWorld();if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());if(!Bar)return false;
        if(!ReadyAt)
        {
            if(!Debug)Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(Phase<2?1:1.5f);
            if(Phase%2)Test->TestTrue(TEXT("map check opens"),Bar->OpenOwnedDestination(ESensorToolDestination::MapCheck));
            else if(W->GetMapCheckWidget())W->GetMapCheckWidget()->CloseWidget();
            ReadyAt=FPlatformTime::Seconds()+1;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        if(!Requested)
        {
            const auto V=World->GetGameViewport()->Viewport->GetSizeXY();Path=ReportRoot/FString::Printf(TEXT("%s-%dx%d-%d-%s.png"),*MapName,V.X,V.Y,Phase<2?100:150,Phase%2?TEXT("map-check"):TEXT("default"));
            Previous=IFileManager::Get().GetTimeStamp(*Path);FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;
            Test->AddInfo(FString::Printf(TEXT("Actual client %dx%d; font %d; automated capture %s"),V.X,V.Y,Phase<2?100:150,*Path));return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=Previous)return false;
        Requested=false;ReadyAt=0;return ++Phase==4;
    }
};

class FPortableLifecycle : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),ReadyAt=0;int32 Phase=0;
public:
    explicit FPortableLifecycle(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>35){Test->AddError(TEXT("Portable lifecycle timeout"));return true;}
        auto* World=PieWorld();if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        if(Phase==0)
        {
            AVirtualSensorToolRig* Rig=nullptr;for(TActorIterator<AVirtualSensorToolRig> It(World);It;++It)Rig=*It;if(!Rig||!Rig->IsSetupReady())return false;
            auto* Duplicate=World->SpawnActor<AVirtualSensorToolRig>();
            Test->TestFalse(TEXT("duplicate Rig rejected"),Duplicate->EnsureSetup());Test->TestEqual(TEXT("duplicate creates no helpers"),Duplicate->GetOwnedActorCount(),0);
            Test->TestFalse(TEXT("both Rigs report duplicate"),Rig->EnsureSetup());Test->TestTrue(TEXT("duplicate has actionable message"),Duplicate->SetupMessage.Contains(TEXT("하나만")));Duplicate->Destroy();
            auto* A=World->SpawnActor<ASlabActor>();auto* B=World->SpawnActor<ASlabActor>();
            Test->TestTrue(TEXT("sensor setup remains ready with multiple Slabs"),Rig->EnsureSetup());
            Test->TestNull(TEXT("multiple Slabs never select iterator first"),Rig->TargetSlab.Get());
            Test->TestTrue(TEXT("multiple Slabs explain selection"),Rig->GetSlabSetupMessage().Contains(TEXT("대상을 선택")));
            auto* Checks=NewObject<USensorMapCheckWidget>(World);Checks->RefreshChecks();
            auto Check=[&](const TCHAR* Key){const auto* Row=Checks->GetRows().FindByPredicate([&](const auto& R){return R.Key==Key;});return Row&&Row->bOk;};
            Test->TestFalse(TEXT("duplicate ReceiverId warning"),Check(TEXT("ReceiverId")));Test->TestFalse(TEXT("missing TrackReference warning"),Check(TEXT("TrackReference 연결")));
            B->Destroy();Test->TestTrue(TEXT("late single Slab setup succeeds"),Rig->EnsureSetup());Test->TestEqual(TEXT("late Slab becomes target"),Rig->TargetSlab.Get(),A);
            B=World->SpawnActor<ASlabActor>();Rig->TargetSlab=B;Rig->EnsureSetup();
            Test->TestEqual(TEXT("explicit multi-Slab target binds PCD settings"),Rig->SensorUiHost->GetCaptureExportWidget()->GetExplicitScenarioSlabActor(),B);
            Rig->SlabUiHost->BindSlabActor(A);Rig->EnsureSetup();
            Test->TestEqual(TEXT("existing 3D panel selection preserved"),Rig->TargetSlab.Get(),A);
            Test->TestEqual(TEXT("PCD settings follow the same selected Slab"),Rig->SensorUiHost->GetCaptureExportWidget()->GetExplicitScenarioSlabActor(),A);B->Destroy();
            Test->TestEqual(TEXT("late setup creates only Slab owners"),Rig->GetOwnedActorCount(),4);const auto Pose=A->GetActorTransform();Rig->Destroy();
            Test->TestTrue(TEXT("late Slab survives Rig cleanup"),IsValid(A)&&!A->IsActorBeingDestroyed()&&A->GetActorTransform().Equals(Pose));A->Destroy();
            ReadyAt=FPlatformTime::Seconds()+.5;++Phase;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        auto* C=World->SpawnActor<AVirtualSensorCoordinator>();
        auto* Host=World->SpawnActorDeferred<AVirtualSensorUiHostActor>(AVirtualSensorUiHostActor::StaticClass(),FTransform::Identity);Host->SensorManager=C;Host->FinishSpawning(FTransform::Identity);
        auto* Rig=World->SpawnActor<AVirtualSensorToolRig>();Test->TestTrue(TEXT("existing configuration reused"),Rig->EnsureSetup());
        Test->TestEqual(TEXT("no helpers created for existing sensor config"),Rig->GetOwnedActorCount(),0);
        Test->TestEqual(TEXT("same Coordinator instance"),Rig->Coordinator.Get(),C);Test->TestEqual(TEXT("same Host instance"),Rig->SensorUiHost.Get(),Host);
        const auto CoordinatorPose=C->GetActorTransform(),HostPose=Host->GetActorTransform();Rig->Destroy();
        Test->TestTrue(TEXT("reused actors survive cleanup"),!C->IsActorBeingDestroyed()&&!Host->IsActorBeingDestroyed());
        Test->TestTrue(TEXT("reused actors retain transforms"),C->GetActorTransform().Equals(CoordinatorPose)&&Host->GetActorTransform().Equals(HostPose));
        Test->TestEqual(TEXT("reused Host binding restored"),Host->SensorManager.Get(),C);
        Host->Destroy();C->Destroy();return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortableSlabMapTest,"MA0T10.MAP01.PortableSetupFlow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPortableSlabMapTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/PortableSetupMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FPortableFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortableNoSlabMapTest,"MA0T10.MAP01.PortableNoSlabFlow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPortableNoSlabMapTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/PortableNoSlabMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FPortableFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortableLifecycleTest,"MA0T10.MAP01.OwnershipDuplicateLateSlab",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPortableLifecycleTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/PortableNoSlabMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FPortableLifecycle(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortableRenderTest,"MAP01.Render.Setup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPortableRenderTest::RunTest(const FString&)
{
    // Deliberate render suite outside MA0T10 release name inventory, run at both real client resolutions.
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/PortableSetupMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FPortableRender(this,TEXT("PortableSetupMap")));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortableNoSlabRenderTest,"MAP01.Render.NoSlab",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPortableNoSlabRenderTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/PortableNoSlabMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FPortableRender(this,TEXT("PortableNoSlabMap")));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
