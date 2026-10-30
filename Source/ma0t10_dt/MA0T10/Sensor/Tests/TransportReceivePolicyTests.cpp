#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorReceiverRequestPolicyTest,
    "MA0T10.SensorStream.HighThroughput.ExplicitReceivePolicy",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVirtualSensorReceiverRequestPolicyTest::RunTest(const FString&)
{
    auto* Raw=NewObject<UVirtualSensorHighThroughputTransportSubsystem>();
    TestFalse(TEXT("publishing does not implicitly request payload subscriptions"),Raw->ReceiverSelection.bEnabled);
    for(int32 Kind=0;Kind<3;++Kind)TestFalse(TEXT("all unrequested payload kinds stay unsubscribed"),Raw->ReceiverSelection.WantsKind(Kind));
    FVirtualSensorReceiveSelection Requested;
    Raw->ConfigureReceiver(Requested);
    TestTrue(TEXT("explicit default selection preserves full observation API"),Raw->ReceiverSelection.bEnabled);
    for(int32 Kind=0;Kind<3;++Kind)TestTrue(TEXT("explicit all-topic request includes each payload kind"),Raw->ReceiverSelection.WantsKind(Kind));
    Requested.bAllTopics=false;Requested.StreamKeys.Add(TEXT("2|sensor-a"));Raw->ConfigureReceiver(Requested);
    TestTrue(TEXT("selected point cloud accepted"),Raw->ReceiverSelection.Accepts(2,TEXT("sensor-a")));
    TestFalse(TEXT("unselected point cloud stays filtered"),Raw->ReceiverSelection.Accepts(2,TEXT("sensor-b")));
    Requested.bEnabled=false;Raw->ConfigureReceiver(Requested);
    TestFalse(TEXT("stopping observation removes the payload request"),Raw->ReceiverSelection.WantsKind(2));
    return true;
}
#endif
