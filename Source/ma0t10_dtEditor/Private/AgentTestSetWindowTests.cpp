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
#include "ImageUtils.h"
#include "AgentTestSetPresentationChecks.h"
#include "Uxr15ToggleTestHelpers.h"

namespace
{
TArray<FString> SendFeedbackText(UAgentTestSetWidget* Widget)
{TArray<FString> Text;if(Widget)SensorPresentationTests::VisibleText(Widget->TakeWidget(),Text);return Text;}
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
        if(Now-Started>(FParse::Param(FCommandLine::Get(),TEXT("TestSet05Manual"))?600:120)){Test->AddError(TEXT("test set window fixture timeout"));return Finish();}
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
            Workspace->SetOwnedPanelFontScale(FParse::Param(FCommandLine::Get(),TEXT("TestSet05Font150"))?1.5f:1.f);
            Observation=Raw->OnTransportObservation.AddLambda([this](const auto&){++NetworkEvents;});
            TC=NewObject<UFactoryAgentScenarioTC>(World);TC->AddToRoot();TC->ReceiverId=Slab->ReceiverId;
            Report=FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-05/window-data");FParse::Value(FCommandLine::Get(),TEXT("TestSet04UiReport="),Report);IFileManager::Get().MakeDirectory(*Report,true);
        }
        if(Phase==0)
        {
            if(Service->HasItems())return false;
            if(Widget&&Now-At<.4)return false;
            if(Widget){const auto Text=SendFeedbackText(Widget);Test->TestTrue(TEXT("reset restores send label"),Text.Contains(TEXT("전송")));Test->TestFalse(TEXT("reset clears toast and completion"),Text.Contains(TEXT("전송 완료 ✓")));}
            FAgentTestSetSettings C;C.Target=FParse::Param(FCommandLine::Get(),TEXT("TestSet05Capture"))&&Mode==0?100:1;C.AutoFillExamples=Mode==0;C.LidarId=Lidar->GetSensorId();
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
            auto SendButton=FindSend();Test->TestNotNull(TEXT("send feedback button exists"),SendButton.Get());
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
                Test->TestTrue(TEXT("click immediately shows sending"),SendFeedbackText(Widget).Contains(TEXT("전송 중…")));
                Test->TestFalse(TEXT("send disabled during progress"),SendButton->IsEnabled());
            }
            At=Now;Phase=4;return false;
        }
        if(Phase==4)
        {
            if(Now-At<1.4)return false;
            CaptureAt=Now;Phase=43;return false;
        }
        if(Phase==43)
        {
            if(Now-CaptureAt<.4)return false;
            const auto Text=SendFeedbackText(Widget);
            Test->TestTrue(TEXT("one-second progress reaches done label"),Text.Contains(TEXT("전송 완료 ✓")));
            const FString Toast=FString::Printf(TEXT("테스트셋 %d개 전송 완료"),Service->Completed());
            Test->TestTrue(TEXT("completion toast visible"),Text.Contains(Toast));
            Test->TestTrue(TEXT("completion has HH:MM status"),Text.ContainsByPredicate([](const FString& V){return V.StartsWith(TEXT("전송 완료 · "))&&V.Len()==13&&V[10]==TCHAR(':');}));
            CheckNoOutput(TEXT("toast"));
            if(Mode==0&&FParse::Param(FCommandLine::Get(),TEXT("TestSet05Capture")))Capture(TEXT("1-send-done-toast.png"));
            Phase=40;return false;
        }
        if(Phase==40)
        {
            if(Now-At<6.5)return false;
            const auto Text=SendFeedbackText(Widget);
            Test->TestFalse(TEXT("toast automatically disappears after five seconds"),Text.ContainsByPredicate([](const FString& V){return V.StartsWith(TEXT("테스트셋 "))&&V.EndsWith(TEXT("개 전송 완료"));}));
            Test->TestTrue(TEXT("done button persists after toast"),Text.Contains(TEXT("전송 완료 ✓")));
            Test->TestTrue(TEXT("completion status persists after toast"),Text.ContainsByPredicate([](const FString& V){return V.StartsWith(TEXT("전송 완료 · "));}));
            CheckNoOutput(TEXT("expired"));
            At=Now;Phase=42;return false;
        }
        if(Phase==42)
        {
            if(Now-At<.4)return false;
            if(Mode==0&&FParse::Param(FCommandLine::Get(),TEXT("TestSet05Capture")))Capture(TEXT("2-send-done-state.png"));
            if(Mode==0&&FParse::Param(FCommandLine::Get(),TEXT("TestSet05Manual")))
            {
                FFileHelper::SaveStringToFile(TEXT("Ready: send feedback manual review"),*(Report/TEXT("manual-ready.txt")));
                if(!FPaths::FileExists(Report/TEXT("manual-continue.flag")))return false;
            }
            Uxr15ToggleTests::Click(Widget->TakeWidget(),TEXT("전송 완료 ✓"),Test);
            Test->TestTrue(TEXT("reclick repeats progress"),SendFeedbackText(Widget).Contains(TEXT("전송 중…")));
            At=Now;Phase=41;return false;
        }
        if(Phase==41)
        {
            if(Now-At<1.4)return false;
            Uxr15ToggleTests::Click(Widget->TakeWidget(),TEXT("닫기"),Test);
            Test->TestFalse(TEXT("close dismisses toast only"),SendFeedbackText(Widget).ContainsByPredicate([](const FString& V){return V.StartsWith(TEXT("테스트셋 "))&&V.EndsWith(TEXT("개 전송 완료"));}));
            Test->TestTrue(TEXT("dismiss retains completed status"),SendFeedbackText(Widget).Contains(TEXT("전송 완료 ✓")));
            CheckNoOutput(TEXT("dismissed"));
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
            ++Mode;Cycle=0;At=Now;Phase=9;return false;
        }
        if(Phase==9)
        {
            if(Service->HasItems()||Now-At<.5)return false;
            const auto Text=SendFeedbackText(Widget);
            Test->TestTrue(TEXT("reset clears completed send label"),Text.Contains(TEXT("전송"))&&!Text.Contains(TEXT("전송 완료 ✓")));
            Test->TestFalse(TEXT("reset disables send"),FindSend()->IsEnabled());
            if(Mode==2)return Finish();Phase=0;return false;
        }
        return false;
    }
private:
    void Capture(const TCHAR* Name)
    {
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-05/captures");IFileManager::Get().MakeDirectory(*Dir,true);
        TArray<FColor> Pixels;
        FIntVector ImageSize;
        Test->TestTrue(TEXT("capture actual client viewport"),FSlateApplication::Get().TakeScreenshot(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,ImageSize));
        Test->TestEqual(TEXT("capture width"),ImageSize.X,FinalAcceptanceViewportSize().X);Test->TestEqual(TEXT("capture height"),ImageSize.Y,FinalAcceptanceViewportSize().Y);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(ImageSize.X,ImageSize.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Dir/Name));
    }
    void CheckNoOutput(const TCHAR* Stage)
    {
        Test->TestTrue(TEXT("send changes zero dataset files including timestamps"),Files.OrderIndependentCompareEqual(WindowTestFiles(Service->Directory())));
        Test->TestEqual(TEXT("send changes zero transport counters"),WindowTestTransport(Raw),Transport);
        Test->TestEqual(TEXT("send emits zero transport observations"),NetworkEvents,Events);
        Test->TestEqual(TEXT("send does not start transport"),Raw->IsHighThroughputTransportRunning(),Running);
        Test->TestEqual(TEXT("send never mutates test set state"),Service->Revision(),Revision);
        Test->TestFalse(TEXT("manifest retains unsupported actual sending"),Service->StatusJson()->GetBoolField(TEXT("sending_supported")));
        Test->TestEqual(TEXT("manifest documents demonstration scope"),Service->StatusJson()->GetStringField(TEXT("send_feedback_scope")),FString(TEXT("시연용 표시, 실제 전송 미구현")));
        auto Proof=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Text;
        for(const auto& V:SendFeedbackText(Widget))Text.Add(MakeShared<FJsonValueString>(V));Proof->SetArrayField(TEXT("visible_text"),Text);
        Proof->SetNumberField(TEXT("transport_events_delta"),NetworkEvents-Events);
        Proof->SetBoolField(TEXT("transport_counters_unchanged"),WindowTestTransport(Raw)==Transport);
        Proof->SetBoolField(TEXT("dataset_files_and_timestamps_unchanged"),Files.OrderIndependentCompareEqual(WindowTestFiles(Service->Directory())));
        Proof->SetNumberField(TEXT("file_count"),Files.Num());Proof->SetBoolField(TEXT("state_unchanged"),Service->Revision()==Revision);
        const auto ViewSize=World->GetGameViewport()->Viewport->GetSizeXY();Proof->SetNumberField(TEXT("width"),ViewSize.X);Proof->SetNumberField(TEXT("height"),ViewSize.Y);
        Proof->SetNumberField(TEXT("font_scale"),Workspace->GetOwnedPanelFontScale());
        const auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());Proof->SetNumberField(TEXT("dpi"),Window?Window->GetDPIScaleFactor():0);
        if(FString(Stage)!=TEXT("dismissed"))
        {
            const auto Panel=Widget->GetCachedGeometry();const auto Min=Panel.GetAbsolutePosition(),Max=Min+Panel.GetAbsoluteSize();
            int32 Checked=0;
            Uxr15ToggleTests::Walk(Widget->TakeWidget(),[&](const auto& W)
            {
                if(W->GetType()!=TEXT("STextBlock"))return;const auto Text=StaticCastSharedRef<STextBlock>(W);
                if(!Text->GetVisibility().IsVisible())return;
                const auto V=Text->GetText().ToString();
                if(V!=TEXT("전송 완료 ✓")&&!V.StartsWith(TEXT("전송 완료 · "))&&!(FString(Stage)==TEXT("toast")&&V.StartsWith(TEXT("테스트셋 "))&&V.EndsWith(TEXT("개 전송 완료"))))return;
                ++Checked;const auto G=W->GetCachedGeometry();const auto Pos=G.GetAbsolutePosition(),End=Pos+G.GetAbsoluteSize();const auto Local=G.GetLocalSize(),Desired=W->GetDesiredSize();
                Test->TestTrue(TEXT("send feedback fits allocated text bounds"),Local.X+1>=Desired.X&&Local.Y+1>=Desired.Y);
                Test->TestTrue(TEXT("send feedback remains inside owned panel"),Pos.X>=Min.X-1&&Pos.Y>=Min.Y-1&&End.X<=Max.X+1&&End.Y<=Max.Y+1);
            });
            Test->TestEqual(TEXT("all completion text checked for clipping"),Checked,FString(Stage)==TEXT("toast")?3:2);
        }
        FString Json;FJsonSerializer::Serialize(Proof,TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*(Report/FString::Printf(TEXT("mode%d-send-%s.json"),Mode,Stage)),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
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
    int32 Phase=0,Mode=0,Cycle=0,NetworkEvents=0,Events=0;int64 Revision=0;bool Running=false;double Started=FPlatformTime::Seconds(),At=0,CaptureAt=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentTestSetWindow,"MA0T10.TestSet.WindowReceiptAndSendFeedback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentTestSetWindow::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FTestSetWindowRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
