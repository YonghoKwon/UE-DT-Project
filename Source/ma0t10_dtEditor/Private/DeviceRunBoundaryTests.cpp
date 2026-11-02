#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/DeviceRunDeliveryLedger.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Scenario/ProjectRunDeliveryAdapter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceRunLedgerBoundary,"MA0T10.DeviceBoundary.RunLedgerBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeviceRunLedgerBoundary::RunTest(const FString&)
{
    FDeviceRunDeliveryLedger Ledger;
    Ledger.Record(TEXT(""),TEXT("s"),2,1);
    TestEqual(TEXT("unbound observations are not runs"),Ledger.Count(TEXT("")),int64(0));
    Ledger.Record(TEXT("r"),TEXT("s"),2,1);Ledger.Record(TEXT("r"),TEXT("s"),2,1);
    TestEqual(TEXT("retry does not duplicate failure"),Ledger.Count(TEXT("r")),int64(1));
    Ledger.Record(TEXT("r"),TEXT("s"),1,1);Ledger.Record(TEXT("r"),TEXT("other"),2,1);
    TestEqual(TEXT("output and sensor remain distinct"),Ledger.Count(TEXT("r")),int64(3));
    Ledger.Reset();
    for(int32 I=0;I<40;++I)Ledger.Record(FString::FromInt(I),TEXT("s"),2,1);
    int64 Retained=0;for(int32 I=0;I<40;++I)Retained+=Ledger.Count(FString::FromInt(I));
    TestEqual(TEXT("existing 32 run bound retained"),Retained,int64(32));
    TestEqual(TEXT("latest run retained"),Ledger.Count(TEXT("39")),int64(1));
    auto* Publisher=NewObject<UVirtualSensorStreamPublisherComponent>();
    TestEqual(TEXT("legacy pending adapter"),Publisher->GetPendingSlabRunCount(TEXT("x")),ProjectRunDeliveryAdapter::Pending(*Publisher,TEXT("x")));
    TestEqual(TEXT("legacy failure adapter"),Publisher->GetFailedSlabRunCount(TEXT("x")),ProjectRunDeliveryAdapter::Failed(*Publisher,TEXT("x")));
    return true;
}
#endif
