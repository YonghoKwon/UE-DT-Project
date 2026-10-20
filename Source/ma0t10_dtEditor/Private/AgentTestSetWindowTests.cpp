#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetTrigger.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/FactoryAgentScenarioTC.h"
#include "Core/DxWidgetSubsystem.h"
#include "Components/CanvasPanelSlot.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "AgentTestSetPresentationChecks.h"
#include "Uxr15ToggleTestHelpers.h"

namespace
{
TMap<FString,FString> WindowTestFiles(const FString& Root)
{
    TArray<FString> Files;IFileManager::Get().FindFilesRecursive(Files,*Root,TEXT("*"),true,false);
    TMap<FString,FString> Result;for(const auto& File:Files)
        Result.Add(File,LexToString(FMD5Hash::HashFile(*File))+IFileManager::Get().GetTimeStamp(*File).ToIso8601());
    return Result;
}
FString WindowTestTransport(UVirtualSensorHighThroughputTransportSubsystem* Raw)
{
    TArray<FString> Values;for(const auto& S:Raw->GetStreamTelemetry())
        Values.Add(FString::Printf(TEXT("%s/%d:%lld:%lld:%lld:%lld"),*S.SensorId,int32(S.StreamKind),S.EnqueuedCount,S.SubmittedCount,S.SubmittedBytes,S.ReceiptCount));
    Values.Sort();return FString::Join(Values,TEXT("|"));
}
class FTestSetWindowRun final:public IAutomationLatentCommand
{
public:
    explicit FTestSetWindowRun(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>120){Test->AddError(TEXT("test set window fixture timeout"));return Finish();}
        if(!Service)
        {
            for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
            if(!World||Now-Started<3||!FinalAcceptanceViewportReady(World))return false;
            for(TActorIterator<ASlabActor> It(World);It;++It){It->StopSimulation();if(!Slab)Slab=*It;}
            for(TActorIterator<AVirtualSensorActorBase> It(World);It;++It)It->StopSensor();
            for(TActorIterator<AVirtualLidarSensorActor> It(World);It;++It)if(It->GetSensorId()==TEXT("SLAB-LIDAR-001"))Lidar=*It;
            if(!Slab||!Lidar){Test->AddError(TEXT("window fixture missing Slab/LiDAR"));return Finish();}
            auto* Scan=Lidar->ScanComponent.Get();Scan->bEnableRangeNoise=false;
            Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
            Service=World->GetSubsystem<UAgentTestSetSubsystem>();Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
            Dx=World->GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>();Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
            Observation=Raw->OnTransportObservation.AddLambda([this](const auto&){++NetworkEvents;});
            TC=NewObject<UFactoryAgentScenarioTC>(World);TC->AddToRoot();TC->ReceiverId=Slab->ReceiverId;
            Report=FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-04/ui-followup/window-data");FParse::Value(FCommandLine::Get(),TEXT("TestSet04UiReport="),Report);IFileManager::Get().MakeDirectory(*Report,true);
        }
        if(Phase==0)
        {
            if(Service->HasItems())return false;
            FAgentTestSetSettings C;C.Target=1;C.AutoFillExamples=Mode==0;C.LidarId=Lidar->GetSensorId();
            Test->TestTrue(TEXT("configure fresh window mode"),Service->ConfigureForTests(C));
            FSlabScenarioDataPtr Data;FString Error;
            Test->TestTrue(TEXT("strict fixture template"),AgentTestSetTrigger::LoadTemplate(FPaths::ProjectDir()/TEXT("Config/AgentTestSetScenario.json"),C.Trigger,Slab->DimensionUnit,Slab->PositionUnit,Data,Error));
            if(!Data)return Finish();FullInput=Data->OriginalJson;
            Send();At=Now;Phase=1;return false;
        }
        if(Phase==1)
        {
            if(!Service->HasItems())return false;if(!Service->IsPaused())Service->SetPaused(true);
            if(Now-At<.7)return false;
            Widget=Workspace->GetTestSetWidget();Test->TestNotNull(TEXT("first receipt opens test set"),Widget);if(!Widget)return Finish();
            auto SendButton=FindSend();Test->TestNotNull(TEXT("placeholder send button exists"),SendButton.Get());
            if(SendButton)Test->TestFalse(TEXT("send disabled before completion"),SendButton->IsEnabled());
            Service->SetPaused(false);Phase=2;return false;
        }
        if(Phase==2)
        {
            if(Service->Items().ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Failed;})){Test->AddError(TEXT("window generation failed: ")+Service->Message());return Finish();}
            if(!Service->IsComplete())return false;At=Now;Phase=3;return false;
        }
        if(Phase==3)
        {
            if(Now-At<.7)return false;
            if(FParse::Param(FCommandLine::Get(),TEXT("TestSet04UiManual"))&&Mode==0)
            {
                FFileHelper::SaveStringToFile(TEXT("Ready: completed test set; click 전송, close, receive again"),*(Report/TEXT("manual-ready.txt")));
                if(!FPaths::FileExists(Report/TEXT("manual-continue.flag")))return false;
            }
            auto SendButton=FindSend();if(SendButton)
            {
                Test->TestTrue(TEXT("send enabled after complete manifest"),SendButton->IsEnabled());
                TArray<FString> Order;SensorPresentationTests::VisibleText(Widget->TakeWidget(),Order);
                Test->TestEqual(TEXT("send immediately follows details in control order"),Order.IndexOfByKey(TEXT("전송")),Order.IndexOfByKey(TEXT("상세"))+1);
                Files=WindowTestFiles(Service->Directory());Transport=WindowTestTransport(Raw);Events=NetworkEvents;Revision=Service->Revision();Running=Raw->IsHighThroughputTransportRunning();
                Uxr15ToggleTests::Click(Widget->TakeWidget(),TEXT("전송"),Test);
            }
            At=Now;Phase=4;return false;
        }
        if(Phase==4)
        {
            if(Now-At<.7)return false;
            if(FindSend())
            {
                Test->TestTrue(TEXT("placeholder notice visible at completion"),AgentTestSetVisibleText(Widget).Contains(TEXT("전송 기능 준비 중")));
                Test->TestTrue(TEXT("send changes zero dataset files including timestamps"),Files.OrderIndependentCompareEqual(WindowTestFiles(Service->Directory())));
                Test->TestEqual(TEXT("send changes zero transport counters"),WindowTestTransport(Raw),Transport);
                Test->TestEqual(TEXT("send emits zero transport observations"),NetworkEvents,Events);
                Test->TestEqual(TEXT("send does not start transport"),Raw->IsHighThroughputTransportRunning(),Running);
                Test->TestEqual(TEXT("send never mutates test set state"),Service->Revision(),Revision);
                auto Proof=MakeShared<FJsonObject>();Proof->SetStringField(TEXT("dataset_root"),Service->Directory());
                Proof->SetStringField(TEXT("notice"),TEXT("전송 기능 준비 중"));Proof->SetNumberField(TEXT("transport_events_delta"),NetworkEvents-Events);
                Proof->SetBoolField(TEXT("transport_counters_unchanged"),WindowTestTransport(Raw)==Transport);
                Proof->SetBoolField(TEXT("dataset_files_and_timestamps_unchanged"),Files.OrderIndependentCompareEqual(WindowTestFiles(Service->Directory())));
                Proof->SetNumberField(TEXT("file_count"),Files.Num());Proof->SetBoolField(TEXT("state_unchanged"),Service->Revision()==Revision);
                FString Json;FJsonSerializer::Serialize(Proof,TJsonWriterFactory<>::Create(&Json));
                FFileHelper::SaveStringToFile(Json,*(Report/FString::Printf(TEXT("mode%d-send-proof.json"),Mode)),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            }
            Root=Service->Directory();RunId=Service->Items()[0].RunId;
            Widget->SetPanelExpandedSize(FVector2D(700,570),false);Widget->SetWorkspacePosition(FVector2D(145,110));Workspace->SavePanel(ESensorToolPanelRole::TestSet);
            Position=Widget->GetCurrentPanelPosition();Size=Widget->GetPanelExpandedSize();Phase=5;return false;
        }
        if(Phase==5)
        {
            if(Cycle==1)Widget->CloseWidget();else Workspace->SetPanelOpen(ESensorToolPanelRole::TestSet,false);
            Test->TestTrue(TEXT("close hides or detaches test set"),!Widget->IsVisible()||!Widget->GetParent());
            At=Now;Phase=6;return false;
        }
        if(Phase==6)
        {
            if(Now-At<.6)return false;
            Test->TestTrue(TEXT("no receipt leaves test set closed"),!Widget->IsVisible()||!Widget->GetParent());
            Send();At=Now;Phase=7;return false;
        }
        if(Phase==7)
        {
            if(Now-At<.7||Slab->HasPendingScenarioInput())return false;
            const bool Open=Widget->IsVisible()&&Widget->GetParent()&&Workspace->IsPanelOpen(ESensorToolPanelRole::TestSet);
            Test->TestTrue(TEXT("duplicate receipt automatically reopens hidden/DTCore-closed test set"),Open);
            if(!Open)Workspace->OpenTestSet(); // Continue checking all three cycles on the pre-fix implementation.
            Test->TestTrue(TEXT("reopened panel retains registration and same instance"),Workspace->GetTestSetWidget()==Widget&&Workspace->GetOwnedPanel(ESensorToolPanelRole::TestSet)==Widget&&Widget->GetToolWorkspace()==Workspace&&Dx->GetOpenWidgets().Contains(Widget));
            Test->TestTrue(TEXT("reopened panel retains moved position and size"),Widget->GetCurrentPanelPosition().Equals(Position,1)&&Widget->GetPanelExpandedSize().Equals(Size,1));
            Test->TestTrue(TEXT("reopened panel is above toolbar"),CastChecked<UCanvasPanelSlot>(Widget->Slot)->GetZOrder()>CastChecked<UCanvasPanelSlot>(Workspace->GetOwnedNavigation()->Slot)->GetZOrder());
            Test->TestTrue(TEXT("duplicate receipt retains completed set and run"),Service->IsComplete()&&Service->Directory()==Root&&Service->Items()[0].RunId==RunId);
            if(++Cycle<3){Phase=5;return false;}
            // An already visible panel must also be raised on the next receipt.
            CastChecked<UCanvasPanelSlot>(Widget->Slot)->SetZOrder(1);Send();At=Now;Phase=8;return false;
        }
        if(Phase==8)
        {
            if(Now-At<.7||Slab->HasPendingScenarioInput())return false;
            Test->TestTrue(TEXT("receipt raises already visible panel"),CastChecked<UCanvasPanelSlot>(Widget->Slot)->GetZOrder()>CastChecked<UCanvasPanelSlot>(Workspace->GetOwnedNavigation()->Slot)->GetZOrder());
            CheckAgentTestSetPresentation(Test,World,true,Report/FString::Printf(TEXT("mode%d-presentation.json"),Mode));
            FString Error;Test->TestTrue(TEXT("reset retains existing files"),Service->ResetConfirmed(Error));
            if(++Mode==2)return Finish();Cycle=0;Phase=0;return false;
        }
        return false;
    }
private:
    TSharedPtr<SButton> FindSend()
    {
        TSharedPtr<SButton> Result;if(Widget)Uxr15ToggleTests::Walk(Widget->TakeWidget(),[&](const auto& W){if(W->GetType()!=TEXT("SButton"))return;TArray<FString> Text;SensorPresentationTests::VisibleText(W,Text);if(Text.Contains(TEXT("전송")))Result=StaticCastSharedRef<SButton>(W);});return Result;
    }
    void Send()
    {
        // Exercise the existing TC -> DataSync path, with material-only and full received inputs.
        TC->ProcessStructData(TC->ParseToStruct(Mode==0?TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[{\"mtl_no\":\"SQ23456 077\"}]}"):FullInput));
    }
    bool Finish(){if(Raw)Raw->OnTransportObservation.Remove(Observation);if(TC)TC->RemoveFromRoot();return true;}
    FAutomationTestBase* Test;UWorld* World=nullptr;ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;
    UAgentTestSetSubsystem* Service=nullptr;UVirtualSensorToolWorkspaceSubsystem* Workspace=nullptr;UAgentTestSetWidget* Widget=nullptr;
    UDxWidgetSubsystem* Dx=nullptr;UVirtualSensorHighThroughputTransportSubsystem* Raw=nullptr;UFactoryAgentScenarioTC* TC=nullptr;
    FDelegateHandle Observation;TMap<FString,FString> Files;FString Transport,Report,FullInput,Root,RunId;FVector2D Position,Size;
    int32 Phase=0,Mode=0,Cycle=0,NetworkEvents=0,Events=0;int64 Revision=0;bool Running=false;double Started=FPlatformTime::Seconds(),At=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentTestSetWindow,"MA0T10.TestSet.WindowReceiptAndSendPlaceholder",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentTestSetWindow::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FTestSetWindowRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
