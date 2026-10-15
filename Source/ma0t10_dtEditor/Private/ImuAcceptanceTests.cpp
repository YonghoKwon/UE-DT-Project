#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationControlSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Json.h"

class FImuAcceptanceCommand:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Began=FPlatformTime::Seconds(),Ready=0,MotionEpoch=0;
    TWeakObjectPtr<AVirtualLidarSensorActor> Sensor;
    FTransform Before;
    FString Request;
    bool Auto=false,Requested=false;
    bool CancellationRequested=false;
    FString Root()const{return FPaths::ProjectSavedDir()/TEXT("Reports/IMU-01/pie");}
public:
    explicit FImuAcceptanceCommand(FAutomationTestBase* T):Test(T){Auto=FParse::Param(FCommandLine::Get(),TEXT("ImuAutomated"));}
    bool Update() override
    {
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||!FinalAcceptanceViewportReady(W)){if(FPlatformTime::Seconds()-Began>60){Test->AddError(TEXT("IMU PIE viewport timeout"));return true;}return false;}
        auto* Workspace=W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Coordinator=Workspace->GetCoordinator();
        if(!Ready)
        {
            if(!Coordinator)return false;
            for(auto* A:Coordinator->GetSensorActors()){A->StopSensor();if(auto* L=Cast<AVirtualLidarSensorActor>(A))Sensor=L;}
            if(!Sensor.IsValid()){Test->AddError(TEXT("IMU fixture LiDAR missing"));return true;}
            Test->TestNull(TEXT("IMU absent before explicit attachment"),Sensor->FindComponentByClass<UVirtualImuComponent>());
            Before=Sensor->GetActorTransform();Coordinator->SelectSensorActor(Sensor.Get());Workspace->SynchronizeOwnedSelection();
            Workspace->ResetOwnedWorkspaceLayout();Workspace->SetOwnedPanelFontScale(1);Workspace->SetPanelOpen(ESensorToolPanelRole::Placement,true);
            for(auto Role:{ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::Replay,ESensorToolPanelRole::Monitor})Workspace->SetPanelOpen(Role,false);
            if(Auto)
            {
                auto* C=UVirtualImuComponent::Attach(Sensor.Get());FVirtualImuConfig Config;Config.Noise=false;FString Error;C->Configure(Config,Error);C->SetRecording(true);
                if(FParse::Param(FCommandLine::Get(),TEXT("ImuBroker")))
                {
                    auto* Transport=Coordinator->SharedTransportComponent.Get();auto Profile=Transport->GetTransportProfile();Profile.BrokerUrl=TEXT("ws://127.0.0.1:61616");
                    Profile.UserName=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_STOMP_USER"));
                    Transport->SetSessionCredentials(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_STOMP_PASSWORD")),TEXT(""));Transport->ConfigureTransportProfile(Profile);Transport->TransportMode=EVirtualSensorTransportMode::StompWebSocket;C->SetPublishing(true);
                }
            }
            IFileManager::Get().MakeDirectory(*Root(),true);Ready=FPlatformTime::Seconds();MotionEpoch=W->GetTimeSeconds();
            auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("viewport_x"),W->GetGameViewport()->Viewport->GetSizeXY().X);J->SetNumberField(TEXT("viewport_y"),W->GetGameViewport()->Viewport->GetSizeXY().Y);J->SetNumberField(TEXT("font_scale"),1);
            auto Window=W->GetGameViewport()->GetWindow();J->SetNumberField(TEXT("dpi"),Window?Window->GetDPIScaleFactor():0);J->SetBoolField(TEXT("automated"),Auto);
            FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Root()/TEXT("ready.json")));return false;
        }
        const double Elapsed=FPlatformTime::Seconds()-Ready,Time=W->GetTimeSeconds()-MotionEpoch;
        if(!Sensor.IsValid()){Test->AddError(TEXT("IMU sensor deleted"));return true;}
        auto Pose=Before;Pose.AddToTranslation(FVector(20*FMath::Sin(2*PI*Time),0,0));Pose.SetRotation(Before.GetRotation()*FQuat(FVector::UpVector,.25*Time));Sensor->SetActorTransform(Pose);
        auto* C=Sensor->FindComponentByClass<UVirtualImuComponent>();
        if(Auto&&Elapsed>8&&!Requested)
        {
            Test->TestTrue(TEXT("PIE fixed-step samples acquired"),C&&C->SampleCount>1000);
            if(C){C->SetRecording(false);Request=C->SaveCsv();FString Text;FJsonSerializer::Serialize(C->StatusJson(),TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Root()/TEXT("status.json")));}
            if(FParse::Param(FCommandLine::Get(),TEXT("ImuBroker")))
            {
                bool Received=false;for(const auto& S:Coordinator->StreamPublisherComponent->GetStreamStatuses())if(S.StreamKind==EVirtualSensorStreamKind::Imu){Received|=S.ReceiptReceivedCount>0;Test->TestTrue(TEXT("IMU messages submitted"),S.SubmittedFrameCount>0);}
                Test->TestTrue(TEXT("Artemis IMU receipt received"),Received);C->SetPublishing(false);
            }
            Requested=true;
        }
        if(Requested)
        {
            const auto Status=W->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(Request);if(!Status.IsTerminal())return false;
            if(CancellationRequested)
            {Test->TestEqual(TEXT("reconfigured IMU write cancelled"),Status.State,EVirtualSensorFileSaveState::Cancelled);C->DestroyComponent();Test->TestFalse(TEXT("IMU tick retired"),C->IsRegistered());return true;}
            Test->TestEqual(TEXT("async IMU CSV saved"),Status.State,EVirtualSensorFileSaveState::Succeeded);
            if(!Status.FileResults.IsEmpty())FFileHelper::SaveStringToFile(Status.FileResults[0].AbsolutePath,*(Root()/TEXT("csv-path.txt")));
            int32 Code=0;const auto Response=W->GetSubsystem<UAutomationControlSubsystem>()->Dispatch(TEXT("GET"),TEXT("/v1/sensors"),nullptr,Code);
            Test->TestEqual(TEXT("AUTO-01 sensors endpoint"),Code,200);bool Found=false;
            for(const auto& Item:Response->GetArrayField(TEXT("sensors")))if(Item->AsObject()->HasField(TEXT("imu")))Found=true;
            Test->TestTrue(TEXT("API contains attached IMU values"),Found);
            Request=C->SaveCsv();auto Config=C->GetConfig();++Config.Seed;FString Error;C->Configure(Config,Error);CancellationRequested=true;return false;
        }
        return FPaths::FileExists(Root()/TEXT("finish.flag"))||Elapsed>240;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuAcceptanceTest,"MA0T10.IMU01.PieAttachmentCsv",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImuAcceptanceTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ImuAutomated"))&&!FParse::Param(FCommandLine::Get(),TEXT("ImuManual")))
    {AddInfo(TEXT("[OPT-IN SKIP] -ImuAutomated or -ImuManual required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FImuAcceptanceCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

class FImuDatasetCommand:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Began=FPlatformTime::Seconds();int32 Stage=0;TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;
public:
    explicit FImuDatasetCommand(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Began>90){Test->AddError(TEXT("IMU dataset timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        auto* Service=W->GetSubsystem<UVirtualDatasetSubsystem>();
        if(Stage==0)
        {for(TActorIterator<AVirtualLidarSensorActor> I(W);I;++I){Lidar=*I;I->StopSensor();UVirtualImuComponent::Attach(*I);break;}Stage=1;return false;}
        auto* C=Lidar.IsValid()?Lidar->FindComponentByClass<UVirtualImuComponent>():nullptr;if(!C||!C->HasSample())return false;
        if(Stage==1)
        {
            ASlabActor* Slab=nullptr;AVirtualCameraSensorActor* Camera=nullptr;for(TActorIterator<ASlabActor> I(W);I;++I){Slab=*I;break;}for(TActorIterator<AVirtualCameraSensorActor> I(W);I;++I){Camera=*I;Camera->StopSensor();break;}
            FVirtualDatasetSettings Settings;Settings.Enabled=true;Settings.Name=TEXT("IMU01_label");Settings.Scenes=1;Settings.IncludeImu=true;
            FString Error;if(!Service->Start(Settings,Slab,Lidar.Get(),Camera,nullptr,Error)){Test->AddError(Error);return true;}Stage=2;return false;
        }
        if(Service->IsActive())return false;
        Test->TestTrue(TEXT("IMU opt-in dataset completes"),Service->HasSucceeded());TArray<FString> Labels;IFileManager::Get().FindFilesRecursive(Labels,*Service->Directory(),TEXT("label.json"),true,false);
        Test->TestEqual(TEXT("one scene label"),Labels.Num(),1);
        if(Labels.Num()==1)
        {FString Text;FFileHelper::LoadFileToString(Text,*Labels[0]);TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O);Test->TestTrue(TEXT("optional IMU snapshot in actual label"),O&&O->GetArrayField(TEXT("imu")).Num()==1&&O->GetArrayField(TEXT("imu"))[0]->AsObject()->HasField(TEXT("latest")));FFileHelper::SaveStringToFile(Labels[0],*(FPaths::ProjectSavedDir()/TEXT("Reports/IMU-01/dataset-label-path.txt")));}
        return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuDatasetTest,"MA0T10.IMU01.DatasetLabel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImuDatasetTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ImuDataset"))){AddInfo(TEXT("[OPT-IN SKIP] -ImuDataset required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FImuDatasetCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
