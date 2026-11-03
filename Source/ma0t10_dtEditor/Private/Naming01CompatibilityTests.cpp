#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UnrealType.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNaming01Compatibility,"MA0T10.Naming01.RedirectCompatibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNaming01Compatibility::RunTest(const FString&)
{
    const TPair<const TCHAR*,const TCHAR*> Classes[]={
        {TEXT("/Script/DeviceRuntime.VirtualCameraGroundTruth"),TEXT("/Script/DeviceRuntime.VirtualCameraGroundTruthComp")},
        {TEXT("/Script/DeviceRuntime.VirtualSensorStreamPublisherComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorStreamPublisherComp")},
        {TEXT("/Script/DeviceRuntime.CameraJsonLiveSourceComponent"),TEXT("/Script/DeviceRuntime.CameraJsonLiveSourceComp")},
        {TEXT("/Script/DeviceRuntime.LidarCsvReplaySourceComponent"),TEXT("/Script/DeviceRuntime.LidarCsvReplaySourceComp")},
        {TEXT("/Script/DeviceRuntime.LidarHttpJsonLiveSourceComponent"),TEXT("/Script/DeviceRuntime.LidarHttpJsonLiveSourceComp")},
        {TEXT("/Script/DeviceRuntime.LidarJsonLinesReplaySourceComponent"),TEXT("/Script/DeviceRuntime.LidarJsonLinesReplaySourceComp")},
        {TEXT("/Script/DeviceRuntime.LidarJsonLiveSourceComponent"),TEXT("/Script/DeviceRuntime.LidarJsonLiveSourceComp")},
        {TEXT("/Script/DeviceRuntime.LidarPlayerDisplayComponent"),TEXT("/Script/DeviceRuntime.LidarPlayerDisplayComp")},
        {TEXT("/Script/DeviceRuntime.LidarUdpJsonLiveSourceComponent"),TEXT("/Script/DeviceRuntime.LidarUdpJsonLiveSourceComp")},
        {TEXT("/Script/DeviceRuntime.Ros2SensorBridgeSourceComponent"),TEXT("/Script/DeviceRuntime.Ros2SensorBridgeSourceComp")},
        {TEXT("/Script/DeviceRuntime.LivoxLidarSourceComponent"),TEXT("/Script/DeviceRuntime.LivoxLidarSourceComp")},
        {TEXT("/Script/DeviceRuntime.RealSenseCameraSourceComponent"),TEXT("/Script/DeviceRuntime.RealSenseCameraSourceComp")},
        {TEXT("/Script/DeviceRuntime.RealSensorSourceComponent"),TEXT("/Script/DeviceRuntime.RealSensorSourceComp")},
        {TEXT("/Script/DeviceRuntime.VirtualImuComponent"),TEXT("/Script/DeviceRuntime.VirtualImuComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarAnalysisComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarAnalysisComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarExportComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarExportComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarScanComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarScanComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarSurfaceResponseComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarSurfaceResponseComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarVisualizationComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarVisualizationComp")},
        {TEXT("/Script/DeviceRuntime.VirtualSensorOutputComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorOutputComp")},
        {TEXT("/Script/DeviceRuntime.VirtualSensorRecorderComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorRecorderComp")},
        {TEXT("/Script/DeviceRuntime.VirtualSensorTransportComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorTransportComp")},
        {TEXT("/Script/ma0t10_dt.SlabDataSyncComponent"),TEXT("/Script/ma0t10_dt.SlabDataSyncComp")},
        {TEXT("/Script/ma0t10_dt.SlabMetricsComponent"),TEXT("/Script/ma0t10_dt.SlabMetricsComp")},
        {TEXT("/Script/ma0t10_dt.SlabMotionComponent"),TEXT("/Script/ma0t10_dt.SlabMotionComp")},
        {TEXT("/Script/ma0t10_dt.SlabVisualizationComponent"),TEXT("/Script/ma0t10_dt.SlabVisualizationComp")},
        {TEXT("/Script/ma0t10_dt.ShipHatchOverlay"),TEXT("/Script/ma0t10_dt.ShipHatchOverlayComp")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorPanelHostComponent"),TEXT("/Script/ma0t10_dt.VirtualSensorPanelHostComp")},
        {TEXT("/Script/DeviceUI.DeviceSensorMonitorPanel"),TEXT("/Script/DeviceUI.DxDeviceSensorMonitorPanel")},
        {TEXT("/Script/DeviceUI.DeviceSensorPanelBase"),TEXT("/Script/DeviceUI.DxDeviceSensorPanelBase")},
        {TEXT("/Script/ma0t10_dt.SensorWorkspaceForeignFixture"),TEXT("/Script/ma0t10_dt.DxWidgetSensorWorkspaceForeignFixture")},
        {TEXT("/Script/ma0t10_dt.AgentTestSetWidget"),TEXT("/Script/ma0t10_dt.DxWidgetAgentTestSet")},
        {TEXT("/Script/ma0t10_dt.CoilLoadingMonitor"),TEXT("/Script/ma0t10_dt.DxWidgetCoilLoadingMonitor")},
        {TEXT("/Script/ma0t10_dt.SensorMapCheckWidget"),TEXT("/Script/ma0t10_dt.DxWidgetSensorMapCheck")},
        {TEXT("/Script/ma0t10_dt.ShipHatchMonitorWidget"),TEXT("/Script/ma0t10_dt.DxWidgetShipHatchMonitor")},
        {TEXT("/Script/ma0t10_dt.SlabChartsPanelWidget"),TEXT("/Script/ma0t10_dt.DxWidgetSlabChartsPanel")},
        {TEXT("/Script/ma0t10_dt.SlabProgressPanelWidget"),TEXT("/Script/ma0t10_dt.DxWidgetSlabProgressPanel")},
        {TEXT("/Script/ma0t10_dt.SlabScenarioReplayPanelWidget"),TEXT("/Script/ma0t10_dt.DxWidgetSlabScenarioReplayPanel")},
        {TEXT("/Script/ma0t10_dt.TrainBatchWidget"),TEXT("/Script/ma0t10_dt.DxWidgetTrainBatch")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorCaptureExportPanelWidget"),TEXT("/Script/ma0t10_dt.DxWidgetVirtualSensorCaptureExportPanel")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorMonitorPanelWidget"),TEXT("/Script/ma0t10_dt.DxWidgetVirtualSensorMonitorPanel")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorPanelWidgetBase"),TEXT("/Script/ma0t10_dt.DxWidgetVirtualSensorPanelBase")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorPlacementPanelWidget"),TEXT("/Script/ma0t10_dt.DxWidgetVirtualSensorPlacementPanel")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorSettingsPanelWidget"),TEXT("/Script/ma0t10_dt.DxWidgetVirtualSensorSettingsPanel")},
        {TEXT("/Script/ma0t10_dtEditor.ArchitectureMonitorProbe"),TEXT("/Script/ma0t10_dtEditor.DxWidgetArchitectureMonitorProbe")},
        {TEXT("/Script/ma0t10_dtEditor.DTCoreMainAcceptanceWidget"),TEXT("/Script/ma0t10_dtEditor.DxWidgetDTCoreMainAcceptance")},
    };
    for(const auto& Pair:Classes)
    {
        const auto Final=FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class,FCoreRedirectObjectName(Pair.Key));
        TestEqual(Pair.Key,Final.ToString(),FString(Pair.Value));
        TestNotNull(Pair.Value,LoadObject<UClass>(nullptr,Pair.Value));
        TestEqual(TEXT("class target is terminal"),FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class,FCoreRedirectObjectName(Pair.Value)).ToString(),FString(Pair.Value));
    }
    const TPair<const TCHAR*,const TCHAR*> Properties[]={
        {TEXT("/Script/ma0t10_dt.VirtualSensorCoordinator.SharedTransportComponent"),TEXT("/Script/ma0t10_dt.VirtualSensorCoordinator.SharedTransportComp")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorCoordinator.SharedRecorderComponent"),TEXT("/Script/ma0t10_dt.VirtualSensorCoordinator.SharedRecorderComp")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorCoordinator.StreamPublisherComponent"),TEXT("/Script/ma0t10_dt.VirtualSensorCoordinator.StreamPublisherComp")},
        {TEXT("/Script/ma0t10_dt.SlabActor.DataSyncComponent"),TEXT("/Script/ma0t10_dt.SlabActor.DataSyncComp")},
        {TEXT("/Script/ma0t10_dt.SlabActor.MotionComponent"),TEXT("/Script/ma0t10_dt.SlabActor.MotionComp")},
        {TEXT("/Script/ma0t10_dt.SlabActor.VisualizationComponent"),TEXT("/Script/ma0t10_dt.SlabActor.VisualizationComp")},
        {TEXT("/Script/ma0t10_dt.SlabActor.MetricsComponent"),TEXT("/Script/ma0t10_dt.SlabActor.MetricsComp")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorMonitorPanelWidget.RealSensorSourceComponent"),TEXT("/Script/ma0t10_dt.DxWidgetVirtualSensorMonitorPanel.RealSensorSourceComp")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorPanelWidgetBase.PanelHostComponent"),TEXT("/Script/ma0t10_dt.DxWidgetVirtualSensorPanelBase.PanelHostComp")},
        {TEXT("/Script/ma0t10_dt.VirtualSensorUiHostActor.PanelHostComponent"),TEXT("/Script/ma0t10_dt.VirtualSensorUiHostActor.PanelHostComp")},
        {TEXT("/Script/DeviceRuntime.VirtualCameraCaptureComponent.TransportComponent"),TEXT("/Script/DeviceRuntime.VirtualCameraCaptureComponent.TransportComp")},
        {TEXT("/Script/DeviceRuntime.VirtualCameraCaptureComponent.RecorderComponent"),TEXT("/Script/DeviceRuntime.VirtualCameraCaptureComponent.RecorderComp")},
        {TEXT("/Script/DeviceRuntime.VirtualSensorStreamPublisherComponent.TransportComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorStreamPublisherComp.TransportComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarAnalysisComponent.ScanComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarAnalysisComp.ScanComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarExportComponent.ScanComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarExportComp.ScanComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarScanComponent.TransportComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarScanComp.TransportComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarScanComponent.RecorderComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarScanComp.RecorderComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarSensorActor.ScanComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarSensorActor.ScanComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarSensorActor.AnalysisComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarSensorActor.AnalysisComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarSensorActor.VisualizationComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarSensorActor.VisualizationComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarSensorActor.ExportComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarSensorActor.ExportComp")},
        {TEXT("/Script/DeviceRuntime.VirtualLidarVisualizationComponent.ScanComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarVisualizationComp.ScanComp")},
        {TEXT("/Script/DeviceRuntime.VirtualSensorActorBase.OutputComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorActorBase.OutputComp")},
        {TEXT("/Script/DeviceRuntime.VirtualSensorOutputComponent.TransportComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorOutputComp.TransportComp")},
        {TEXT("/Script/DeviceRuntime.VirtualSensorOutputComponent.RecorderComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorOutputComp.RecorderComp")},
        {TEXT("/Script/DeviceRuntime.VirtualSensorOutputComponent.StreamPublisherComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorOutputComp.StreamPublisherComp")},
        {TEXT("/Script/DeviceRuntime.VirtualCameraSensorActor.CaptureComponent"),TEXT("/Script/DeviceRuntime.VirtualCameraSensorActor.CaptureComp")},
    };
    for(const auto& Pair:Properties)
    {
        TestEqual(Pair.Key,FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Property,FCoreRedirectObjectName(Pair.Key)).ToString(),FCoreRedirectObjectName(Pair.Value).ToString());
        FString Owner,Field;FString(Pair.Value).Split(TEXT("."),&Owner,&Field,ESearchCase::CaseSensitive,ESearchDir::FromEnd);
        auto* Class=LoadObject<UClass>(nullptr,*Owner);
        TestNotNull(Pair.Value,Class?FindFProperty<FProperty>(Class,*Field):nullptr);
    }
    return true;
}
#endif
