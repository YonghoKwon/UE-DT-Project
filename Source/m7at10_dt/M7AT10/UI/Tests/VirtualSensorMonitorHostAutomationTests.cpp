#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorHostActor.h"
#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorHostFallbackTest, "M7AT10.SensorMonitor.HostNativeFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorMonitorHostFallbackTest::RunTest(const FString& Parameters)
{
    AVirtualSensorMonitorHostActor* HostActor = NewObject<AVirtualSensorMonitorHostActor>();
    TestNotNull(TEXT("monitor host actor"), HostActor);

    HostActor->MonitorWidgetClass = nullptr;
    HostActor->bUseNativeMonitorWidgetFallback = true;
    TestEqual(TEXT("native fallback class"), HostActor->GetEffectiveMonitorWidgetClass(), TSubclassOf<UVirtualSensorMonitorWidget>(UVirtualSensorMonitorWidget::StaticClass()));

    HostActor->bUseNativeMonitorWidgetFallback = false;
    TestNull(TEXT("fallback disabled returns null"), HostActor->GetEffectiveMonitorWidgetClass().Get());

    HostActor->MonitorWidgetClass = UVirtualSensorMonitorWidget::StaticClass();
    TestEqual(TEXT("explicit class wins"), HostActor->GetEffectiveMonitorWidgetClass(), TSubclassOf<UVirtualSensorMonitorWidget>(UVirtualSensorMonitorWidget::StaticClass()));
    return true;
}

#endif
