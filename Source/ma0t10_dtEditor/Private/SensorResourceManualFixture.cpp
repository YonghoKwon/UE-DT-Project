#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"

static FAutoConsoleCommandWithWorldAndArgs FreezeResourceManualAcquisition(
    TEXT("SensorResource.Freeze"),TEXT("Explicit manual fixture only: Camera, LiDAR or None. Keeps registration/rate history to test real stalled-state diagnostics."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld*){
        if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RESOURCE_MANUAL_FIXTURE"))!=TEXT("1")||Args.Num()!=1)return;
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!World)return;
        for(TActorIterator<AVirtualCameraSensorActor> It(World);It;++It)It->CaptureComponent->FreezeAcquisitionForTests=Args[0].Equals(TEXT("Camera"),ESearchCase::IgnoreCase);
        for(TActorIterator<AVirtualLidarSensorActor> It(World);It;++It)It->ScanComponent->FreezeAcquisitionForTests=Args[0].Equals(TEXT("LiDAR"),ESearchCase::IgnoreCase);
        UE_LOG(LogTemp,Display,TEXT("[ResourceManual] freeze=%s (test fixture only)"),*Args[0]);
    }));

/** Opens the exact-sized interactive fixture only. Mouse acceptance is recorded separately. */
class FWaitForResourceManualFixture : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; double Started=FPlatformTime::Seconds();bool SawPIE=false,LoggedSize=false;
public:
    explicit FWaitForResourceManualFixture(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(World){SawPIE=true;if(FinalAcceptanceViewportReady(World)&&!LoggedSize){
            Test->AddInfo(FString::Printf(TEXT("Manual fixture actual client %dx%d; this is not a mouse checklist result"),World->GetGameViewport()->Viewport->GetSizeXY().X,World->GetGameViewport()->Viewport->GetSizeXY().Y));LoggedSize=true;}}
        const FString Stop=FPaths::ProjectSavedDir()/TEXT("Reports/SensorResourceLifecycle/finish-manual.flag");
        if(IFileManager::Get().FileExists(*Stop))return true;
        if(SawPIE&&!World)return true;
        if(FPlatformTime::Seconds()-Started>1800){Test->AddError(TEXT("Manual fixture expired; direct checks must be reported separately"));return true;}
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorResourceManualFixture,"SensorResource.ManualFixture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorResourceManualFixture::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RESOURCE_MANUAL_FIXTURE"))!=TEXT("1")){AddInfo(TEXT("SKIP: explicit interactive fixture only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FWaitForResourceManualFixture(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
