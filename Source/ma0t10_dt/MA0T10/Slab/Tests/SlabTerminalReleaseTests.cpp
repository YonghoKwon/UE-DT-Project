#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "HAL/PlatformProcess.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabTerminalReleaseTest,"MA0T10.SlabReliability.TerminalReleasesIndependentUiStream",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabTerminalReleaseTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();
    auto* M=W->SpawnActor<AVirtualSensorCoordinator>();auto* Sensor=W->SpawnActor<AVirtualLidarSensorActor>();M->RegisterSensorActor(Sensor);
    auto* Panel=NewObject<UVirtualSensorCaptureExportPanelWidget>(W);Panel->BindSensorManager(M);
    auto* P=M->StreamPublisherComponent.Get();auto* T=M->SharedTransportComponent.Get();auto Profile=T->GetTransportProfile();Profile.BrokerUrl=TEXT("tcp://127.0.0.1:1");T->ConfigureTransportProfile(Profile);T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
    auto* Session=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>();auto* Results=W->GetSubsystem<USlabRunResultsSubsystem>();
    const FString Id=Sensor->GetSensorId();
    for(bool Cancelled:{false,true})
    {
        const FString Run=Session->BeginReplaySensorSession(FString(),{Id},FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),false);
        TestFalse(TEXT("ordinary observation starts"),Run.IsEmpty());Session->NotifySlabFrameApplied(Run,TEXT("SCN-RELEASE-01"),7,.35);
        const auto OldContext=Session->CaptureContext(Id,41);
        TestTrue(TEXT("active observation owns sensor"),Session->ControlsSensor(Id));
        TestFalse(TEXT("active observation never sends PCD"),Session->AllowsStreamDemand(Id,EVirtualSensorStreamKind::PointCloud));
        Session->EndSlabSensorSession(Run,Cancelled);
        TestTrue(TEXT("draining retains ownership"),Session->ControlsSensor(Id));Session->Tick(0);
        const auto Terminal=Session->GetSlabSensorSessionStatus();FSlabRunDeliverySummary Before,After;
        TestTrue(TEXT("terminal result exists before release"),Results->GetRunResult(Run,Before));
        TestEqual(TEXT("cancel flag preserved"),Terminal.bAborted,Cancelled);
        TestFalse(TEXT("terminal no longer owns sensor"),Session->ControlsSensor(Id));
        // This is the same method as the normal native UI Start button, without AUTO.
        Panel->ToggleSelectedStream(EVirtualSensorStreamKind::PointCloud);
        TestTrue(TEXT("ordinary UI starts independent stream"),P->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,Id));
        TestTrue(TEXT("independent PCD demand restored"),Session->AllowsStreamDemand(Id,EVirtualSensorStreamKind::PointCloud));
        const auto Independent=Session->CaptureContext(Id,42);
        TestFalse(TEXT("new independent acquisition has no old run"),Independent.bEligible);
        TestTrue(TEXT("new independent frame can route"),Session->AllowsStreamFrame(Id,EVirtualSensorStreamKind::PointCloud,Independent));
        TestFalse(TEXT("late old-run frame stays rejected"),Session->AllowsStreamFrame(Id,EVirtualSensorStreamKind::PointCloud,OldContext));
        TestEqual(TEXT("terminal status context retained"),Session->GetSlabSensorSessionStatus().CurrentSlab.SlabFrameNo,Terminal.CurrentSlab.SlabFrameNo);
        Results->GetRunResult(Run,After);TestEqual(TEXT("result finalization unchanged by new stream"),After.FinishedUtc,Before.FinishedUtc);TestEqual(TEXT("terminal outcome preserved"),After.Outcome,Before.Outcome);
        Panel->ToggleSelectedStream(EVirtualSensorStreamKind::PointCloud);
    }
    FVirtualSlabSensorOutputSelection Outputs=FVirtualSlabSensorOutputSelection::ObservationOnly();Outputs.bPointCloud=true;
    FSlabExecutionOptions Options;Options.Policy=ESlabExecutionPolicy::RequireData;FString Error;
    const FString Preparing=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    TestTrue(TEXT("required-data preparation accepted"),Session->PrepareScenarioTransmission(Preparing,{Id},FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),Outputs,Options,Error));
    TestTrue(TEXT("required preparation still locks configuration"),Session->IsSensorConfigurationLocked(Id));
    Session->EndSlabSensorSession(Preparing,true);
    TestFalse(TEXT("cancelled preparation unlocks configuration"),Session->IsSensorConfigurationLocked(Id));
    TestTrue(TEXT("cancelled preparation permits independent demand"),Session->AllowsStreamDemand(Id,EVirtualSensorStreamKind::PointCloud));
    Options.PreparationTimeoutSeconds=1;
    TestTrue(TEXT("disconnected required-data preparation accepted"),Session->PrepareScenarioTransmission(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),{Id},FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),Outputs,Options,Error));
    FPlatformProcess::SleepNoStats(1.05f);Session->Tick(0);
    TestEqual(TEXT("required-data failure remains incomplete"),Session->GetSlabSensorSessionStatus().State,EVirtualSlabSessionState::Incomplete);
    TestFalse(TEXT("failed preparation unlocks configuration"),Session->IsSensorConfigurationLocked(Id));
    TestTrue(TEXT("failed preparation permits independent demand"),Session->AllowsStreamDemand(Id,EVirtualSensorStreamKind::PointCloud));
    W->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()->StopHighThroughputTransport();M->Destroy();Sensor->Destroy();return true;
}
#endif
