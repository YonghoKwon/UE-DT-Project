#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Uxr15ToggleTestHelpers.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolNumericInput.h"
#include "ma0t10_dt/MA0T10/UI/SensorLidarHelp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationControlSubsystem.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionNumericFeedbackTest,"MA0T10.Inspection.NumericClampFeedback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionNumericFeedbackTest::RunTest(const FString&)
{
    using namespace Uxr15ToggleTests;
    auto Input=MakeShared<TSensorToolNumericInput<int32>>(1,100);int32 Value=10;
    auto Spin=SNew(SSpinBox<int32>).TypeInterface(Input).MinValue(1).MaxValue(100).Value_Lambda([&](){return Value;})
        .OnValueChanged_Lambda([&](int32 V){Value=V;}).OnValueCommitted_Lambda([&](int32 V,ETextCommit::Type){Value=V;});
    TSharedPtr<SEditableText> Edit;
    Walk(Spin,[&](const TSharedRef<SWidget>& Item){if(Item->GetType()==TEXT("SEditableText"))Edit=StaticCastSharedRef<SEditableText>(Item);});
    if(!TestNotNull(TEXT("real Slate numeric editor"),Edit.Get()))return false;
    auto Enter=[&](const TCHAR* Text){Edit->SetText(FText::FromString(Text));StaticCastSharedRef<SWidget>(Edit.ToSharedRef())->OnKeyDown(FGeometry(),FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));};
    Enter(TEXT("501"));TestEqual(TEXT("typed upper bound is still clamped by Slate"),Value,100);TestEqual(TEXT("upper adjustment explained"),Input->Notice.ToString(),FString(TEXT("최대값 100으로 조정됨")));
    Enter(TEXT("-8"));TestEqual(TEXT("typed lower bound is still clamped by Slate"),Value,1);TestEqual(TEXT("lower adjustment explained"),Input->Notice.ToString(),FString(TEXT("최소값 1으로 조정됨")));
    Enter(TEXT("42"));TestEqual(TEXT("valid input preserved"),Value,42);TestTrue(TEXT("next valid entry clears old explanation"),Input->Notice.IsEmpty());
    auto Floating=MakeShared<TSensorToolNumericInput<float>>(.033f,60.f);
    TestTrue(TEXT("float parsing still accepts decimal expressions"),Floating->FromString(TEXT("0.1"),1).IsSet());
    Floating->FromString(TEXT("-1"),1);TestTrue(TEXT("float minimum explained"),Floating->Notice.ToString().StartsWith(TEXT("최소값")));
    TestFalse(TEXT("invalid input is not converted into zero"),Floating->FromString(TEXT("invalid"),1).IsSet());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionLidarHelpTest,"MA0T10.Inspection.LidarHelpVocabulary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionLidarHelpTest::RunTest(const FString&)
{
    for(const TCHAR* Key:{TEXT("PointTime"),TEXT("MotionDistortion"),TEXT("MaterialResponse"),TEXT("MultiHit"),TEXT("ReflectionIntensity"),TEXT("MeasurementTime")})
    {
        const FString Short=SensorLidarHelp::Summary(Key).ToString(),Detail=SensorLidarHelp::Detail(Key).ToString();
        TestTrue(TEXT("each option has a short adjacent explanation"),!Short.IsEmpty()&&Short.Len()<50);
        TestTrue(TEXT("each help explains visible result, use and cost"),Detail.Contains(TEXT("켜면 화면에서"))&&Detail.Contains(TEXT("언제:"))&&Detail.Contains(TEXT("비용:"))&&Detail.Contains(TEXT("학습 데이터"))&&Detail.Contains(TEXT("기본 꺼짐")));
        for(const auto& Word:SensorPresentationTests::ForbiddenWords())TestFalse(*FString::Printf(TEXT("help avoids %s"),*Word),Detail.Contains(Word,ESearchCase::IgnoreCase));
    }
    return true;
}

class FInspectionUiFeedbackCommand final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),Next=0;
    int32 Phase=0;
    TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;
    TWeakObjectPtr<AVirtualCameraSensorActor> Camera;
    TWeakObjectPtr<UVirtualImuComponent> Imu;
    FString Csv;
    static bool ApiHasImu(UWorld* World,const FString& SensorId)
    {
        int32 Code=0;const auto Reply=World->GetSubsystem<UAutomationControlSubsystem>()->Dispatch(TEXT("GET"),TEXT("/v1/sensors"),nullptr,Code);
        if(Code!=200)return false;
        for(const auto& Value:Reply->GetArrayField(TEXT("sensors")))
        {const auto Item=Value->AsObject();if(Item->GetStringField(TEXT("sensor_id"))==SensorId)return Item->HasField(TEXT("imu"));}
        return false;
    }
public:
    explicit FInspectionUiFeedbackCommand(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        using namespace Uxr15ToggleTests;
        const double Now=FPlatformTime::Seconds();if(Now-Started>90){Test->AddError(TEXT("INSP-FIX-4 lifecycle timeout"));return true;}
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!FinalAcceptanceViewportReady(World)||Now<Next)return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();
        auto* P=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement));
        auto* M=Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor));
        auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());
        if(!C||!P||!M||!Bar)return false;
        if(Phase==0)
        {
            W->ResetOwnedWorkspaceLayout();W->SetSidebarMode(true);
            for(auto* A:C->GetSensorActors())if(A){A->StopSensor();if(auto* L=Cast<AVirtualLidarSensorActor>(A))Lidar=L;if(auto* Cam=Cast<AVirtualCameraSensorActor>(A))Camera=Cam;}
            if(!Test->TestTrue(TEXT("fixture Camera and LiDAR"),Lidar.IsValid()&&Camera.IsValid()))return true;
            auto* Extra=World->SpawnActor<AVirtualCameraSensorActor>();Extra->Tags.Add(TEXT("SensorTestManaged"));Extra->StopSensor();Extra->CaptureComponent->SensorId=TEXT("INSP4-CAMERA-B");Extra->CaptureComponent->CaptureResolution=FIntPoint(640,480);Extra->CaptureComponent->CaptureInterval=.1f;Extra->CaptureComponent->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;C->RegisterSensorActor(Extra);Extra->StartSensor();Camera->StartSensor();
            P->SelectEquipment(Lidar.Get());Bar->OpenOwnedDestination(ESensorToolDestination::Placement);P->OpenOwnedImuSettings();
            Click(P->TakeWidget(),TEXT("선택 장비에 IMU 부착"),Test);Imu=Lidar->FindComponentByClass<UVirtualImuComponent>();
            if(!Test->TestTrue(TEXT("explicit attachment"),Imu.IsValid()))return true;
            Test->TestTrue(TEXT("API exposes attached IMU"),ApiHasImu(World,Lidar->GetSensorId()));
            Click(P->OwnedImuArea.Pin().ToSharedRef(),TEXT("IMU 해제…"),Test);Test->TestTrue(TEXT("request alone does not detach"),Imu->IsRegistered());Click(P->OwnedImuArea.Pin().ToSharedRef(),TEXT("취소"),Test);Test->TestTrue(TEXT("cancel retains component"),Imu->IsRegistered());
            Click(P->TakeWidget(),TEXT("IMU 해제…"),Test);P->SelectEquipment(Camera.Get());Test->TestFalse(TEXT("confirmation cannot detach after selection changed"),P->ConfirmSelectedImuDetach());Test->TestTrue(TEXT("original target retained"),Imu->IsRegistered());P->SelectEquipment(Lidar.Get());
            Imu->SetRecording(true);Next=Now+.5;Phase=1;return false;
        }
        if(Phase==1)
        {
            Test->TestTrue(TEXT("recording collected samples"),!Imu->Recorder->GetImuSamples(Imu->GetImuId()).IsEmpty());
            Imu->SetPublishing(true);Test->TestTrue(TEXT("IMU transmission selected before confirmation"),Imu->IsPublishing());
            Click(P->TakeWidget(),TEXT("IMU 해제…"),Test);Click(P->TakeWidget(),TEXT("해제 확인"),Test);Csv=Imu->GetDetachCsvRequest();
            Test->TestTrue(TEXT("recording stopped before CSV finalization"),!Imu->IsRecording()&&!Imu->IsPublishing()&&Imu->IsRegistered()&&Imu->IsDetaching());
            Test->TestFalse(TEXT("CSV has request identity"),Csv.IsEmpty());
            FString Error;Test->TestFalse(TEXT("pending detach rejects IMU reconfiguration"),Imu->Configure(Imu->GetConfig(),Error));
            Imu->SetRecording(true);Imu->SetPublishing(true);Test->TestTrue(TEXT("pending detach cannot resume recording or transmission"),!Imu->IsRecording()&&!Imu->IsPublishing());
            // Hiding the owner must not cancel pending CSV or detach completion.
            W->SetPanelOpen(ESensorToolPanelRole::Placement,false);Phase=2;return false;
        }
        if(Phase==2)
        {
            const auto Status=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(Csv);if(!Status.IsTerminal())return false;
            if(Imu.IsValid()&&Imu->IsRegistered())return false;
            Test->TestEqual(TEXT("CSV finalized before detach"),Status.State,EVirtualSensorFileSaveState::Succeeded);
            FString Text;if(!Status.FileResults.IsEmpty())FFileHelper::LoadFileToString(Text,*Status.FileResults[0].AbsolutePath);
            Test->TestTrue(TEXT("recorded CSV exists with samples"),Text.Contains(TEXT("sequence"))&&Text.ParseIntoArrayLines(TextLines)>1);
            Test->TestNull(TEXT("detached IMU removed from actor"),Lidar->FindComponentByClass<UVirtualImuComponent>());
            Test->TestFalse(TEXT("AUTO-01 no longer exposes imu object"),ApiHasImu(World,Lidar->GetSensorId()));
            Test->TestTrue(TEXT("reattachment does not inherit old recorded samples"),Imu.IsValid()?Imu->Recorder->GetImuSamples(Lidar->GetSensorId()+TEXT(".imu")).IsEmpty():true);
            Bar->OpenOwnedDestination(ESensorToolDestination::Preview);M->BindVirtualLidar(Lidar->ScanComponent);M->ShowLidarView();
            FVirtualSensorEditableState State;FString Error;Lidar->ReadEditableState(State);State.LidarHorizontalSamples=8;State.LidarVerticalChannels=4;Lidar->ApplyEditableState(State,Error);
            M->RefreshLidarPreviewWithoutTransport(true);
            const FString Payload=Lidar->ScanComponent->BuildLastPhysicalJsonPayload();const auto Snapshot=Lidar->ScanComponent->GetLastFrameSnapshot();const auto Frame=Lidar->GetSensorRuntimeStatus().FrameId;
            Test->TestTrue(TEXT("payload invariance uses an acquired frame"),Snapshot.IsValid()&&Frame>0);
            Toggle(M->TakeWidget(),TEXT("점군만 보기"),Test);Test->TestTrue(TEXT("direct card toggle uses existing point-only state"),C->IsPointCloudOnlyModeEnabled());Toggle(M->TakeWidget(),TEXT("점군만 보기"),Test);Test->TestFalse(TEXT("second click returns to normal"),C->IsPointCloudOnlyModeEnabled());
            Test->TestEqual(TEXT("toggle leaves server payload bytes unchanged"),Lidar->ScanComponent->BuildLastPhysicalJsonPayload(),Payload);Test->TestTrue(TEXT("toggle keeps immutable frame identity"),Snapshot==Lidar->ScanComponent->GetLastFrameSnapshot());Test->TestEqual(TEXT("toggle adds no acquisition"),Lidar->GetSensorRuntimeStatus().FrameId,Frame);
            Test->TestTrue(TEXT("preview selected while another sidebar remains open"),Bar->IsSidebarRouteSelected(ESensorToolDestination::Preview));
            Bar->OpenOwnedDestination(ESensorToolDestination::Placement);Test->TestTrue(TEXT("preview selection persists beside placement"),Bar->IsSidebarRouteSelected(ESensorToolDestination::Preview));
            Bar->ToggleOwnedDestination(ESensorToolDestination::Preview);Test->TestFalse(TEXT("closed preview is unselected"),Bar->IsSidebarRouteSelected(ESensorToolDestination::Preview));
            P->SelectEquipment(Camera.Get());Bar->OpenOwnedDestination(ESensorToolDestination::Preview);M->SetDualCameraModeEnabled(true);
            Test->TestTrue(TEXT("fixture has dual cameras"),M->IsDualCameraModeEnabled());M->SetDualCameraSplitRatio(.7f);Click(M->TakeWidget(),TEXT("비율 70:30"),Test);Test->TestEqual(TEXT("one click splits evenly"),M->GetDualCameraSplitRatio(),.5f);
            M->SetDualCameraSplitRatio(-2);Test->TestEqual(TEXT("small pane has 30 percent minimum"),M->GetDualCameraSplitRatio(),.3f);M->SetDualCameraSplitRatio(9);Test->TestEqual(TEXT("opposite pane has 30 percent minimum"),M->GetDualCameraSplitRatio(),.7f);M->SetDualCameraSplitRatio(.3f);
            W->SetPanelOpen(ESensorToolPanelRole::Monitor,false);W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);W->SetSidebarMode(false);W->SetSidebarMode(true);Test->TestTrue(TEXT("rehosting uses same preview instance"),W->GetOwnedPanel(ESensorToolPanelRole::Monitor)==M);Test->TestEqual(TEXT("split remembered for session"),M->GetDualCameraSplitRatio(),.3f);
            W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);W->SetOwnedPanelFontScale(1.5f);W->SetDockedCardRect(M->GetCurrentPanelPosition(),W->GetDockedCardMinimum());Next=Now+.5;Phase=3;return false;
        }
        if(Phase==3)
        {
            const auto A=M->NativeViewImage->GetCachedGeometry().GetAbsoluteSize(),B=M->NativeSecondaryCameraImage->GetCachedGeometry().GetAbsoluteSize();
            Test->AddInfo(FString::Printf(TEXT("F8 rendered geometry primary=%s secondary=%s dual=%d lidar=%d equipment=%d empty=%d waiting=%d panel=%s"),*A.ToString(),*B.ToString(),M->bDualCameraModeEnabled,M->bShowingLidar,M->bOwnedHasEquipment,M->bOwnedPreviewEmpty,M->bOwnedPreviewWaiting,*M->GetEffectivePanelSize().ToString()));
            Test->TestTrue(TEXT("both camera images remain nonzero at minimum card and 150 percent"),A.X>1&&A.Y>1&&B.X>1&&B.Y>1);
            Test->TestTrue(TEXT("primary image preserves texture aspect"),FMath::IsNearlyEqual(float(A.X/A.Y),float(M->NativeViewBrush.ImageSize.X/M->NativeViewBrush.ImageSize.Y),.02f));
            Test->TestTrue(TEXT("secondary image preserves texture aspect"),FMath::IsNearlyEqual(float(B.X/B.Y),float(M->NativeSecondaryCameraBrush.ImageSize.X/M->NativeSecondaryCameraBrush.ImageSize.Y),.02f));
            Test->TestTrue(TEXT("30 percent split gives smaller primary pane"),A.Y<B.Y);
            P->SelectEquipment(Lidar.Get());P->AttachSelectedImu();auto* Empty=Lidar->FindComponentByClass<UVirtualImuComponent>();
            Test->TestTrue(TEXT("reattached IMU starts without old samples"),Empty&&Empty->Recorder&&Empty->Recorder->GetImuSamples(Empty->GetImuId()).IsEmpty());
            P->RequestSelectedImuDetach();Test->TestTrue(TEXT("unrecorded IMU detaches on confirmation"),P->ConfirmSelectedImuDetach());
            Test->TestFalse(TEXT("unrecorded detach also removes API object"),ApiHasImu(World,Lidar->GetSensorId()));
            P->AttachSelectedImu();Imu=Lidar->FindComponentByClass<UVirtualImuComponent>();Imu->SetRecording(true);
            Next=Now+.3;Phase=4;return false;
        }
        if(Phase==4)
        {
            P->RequestSelectedImuDetach();P->ConfirmSelectedImuDetach();Csv=Imu->GetDetachCsvRequest();
            Test->TestTrue(TEXT("inject cancellation of the asynchronous CSV"),World->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->CancelRequest(Csv));
            Next=Now+.3;Phase=5;return false;
        }
        if(Phase==5)
        {
            if(Imu->IsDetaching())return false;
            Test->TestTrue(TEXT("cancelled CSV retains attached IMU and recorded samples"),Imu->IsRegistered()&&!Imu->Recorder->GetImuSamples(Imu->GetImuId()).IsEmpty());
            Test->TestTrue(TEXT("failed finalization retains the API object"),ApiHasImu(World,Lidar->GetSensorId()));
            P->RefreshImu();Test->TestTrue(TEXT("failure explains retention and releases UI request tracking"),P->Status.Contains(TEXT("유지"))&&P->DetachingCsvRequest.IsEmpty());
            P->RequestSelectedImuDetach();Test->TestTrue(TEXT("retained samples can be saved on retry"),P->ConfirmSelectedImuDetach());Csv=Imu->GetDetachCsvRequest();Phase=6;return false;
        }
        if(Phase==6)
        {
            const auto Status=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(Csv);if(!Status.IsTerminal()||(Imu.IsValid()&&Imu->IsRegistered()))return false;
            Test->TestEqual(TEXT("retry finalized CSV before removal"),Status.State,EVirtualSensorFileSaveState::Succeeded);Test->TestFalse(TEXT("retry removed API object"),ApiHasImu(World,Lidar->GetSensorId()));return true;
        }
        return true;
    }
private:
    TArray<FString> TextLines;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionUiFeedbackTest,"MA0T10.Inspection.UiFeedbackLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionUiFeedbackTest::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FInspectionUiFeedbackCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
