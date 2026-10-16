#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Uxr15ToggleTestHelpers.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "Misc/Paths.h"

class FUxr15LayoutRoundTrip final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;TWeakObjectPtr<UVirtualSensorLayoutSubsystem> Layout;TWeakObjectPtr<AVirtualCameraSensorActor> Camera;TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;TWeakObjectPtr<UVirtualSensorSettingsPanelWidget> Settings;TSharedPtr<SWidget> Slate;FString Request;double Started=FPlatformTime::Seconds();
public:
    FUxr15LayoutRoundTrip(FAutomationTestBase* T,UVirtualSensorLayoutSubsystem* L,AVirtualCameraSensorActor* C,AVirtualLidarSensorActor* A,UVirtualSensorSettingsPanelWidget* S,TSharedRef<SWidget> Widget,FString Id):Test(T),Layout(L),Camera(C),Lidar(A),Settings(S),Slate(Widget),Request(MoveTemp(Id)){}
    bool Update() override
    {
        if(!Layout.IsValid()||!Camera.IsValid()||!Lidar.IsValid()||!Settings.IsValid()){Test->AddError(TEXT("toggle layout fixture lost ownership"));return true;}
        Layout->Tick(0);const auto Status=Layout->GetOperationStatus(Request);
        if(Status.State==EVirtualSensorLayoutOperation::Writing){if(FPlatformTime::Seconds()-Started<10)return false;Test->AddError(TEXT("toggle layout writer timeout"));return true;}
        Test->TestEqual(TEXT("production asynchronous layout writer completed"),Status.State,EVirtualSensorLayoutOperation::Saved);
        auto* C=Layout->Coordinator();C->SelectSensorActor(Camera.Get());Settings->SelectTargetKind(EVirtualSensorTargetKind::Camera);Uxr15ToggleTests::Toggle(Slate.ToSharedRef(),TEXT("정답 데이터"),Test);
        C->SelectSensorActor(Lidar.Get());Settings->SelectTargetKind(EVirtualSensorTargetKind::Lidar);Uxr15ToggleTests::Toggle(Slate.ToSharedRef(),TEXT("점별 시간차"),Test);Uxr15ToggleTests::Toggle(Slate.ToSharedRef(),TEXT("재질 반사"),Test);
        Test->TestFalse(TEXT("UI switched runtime options off before disk restore"),Camera->CaptureComponent->bGroundTruthEnabled||Lidar->ScanComponent->bUseRollingPointTimestamps||Lidar->ScanComponent->bEnableMotionDistortion||Lidar->ScanComponent->bEnableMaterialResponse);
        const FString Preview=Layout->PreviewLayoutRestore(TEXT("UXR15 toggle flags"));Test->TestEqual(TEXT("production disk loader accepts existing schema"),Layout->GetOperationStatus(Preview).State,EVirtualSensorLayoutOperation::ReadyToApply);FString Error;
        Test->TestTrue(TEXT("production layout restore applies saved runtime options"),Layout->ApplyLayoutRestore(Preview,Error));Test->TestTrue(TEXT("disk roundtrip restores GT and all three fidelity flags"),Camera->CaptureComponent->bGroundTruthEnabled&&Lidar->ScanComponent->bUseRollingPointTimestamps&&Lidar->ScanComponent->bEnableMotionDistortion&&Lidar->ScanComponent->bEnableMaterialResponse);
        Camera->GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->UnregisterPanel(Settings.Get());C->Destroy();Camera->Destroy();Lidar->Destroy();return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr15LayoutTest,"MA0T10.UXR15.UiSettingsDiskLayoutRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr15LayoutTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* C=World->SpawnActor<AVirtualSensorCoordinator>();auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();for(auto* A:TArray<AActor*>{C,Camera,Lidar})A->Tags.Add(TEXT("SensorTestManaged"));Camera->StopSensor();Lidar->StopSensor();C->RegisterSensorActor(Camera);C->RegisterSensorActor(Lidar);
    auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();W->SetCoordinator(C);auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(World);Settings->Initialize();W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Settings);Settings->BindSensorManager(C);Settings->SetVisibility(ESlateVisibility::Visible);const auto Slate=Settings->TakeWidget();
    C->SelectSensorActor(Camera);Settings->SelectTargetKind(EVirtualSensorTargetKind::Camera);Settings->SelectOwnedSettingsTab(1);Uxr15ToggleTests::Toggle(Slate,TEXT("정답 데이터"),this);C->SelectSensorActor(Lidar);Settings->SelectTargetKind(EVirtualSensorTargetKind::Lidar);for(const TCHAR* Name:{TEXT("점별 시간차"),TEXT("움직임 왜곡"),TEXT("재질 반사")})Uxr15ToggleTests::Toggle(Slate,Name,this);
    auto* Layout=World->GetSubsystem<UVirtualSensorLayoutSubsystem>();Layout->StorageRootOverrideForTests=FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-15/toggle-layout")/FGuid::NewGuid().ToString();const FString Request=Layout->RequestSaveLayout(TEXT("UXR15 toggle flags"));
    ADD_LATENT_AUTOMATION_COMMAND(FUxr15LayoutRoundTrip(this,Layout,Camera,Lidar,Settings,Slate,Request));return true;
}
#endif
