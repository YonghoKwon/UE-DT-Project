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
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "Misc/FileHelper.h"

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
            Test->AddInfo(FString::Printf(TEXT("Manual fixture actual client %dx%d; this is not a mouse checklist result"),World->GetGameViewport()->Viewport->GetSizeXY().X,World->GetGameViewport()->Viewport->GetSizeXY().Y));
            if(auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef()))
                Test->AddInfo(FString::Printf(TEXT("Manual fixture window DPI scale=%.3f screen size=%.0fx%.0f"),Window->GetDPIScaleFactor(),Window->GetSizeInScreen().X,Window->GetSizeInScreen().Y));
            LoggedSize=true;}}
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

/** Render evidence only; this does not certify physical mouse interaction. */
class FPlacementPresentationCapture : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),ReadyAt=0;int32 ScaleIndex=0;bool Requested=false;FString Path;FDateTime PreviousTimestamp;
public:
    explicit FPlacementPresentationCapture(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>60){Test->AddError(TEXT("Placement presentation fixture timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        if(!Workspace->GetOwnedPanel(ESensorToolPanelRole::Placement))return false;
        const float Scales[]={.85f,1.f,1.25f,1.5f};
        if(ReadyAt==0)
        {
            Workspace->ResetOwnedWorkspaceLayout();Workspace->SetOwnedPanelFontScale(Scales[ScaleIndex]);
            Workspace->SetPanelOpen(ESensorToolPanelRole::Placement,true);ReadyAt=FPlatformTime::Seconds()+1;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        if(!Requested)
        {
            const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();
            Path=FPaths::ProjectSavedDir()/TEXT("Reports/PlacementUiPreflightClosure")/FString::Printf(TEXT("rendered-final-%dx%d-%d.png"),Size.X,Size.Y,FMath::RoundToInt(Scales[ScaleIndex]*100));
            PreviousTimestamp=IFileManager::Get().GetTimeStamp(*Path);
            FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());
            Test->AddInfo(FString::Printf(TEXT("Rendered UI actual client=%dx%d scale=%d windowDpi=%.3f; automated capture, not mouse verification"),Size.X,Size.Y,FMath::RoundToInt(Scales[ScaleIndex]*100),Window?Window->GetDPIScaleFactor():0));
            return false;
        }
        if(!IFileManager::Get().FileExists(*Path)||IFileManager::Get().GetTimeStamp(*Path)<=PreviousTimestamp)return false;
        TArray<uint8> Bytes;FFileHelper::LoadFileToArray(Bytes,*Path);Test->TestTrue(TEXT("rendered UI PNG exists"),Bytes.Num()>24);
        ++ScaleIndex;if(ScaleIndex==UE_ARRAY_COUNT(Scales))return true;
        Requested=false;ReadyAt=0;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementUiPresentationRhi,"PlacementUi.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlacementUiPresentationRhi::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PLACEMENT_PRESENTATION_RHI"))!=TEXT("1")){AddInfo(TEXT("SKIP: explicit rendered presentation evidence only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FPlacementPresentationCapture(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
