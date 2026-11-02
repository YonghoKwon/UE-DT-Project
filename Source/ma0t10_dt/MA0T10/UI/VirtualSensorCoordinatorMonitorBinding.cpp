#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "VirtualSensorMonitorPanelWidget.h"

void AVirtualSensorCoordinator::BindMonitorWidget(UVirtualSensorMonitorPanelWidget* MonitorWidget)
{
    BoundMonitorWidget = MonitorWidget;
    BindDeviceMonitorView(MonitorWidget);
}

void AVirtualSensorCoordinator::EndPlay(const EEndPlayReason::Type Reason)
{
    Super::EndPlay(Reason);
    BoundMonitorWidget = nullptr;
}
