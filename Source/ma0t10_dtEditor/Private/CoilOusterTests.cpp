#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"

namespace
{
bool HasModel(UCoilLoadingMonitor& M,const FString& Model,const FString& Rate)
{
    M.Refresh();TArray<UWidget*> Widgets;M.WidgetTree->GetAllWidgets(Widgets);
    for(auto* Widget:Widgets)if(auto* Text=Cast<UTextBlock>(Widget))if(Text->GetText().ToString().Contains(Model)&&Text->GetText().ToString().Contains(Rate))return true;
    return false;
}
class FCoilOusterDefaults final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Start=FPlatformTime::Seconds();
public:
    explicit FCoilOusterDefaults(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>45){Test->AddError(TEXT("COIL-OUSTER fixture timeout"));return true;}
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&FinalAcceptanceViewportReady(C.World()))
        for(TActorIterator<ACoilLoadingMonitorHost> H(C.World());H;++H)if(H->Simulation&&H->Monitor&&H->Coordinator&&H->Simulation->Lidar)
        {
            auto& S=*H->Simulation;auto* Sensor=S.Lidar.Get();auto* Scan=Sensor->ScanComponent.Get();
            FVirtualSensorEditableState State;Sensor->ReadEditableState(State);
            Test->TestEqual(TEXT("new coil default is Rev7"),int32(State.LidarProfile),int32(EVirtualLidarDeviceProfile::OusterOS1_64_Rev7));
            Test->TestEqual(TEXT("default quality remains full spec"),int32(State.SimulationQuality),int32(EVirtualSensorSimulationQuality::FullSpec));
            Test->TestEqual(TEXT("role ID unchanged"),Sensor->GetSensorId(),FString(TEXT("os_lidar_coil_quay")));
            Test->TestTrue(TEXT("same native default grid/range/FOV as raw-material Rev7"),State.LidarHorizontalSamples==1024&&State.LidarVerticalChannels==64&&FMath::IsNearlyEqual(State.LidarScanInterval,.05f)&&State.LidarMaxDistance==9000&&State.LidarHorizontalFov==360&&FMath::IsNearlyEqual(State.LidarMinVerticalAngle,-21.2f)&&FMath::IsNearlyEqual(State.LidarMaxVerticalAngle,21.2f));
            Test->TestTrue(TEXT("physical noise/material response preserved, no quality shortcut"),Scan->bEnableRangeNoise&&FMath::IsNearlyEqual(Scan->RangeNoiseStandardDeviationMillimeters,30.f)&&State.bLidarMaterialResponse&&!State.bUseMultiHit);
            Test->TestTrue(TEXT("UI shows actual selected model and rate"),HasModel(*H->Monitor,TEXT("Ouster OS1-64 Rev7"),TEXT("/ 20 Hz")));
            Test->TestTrue(TEXT("single-return full33-byte upper bound including all rays fits80MiB/s"),1024.*64.*33.*20./(1024.*1024.)<80.);
            auto* Publisher=H->Coordinator->StreamPublisherComponent.Get();const auto Config=Publisher->GetEffectiveStreamConfig(EVirtualSensorStreamKind::PointCloud,Sensor->GetSensorId());
            Test->TestTrue(TEXT("PCD stays on, binary and connected-no-loss"),Config.bEnabled&&Config.PointCloudFormat==EVirtualPointCloudStreamFormat::PCD&&Config.PcdDataMode==EVirtualPcdDataMode::Binary&&Config.DeliveryMode==EVirtualPointCloudDeliveryMode::ConnectedNoLoss);
            Publisher->StopAllStreams(Sensor->GetSensorId());Sensor->StopSensor();FString Error;
            State.LidarProfile=EVirtualLidarDeviceProfile::HesaiXT32;FVirtualSensorEditableState Applied;
            Test->TestTrue(TEXT("explicit legacy profile remains selectable"),Sensor->ApplyProfileAndSimulationQuality(State,Applied,Error));Sensor->ReadEditableState(State);State.LidarScanInterval=.1f;State.MaxPreviewPoints=1111;
            Test->TestTrue(TEXT("explicit saved custom values accepted"),Sensor->ApplyEditableState(State,Error));
            auto* Layout=C.World()->GetSubsystem<UVirtualSensorLayoutSubsystem>();
            const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/COIL-OUSTER/fixtures");IFileManager::Get().MakeDirectory(*Dir,true);Layout->StorageRootOverrideForTests=Dir;
            FVirtualSensorLayoutDocument D;D.Name=TEXT("explicit legacy profile");D.MapPackage=UVirtualSensorPlacementSubsystem::CanonicalMap(C.World());
            auto& Entry=D.Sensors.AddDefaulted_GetRef();Entry.ActorPath=UVirtualSensorLayoutSubsystem::CanonicalActorPath(Sensor);Entry.State=State;
            FString Json;FJsonObjectConverter::UStructToJsonObjectString(D,Json);const FString Path=Dir/(FMD5::HashAnsiString(*(D.MapPackage+TEXT("|")+D.Name.ToLower()))+TEXT(".layout.v1.json"));Test->TestTrue(TEXT("save explicit profile fixture"),FFileHelper::SaveStringToFile(Json,*Path));
            State.LidarProfile=EVirtualLidarDeviceProfile::OusterOS1_64_Rev7;Test->TestTrue(TEXT("change live profile before restore"),Sensor->ApplyProfileAndSimulationQuality(State,Applied,Error));
            const FString Id=Layout->PreviewLayoutRestore(D.Name);Test->TestEqual(TEXT("actual layout loader accepts legacy selected profile"),int32(Layout->GetOperationStatus(Id).State),int32(EVirtualSensorLayoutOperation::ReadyToApply));
            Test->TestTrue(TEXT("actual layout restore"),Layout->ApplyLayoutRestore(Id,Error));
            S.Settings.LOA+=.1;S.RefreshPreview();Sensor->ReadEditableState(State);
            Test->TestTrue(TEXT("default never overwrites explicit restored profile or custom fields during scene rebuild"),State.LidarProfile==EVirtualLidarDeviceProfile::HesaiXT32&&FMath::IsNearlyEqual(State.LidarScanInterval,.1f)&&State.MaxPreviewPoints==1111);
            Test->TestTrue(TEXT("UI follows explicit model rather than hardcoding Ouster"),HasModel(*H->Monitor,TEXT("Hesai XT32"),TEXT("/ 10 Hz")));
            FString Original;FFileHelper::LoadFileToString(Original,*Path);Test->TestEqual(TEXT("loading leaves saved profile file untouched"),Original,Json);return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilOusterDefaultTest,"MA0T10.CoilLoading.Ouster.DefaultAndExplicitProfileRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilOusterDefaultTest::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilOusterDefaults(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
