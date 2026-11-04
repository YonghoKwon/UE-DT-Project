#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "Device/Core/DeviceSensorAssetSettings.h"
#include "Device/Sensor/VirtualSensorActorBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceLegacyNativeTypeRedirects,"MA0T10.DevicePlugin.LegacyNativeTypeRedirects",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeviceLegacyNativeTypeRedirects::RunTest(const FString&)
{
    struct FEntry {const TCHAR* Old;const TCHAR* New;UClass* Kind;};
    const FEntry Entries[]={
    {TEXT("/Script/ma0t10_dt.EVirtualCameraOutputMode"),TEXT("/Script/DeviceRuntime.EVirtualCameraOutputMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualCameraCaptureMode"),TEXT("/Script/DeviceRuntime.EVirtualCameraCaptureMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualCameraCaptureComponent"),TEXT("/Script/DeviceRuntime.VirtualCameraCaptureComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualCameraGroundTruth"),TEXT("/Script/DeviceRuntime.VirtualCameraGroundTruthComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualCameraSensorActor"),TEXT("/Script/DeviceRuntime.VirtualCameraSensorActor"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorMotionProvider"),TEXT("/Script/DeviceRuntime.VirtualSensorMotionProvider"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorTopicReceiveKind"),TEXT("/Script/DeviceRuntime.EVirtualSensorTopicReceiveKind"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorTopicReceiverScope"),TEXT("/Script/DeviceRuntime.EVirtualSensorTopicReceiverScope"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorTopicReceiverState"),TEXT("/Script/DeviceRuntime.EVirtualSensorTopicReceiverState"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorTopicReceiverStatus"),TEXT("/Script/DeviceRuntime.VirtualSensorTopicReceiverStatus"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorTopicReceiveLogEntry"),TEXT("/Script/DeviceRuntime.VirtualSensorTopicReceiveLogEntry"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSlabSensorOutputSelection"),TEXT("/Script/DeviceRuntime.VirtualSlabSensorOutputSelection"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSlabFrameContext"),TEXT("/Script/DeviceRuntime.VirtualSlabFrameContext"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.DeviceSensorAssetSettings"),TEXT("/Script/DeviceRuntime.DeviceSensorAssetSettings"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorFileSaveSubsystem"),TEXT("/Script/DeviceRuntime.VirtualSensorFileSaveSubsystem"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorFileSaveMode"),TEXT("/Script/DeviceRuntime.EVirtualSensorFileSaveMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorFileSaveState"),TEXT("/Script/DeviceRuntime.EVirtualSensorFileSaveState"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorFileSaveStatus"),TEXT("/Script/DeviceRuntime.VirtualSensorFileSaveStatus"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorHighThroughputProfile"),TEXT("/Script/DeviceRuntime.VirtualSensorHighThroughputProfile"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorStreamTelemetry"),TEXT("/Script/DeviceRuntime.VirtualSensorStreamTelemetry"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorHighThroughputTransportSubsystem"),TEXT("/Script/DeviceRuntime.VirtualSensorHighThroughputTransportSubsystem"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorLayoutItem"),TEXT("/Script/DeviceRuntime.VirtualSensorLayoutItem"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorLayoutSubsystem"),TEXT("/Script/DeviceRuntime.VirtualSensorLayoutSubsystem"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorLayoutEntry"),TEXT("/Script/DeviceRuntime.VirtualSensorLayoutEntry"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorLayoutDocument"),TEXT("/Script/DeviceRuntime.VirtualSensorLayoutDocument"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorLayoutOperation"),TEXT("/Script/DeviceRuntime.EVirtualSensorLayoutOperation"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorLayoutOperationStatus"),TEXT("/Script/DeviceRuntime.VirtualSensorLayoutOperationStatus"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorPreflightPurpose"),TEXT("/Script/DeviceRuntime.EVirtualSensorPreflightPurpose"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorOutputPreflightStream"),TEXT("/Script/DeviceRuntime.VirtualSensorOutputPreflightStream"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorOutputPreflightResult"),TEXT("/Script/DeviceRuntime.VirtualSensorOutputPreflightResult"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorOutputPreflight"),TEXT("/Script/DeviceRuntime.VirtualSensorOutputPreflight"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorPeriodicSaveWaitReason"),TEXT("/Script/DeviceRuntime.EVirtualSensorPeriodicSaveWaitReason"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorPeriodicFileSaveStatus"),TEXT("/Script/DeviceRuntime.VirtualSensorPeriodicFileSaveStatus"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorPeriodicFileSaveSubsystem"),TEXT("/Script/DeviceRuntime.VirtualSensorPeriodicFileSaveSubsystem"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorPlacementSubsystem"),TEXT("/Script/DeviceRuntime.VirtualSensorPlacementSubsystem"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorPoseBookmark"),TEXT("/Script/DeviceRuntime.VirtualSensorPoseBookmark"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorPoseSaveState"),TEXT("/Script/DeviceRuntime.EVirtualSensorPoseSaveState"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorPoseSaveStatus"),TEXT("/Script/DeviceRuntime.VirtualSensorPoseSaveStatus"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorPlacementPreviewState"),TEXT("/Script/DeviceRuntime.EVirtualSensorPlacementPreviewState"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorPlacementPreviewStatus"),TEXT("/Script/DeviceRuntime.VirtualSensorPlacementPreviewStatus"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorPlacementSnapshot"),TEXT("/Script/DeviceRuntime.VirtualSensorPlacementSnapshot"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorRateState"),TEXT("/Script/DeviceRuntime.EVirtualSensorRateState"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorRateDiagnostic"),TEXT("/Script/DeviceRuntime.VirtualSensorRateDiagnostic"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorPerformanceTelemetry"),TEXT("/Script/DeviceRuntime.VirtualSensorPerformanceTelemetry"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorSchedulerSubsystem"),TEXT("/Script/DeviceRuntime.VirtualSensorSchedulerSubsystem"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorStreamPublisherComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorStreamPublisherComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.CameraJsonLiveSourceComponent"),TEXT("/Script/DeviceRuntime.CameraJsonLiveSourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.ECsvPointCloudPreviewRenderMode"),TEXT("/Script/DeviceRuntime.ECsvPointCloudPreviewRenderMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.CsvPointCloudPreviewActor"),TEXT("/Script/DeviceRuntime.CsvPointCloudPreviewActor"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.DeviceSensorCoordinator"),TEXT("/Script/DeviceRuntime.DeviceSensorCoordinator"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorViewMode"),TEXT("/Script/DeviceRuntime.EVirtualSensorViewMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorSummary"),TEXT("/Script/DeviceRuntime.VirtualSensorSummary"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorHealthSummary"),TEXT("/Script/DeviceRuntime.VirtualSensorHealthSummary"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorHiddenComponentState"),TEXT("/Script/DeviceRuntime.VirtualSensorHiddenComponentState"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.LidarCsvReplaySourceComponent"),TEXT("/Script/DeviceRuntime.LidarCsvReplaySourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.LidarHttpJsonLiveSourceComponent"),TEXT("/Script/DeviceRuntime.LidarHttpJsonLiveSourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.LidarJsonLinesReplaySourceComponent"),TEXT("/Script/DeviceRuntime.LidarJsonLinesReplaySourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.LidarJsonLiveSourceComponent"),TEXT("/Script/DeviceRuntime.LidarJsonLiveSourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.LidarPlayerDisplayComponent"),TEXT("/Script/DeviceRuntime.LidarPlayerDisplayComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.LidarUdpJsonLiveSourceComponent"),TEXT("/Script/DeviceRuntime.LidarUdpJsonLiveSourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.Ros2SensorBridgeSourceComponent"),TEXT("/Script/DeviceRuntime.Ros2SensorBridgeSourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.LivoxLidarSourceComponent"),TEXT("/Script/DeviceRuntime.LivoxLidarSourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.RealSenseCameraSourceComponent"),TEXT("/Script/DeviceRuntime.RealSenseCameraSourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.ERealSensorSourceKind"),TEXT("/Script/DeviceRuntime.ERealSensorSourceKind"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.ERealSensorSourceConnectionState"),TEXT("/Script/DeviceRuntime.ERealSensorSourceConnectionState"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.RealSensorSourceComponent"),TEXT("/Script/DeviceRuntime.RealSensorSourceComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualImuComponent"),TEXT("/Script/DeviceRuntime.VirtualImuComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualImuProfile"),TEXT("/Script/DeviceRuntime.EVirtualImuProfile"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualImuConfig"),TEXT("/Script/DeviceRuntime.VirtualImuConfig"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualImuSample"),TEXT("/Script/DeviceRuntime.VirtualImuSample"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarAnalysisComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarAnalysisComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarExportComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarExportComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarGpuDepthProjectionComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarGpuDepthProjectionComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarPayloadDescriptor"),TEXT("/Script/DeviceRuntime.VirtualLidarPayloadDescriptor"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarV2EncodeOptions"),TEXT("/Script/DeviceRuntime.VirtualLidarV2EncodeOptions"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarCompatibilityOutputPolicy"),TEXT("/Script/DeviceRuntime.EVirtualLidarCompatibilityOutputPolicy"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarCompatibilityOutputStatus"),TEXT("/Script/DeviceRuntime.VirtualLidarCompatibilityOutputStatus"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.ELidarPointCloudPreviewBackend"),TEXT("/Script/DeviceRuntime.ELidarPointCloudPreviewBackend"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarSemanticClassRule"),TEXT("/Script/DeviceRuntime.VirtualLidarSemanticClassRule"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarScanComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarScanComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarSensorActor"),TEXT("/Script/DeviceRuntime.VirtualLidarSensorActor"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarPreset"),TEXT("/Script/DeviceRuntime.EVirtualLidarPreset"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarViewMode"),TEXT("/Script/DeviceRuntime.EVirtualLidarViewMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.ELidarMonitorProjectionMode"),TEXT("/Script/DeviceRuntime.ELidarMonitorProjectionMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.ELidarColorMode"),TEXT("/Script/DeviceRuntime.ELidarColorMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.ELidarHeightReference"),TEXT("/Script/DeviceRuntime.ELidarHeightReference"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.ELidarPointCloudRenderPolicy"),TEXT("/Script/DeviceRuntime.ELidarPointCloudRenderPolicy"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.ELidarPointCloudRendererState"),TEXT("/Script/DeviceRuntime.ELidarPointCloudRendererState"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarPreviewPolicy"),TEXT("/Script/DeviceRuntime.VirtualLidarPreviewPolicy"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarRendererTelemetry"),TEXT("/Script/DeviceRuntime.VirtualLidarRendererTelemetry"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarVisualizationSettings"),TEXT("/Script/DeviceRuntime.VirtualLidarVisualizationSettings"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarEchoType"),TEXT("/Script/DeviceRuntime.EVirtualLidarEchoType"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarPointValidity"),TEXT("/Script/DeviceRuntime.EVirtualLidarPointValidity"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarTimeSyncState"),TEXT("/Script/DeviceRuntime.EVirtualLidarTimeSyncState"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarEchoSelectionPolicy"),TEXT("/Script/DeviceRuntime.EVirtualLidarEchoSelectionPolicy"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualPhysicalLidarPoint"),TEXT("/Script/DeviceRuntime.VirtualPhysicalLidarPoint"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarPoint"),TEXT("/Script/DeviceRuntime.VirtualLidarPoint"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualPhysicalLidarFrame"),TEXT("/Script/DeviceRuntime.VirtualPhysicalLidarFrame"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarSlabAnalysisResult"),TEXT("/Script/DeviceRuntime.VirtualLidarSlabAnalysisResult"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorRuntimeStatus"),TEXT("/Script/DeviceRuntime.VirtualSensorRuntimeStatus"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarOutputMode"),TEXT("/Script/DeviceRuntime.EVirtualLidarOutputMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarSurfaceResponseComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarSurfaceResponseComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarVisualizationComponent"),TEXT("/Script/DeviceRuntime.VirtualLidarVisualizationComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualPcdFieldProfile"),TEXT("/Script/DeviceRuntime.EVirtualPcdFieldProfile"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualPcdSendOptions"),TEXT("/Script/DeviceRuntime.VirtualPcdSendOptions"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorActorBase"),TEXT("/Script/DeviceRuntime.VirtualSensorActorBase"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualCameraDeviceProfile"),TEXT("/Script/DeviceRuntime.EVirtualCameraDeviceProfile"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarDeviceProfile"),TEXT("/Script/DeviceRuntime.EVirtualLidarDeviceProfile"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarProfileClass"),TEXT("/Script/DeviceRuntime.EVirtualLidarProfileClass"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorFidelityMode"),TEXT("/Script/DeviceRuntime.EVirtualSensorFidelityMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualLidarAcquisitionBackend"),TEXT("/Script/DeviceRuntime.EVirtualLidarAcquisitionBackend"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorSimulationQuality"),TEXT("/Script/DeviceRuntime.EVirtualSensorSimulationQuality"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorDeviceSpec"),TEXT("/Script/DeviceRuntime.VirtualSensorDeviceSpec"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarProfilePreset"),TEXT("/Script/DeviceRuntime.VirtualLidarProfilePreset"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorTargetKind"),TEXT("/Script/DeviceRuntime.EVirtualSensorTargetKind"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorExportKind"),TEXT("/Script/DeviceRuntime.EVirtualSensorExportKind"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorCaptureSelection"),TEXT("/Script/DeviceRuntime.VirtualSensorCaptureSelection"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorEditableState"),TEXT("/Script/DeviceRuntime.VirtualSensorEditableState"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorExportResult"),TEXT("/Script/DeviceRuntime.VirtualSensorExportResult"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorExternalSourceHostActor"),TEXT("/Script/DeviceRuntime.VirtualSensorExternalSourceHostActor"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorMonitorView"),TEXT("/Script/DeviceRuntime.VirtualSensorMonitorView"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorOutputComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorOutputComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorPlacementInputOwner"),TEXT("/Script/DeviceRuntime.VirtualSensorPlacementInputOwner"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorRecordedFrame"),TEXT("/Script/DeviceRuntime.VirtualSensorRecordedFrame"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorRecorderComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorRecorderComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorKind"),TEXT("/Script/DeviceRuntime.EVirtualSensorKind"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorStreamKind"),TEXT("/Script/DeviceRuntime.EVirtualSensorStreamKind"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorStreamTransportBackend"),TEXT("/Script/DeviceRuntime.EVirtualSensorStreamTransportBackend"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualPointCloudStreamFormat"),TEXT("/Script/DeviceRuntime.EVirtualPointCloudStreamFormat"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualPcdDataMode"),TEXT("/Script/DeviceRuntime.EVirtualPcdDataMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualPointCloudDeliveryMode"),TEXT("/Script/DeviceRuntime.EVirtualPointCloudDeliveryMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualPointCloudFilterConfig"),TEXT("/Script/DeviceRuntime.VirtualPointCloudFilterConfig"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualPointCloudBinaryMetadata"),TEXT("/Script/DeviceRuntime.VirtualPointCloudBinaryMetadata"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualCameraJpegMetadata"),TEXT("/Script/DeviceRuntime.VirtualCameraJpegMetadata"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarTelemetryFrame"),TEXT("/Script/DeviceRuntime.VirtualLidarTelemetryFrame"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorStreamConfig"),TEXT("/Script/DeviceRuntime.VirtualSensorStreamConfig"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorStreamStatus"),TEXT("/Script/DeviceRuntime.VirtualSensorStreamStatus"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorTransportLogEntry"),TEXT("/Script/DeviceRuntime.VirtualSensorTransportLogEntry"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorInteractionRequest"),TEXT("/Script/DeviceRuntime.VirtualSensorInteractionRequest"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorTransportMode"),TEXT("/Script/DeviceRuntime.EVirtualSensorTransportMode"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorTransportProfile"),TEXT("/Script/DeviceRuntime.VirtualSensorTransportProfile"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorTransportResult"),TEXT("/Script/DeviceRuntime.VirtualSensorTransportResult"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorTransportComponent"),TEXT("/Script/DeviceRuntime.VirtualSensorTransportComp"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.LidarJsonLiveFrameTC"),TEXT("/Script/DeviceRuntime.LidarJsonLiveFrameTC"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualCameraStreamReceiverTC"),TEXT("/Script/DeviceRuntime.VirtualCameraStreamReceiverTC"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualLidarStreamReceiverTC"),TEXT("/Script/DeviceRuntime.VirtualLidarStreamReceiverTC"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualPointCloudStreamReceiverTC"),TEXT("/Script/DeviceRuntime.VirtualPointCloudStreamReceiverTC"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.DeviceSensorPanelBase"),TEXT("/Script/DeviceUI.DxDeviceSensorPanelBase"),UClass::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorPanelPlacement"),TEXT("/Script/DeviceUI.EVirtualSensorPanelPlacement"),UEnum::StaticClass()},
    {TEXT("/Script/ma0t10_dt.VirtualSensorSettingHelpDescriptor"),TEXT("/Script/DeviceUI.VirtualSensorSettingHelpDescriptor"),UScriptStruct::StaticClass()},
    {TEXT("/Script/ma0t10_dt.EVirtualSensorCaptureExportTab"),TEXT("/Script/DeviceUI.EVirtualSensorCaptureExportTab"),UEnum::StaticClass()},
    };
    for(const auto& Entry:Entries)
    {
        UObject* Current=StaticLoadObject(Entry.Kind,nullptr,Entry.New);
        const ECoreRedirectFlags Flag=Entry.Kind==UClass::StaticClass()?ECoreRedirectFlags::Type_Class:Entry.Kind==UScriptStruct::StaticClass()?ECoreRedirectFlags::Type_Struct:ECoreRedirectFlags::Type_Enum;
        const FString Redirected=FCoreRedirects::GetRedirectedName(Flag,FCoreRedirectObjectName(Entry.Old)).ToString();
        TestEqual(Entry.Old,Redirected,FString(Entry.New));
        UObject* Legacy=StaticLoadObject(Entry.Kind,nullptr,*Redirected);
        TestNotNull(Entry.New,Current);
        TestTrue(Entry.Old,Legacy&&Legacy==Current);
        if(Current)TestEqual(TEXT("canonical owner package"),Current->GetPathName(),FString(Entry.New));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceLegacyContentReadOnly,"MA0T10.DevicePlugin.LegacyContentReadOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeviceLegacyContentReadOnly::RunTest(const FString&)
{
    const FString MapFile=FPaths::ProjectContentDir()/TEXT("MA0T10/Maps/SensorTestMap.umap");
    const FMD5Hash Before=FMD5Hash::HashFile(*MapFile);
    UPackage* Package=LoadPackage(nullptr,TEXT("/Game/MA0T10/Maps/SensorTestMap"),LOAD_None);
    UWorld* Loaded=Package?UWorld::FindWorldInPackage(Package):nullptr;
    TestNotNull(TEXT("old committed map loads without resave"),Loaded);
    int32 Sensors=0;
    if(Loaded&&Loaded->PersistentLevel)
        for(AActor* Actor:Loaded->PersistentLevel->Actors)if(IsValid(Actor)&&Actor->IsA<AVirtualSensorActorBase>())++Sensors;
    TestTrue(TEXT("old map contains native sensor classes"),Sensors>=2);
    UDataTable* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/MA0T10/Common/DataTables/DT_TransactionCode.DT_TransactionCode"));
    TestNotNull(TEXT("old host transaction DataTable loads"),Table);
    if(Table)TestTrue(TEXT("business rows remain present"),Table->GetRowMap().Num()>0);
    TestTrue(TEXT("read-only map hash unchanged"),FMD5Hash::HashFile(*MapFile)==Before);
    return true;
}
#endif
