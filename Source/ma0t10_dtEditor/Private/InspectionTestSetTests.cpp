#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/AgentTestSetWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "RHIGlobals.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "UnrealClient.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "HAL/FileManager.h"
#include "GenericPlatform/GenericWindow.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "ma0t10_dt/MA0T10/UI/SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
namespace
{
class FInspectionTestSetRun final:public IAutomationLatentCommand
{
public:
    FInspectionTestSetRun(FAutomationTestBase* T,bool Hundred):Test(T),Large(Hundred),ManualBusy(FParse::Param(FCommandLine::Get(),TEXT("InspectionManualBusy"))){if(ManualBusy)Mode=3;}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Started>1500){Test->AddError(TEXT("INSP-FIX-2 busy-work timeout"));return Finish();}
        if(FPaths::FileExists(Report()/TEXT("stop.flag"))){Test->AddError(TEXT("Owner cancelled inspection fixture"));return Finish();}
        if(!Service)
        {
            UWorld* Candidate=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){Candidate=C.World();break;}
            if(World!=Candidate){World=Candidate;Observed=Now;return false;}
            if(!World)return false;
        }
        if(!Service)
        {
            if(Now-Observed<3||!FinalAcceptanceViewportReady(World))return false;
            for(TActorIterator<ASlabActor> It(World);It;++It){It->StopSimulation();if(!Slab)Slab=*It;}
            for(TActorIterator<AVirtualSensorActorBase> It(World);It;++It)It->StopSensor();
            for(TActorIterator<AVirtualLidarSensorActor> It(World);It;++It)if(It->GetSensorId()==TEXT("SLAB-LIDAR-001")){Lidar=*It;break;}
            if(!Slab||!Lidar||!Lidar->OutputComponent||!Lidar->OutputComponent->StreamPublisherComponent){Test->AddError(TEXT("inspection map Slab/LiDAR/publisher unavailable"));return Finish();}
            if(ManualBusy)for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);
            Service=World->GetSubsystem<UAgentTestSetSubsystem>();Periodic=World->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>();Publisher=Lidar->OutputComponent->StreamPublisherComponent;
            FString Json,Fixture;TArray<TSharedPtr<FJsonValue>> Values;
            if(Large&&FParse::Value(FCommandLine::Get(),TEXT("TestSetInspectionFixture="),Fixture))
            {
                if(!FFileHelper::LoadFileToString(Json,*Fixture)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Values)||Values.Num()!=100){Test->AddError(TEXT("inspection fixture must contain 100 JSON strings"));return Finish();}
                for(const auto& V:Values)Inputs.Add(V->AsString());
            }
            else
            {
                Fixture=FPaths::ProjectDir()/TEXT("Source/ma0t10_dtEditor/Private/Fixtures/inspection-testset-20.json");TSharedPtr<FJsonObject> Template;
                if(!FFileHelper::LoadFileToString(Json,*Fixture)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Template)||!Template){Test->AddError(TEXT("portable 20-row inspection fixture unavailable"));return Finish();}
                for(int32 I=0;I<(Large?100:1);++I){Template->GetObjectField(TEXT("_meta"))->SetStringField(TEXT("UUID"),FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));FString Input;FJsonSerializer::Serialize(Template.ToSharedRef(),TJsonWriterFactory<>::Create(&Input));Inputs.Add(Input);}
            }
            InputSource=Fixture;
            OriginalOutputs=Slab->GetSensorOutputs();Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
            BeginMode();return false;
        }
        if(Phase==0)
        {
            if(Injected<(Large?100:1)&&Slab->SubmitScenarioJson(Inputs[Injected]))++Injected;
            if(Service->Items().Num()<(Large?100:1))return false;
            if(Now-At<2)return false;
            Test->TestEqual(TEXT("busy inputs all stay waiting"),Service->Completed(),0);
            for(const auto& I:Service->Items()){Test->TestTrue(TEXT("busy input not failed or attempted"),I.State==EAgentTestSetItemState::Waiting&&I.Attempts==0);}
            const FString Expected=Mode==0?TEXT("측정"):Mode==1?TEXT("PCD"):TEXT("주기 저장");
            Test->TestTrue(TEXT("visible blocker identifies sensor and work"),Service->Message().Contains(Lidar->GetSensorId())&&Service->Message().Contains(Expected));
            Save(TEXT("waiting"));
            FScreenshotRequest::RequestScreenshot(Report()/FString::Printf(TEXT("busy-%d-%d.png"),Large,Mode),true,false);
            if(ManualBusy){Phase=1;return false;}
            if(Mode==3){Test->TestTrue(TEXT("one-click stop and start"),Service->StopBusyWorkAndStart(Error));}
            else if(Mode==0)Lidar->StopSensor();else if(Mode==1)Publisher->StopStream(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());else Periodic->StopPeriodicSave(PeriodicId);
            Phase=1;return false;
        }
        if(Phase==1)
        {
            if(ManualBusy&&!Publisher->IsLocalCaptureSuspended(Lidar->GetSensorId()))
            {
                PreStopEnabled=Publisher->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());
                PreStopIntent=Publisher->IsStreamActiveOrAwaitingResume(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());
            }
            if(Service->Items().ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Failed;})){Test->AddError(TEXT("inspection generation failed: ")+Service->Message());return Finish();}
            if(!Service->IsComplete())return false;
            At=Now;Phase=2;return false;
        }
        if(Phase==2)
        {
            if(Now-At<.5)return false;
            if(Mode==3)
            {
                Test->TestTrue(TEXT("only owned measurement restored"),Lidar->IsSensorRunning());
                Test->TestEqual(TEXT("PCD user active or awaiting-resume intent preserved"),Publisher->IsStreamActiveOrAwaitingResume(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId()),ManualBusy?PreStopIntent:true);
                const auto P=Periodic->GetPeriodicStatus(PeriodicId);Test->TestTrue(TEXT("same periodic session restored"),P.bActive&&P.Folder==PeriodicFolder&&P.IntervalSeconds==.2f);
                Test->TestFalse(TEXT("temporary publisher hold released"),Publisher->IsLocalCaptureSuspended(Lidar->GetSensorId()));
            }
            Save(TEXT("completed"));Lidar->StopSensor();Publisher->StopStream(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());if(!PeriodicId.IsEmpty())Periodic->StopPeriodicSave(PeriodicId);
            auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();Workspace->OpenTestSet();
            auto* Widget=Workspace->GetTestSetWidget();Test->TestTrue(TEXT("test set uses common floating panel"),Widget&&Widget->IsWorkspaceOwned()&&!Widget->IsWorkspaceDocked()&&Widget->bPanelDraggable);
            if(Widget)Test->TestTrue(TEXT("floating header stays above owned summary card"),CastChecked<UCanvasPanelSlot>(Widget->Slot)->GetZOrder()>CastChecked<UCanvasPanelSlot>(Workspace->GetOwnedNavigation()->Slot)->GetZOrder());
            if(Widget){const auto Before=Widget->GetCurrentPanelPosition();Widget->SetWorkspacePosition(Before+FVector2D(30,15));Workspace->SavePanel(ESensorToolPanelRole::TestSet);const auto Moved=Widget->GetCurrentPanelPosition();Widget->SetWorkspacePosition(Before);Workspace->RestorePanel(ESensorToolPanelRole::TestSet);Test->TestTrue(TEXT("Workspace v1 position restoration"),Widget->GetCurrentPanelPosition().Equals(Moved,1));}
            if(Widget)
            {
                Widget->SetWorkspacePosition(FVector2D(100000,100000));
                const double Footer=FSensorOwnedPresentationStyle::StatusHeight*Workspace->GetOwnedPanelFontScale()*Workspace->GetOwnedPixelScale();
                Test->TestTrue(TEXT("floating test set remains above status strip"),Widget->GetCurrentPanelPosition().Y+Widget->GetEffectivePanelSize().Y<=Widget->GetParent()->GetCachedGeometry().GetLocalSize().Y-Footer+1);
                Workspace->RestorePanel(ESensorToolPanelRole::TestSet);
                Widget->CloseWidget();Test->TestTrue(TEXT("DTCore close can reopen the same owned test set instance"),Workspace->OpenTestSet()&&Workspace->GetTestSetWidget()==Widget&&Workspace->GetOwnedPanel(ESensorToolPanelRole::TestSet)==Widget&&Widget->GetToolWorkspace()==Workspace);
                Workspace->SetPanelOpen(ESensorToolPanelRole::TestSet,false);Test->TestTrue(TEXT("header hide and reopen retain instance and generation"),Workspace->OpenTestSet()&&Workspace->GetTestSetWidget()==Widget&&Service->IsComplete());
            }
            SourceFrame=Lidar->ScanComponent->GetRuntimeStatus().FrameId;Submitted=0;for(const auto& Stream:Publisher->GetStreamStatuses())Submitted+=Stream.SubmittedFrameCount;
            Preview=0;SeenFrame=INDEX_NONE;Test->TestTrue(TEXT("saved PCD select"),Service->SelectPreview(0,Error));
            // Immediately replace an outstanding file read; it must never populate the new selection.
            if(Large){Service->SelectPreview(1,Error);Service->SelectPreview(0,Error);}
            FileHash=HashFiles(Service->Items()[0]);Phase=3;return false;
        }
        if(Phase==3)
        {
            const auto V=Service->PreviewStatus();if(V.Frame<1)return false;
            if(auto* Bar=Cast<USensorToolToolbarWidget>(World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->GetOwnedNavigation()))Test->TestTrue(TEXT("test set activity caption is readable"),Bar->GetGlobalStatus().TestSet.ToString().StartsWith(TEXT("테스트셋 ")));
            Test->TestEqual(TEXT("correct saved scenario after replacement"),V.Scenario,Preview);
            Test->TestFalse(TEXT("PCD preview does not start Slab simulation"),Slab->IsSimulationActive());
            Test->TestTrue(TEXT("PCD point count matches file"),V.Points==PcdCount(Service->Items()[Preview],V.Frame));
            Test->TestTrue(TEXT("PCD projection texture exists"),Service->PreviewTexture()!=nullptr);
            Service->SetPreviewPaused(true);SeenFrame=V.Frame;At=Now;Phase=4;return false;
        }
        if(Phase==4)
        {
            if(Now-At<.35)return false;Test->TestEqual(TEXT("pause holds current frame"),Service->PreviewStatus().Frame,SeenFrame);
            Service->SetPreviewPaused(false);Phase=5;return false;
        }
        if(Phase==5)
        {
            const auto V=Service->PreviewStatus();if(V.Playing||V.Frame+1!=V.Frames)return false;
            Test->TestEqual(TEXT("all saved frames replayed"),V.Frames,20);Test->TestEqual(TEXT("saved PCD and label bytes immutable"),HashFiles(Service->Items()[Preview]),FileHash);
            if(!Large&&Mode==0&&!MatrixDone){OriginalX=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ACCEPTANCE_RES_X"));OriginalY=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ACCEPTANCE_RES_Y"));MatrixStep=0;SetMatrix();Phase=7;return false;}
            FScreenshotRequest::RequestScreenshot(Report()/FString::Printf(TEXT("pcd-%d-%d-%d.png"),Large,Mode,Preview),true,false);
            Service->StopPreview();Test->TestTrue(TEXT("stop removes view selection"),Service->PreviewStatus().Scenario==INDEX_NONE);
            if(Large&&++Preview<3){FileHash=HashFiles(Service->Items()[Preview]);Service->SelectPreview(Preview,Error);Phase=3;return false;}
            int64 Sent=0;for(const auto& Stream:Publisher->GetStreamStatuses())Sent+=Stream.SubmittedFrameCount;
            Test->TestEqual(TEXT("PCD viewing submits zero transport frames"),Sent,Submitted);Test->TestEqual(TEXT("PCD viewing never acquires on source sensor"),Lidar->ScanComponent->GetRuntimeStatus().FrameId,SourceFrame);
            Test->TestTrue(TEXT("clear completed list, preserve artifacts"),Service->ResetConfirmed(Error));Phase=6;return false;
        }
        if(Phase==6)
        {
            if(Service->HasItems())return false;if(++Mode<4){BeginMode();return false;}return Finish();
        }
        if(Phase==7)
        {
            const auto Wanted=FinalAcceptanceViewportSize();auto* Client=World->GetGameViewport();const auto Actual=Client->Viewport->GetSizeXY();
            if(Actual!=Wanted)
            {
                if(Now-At>8){Test->AddError(TEXT("layout fixture could not arrange exact client size"));return Finish();}
                if(Now-LastResize>.2)if(auto Window=FSlateApplication::Get().FindWidgetWindow(Client->GetGameViewportWidget().ToSharedRef()))
                {Window->Resize(FVector2D(Window->GetClientSizeInScreen())+FVector2D(Wanted.X-Actual.X,Wanted.Y-Actual.Y));LastResize=Now;}
                return false;
            }
            if(Now-At<.7)return false;
            auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Widget=W->GetTestSetWidget();
            const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();const auto Geometry=Widget->GetCachedGeometry();
            auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("client_width"),Size.X);O->SetNumberField(TEXT("client_height"),Size.Y);O->SetNumberField(TEXT("font_scale"),W->GetOwnedPanelFontScale());O->SetNumberField(TEXT("viewport_scale"),UWidgetLayoutLibrary::GetViewportScale(World));
            if(auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef()))if(Window->GetNativeWindow())O->SetNumberField(TEXT("window_dpi_scale"),Window->GetNativeWindow()->GetDPIScaleFactor());
            O->SetNumberField(TEXT("panel_width"),Geometry.GetAbsoluteSize().X);O->SetNumberField(TEXT("panel_height"),Geometry.GetAbsoluteSize().Y);
            FString J;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&J));const FString Name=FString::Printf(TEXT("after-%dx%d-%d"),Size.X,Size.Y,MatrixStep%2?150:100);
            FFileHelper::SaveStringToFile(J,*(Report()/TEXT("layout")/(Name+TEXT(".json"))));FScreenshotRequest::RequestScreenshot(Report()/TEXT("layout")/(Name+TEXT(".png")),true,false);At=Now;Phase=8;return false;
        }
        if(Phase==8)
        {
            if(Now-At<.3)return false;
            if(++MatrixStep<4){SetMatrix();Phase=7;return false;}
            FPlatformMisc::SetEnvironmentVar(TEXT("MA0T10_ACCEPTANCE_RES_X"),*OriginalX);FPlatformMisc::SetEnvironmentVar(TEXT("MA0T10_ACCEPTANCE_RES_Y"),*OriginalY);
            MatrixDone=true;Phase=5;return false;
        }
        return false;
    }
private:
    FString Report() const{return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Reports/INSP-FIX-2"));}
    int32 PcdCount(const FAgentTestSetItem& I,int32 Frame)
    {
        TArray<uint8> B;FFileHelper::LoadFileToArray(B,*(I.Directory/(TEXT("run_")+I.RunId)/FString::Printf(TEXT("scene_%05d/lidar.pcd"),Frame)));
        const int32 H=FMath::Min(B.Num(),65536);FUTF8ToTCHAR U(reinterpret_cast<const ANSICHAR*>(B.GetData()),H);TArray<FString> Lines;FString(U.Length(),U.Get()).ParseIntoArrayLines(Lines);
        for(const auto& L:Lines)if(L.StartsWith(TEXT("POINTS ")))return FCString::Atoi(*L.Mid(7));return -1;
    }
    FString HashFiles(const FAgentTestSetItem& I)
    {
        FSHA1 H;for(int32 F=0;F<I.Frames;++F)for(const auto* Name:{TEXT("lidar.pcd"),TEXT("label.json"),TEXT("lidar_labels.bin")})
        {TArray<uint8> B;FFileHelper::LoadFileToArray(B,*(I.Directory/(TEXT("run_")+I.RunId)/FString::Printf(TEXT("scene_%05d"),F)/Name));H.Update(B.GetData(),B.Num());}
        H.Final();uint8 Bytes[20];H.GetHash(Bytes);return BytesToHex(Bytes,20);
    }
    void Save(const TCHAR* Stage)
    {auto O=Service->StatusJson();O->SetBoolField(TEXT("pcd_pre_stop_enabled"),PreStopEnabled);O->SetBoolField(TEXT("pcd_pre_stop_intent"),PreStopIntent);O->SetBoolField(TEXT("pcd_current_enabled"),Publisher->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId()));O->SetBoolField(TEXT("pcd_current_intent"),Publisher->IsStreamActiveOrAwaitingResume(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId()));O->SetStringField(TEXT("input_source"),InputSource);O->SetNumberField(TEXT("mode"),Mode);O->SetBoolField(TEXT("large"),Large);FString J;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&J));FFileHelper::SaveStringToFile(J,*(Report()/FString::Printf(TEXT("%s-%d-%d.json"),Stage,Large,Mode)));}
    void SetMatrix()
    {
        FPlatformMisc::SetEnvironmentVar(TEXT("MA0T10_ACCEPTANCE_RES_X"),MatrixStep<2?TEXT("1280"):TEXT("1920"));FPlatformMisc::SetEnvironmentVar(TEXT("MA0T10_ACCEPTANCE_RES_Y"),MatrixStep<2?TEXT("720"):TEXT("1080"));
        World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->SetOwnedPanelFontScale(MatrixStep%2?1.5f:1.f);IFileManager::Get().MakeDirectory(*(Report()/TEXT("layout")),true);At=FPlatformTime::Seconds();
    }
    void BeginMode()
    {
        FAgentTestSetSettings C;C.AutoFillExamples=false;C.Target=Large?100:1;C.LidarId=Lidar->GetSensorId();Test->TestTrue(TEXT("fresh inspection configuration"),Service->ConfigureForTests(C));
        if(Mode==0||Mode==3)Lidar->StartSensor();
        if(Mode==1||Mode==3)Publisher->StartStream(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());
        PeriodicId.Reset();if(Mode>=2){FVirtualSensorCaptureSelection Selection;Selection.bPointCloud=true;Selection.bLidarPayload=false;Selection.PointCloudFormat=EVirtualSensorExportKind::PointCloudPcd;Selection.IntervalSeconds=.2f;PeriodicId=Periodic->StartPeriodicSave(Lidar,Selection);PeriodicFolder=Periodic->GetPeriodicStatus(PeriodicId).Folder;}
        At=FPlatformTime::Seconds();Injected=0;Phase=0;
    }
    bool Finish()
    {
        if(Phase==7||Phase==8){FPlatformMisc::SetEnvironmentVar(TEXT("MA0T10_ACCEPTANCE_RES_X"),*OriginalX);FPlatformMisc::SetEnvironmentVar(TEXT("MA0T10_ACCEPTANCE_RES_Y"),*OriginalY);}
        if(ManualBusy&&World)for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(true);
        if(Service){Service->StopPreview();Service->ResetConfirmed(Error);}if(Lidar)Lidar->StopSensor();if(Publisher&&Lidar)Publisher->StopStream(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());if(Periodic&&!PeriodicId.IsEmpty())Periodic->StopPeriodicSave(PeriodicId);
        if(Slab)Slab->SetSensorOutputs(OriginalOutputs);return true;
    }
    FAutomationTestBase* Test;bool Large,ManualBusy=false,MatrixDone=false,PreStopEnabled=true,PreStopIntent=true;int32 MatrixStep=0;FString OriginalX,OriginalY,InputSource;double LastResize=0;UWorld* World=nullptr;UAgentTestSetSubsystem* Service=nullptr;UVirtualSensorPeriodicFileSaveSubsystem* Periodic=nullptr;UVirtualSensorStreamPublisherComponent* Publisher=nullptr;ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;
    FVirtualSlabSensorOutputSelection OriginalOutputs;TArray<FString> Inputs;FString Error,PeriodicId,PeriodicFolder,FileHash;int64 Submitted=0,SourceFrame=0;int32 Mode=0,Phase=0,Injected=0,Preview=0,SeenFrame=INDEX_NONE;double Started=FPlatformTime::Seconds(),Observed=0,At=0;
};
bool InspectionCase(FAutomationTestBase* Test,bool Large)
{
    if(GUsingNullRHI){Test->AddInfo(TEXT("[OPT-IN SKIP] INSP-FIX-2 requires D3D12"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FInspectionTestSetRun(Test,Large));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionTestSetLifecycle,"MA0T10.TestSet.InspectionBusyLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionTestSetLifecycle::RunTest(const FString&){return InspectionCase(this,false);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionTestSet100,"MA0T10.TestSet.InspectionBusy100",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionTestSet100::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("TestSet100"))){AddInfo(TEXT("[OPT-IN SKIP] pass -TestSet100 for original inspection 100 inputs in four modes"));return true;}return InspectionCase(this,true);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionTestSetScope,"MA0T10.TestSet.ScopedStopPreservesUserSettings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionTestSetScope::RunTest(const FString&)
{
    auto* Publisher=NewObject<UVirtualSensorStreamPublisherComponent>();FVirtualSensorStreamConfig Config;Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.SensorId.Reset();Config.bEnabled=true;Publisher->ConfigureStream(Config);
    Publisher->SetLocalCaptureSuspended(TEXT("A"),true);FVirtualSensorStreamConfig Effective;
    TestFalse(TEXT("held sensor cannot enter wildcard output"),Publisher->ResolveActiveStreamForSensor(EVirtualSensorStreamKind::PointCloud,TEXT("A"),Effective));
    TestTrue(TEXT("other sensor keeps wildcard route"),Publisher->ResolveActiveStreamForSensor(EVirtualSensorStreamKind::PointCloud,TEXT("B"),Effective));
    TestTrue(TEXT("wildcard configured enable unchanged"),Publisher->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,FString()));
    TestFalse(TEXT("local held sensor does not lock placement"),Publisher->HasSensorPlacementWork(TEXT("A"),EVirtualSensorKind::Lidar));
    TestTrue(TEXT("other sensor placement remains protected"),Publisher->HasSensorPlacementWork(TEXT("B"),EVirtualSensorKind::Lidar));
    Publisher->StopStream(EVirtualSensorStreamKind::PointCloud,FString());Publisher->SetLocalCaptureSuspended(TEXT("A"),false);
    TestFalse(TEXT("releasing hold preserves user stop"),Publisher->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,FString()));
    auto* Recorder=NewObject<UVirtualSensorRecorderComponent>();Recorder->StartRecording();Recorder->SetLocalCaptureSuspended(TEXT("A"),true);
    TestFalse(TEXT("only selected recording is held"),Recorder->IsRecordingSensor(TEXT("A")));TestTrue(TEXT("other recording unchanged"),Recorder->IsRecordingSensor(TEXT("B")));
    Recorder->StopRecording();Recorder->SetLocalCaptureSuspended(TEXT("A"),false);TestFalse(TEXT("releasing recording hold preserves user stop"),Recorder->IsRecording());return true;
}
#endif
