#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorPlacementInteraction.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "UxrScenarioUiFixture.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/SViewport.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineGlobals.h"
#include "Serialization/JsonSerializer.h"

// Real Slate down/up with a model refresh between them. Native manual input is separate.
class FInspectionBehaviorCommand final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;bool TruthCase;
    double Started=FPlatformTime::Seconds(),ReadyAt=0,Next=0;
    UWorld* World=nullptr;int32 Stage=0,SelectionCase=0;
    TWeakObjectPtr<AVirtualCameraSensorActor> Camera;
    TWeakObjectPtr<AVirtualSensorActorBase> Extra,Target;
    FString SuccessId,Directory;
    FVector2D Press;
    TSharedPtr<ITableRow> PressedRow;
    bool Manual=false;
    FString ManualSelection;int32 ManualChanges=0;
public:
    FInspectionBehaviorCommand(FAutomationTestBase* In,bool InTruth):Test(In),TruthCase(InTruth),Manual(FParse::Param(FCommandLine::Get(),TEXT("InspectionBehaviorManual"))){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>(Manual?1800:120)){Test->AddError(TEXT("INSP-FIX-3 timeout"));return true;}
        UWorld* Candidate=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){Candidate=Context.World();break;}
        if(Candidate!=World){World=Candidate;ReadyAt=Now;return false;}
        if(!World||Now-ReadyAt<3||!FinalAcceptanceViewportReady(World)||Now<Next)return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W?W->GetCoordinator():nullptr;if(!C)return false;
        auto* P=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement));
        auto* M=Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor));
        auto* D=Cast<UVirtualSensorCaptureExportPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Data));
        auto* Files=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>();auto* Layout=World->GetSubsystem<UVirtualSensorLayoutSubsystem>();
        auto& App=FSlateApplication::Get();
        if(!P||!M||!D){Test->AddError(TEXT("Missing owned inspection panels"));return true;}
        if(Stage==0)
        {
            W->ResetOwnedWorkspaceLayout();W->SetSidebarMode(true);W->SetOwnedPanelFontScale(1);
            for(auto* A:C->GetSensorActors())if(A){A->StopSensor();if(auto* Cam=Cast<AVirtualCameraSensorActor>(A))Camera=Cam;}
            if(!Camera.IsValid()){Test->AddError(TEXT("Inspection fixture needs Camera"));return true;}
            P->SelectEquipment(Camera.Get());W->SetPanelOpen(ESensorToolPanelRole::Placement,true);W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);
            if(TruthCase||Manual)
            {
                FVirtualSensorEditableState State;Camera->ReadEditableState(State);State.bCameraGroundTruth=true;State.CameraResolution=FIntPoint(320,180);State.CameraCaptureInterval=.1f;State.CameraCaptureMode=EVirtualCameraCaptureMode::PreviewOnly;
                FString Error;Test->TestTrue(TEXT("memory Camera truth setup"),Camera->ApplyEditableState(State,Error));Camera->StartSensor();
            }
            if(Manual){Stage=90;Next=Now+1;return false;}
            if(TruthCase){M->bOwnedAdvancedMenuOpen=true;M->CameraTruthDisplayMode=0;M->RequestCameraTruthSave();SuccessId=M->CameraTruthSaveRequest;Test->TestFalse(TEXT("F7 save has request identity"),SuccessId.IsEmpty());Test->TestEqual(TEXT("F7 immediately reports saving"),Files->GetRequestStatus(SuccessId).State,EVirtualSensorFileSaveState::Saving);Stage=1;return false;}
            W->GetPlacementInteraction()->GetGizmo(P);Stage=10;Next=Now+.5;return false;
        }
        if(Stage==90)
        {
            // Opt-in holds this same PIE for Computer Use. It never clicks on behalf of the operator.
            const FString Root=FPaths::ProjectSavedDir()/TEXT("Reports/INSP-FIX-3/manual");
            IFileManager::Get().MakeDirectory(*Root,true);
            if(!IFileManager::Get().FileExists(*(Root/TEXT("ready.txt"))))FFileHelper::SaveStringToFile(TEXT("client fixture ready; default validation-map helper retained"),*(Root/TEXT("ready.txt")));
            auto* Selected=C->GetSelectedSensorActor();const FString Id=Selected?Selected->GetSensorId():TEXT("none");
            if(Id!=ManualSelection)
            {
                // Wait for the ordinary <=5 Hz display refresh; observe, never drive selection.
                if(Selected&&!P->CachedSelectedText.ToString().Contains(Id))return false;
                ManualSelection=Id;++ManualChanges;const auto Rows=P->SensorList->GetSelectedItems();
                auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("selected"),Id);J->SetStringField(TEXT("detail"),P->CachedSelectedText.ToString());
                J->SetStringField(TEXT("row"),Rows.Num()==1&&Rows[0]->Actor.IsValid()?Rows[0]->Actor->GetSensorId():TEXT("none"));
                auto* Preview=M->GetOwnedMonitoredActor();J->SetStringField(TEXT("preview"),Preview?Preview->GetSensorId():TEXT("none"));
                auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));auto* Detail=Settings?Cast<AVirtualSensorActorBase>(Settings->GetSelectedSensorActor()):nullptr;
                J->SetStringField(TEXT("settings"),Detail?Detail->GetSensorId():TEXT("none"));J->SetNumberField(TEXT("font"),W->GetOwnedPanelFontScale());J->SetNumberField(TEXT("dpi"),P->GetCachedGeometry().Scale);
                const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();J->SetStringField(TEXT("client"),Size.ToString());J->SetNumberField(TEXT("engine_frame"),GFrameCounter);
                FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Root/FString::Printf(TEXT("selection-%02d.json"),ManualChanges)));
                FScreenshotRequest::RequestScreenshot(Root/FString::Printf(TEXT("selection-%02d.png"),ManualChanges),true,false);
            }
            if(IFileManager::Get().FileExists(*(Root/TEXT("finish.flag"))))return true;
            return false;
        }
        if(TruthCase)
        {
            M->bOwnedAdvancedMenuOpen=true;M->RefreshCameraTruth();
            if(Stage==1)
            {
                const auto Status=Files->GetRequestStatus(SuccessId);if(!Status.IsTerminal())return false;
                Test->TestEqual(TEXT("F7 actual worker completes"),Status.State,EVirtualSensorFileSaveState::Succeeded);
                if(Status.State!=EVirtualSensorFileSaveState::Succeeded){Test->AddError(Status.Message);return true;}
                Test->TestEqual(TEXT("F7 four unchanged output files"),Status.FileResults.Num(),4);
                for(const auto& Result:Status.FileResults){Test->TestTrue(TEXT("F7 reported path exists"),IFileManager::Get().FileExists(*Result.AbsolutePath));Test->TestEqual(TEXT("F7 worker byte count matches disk"),Result.FileSizeBytes,IFileManager::Get().FileSize(*Result.AbsolutePath));Test->TestTrue(TEXT("F7 existing recent files receives result"),D->GetRecentResults().ContainsByPredicate([&Result](const auto& R){return R.AbsolutePath==Result.AbsolutePath;}));}
                Directory=FPaths::GetPath(Status.FileResults[0].AbsolutePath);
                Test->TestEqual(TEXT("F7 folder button targets completed bundle"),M->CameraTruthSavedDirectory,Directory);
                Test->TestTrue(TEXT("F7 completion visible with path"),M->CameraTruthStatus.ToString().Contains(Directory)&&M->CameraTruthStatus.ToString().Contains(TEXT("저장 완료")));
                Camera->CaptureComponent->GetGroundTruth()->Status=TEXT("정답 미리보기 갱신");M->RefreshCameraTruth();
                Test->TestTrue(TEXT("F7 preview cannot overwrite save completion"),M->CameraTruthStatus.ToString().Contains(Directory));
                Camera->StopSensor();M->RequestCameraTruthSave();
                Test->TestEqual(TEXT("F7 stopped request fails explicitly"),Files->GetRequestStatus(M->CameraTruthSaveRequest).State,EVirtualSensorFileSaveState::Failed);
                Test->TestTrue(TEXT("F7 failure reason stays in initiating screen"),M->CameraTruthStatus.ToString().Contains(TEXT("측정을 시작"))&&M->CameraTruthSavedDirectory.IsEmpty());
                Camera->StartSensor();M->RequestCameraTruthSave();const FString Cancelled=M->CameraTruthSaveRequest;Camera->StopSensor();
                Test->TestTrue(TEXT("F7 stop completes queued request without success path"),Files->GetRequestStatus(Cancelled).IsTerminal()&&Files->GetRequestStatus(Cancelled).FileResults.IsEmpty());
                Camera->CaptureComponent->ProjectionType=ECameraProjectionMode::Orthographic;Camera->StartSensor();M->RequestCameraTruthSave();Stage=2;return false;
            }
            const auto Failed=Files->GetRequestStatus(M->CameraTruthSaveRequest);if(!Failed.IsTerminal())return false;
            Test->TestEqual(TEXT("F7 asynchronous capture failure is terminal"),Failed.State,EVirtualSensorFileSaveState::Failed);
            Test->TestTrue(TEXT("F7 asynchronous failure reason and no stale folder"),M->CameraTruthStatus.ToString().Contains(TEXT("기본 원근"))&&M->CameraTruthSavedDirectory.IsEmpty());
            Camera->CaptureComponent->ProjectionType=ECameraProjectionMode::Perspective;Camera->StopSensor();
            Test->AddInfo(TEXT("F7 retained actual bundle: ")+Directory);return true;
        }
        if(Stage==10)
        {
            W->SetPanelOpen(ESensorToolPanelRole::Placement,true);P->RefreshInventory();
            if(SelectionCase==1){FString Reason;Extra=Layout->CreateSensorAtView(EVirtualSensorKind::Lidar,Reason);Test->TestTrue(TEXT("F10 add fixture"),Extra.IsValid());P->RefreshInventory();}
            if(SelectionCase==3)for(TActorIterator<ASlabActor> It(World);It;++It){Test->TestTrue(TEXT("F10 ordinary scenario playing"),StartUxrScenarioUiFixture(*It));break;}
            if(SelectionCase==4){W->SetPanelOpen(ESensorToolPanelRole::Placement,false);W->SetPanelOpen(ESensorToolPanelRole::Settings,true);W->SetPanelOpen(ESensorToolPanelRole::Placement,true);}
            if(SelectionCase==5)W->SetOwnedPanelFontScale(1.5f);
            Stage=11;Next=Now+(SelectionCase>=6?0:.4);return false;
        }
        if(Stage==11)
        {
            P->RefreshInventory();if(P->SensorOptions.Num()<2){Test->AddError(TEXT("F10 requires two inventory rows"));return true;}
            auto Option=P->SensorOptions[SelectionCase%2];Target=Option->Actor;
            // Seed a different model so the down/refresh/up race cannot be masked by an already selected row.
            P->SelectEquipment(P->SensorOptions[(SelectionCase+1)%2]->Actor.Get());P->RefreshInventory();
            PressedRow=P->SensorList->WidgetFromItem(Option);if(!PressedRow.IsValid()){P->SensorList->RequestScrollIntoView(Option);Next=Now+.2;return false;}
            const auto Row=PressedRow->AsWidget();const auto G=Row->GetCachedGeometry();Press=G.LocalToAbsolute(G.GetLocalSize()*FVector2D(.4,.5));
            const auto Hit=App.LocateWindowUnderMouse(Press,App.GetInteractiveTopLevelWindows());bool HitsRow=false;FString Types;
            for(int32 I=0;I<Hit.Widgets.Num();++I){Types+=Hit.Widgets[I].Widget->GetTypeAsString()+TEXT("/");HitsRow|=Hit.Widgets[I].Widget==Row;}
            Test->TestTrue(FString::Printf(TEXT("F10 case %d row not covered by map helper: %s"),SelectionCase,*Types),HitsRow);if(!HitsRow)return true;
            if(SelectionCase==5){const auto View=World->GetGameViewport()->GetGameViewportWidget().ToSharedRef();FWidgetPath ViewPath;App.GeneratePathToWidgetUnchecked(View,ViewPath);App.ProcessReply(ViewPath,FReply::Handled().UseHighPrecisionMouseMovement(View),nullptr,nullptr);}
            App.GetPlatformApplication()->Cursor->SetPosition(Press.X,Press.Y);App.OnMouseMove();
            App.OnMouseDown(App.FindWidgetWindow(Row)->GetNativeWindow(),EMouseButtons::Left);
            Test->TestTrue(TEXT("F10 down commits the requested actor before refresh"),C->GetSelectedSensorActor()==Target.Get());
            // Force the old race, including inventory mutation while the mouse is held.
            if(SelectionCase==2&&Extra.IsValid()){FString Reason;Test->TestTrue(TEXT("F10 deletion fixture"),Layout->DeleteSensor(Extra.Get(),Reason));Extra.Reset();}
            P->RefreshInventory();Stage=12;Next=SelectionCase>=6?Now:Now+.3;return false;
        }
        App.OnMouseUp(EMouseButtons::Left);PressedRow.Reset();P->RefreshInventory();W->SynchronizeOwnedSelection();
        const auto SelectedRows=P->SensorList->GetSelectedItems();
        Test->TestTrue(TEXT("F10 highlight and coordinator agree after down/refresh/up"),C->GetSelectedSensorActor()==Target.Get()&&SelectedRows.Num()==1&&SelectedRows[0]->Actor==Target);
        Test->TestTrue(TEXT("F10 selected detail within 5 Hz"),P->CachedSelectedText.ToString().Contains(Target->GetSensorId()));
        auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));
        Test->TestTrue(TEXT("F10 sensor settings target agrees"),Settings&&Settings->GetSelectedSensorActor()==Target.Get());
        Test->TestTrue(TEXT("F10 3D gizmo target agrees"),W->GetPlacementInteraction()->GetExistingGizmo()->GetBoundTargetActor()==Target.Get());
        Test->TestTrue(TEXT("F10 preview target agrees"),Target->GetSensorKind()==EVirtualSensorKind::Camera?M->CameraComp==C->GetSelectedCamera()&&!M->bShowingLidar:M->LidarComp==C->GetSelectedLidar()&&M->bShowingLidar);
        if(++SelectionCase<26){Stage=10;Next=SelectionCase>=6?Now:Now+.1;return false;}
        App.ReleaseAllPointerCapture();Test->AddInfo(TEXT("F10 six conditions plus twenty consecutive selections; actual Slate, default helper retained, font 100/150%."));return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionTruthFeedbackTest,"MA0T10.Inspection.CameraTruthFeedback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionTruthFeedbackTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP F7 actual save requires RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FInspectionBehaviorCommand(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionPlacementSelectionTest,"MA0T10.Inspection.PlacementSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionPlacementSelectionTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP F10 Slate input requires RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FInspectionBehaviorCommand(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
