#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Json.h"

class FDatasetManualAcceptanceCommand final:public IAutomationLatentCommand
{
public:
    explicit FDatasetManualAcceptanceCommand(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(!World.IsValid())
        {
            UWorld* Candidate=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){Candidate=C.World();break;}
            if(Candidate!=Observed.Get()){Observed=Candidate;ObservedAt=Now;return false;}
            if(!Candidate||Now-ObservedAt<3)return false;World=Candidate;Started=Now;
            for(TActorIterator<ASlabActor> It(Candidate);It;++It)It->StopSimulation();for(TActorIterator<AVirtualSensorActorBase> It(Candidate);It;++It)It->StopSensor();
            Slab=Candidate->SpawnActor<ASlabActor>();Slab->Tags.Add(TEXT("SensorTestManaged"));Slab->SetActorLocation(FVector(1500,0,200));Slab->ApplyDatasetGeometry(FVector(800,200,50));Slab->SetDiagnosticHelpersVisible(false);
            Lidar=Candidate->SpawnActor<AVirtualLidarSensorActor>();Lidar->Tags.Add(TEXT("SensorTestManaged"));Lidar->StopSensor();Lidar->SetActorLocation(FVector(1500,-1000,1000));Lidar->SetActorRotation((Slab->GetActorLocation()-Lidar->GetActorLocation()).Rotation());
            Lidar->ScanComponent->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Lidar->ScanComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Lidar->ScanComponent->SensorId=TEXT("DATASET-MANUAL-LIDAR");Lidar->ScanComponent->OutputMode=EVirtualLidarOutputMode::None;
            Camera=Candidate->SpawnActor<AVirtualCameraSensorActor>();Camera->Tags.Add(TEXT("SensorTestManaged"));Camera->StopSensor();Camera->SetActorTransform(Lidar->GetActorTransform());Camera->CaptureComponent->ApplyDeviceProfile(EVirtualCameraDeviceProfile::IntelRealSenseD455);Camera->CaptureComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Camera->CaptureComponent->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;
            Light=Candidate->SpawnActor<ADirectionalLight>();Light->Tags.Add(TEXT("SensorTestManaged"));Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            return false;
        }
        auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Service=World->GetSubsystem<UVirtualDatasetSubsystem>();
        if(!Ready&&Now-Started>5&&Workspace)
        {
            auto* Panel=Cast<UVirtualSensorCaptureExportPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Data));if(!Panel)return false;
            Panel->DatasetDraft.Name=TEXT("DATASET01_manual");Panel->DatasetDraft.Scenes=100;Panel->DatasetDraft.Seed=22015;
            Panel->DatasetSlab=Slab.Get();Panel->DatasetLidar=Lidar.Get();Panel->DatasetCamera=Camera.Get();Panel->DatasetLight=Light.Get();
            Workspace->SetOwnedPanelFontScale(1);Workspace->SetPanelOpen(ESensorToolPanelRole::Data,true);Panel->OpenOwnedTab(ESensorOwnedDataTab::FileSave);
            if(FParse::Param(FCommandLine::Get(),TEXT("Dataset01Automated")))
            {
                Panel->DatasetDraft.Enabled=true;Panel->DatasetDraft.CameraTruth=true;Panel->DatasetDraft.LidarFidelity=true;Panel->DatasetDraft.Yaw.Enabled=true;Panel->DatasetDraft.Scenes=100;
                if(auto Area=Panel->DatasetArea.Pin())Area->SetExpanded(true);
                FString Error;if(!Service->Start(Panel->DatasetDraft,Slab.Get(),Lidar.Get(),Camera.Get(),Light.Get(),Error))Test->AddError(Error);
            }
            if(GEngine->GameViewport&&GEngine->GameViewport->GetGameViewport())GEngine->GameViewport->GetGameViewport()->ResizeFrame(1920,1080,EWindowMode::Windowed);
            Ready=true;ReadyAt=Now;return false;
        }
        if(Ready&&!Recorded&&Now-ReadyAt>2)
        {
            auto O=MakeShared<FJsonObject>();const FIntPoint Size=GEngine->GameViewport->Viewport->GetSizeXY();O->SetNumberField(TEXT("viewport_width"),Size.X);O->SetNumberField(TEXT("viewport_height"),Size.Y);O->SetNumberField(TEXT("font_scale"),Workspace->GetOwnedPanelFontScale());
            const auto Window=GEngine->GameViewport->GetWindow();O->SetNumberField(TEXT("window_dpi_scale"),Window?Window->GetDPIScaleFactor():0);
            FString T;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&T));FFileHelper::SaveStringToFile(T,*(Report()/TEXT("ready.json")));
            FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(Report()/TEXT("automated-ready.png")),true,false);Recorded=true;
        }
        if(Recorded&&Service->IsActive()&&!CapturedProgress)
        {FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(Report()/TEXT("automated-progress.png")),true,false);CapturedProgress=true;}
        if(FPaths::FileExists(Report()/TEXT("finish.flag"))||Now-Started>300||(CapturedProgress&&FParse::Param(FCommandLine::Get(),TEXT("Dataset01Automated"))&&Service->CompletedScenes()>=5))
        {
            Service->Stop();if(Service->IsActive())return false;FFileHelper::SaveStringToFile(Service->Directory(),*(Report()/TEXT("result-path.txt")));return true;
        }
        return false;
    }
private:
    FString Report() const{return FPaths::ProjectSavedDir()/TEXT("Reports/DATASET-01/manual");}
    FAutomationTestBase* Test;TWeakObjectPtr<UWorld> World,Observed;double ObservedAt=0,Started=0,ReadyAt=0;bool Ready=false,Recorded=false,CapturedProgress=false;
    TWeakObjectPtr<ASlabActor> Slab;TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;TWeakObjectPtr<AVirtualCameraSensorActor> Camera;TWeakObjectPtr<ADirectionalLight> Light;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDatasetManualAcceptanceTest,"MA0T10.Dataset.ManualAcceptanceSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDatasetManualAcceptanceTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("Dataset01Manual"))){AddInfo(TEXT("[OPT-IN SKIP] -Dataset01Manual not supplied; direct session requires an operator"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FDatasetManualAcceptanceCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
