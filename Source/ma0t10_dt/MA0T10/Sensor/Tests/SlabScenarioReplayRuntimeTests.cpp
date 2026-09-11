#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "UnrealClient.h"
#include "SlabScenarioReplayTestAdapter.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"

namespace {
class FReplayRuntimeCheck : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Start=FPlatformTime::Seconds();
	int32 Stage=-1;
	FString Scenario=TEXT("d8ddf0b1-b00b-4724-a529-90b8348726e6"),Json,LidarId;
	TSet<FString> Runs;
	TWeakObjectPtr<ASlabScenarioReplayTestAdapter> Adapter;
	TWeakObjectPtr<ASlabScenarioReplayUiHostActor> Ui;
	TWeakObjectPtr<UVirtualSensorHighThroughputTransportSubsystem> Raw;
	FDelegateHandle ReceiveHandle;
	int64 Correlated=0,InvalidContext=0;
	TArray<double> FrameTimes;
public:
	explicit FReplayRuntimeCheck(FAutomationTestBase* T):Test(T){}
	virtual ~FReplayRuntimeCheck() { if(Raw.IsValid()) Raw->OnReceived.Remove(ReceiveHandle); }
	bool Update() override
	{
		if(FPlatformTime::Seconds()-Start>155) { Test->AddError(TEXT("Scenario replay timeout")); return true; }
		UWorld* World=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::PIE) {World=C.World();break;}
		if(!World||!World->GetGameInstance()) return false;
		auto* Catalog=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
		AVirtualSensorCoordinator* Coordinator=nullptr; for(TActorIterator<AVirtualSensorCoordinator> It(World);It;++It){Coordinator=*It;break;}
		if(!Coordinator) return false;
		if(Stage<0)
		{
			if(FPlatformTime::Seconds()-Start<2) return false;
			FVirtualSensorTransportProfile Profile; Profile.BrokerUrl=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_URL")); Profile.UserName=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_USER"));
			Coordinator->SharedTransportComponent->ConfigureTransportProfile(Profile);
			Coordinator->SharedTransportComponent->SetSessionCredentials(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_PASSWORD")),FString());
			Coordinator->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
			for(TActorIterator<AVirtualLidarSensorActor> It(World);It;++It){
				// Test preset setup uses the existing public profile resolver.
				It->ScanComponent->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE); It->ScanComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec); LidarId=It->GetSensorId(); break;
			}
			for(TActorIterator<AVirtualCameraSensorActor> It(World);It;++It) It->StopSensor();
			Coordinator->StreamPublisherComponent->StopAllStreams(FString());
			for(TActorIterator<AVirtualSensorExternalSourceHostActor> It(World);It;++It) { It->ConfigureReceiverTopics(Profile.LidarTopic,Profile.CameraTopic,Profile.ExportTopic);It->StartTopicReceivers(); }
			Adapter=World->SpawnActor<ASlabScenarioReplayTestAdapter>(); Ui=World->SpawnActor<ASlabScenarioReplayUiHostActor>();
			Test->TestNotNull(TEXT("compiled replay WBP loaded"),Ui->ReplayWidget.Get());
			Json=ASlabScenarioReplayTestAdapter::MakeBulkJson(Scenario); Test->TestTrue(TEXT("async registration accepted"),Catalog->RegisterScenarioJson(Json));
			Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
			ReceiveHandle=Raw->OnReceived.AddLambda([this](const TSharedPtr<FVirtualSensorTopicReceivedDataBase>& D){if(D.IsValid()&&!D->bFiltered&&D->Kind==EVirtualSensorTopicReceiveKind::PointCloud){++Correlated;if(!D->bValid||D->ScenarioUUID!=Scenario||!Runs.Contains(D->RunId)||D->SlabFrameNo<0||D->SlabFrameNo>599)++InvalidContext;}});
			Stage=0;return false;
		}
		if(Stage==0)
		{
			if(Catalog->GetPendingRegistrationCount()>0||FPlatformTime::Seconds()-Start<10) return false;
			Test->TestEqual(TEXT("async result stored"),Catalog->GetScenarios().Num(),1);
			Catalog->SetLivePlaybackActive(true); Test->TestFalse(TEXT("existing live playback blocks request"),Catalog->RequestScenarioReplay(Scenario,false,{LidarId})); Catalog->SetLivePlaybackActive(false);
			Test->TestTrue(TEXT("UI selects stored scenario"),Ui->ReplayWidget->SelectScenario(Scenario));
			const FVector2D Expanded=Ui->ReplayWidget->GetEffectivePanelSize();
			Ui->ReplayWidget->TogglePanelCollapsed(); Test->TestTrue(TEXT("replay panel collapses real slot"),Ui->ReplayWidget->GetEffectivePanelSize().Y<=48.0f);
			Ui->ReplayWidget->TogglePanelCollapsed(); Test->TestEqual(TEXT("expanded replay size restored"),Ui->ReplayWidget->GetEffectivePanelSize(),Expanded);
			Ui->ReplayWidget->TargetSensorIds={LidarId}; Test->TestFalse(TEXT("PCD default off"),Ui->ReplayWidget->bSendPcd);
			Test->TestTrue(TEXT("UI starts observation"),Ui->ReplayWidget->ReplaySelected());
			Runs.Add(Catalog->GetReplayStatus().RunUUID); Stage=1; return false;
		}
		FrameTimes.Add(World->GetDeltaSeconds()*1000.0);
		if(Catalog->GetReplayStatus().State==ESlabScenarioReplayState::Failed){Test->AddError(Catalog->GetReplayStatus().Message);return true;}
		if(Catalog->IsReplayBusy()) return false;
		if(Adapter->CompletedRuns<Stage) return false;
		Test->TestFalse(TEXT("fixture frame notifications succeed"),Adapter->bFailed);
		Test->TestEqual(TEXT("colleague receives exact original JSON"),Adapter->LastReceivedJson,Json);
		Test->TestEqual(TEXT("source UUID stays fixed"),Adapter->LastScenarioUUID,Scenario);
		if(Stage==1)
		{
			Test->TestEqual(TEXT("observation produces zero consumer messages"),Correlated,static_cast<int64>(0));
			for(const auto& T:Raw->GetStreamTelemetry())Test->TestEqual(TEXT("observation produces zero socket submissions"),T.SubmittedCount,static_cast<int64>(0));
		}
		if(Stage<3)
		{
			const FString OldRun=Catalog->GetReplayStatus().RunUUID;
			Ui->ReplayWidget->bSendPcd=true; Test->TestTrue(TEXT("replay with PCD accepted"),Ui->ReplayWidget->ReplaySelected());
			const FString NewRun=Catalog->GetReplayStatus().RunUUID; Test->TestFalse(TEXT("fresh execution UUID"),Runs.Contains(NewRun));Runs.Add(NewRun);
			Test->TestFalse(TEXT("stale completion discarded"),Catalog->NotifyPlaybackFinished(OldRun,false));
			Test->TestFalse(TEXT("concurrent playback rejected"),Catalog->RequestScenarioReplay(Scenario,true,{LidarId}));
			++Stage;return false;
		}
		FVirtualSensorStreamTelemetry Pcd;
		for(const auto& T:Raw->GetStreamTelemetry())if(T.StreamKind==EVirtualSensorStreamKind::PointCloud)Pcd=T;
		if(Pcd.ConsumerReceivedCount!=Pcd.SubmittedCount&&FPlatformTime::Seconds()-Start<145)return false;
		Test->TestTrue(TEXT("two 30s PCD runs produce real data"),Pcd.SubmittedCount>=1100);
		Test->TestEqual(TEXT("all broker receipts"),Pcd.ReceiptCount,Pcd.SubmittedCount); Test->TestEqual(TEXT("all consumers"),Pcd.ConsumerReceivedCount,Pcd.SubmittedCount);
		Test->TestEqual(TEXT("all real messages correlate"),Correlated,Pcd.SubmittedCount); Test->TestEqual(TEXT("valid original/run metadata"),InvalidContext,static_cast<int64>(0));
		Test->TestEqual(TEXT("no gaps"),Pcd.FrameGapCount,static_cast<int64>(0)); Test->TestEqual(TEXT("no duplicates"),Pcd.DuplicateCount,static_cast<int64>(0));Test->TestEqual(TEXT("no overload"),Pcd.OverloadCount,static_cast<int64>(0));
		double Sum=0; for(double V:FrameTimes)Sum+=V; FrameTimes.Sort(); const double Fps=1000.0*FrameTimes.Num()/FMath::Max(1.0,Sum);
		auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("scenario_uuid"),Scenario); Report->SetNumberField(TEXT("replays"),3); Report->SetNumberField(TEXT("submitted"),Pcd.SubmittedCount);Report->SetNumberField(TEXT("receipt"),Pcd.ReceiptCount);Report->SetNumberField(TEXT("consumer"),Pcd.ConsumerReceivedCount);Report->SetNumberField(TEXT("invalid_context"),InvalidContext);Report->SetNumberField(TEXT("average_engine_fps"),Fps);Report->SetNumberField(TEXT("engine_frame_p95_ms"),FrameTimes[FMath::Clamp(FMath::CeilToInt(FrameTimes.Num()*0.95)-1,0,FrameTimes.Num()-1)]);
		FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));
		const FString ReportPath=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_REPLAY_REPORT"));
		Test->TestTrue(TEXT("runtime JSON evidence saved"),!ReportPath.IsEmpty()&&FFileHelper::SaveStringToFile(Text,*ReportPath));
		FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(ReportPath,TEXT("png")),true,false);
		UE_LOG(LogTemp,Display,TEXT("[ScenarioReplayRhi] observation=1 pcdRuns=2 received=%lld invalid=%lld fps=%.2f"),Correlated,InvalidContext,Fps);
		Coordinator->StopAllSensors(); return true;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabReplayRuntimeTest,"MA0T10.ScenarioReplay.Runtime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabReplayRuntimeTest::RunTest(const FString&)
{
	if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_REPLAY_RHI"))!=TEXT("1")){AddInfo(TEXT("Requires run_slab_scenario_replay_smoke.ps1; actual RHI test skipped."));return true;}
	for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("WebSocketUrl"),TEXT("MA0T10_ARTEMIS_URL")},{TEXT("WebSocketLogin"),TEXT("MA0T10_ARTEMIS_USER")},{TEXT("WebSocketPasscode"),TEXT("MA0T10_ARTEMIS_PASSWORD")}})
		GConfig->SetString(TEXT("DTCoreRuntimeOverride"),*Pair.Key,*FPlatformMisc::GetEnvironmentVariable(*Pair.Value),GGameIni);
	if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"),true))return false;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FReplayRuntimeCheck(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
