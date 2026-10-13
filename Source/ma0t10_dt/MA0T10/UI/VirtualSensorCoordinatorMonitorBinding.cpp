#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "VirtualSensorMonitorPanelWidget.h"

void AVirtualSensorCoordinator::BindMonitorWidget(UVirtualSensorMonitorPanelWidget* MonitorWidget)
{
    BoundMonitorWidget = MonitorWidget;
    ApplyWidgetBinding();
}

IVirtualSensorMonitorView* AVirtualSensorCoordinator::GetMonitorView() const
{
    return Cast<IVirtualSensorMonitorView>(BoundMonitorWidget.Get());
}
