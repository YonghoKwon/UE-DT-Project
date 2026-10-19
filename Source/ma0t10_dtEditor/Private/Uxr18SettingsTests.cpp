#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Uxr15ToggleTestHelpers.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/AgentTestSetWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationControlSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr18SettingsTest,"MA0T10.UXR18.GroupsCountsAndSessionMemory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr18SettingsTest::RunTest(const FString&)
{
    using namespace Uxr15ToggleTests;
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* C=World->SpawnActor<AVirtualSensorCoordinator>();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();W->SetCoordinator(C);
    auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();
    for(auto* A:TArray<AActor*>{C,Camera,Lidar})A->Tags.Add(TEXT("SensorTestManaged"));Camera->StopSensor();Lidar->StopSensor();C->RegisterSensorActor(Camera);C->RegisterSensorActor(Lidar);
    auto* S=NewObject<UVirtualSensorSettingsPanelWidget>(World);S->Initialize();W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,S);S->BindSensorManager(C);S->SetVisibility(ESlateVisibility::Visible);const auto Slate=S->TakeWidget();
    TestEqual(TEXT("fresh settings shows basic only"),S->WorkspaceSettingsTab,0);
    C->SelectSensorActor(Lidar);S->SynchronizeWorkspaceSelection(true);S->RefreshOwnedSettingsText();
    TestEqual(TEXT("default fidelity count is zero"),S->OwnedTabEnabledCounts[1],0);TestEqual(TEXT("default automation count is zero"),S->OwnedTabEnabledCounts[2],0);
    TArray<FString> Text;SensorPresentationTests::VisibleText(Slate,Text);const FString Basic=FString::Join(Text,TEXT("\n"));
    TestTrue(TEXT("basic includes measurement and preview"),Basic.Contains(TEXT("측정 시작"))&&Basic.Contains(TEXT("미리보기 최대 점 수")));
    TestFalse(TEXT("basic does not expose opt-in fidelity or API"),Basic.Contains(TEXT("점별 시간차"))||Basic.Contains(TEXT("외부 제어 API")));
    S->SelectOwnedSettingsTab(1);Toggle(Slate,TEXT("움직임 왜곡"),this);S->RefreshOwnedSettingsText();
    TestEqual(TEXT("motion and its point-time dependency each count once"),S->OwnedTabEnabledCounts[1],2);
    Toggle(Slate,TEXT("재질 반사"),this);S->RefreshOwnedSettingsText();TestEqual(TEXT("all three actual fidelity flags count"),S->OwnedTabEnabledCounts[1],3);
    Toggle(Slate,TEXT("점별 시간차"),this);S->RefreshOwnedSettingsText();TestEqual(TEXT("time off clears dependent motion in count"),S->OwnedTabEnabledCounts[1],1);
    Toggle(Slate,TEXT("재질 반사"),this);S->RefreshOwnedSettingsText();TestTrue(TEXT("off removes the badge"),S->OwnedTabActivity[1].IsEmpty());
    Toggle(Slate,TEXT("다중 검출 사용"),this);S->RefreshOwnedSettingsText();TestEqual(TEXT("multi-hit counts as one enabled feature"),S->OwnedTabEnabledCounts[1],1);Toggle(Slate,TEXT("다중 검출 사용"),this);
    S->SelectOwnedSettingsTab(2);Toggle(Slate,TEXT("스캔 시 자동 파일 저장 · CSV"),this);S->RefreshOwnedSettingsText();TestEqual(TEXT("automatic scan file selection belongs to automation count"),S->OwnedTabEnabledCounts[2],1);Toggle(Slate,TEXT("스캔 시 자동 파일 저장 · CSV"),this);S->SelectOwnedSettingsTab(1);
    // An external configuration transaction updates the badge without writing pending UI values.
    FVirtualSensorEditableState State;FString Error;Camera->ReadEditableState(State);State.bCameraGroundTruth=true;Camera->ApplyEditableState(State,Error);
    C->SelectSensorActor(Camera);S->SynchronizeWorkspaceSelection(true);S->RefreshOwnedSettingsText();TestEqual(TEXT("selected Camera truth counted from runtime"),S->OwnedTabEnabledCounts[1],1);
    Toggle(Slate,TEXT("정답 데이터"),this);S->RefreshOwnedSettingsText();TestEqual(TEXT("UI off clears truth badge"),S->OwnedTabEnabledCounts[1],0);
    S->SelectOwnedSettingsTab(2);S->OpenOwnedSettingsSection(false);TestEqual(TEXT("reopen retains automation tab"),S->WorkspaceSettingsTab,2);
    S->SetVisibility(ESlateVisibility::Collapsed);S->OwnedAutomationText=FText::FromString(TEXT("hidden sentinel"));S->OwnedTabActivity[1]=FText::FromString(TEXT("hidden badge sentinel"));S->NativeTick(FGeometry(),.3f);
    TestEqual(TEXT("hidden settings do not format API details"),S->OwnedAutomationText.ToString(),FString(TEXT("hidden sentinel")));TestEqual(TEXT("hidden settings do not format badges"),S->OwnedTabActivity[1].ToString(),FString(TEXT("hidden badge sentinel")));
    S->SetVisibility(ESlateVisibility::Visible);S->SelectOwnedSettingsTab(0);S->RefreshOwnedSettingsText();TestEqual(TEXT("inactive automation tab leaves detail cache untouched"),S->OwnedAutomationText.ToString(),FString(TEXT("hidden sentinel")));
    auto* Fresh=NewObject<UVirtualSensorSettingsPanelWidget>(World);TestEqual(TEXT("new session instance defaults to basic"),Fresh->WorkspaceSettingsTab,0);
    TestNull(TEXT("no settings tab added to SensorUI SaveGame"),UVirtualSensorUiPreferencesSaveGame::StaticClass()->FindPropertyByName(TEXT("WorkspaceSettingsTab")));
    TestEqual(TEXT("existing SensorUI version remains six"),GetDefault<UVirtualSensorUiPreferencesSaveGame>()->Version,6);TestEqual(TEXT("existing Workspace version remains one"),GetDefault<USensorToolWorkspaceSaveGame>()->Version,1);
    W->UnregisterPanel(S);C->Destroy();Camera->Destroy();Lidar->Destroy();return true;
}

class FUxr18Routes final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),Wait=0;int32 Phase=0;
    UWorld* World=nullptr;UVirtualSensorToolWorkspaceSubsystem* W=nullptr;UVirtualSensorSettingsPanelWidget* S=nullptr;AVirtualSensorActorBase* Actor=nullptr;
    TSharedPtr<SWidget> Slate;FString Job;
public:
    explicit FUxr18Routes(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        using namespace Uxr15ToggleTests;
        const double Now=FPlatformTime::Seconds();if(Now-Started>70){Test->AddError(TEXT("UXR18 routes timeout"));return true;}
        if(!World)
        {
            for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&FinalAcceptanceViewportReady(C.World()))World=C.World();if(!World)return false;
            W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();C->StopAllSensors();
            for(auto* A:C->GetSensorActors())if(A&&A->GetSensorKind()==EVirtualSensorKind::Lidar){Actor=A;C->SelectSensorActor(A);break;}
            if(!Actor){Test->AddError(TEXT("UXR18 requires existing LiDAR fixture"));return true;}
            W->SynchronizeOwnedSelection();S=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));W->SetPanelOpen(ESensorToolPanelRole::Settings,true);S->SelectOwnedSettingsTab(0);Slate=S->TakeWidget();Wait=Now+.3;return false;
        }
        if(Now<Wait)return false;
        auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());auto* Data=Cast<UVirtualSensorCaptureExportPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Data));
        if(Phase==0)
        {
            Click(Slate.ToSharedRef(),TEXT("측정 시작"),Test);Test->TestTrue(TEXT("basic start uses existing placement action"),Actor->IsSensorRunning());
            Click(Slate.ToSharedRef(),TEXT("측정 중지"),Test);Test->TestFalse(TEXT("basic stop uses existing placement action"),Actor->IsSensorRunning());
            S->SelectOwnedSettingsTab(1);Click(Slate.ToSharedRef(),TEXT("IMU 부착·노이즈·설정 열기"),Test);Test->TestEqual(TEXT("IMU link opens original owner"),W->GetSidebarRole(),ESensorToolPanelRole::Placement);
            Test->TestNull(TEXT("navigation alone never attaches IMU"),Actor->FindComponentByClass<UVirtualImuComponent>());
            auto* Placement=W->GetOwnedPanel(ESensorToolPanelRole::Placement);Click(Placement->TakeWidget(),TEXT("선택 장비에 IMU 부착"),Test);Test->TestNotNull(TEXT("same owner attachment callback works"),Actor->FindComponentByClass<UVirtualImuComponent>());
            Bar->OpenOwnedDestination(ESensorToolDestination::SensorSettings);W->SetSidebarMode(false);W->SetSidebarMode(true);
            // Existing sidebar-mode restoration selects Placement. Reopen Settings
            // before observing its retained tab; do not change that workspace policy.
            Bar->OpenOwnedDestination(ESensorToolDestination::SensorSettings);Phase=1;Wait=Now+.5;return false;
        }
        if(Phase==1)
        {
            Test->TestTrue(TEXT("rehosting retains exact settings instance"),W->GetOwnedPanel(ESensorToolPanelRole::Settings)==S);
            TArray<FString> Text;SensorPresentationTests::VisibleText(Slate.ToSharedRef(),Text);Test->TestTrue(TEXT("ordinary reopen keeps advanced and shows attached count"),Text.Contains(TEXT("IMU 부착·노이즈·설정 열기"))&&Text.Contains(TEXT("1개 켜짐")));
            S->SelectOwnedSettingsTab(2);Click(Slate.ToSharedRef(),TEXT("데이터셋 생성·형식 설정 열기"),Test);Test->TestTrue(TEXT("dataset link opens existing data instance and file tab"),W->GetSidebarRole()==ESensorToolPanelRole::Data&&Data->GetOwnedTab()==ESensorOwnedDataTab::FileSave);
            Test->TestFalse(TEXT("navigation never starts dataset"),World->GetSubsystem<UVirtualDatasetSubsystem>()->IsActive());
            Toggle(Data->TakeWidget(),TEXT("데이터셋 생성 사용"),Test);Test->TestEqual(TEXT("dataset badge reads original draft"),Data->GetOwnedDatasetEnabledCount(),1);
            Click(Data->TakeWidget(),TEXT("생성 후 내보내기 형식: 없음"),Test);Test->TestEqual(TEXT("chosen export format adds one feature"),Data->GetOwnedDatasetEnabledCount(),2);
            for(const TCHAR* Caption:{TEXT("생성 후 내보내기 형식: COCO"),TEXT("생성 후 내보내기 형식: KITTI 3D"),TEXT("생성 후 내보내기 형식: COCO + KITTI 3D")})Click(Data->TakeWidget(),Caption,Test);
            Test->TestEqual(TEXT("format None clears only export count"),Data->GetOwnedDatasetEnabledCount(),1);
            Test->TestTrue(TEXT("dataset Back returns to settings"),Data->NavigatePanelBack());Test->TestEqual(TEXT("dataset Back returns original owner"),W->GetSidebarRole(),ESensorToolPanelRole::Settings);
            Phase=2;Wait=Now+.4;return false;
        }
        if(Phase==2)
        {
            Click(Slate.ToSharedRef(),TEXT("시나리오 테스트셋 열기"),Test);auto* First=W->GetTestSetWidget();Test->TestNotNull(TEXT("TESTSET link uses existing DTCore-managed screen"),First);
            Click(Slate.ToSharedRef(),TEXT("시나리오 테스트셋 열기"),Test);Test->TestTrue(TEXT("repeated TESTSET link keeps same instance"),First==W->GetTestSetWidget());if(First)First->RemoveFromParent();
            Job=FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-18/routes-job.json");FFileHelper::SaveStringToFile(TEXT("{\"schema\":\"ma0t10.job.v1\",\"result_path\":\"routes-job-result.json\",\"steps\":[{\"op\":\"wait\",\"seconds\":1}]}"),*Job);
            Test->TestTrue(TEXT("test job uses existing admission"),World->GetSubsystem<UAutomationControlSubsystem>()->StartJob(Job,false));Phase=3;Wait=Now+.5;return false;
        }
        if(Phase==3)
        {
            TArray<FString> Text;SensorPresentationTests::VisibleText(Slate.ToSharedRef(),Text);const FString All=FString::Join(Text,TEXT("\n"));
            Test->TestTrue(TEXT("automation badge combines dataset and running control group"),Text.Contains(TEXT("2개 켜짐")));Test->TestTrue(TEXT("automation shows original batch state"),All.Contains(TEXT("일괄 실행 중")));
            Phase=4;Wait=Now+2;return false;
        }
        Test->TestEqual(TEXT("job completion leaves dataset opt-in only"),Data->GetOwnedDatasetEnabledCount(),1);
        S->SelectOwnedSettingsTab(2);Click(Slate.ToSharedRef(),TEXT("데이터셋 생성·형식 설정 열기"),Test);Toggle(Data->TakeWidget(),TEXT("데이터셋 생성 사용"),Test);Data->NavigatePanelBack();
        if(auto* Imu=Actor->FindComponentByClass<UVirtualImuComponent>())Imu->DestroyComponent();return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr18RoutesTest,"MA0T10.UXR18.OwnedRoutesAndActivityRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr18RoutesTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP actual RHI required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr18Routes(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
