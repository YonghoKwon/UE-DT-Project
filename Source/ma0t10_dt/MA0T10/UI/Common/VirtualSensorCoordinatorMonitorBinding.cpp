#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorMonitorPanel.h"

void AVirtualSensorCoordinator::BindMonitorWidget(UDxWidgetVirtualSensorMonitorPanel* MonitorWidget)
{
    BoundMonitorWidget = MonitorWidget;
    BindDeviceMonitorView(MonitorWidget);
}

void AVirtualSensorCoordinator::EndPlay(const EEndPlayReason::Type Reason)
{
    Super::EndPlay(Reason);
    BoundMonitorWidget = nullptr;
}
