# NAMING-01 변경 대상 목록 (자동 생성 + 검토)

생성일: 2026-11-03. 기준 커밋: host master `7aefff54`(#99, #100 머지 후), DTDevice `a624895`, DTCore `bc60f05`.
분석은 읽기만 했고 빌드는 하지 않았습니다. 다시 만들 때는 아래 명령을 씁니다.

```bash
python -X utf8 -I Tools/NamingPlan/inventory.py <host> <host>/Plugins/DTDevice <host>/Plugins/DTCore inventory.json
python -X utf8 -I Tools/NamingPlan/movemap.py <host>
```

규칙은 `NAMING_01_RULES.md`를 따릅니다.

## 1. 요약

| 항목 | 수 |
|---|---|
| 분석한 reflected 타입 (UCLASS/USTRUCT/UENUM, 3개 저장소) | 341 |
| ActorComponent 계열 클래스 | 31 (DTDevice 22, 과제 9) |
| 이름 변경 대상 (`…Component` → `…Comp`, 접미사 없음 → `Comp`) | 28 |
| 이미 `Comp`라 변경 없음 | 3 (Crane 3종) |
| Dx 클래스 상속 | 24 (6개는 이미 Dx 형태, 과제 위젯 16개 → `UDxWidget<기능>`, DTDevice 2개 → `UDxDevice…`) |
| `UI/`, `Scenario/` 아래 옮길 파일 | 206 |
| 경로 include(`ma0t10_dt/MA0T10/(Scenario|UI)/…`) 갱신 | 628곳 / 220개 파일 |

**참조 에셋 수**는 `Content/` 아래 `.uasset/.umap` 바이너리에서 클래스 이름 문자열을 찾은 개수입니다. 이 에셋들은 다시 저장하지 않고 redirect로 처리합니다.
**기존 redirect 갱신**은 `DefaultEngine.ini`에서 NewName이 이 클래스인 항목 수입니다. CoreRedirects는 한 번만 적용되므로, 이 항목들도 새 이름으로 직접 고쳐야 합니다.

## 2. 클래스 이름

### 2.1 ActorComponent 이름 변경 (`Comp`)

| 소유 | 현재 | 변경 후 | 상속 | 참조 에셋 수 | 기존 redirect 갱신 |
|---|---|---|---|---|---|
| DTDevice | `UVirtualCameraGroundTruth` | `UVirtualCameraGroundTruthComp` | UActorComponent | 0 | 1 |
| DTDevice | `UVirtualSensorStreamPublisherComponent` | `UVirtualSensorStreamPublisherComp` | UActorComponent | 5 | 1 |
| DTDevice | `UCameraJsonLiveSourceComponent` | `UCameraJsonLiveSourceComp` | URealSensorSourceComponent > USceneComponent | 2 | 2 |
| DTDevice | `ULidarCsvReplaySourceComponent` | `ULidarCsvReplaySourceComp` | URealSensorSourceComponent > USceneComponent | 2 | 2 |
| DTDevice | `ULidarHttpJsonLiveSourceComponent` | `ULidarHttpJsonLiveSourceComp` | ULidarJsonLiveSourceComponent > URealSensorSourceComponent | 2 | 2 |
| DTDevice | `ULidarJsonLinesReplaySourceComponent` | `ULidarJsonLinesReplaySourceComp` | URealSensorSourceComponent > USceneComponent | 2 | 2 |
| DTDevice | `ULidarJsonLiveSourceComponent` | `ULidarJsonLiveSourceComp` | URealSensorSourceComponent > USceneComponent | 2 | 2 |
| DTDevice | `ULidarPlayerDisplayComponent` | `ULidarPlayerDisplayComp` | ULineBatchComponent | 0 | 1 |
| DTDevice | `ULidarUdpJsonLiveSourceComponent` | `ULidarUdpJsonLiveSourceComp` | ULidarJsonLiveSourceComponent > URealSensorSourceComponent | 2 | 2 |
| DTDevice | `URos2SensorBridgeSourceComponent` | `URos2SensorBridgeSourceComp` | URealSensorSourceComponent > USceneComponent | 0 | 2 |
| DTDevice | `ULivoxLidarSourceComponent` | `ULivoxLidarSourceComp` | URealSensorSourceComponent > USceneComponent | 0 | 2 |
| DTDevice | `URealSenseCameraSourceComponent` | `URealSenseCameraSourceComp` | URealSensorSourceComponent > USceneComponent | 0 | 2 |
| DTDevice | `URealSensorSourceComponent` | `URealSensorSourceComp` | USceneComponent | 0 | 2 |
| DTDevice | `UVirtualImuComponent` | `UVirtualImuComp` | USceneComponent | 0 | 1 |
| DTDevice | `UVirtualLidarAnalysisComponent` | `UVirtualLidarAnalysisComp` | UActorComponent | 6 | 1 |
| DTDevice | `UVirtualLidarExportComponent` | `UVirtualLidarExportComp` | UActorComponent | 6 | 1 |
| DTDevice | `UVirtualLidarScanComponent` | `UVirtualLidarScanComp` | USceneComponent | 6 | 2 |
| DTDevice | `UVirtualLidarSurfaceResponseComponent` | `UVirtualLidarSurfaceResponseComp` | UActorComponent | 0 | 1 |
| DTDevice | `UVirtualLidarVisualizationComponent` | `UVirtualLidarVisualizationComp` | UStatusVisualizerCompBase > UActorComponent | 6 | 1 |
| DTDevice | `UVirtualSensorOutputComponent` | `UVirtualSensorOutputComp` | UActorComponent | 6 | 1 |
| DTDevice | `UVirtualSensorRecorderComponent` | `UVirtualSensorRecorderComp` | UActorComponent | 6 | 2 |
| DTDevice | `UVirtualSensorTransportComponent` | `UVirtualSensorTransportComp` | UActorComponent | 6 | 2 |
| Project | `UCraneDataSyncComp` | 변경 없음 | UDataSyncCompBase > UActorComponent | 0 | 0 |
| Project | `UCraneMechDriverComp` | 변경 없음 | UMechDriverCompBase > UActorComponent | 1 | 0 |
| Project | `UCraneStatusVisualizerComp` | 변경 없음 | UStatusVisualizerCompBase > UActorComponent | 1 | 0 |
| Project | `USlabDataSyncComponent` | `USlabDataSyncComp` | UDataSyncCompBase > UActorComponent | 2 | 0 |
| Project | `USlabMetricsComponent` | `USlabMetricsComp` | UActorComponent | 2 | 0 |
| Project | `USlabMotionComponent` | `USlabMotionComp` | UMechDriverCompBase > UActorComponent | 2 | 0 |
| Project | `USlabVisualizationComponent` | `USlabVisualizationComp` | UStatusVisualizerCompBase > UActorComponent | 2 | 0 |
| Project | `UShipHatchOverlay` | `UShipHatchOverlayComp` | ULineBatchComponent | 0 | 0 |
| Project | `UVirtualSensorPanelHostComponent` | `UVirtualSensorPanelHostComp` | UActorComponent | 4 | 0 |

### 2.2 Dx 클래스 상속 (결정 2026-11-04: 과제 = `UDxWidget<기능>`, DTDevice = `UDxDevice…`)

| 소유 | 현재 | Dx 부모 | 변경 후 | 참조 에셋 수 |
|---|---|---|---|---|
| DTDevice | `UDeviceSensorMonitorPanel` | `UDxWidget` | `UDxDeviceSensorMonitorPanel` | 0 |
| DTDevice | `UDeviceSensorPanelBase` | `UDxWidget` | `UDxDeviceSensorPanelBase` | 0 |
| Project | `ADxLevelManagerBasic` | `ADxLevelManagerBase` | 이미 규칙에 맞음 | 2 |
| Project | `ADxLevelManagerTest` | `ADxLevelManagerBase` | 이미 규칙에 맞음 | 1 |
| Project | `ADxPlayerControllerTest` | `ADxPlayerControllerBase` | 이미 규칙에 맞음 | 1 |
| Project | `ADxPlayerTest` | `ADxPlayerBase` | 이미 규칙에 맞음 | 1 |
| Project | `USensorWorkspaceForeignFixture` | `UDxWidget` | `UDxWidgetSensorWorkspaceForeignFixture` | 0 |
| Project | `UAgentTestSetWidget` | `UDxWidget` | `UDxWidgetAgentTestSet` | 0 |
| Project | `UCoilLoadingMonitor` | `UDxWidget` | `UDxWidgetCoilLoadingMonitor` | 0 |
| Project | `UDxM7atMainBasic` | `UDxWidget` | 이미 규칙에 맞음 | 1 |
| Project | `UDxM7atMainTest` | `UDxWidget` | 이미 규칙에 맞음 | 2 |
| Project | `USensorMapCheckWidget` | `UDxWidget` | `UDxWidgetSensorMapCheck` | 0 |
| Project | `UShipHatchMonitorWidget` | `UDxWidget` | `UDxWidgetShipHatchMonitor` | 0 |
| Project | `USlabChartsPanelWidget` | `UDxWidget` | `UDxWidgetSlabChartsPanel` | 1 |
| Project | `USlabProgressPanelWidget` | `UDxWidget` | `UDxWidgetSlabProgressPanel` | 1 |
| Project | `USlabScenarioReplayPanelWidget` | `UDxWidget` | `UDxWidgetSlabScenarioReplayPanel` | 1 |
| Project | `UTrainBatchWidget` | `UDxWidget` | `UDxWidgetTrainBatch` | 0 |
| Project | `UVirtualSensorCaptureExportPanelWidget` | `UDxWidget` | `UDxWidgetVirtualSensorCaptureExportPanel` | 1 |
| Project | `UVirtualSensorMonitorPanelWidget` | `UDxWidget` | `UDxWidgetVirtualSensorMonitorPanel` | 2 |
| Project | `UVirtualSensorPanelWidgetBase` | `UDxWidget` | `UDxWidgetVirtualSensorPanelBase` | 0 |
| Project | `UVirtualSensorPlacementPanelWidget` | `UDxWidget` | `UDxWidgetVirtualSensorPlacementPanel` | 1 |
| Project | `UVirtualSensorSettingsPanelWidget` | `UDxWidget` | `UDxWidgetVirtualSensorSettingsPanel` | 1 |
| Project | `UArchitectureMonitorProbe` | `UDxWidget` | `UDxWidgetArchitectureMonitorProbe` | 0 |
| Project | `UDTCoreMainAcceptanceWidget` | `UDxWidget` | `UDxWidgetDTCoreMainAcceptance` | 0 |

## 3. 파일 이동 (`UI/`, `Scenario/`)

| 현재 | 제안 위치 | 근거 |
|---|---|---|
| `UI/AgentTestSetWidget.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/AgentTestSetWidget.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/CoilLoadingMonitor.cpp` | `UI/Main/Coil/` | CoilLoadingTestMap 메인 고정 패널 |
| `UI/CoilLoadingMonitor.h` | `UI/Main/Coil/` | CoilLoadingTestMap 메인 고정 패널 |
| `UI/CoilLoadingMonitorHost.cpp` | `UI/Main/Coil/` | CoilLoadingTestMap 메인 고정 패널 |
| `UI/CoilLoadingPlanProjection.cpp` | `UI/Main/Coil/` | CoilLoadingTestMap 메인 고정 패널 |
| `UI/CoilLoadingPlanProjection.h` | `UI/Main/Coil/` | CoilLoadingTestMap 메인 고정 패널 |
| `UI/CoilLoadingPlanView.cpp` | `UI/Main/Coil/` | CoilLoadingTestMap 메인 고정 패널 |
| `UI/CoilLoadingPresentation.h` | `UI/Main/Coil/` | CoilLoadingTestMap 메인 고정 패널 |
| `UI/HatchMemoryExperimentDiagnostics.cpp` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/HatchMemoryExperimentDiagnostics.h` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/HatchMemoryRuntimeProbe.cpp` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/HatchMemoryRuntimeProbe.h` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/LidarMapMemoryRuntimeProbe.cpp` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/LidarMapMemoryRuntimeProbe.h` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/LidarMemoryAttributionProbe.cpp` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/LidarMemoryAttributionProbe.h` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/PcdTransportRuntimeProbe.cpp` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/PcdTransportRuntimeProbe.h` | `Diagnostics/` | UI가 아닌 측정 probe |
| `UI/SensorMapCheckWidget.cpp` | `UI/Topview/` | 닫을 수 있는 고정 패널(맵 점검·학습) |
| `UI/SensorMapCheckWidget.h` | `UI/Topview/` | 닫을 수 있는 고정 패널(맵 점검·학습) |
| `UI/SensorPlacementInteraction.cpp` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/SensorPlacementInteraction.h` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/SensorToolAppearance.cpp` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/SensorToolAppearance.h` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/SensorToolToolbarWidget.Guide.cpp` | `UI/Main/Sensor/` | 센서 도구 기능의 메인 고정 패널(사이드바·툴바·상태바) |
| `UI/SensorToolToolbarWidget.Sidebar.cpp` | `UI/Main/Sensor/` | 센서 도구 기능의 메인 고정 패널(사이드바·툴바·상태바) |
| `UI/SensorToolToolbarWidget.Status.cpp` | `UI/Main/Sensor/` | 센서 도구 기능의 메인 고정 패널(사이드바·툴바·상태바) |
| `UI/SensorToolToolbarWidget.cpp` | `UI/Main/Sensor/` | 센서 도구 기능의 메인 고정 패널(사이드바·툴바·상태바) |
| `UI/SensorToolToolbarWidget.h` | `UI/Main/Sensor/` | 센서 도구 기능의 메인 고정 패널(사이드바·툴바·상태바) |
| `UI/SensorToolUiText.h` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/ShipHatchAnalysisView.cpp` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchFeedback.cpp` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchLabelPresentation.h` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchMetricPresentation.h` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchMonitorHost.cpp` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchMonitorHost.h` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchMonitorWidget.cpp` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchMonitorWidget.h` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchOverlay.cpp` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchOverlay.h` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchSensorLabelView.cpp` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchSiteVisualization.cpp` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/ShipHatchViewTransform.h` | `UI/Main/Ship/` | ShipHatchTestMap 메인 고정 패널 |
| `UI/SlabChartsPanelWidget.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/SlabChartsPanelWidget.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/SlabInformationCard.cpp` | `UI/Main/Slab/` | Slab 검증맵 메인 고정 패널 |
| `UI/SlabInformationCard.h` | `UI/Main/Slab/` | Slab 검증맵 메인 고정 패널 |
| `UI/SlabProgressPanelWidget.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/SlabProgressPanelWidget.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/SlabScenarioChartWidget.cpp` | `UI/Main/Slab/` | Slab 검증맵 메인 고정 패널 |
| `UI/SlabScenarioChartWidget.h` | `UI/Main/Slab/` | Slab 검증맵 메인 고정 패널 |
| `UI/SlabScenarioReplayPanelWidget.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/SlabScenarioReplayPanelWidget.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/SlabScenarioReplayUiHostActor.cpp` | `UI/Main/Slab/` | Slab 검증맵 메인 고정 패널 |
| `UI/SlabScenarioReplayUiHostActor.h` | `UI/Main/Slab/` | Slab 검증맵 메인 고정 패널 |
| `UI/SlabSimulationUiHostActor.cpp` | `UI/Main/Slab/` | Slab 검증맵 메인 고정 패널 |
| `UI/SlabSimulationUiHostActor.h` | `UI/Main/Slab/` | Slab 검증맵 메인 고정 패널 |
| `UI/TrainBatchWidget.cpp` | `UI/Topview/` | 닫을 수 있는 고정 패널(맵 점검·학습) |
| `UI/TrainBatchWidget.h` | `UI/Topview/` | 닫을 수 있는 고정 패널(맵 점검·학습) |
| `UI/VirtualSensorCaptureExportPanelWidget.Bandwidth.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorCaptureExportPanelWidget.Dataset.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorCaptureExportPanelWidget.Owned.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorCaptureExportPanelWidget.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorCaptureExportPanelWidget.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorControlTypes.h` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorCoordinatorMonitorBinding.cpp` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorMonitorPanelWidget.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorMonitorPanelWidget.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorMonitorPanelWidgetCapture.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorMonitorPanelWidgetConstruction.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorMonitorPanelWidgetPreview.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorMonitorPanelWidgetStatus.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorPanelHostComponent.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorPanelHostComponent.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorPanelWidgetBase.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorPanelWidgetBase.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorPlacementPanelWidget.Imu.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorPlacementPanelWidget.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorPlacementPanelWidget.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorPlayerInputBinding.cpp` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorSettingsPanelWidget.cpp` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorSettingsPanelWidget.h` | `UI/Freeview/` | 떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열 |
| `UI/VirtualSensorToolRig.cpp` | `Sensor/Tool/` | UI가 아닌 배치 도구 actor |
| `UI/VirtualSensorToolRig.h` | `Sensor/Tool/` | UI가 아닌 배치 도구 actor |
| `UI/VirtualSensorToolWorkspaceSubsystem.Docking.cpp` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorToolWorkspaceSubsystem.Input.cpp` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorToolWorkspaceSubsystem.cpp` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorToolWorkspaceSubsystem.h` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorTransformGizmoActor.cpp` | `Sensor/Tool/` | UI가 아닌 배치 도구 actor |
| `UI/VirtualSensorTransformGizmoActor.h` | `Sensor/Tool/` | UI가 아닌 배치 도구 actor |
| `UI/VirtualSensorUiHostActor.cpp` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorUiHostActor.h` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorUiPreferences.cpp` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/VirtualSensorUiPreferences.h` | `UI/Common/` | 여러 위젯이 공유하는 상태·문구·입력·호스트 |
| `UI/Main/DxM7atMainBasic.cpp` | `UI/Main/` | 기존 Main 유지(특정 맵 메인 위젯) |
| `UI/Main/DxM7atMainBasic.h` | `UI/Main/` | 기존 Main 유지(특정 맵 메인 위젯) |
| `UI/Main/DxM7atMainTest.cpp` | `UI/Main/` | 기존 Main 유지(특정 맵 메인 위젯) |
| `UI/Main/DxM7atMainTest.h` | `UI/Main/` | 기존 Main 유지(특정 맵 메인 위젯) |
| `UI/Tests/LidarNativeSafetyTextTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/SensorAcquisitionRateWarningTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/SensorPresentationTestHelpers.h` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/SensorPreviewPresentationTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/SensorSettingsPresentationTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/SensorToolAppearanceTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/SensorV2UiAutomationTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/SensorVocabularyTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/SlabInformationCardTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/SlabScenarioScreenTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/TrainBatchPartialDeliveryTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/UxrP1PresentationTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/UxrP2PresentationTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/UxrP2bPresentationTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/UxrSlabCapabilityTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/VirtualSensorControlAutomationTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `UI/Tests/VirtualSensorMonitorHostAutomationTests.cpp` | `UI/Tests/` | 테스트는 위치 유지 |
| `Scenario/AgentTestSetExamples.cpp` | `Agent/` | AI Agent 테스트 세트 |
| `Scenario/AgentTestSetExamples.h` | `Agent/` | AI Agent 테스트 세트 |
| `Scenario/AgentTestSetPcdPreview.cpp` | `Agent/` | AI Agent 테스트 세트 |
| `Scenario/AgentTestSetPcdPreview.h` | `Agent/` | AI Agent 테스트 세트 |
| `Scenario/AgentTestSetSubsystem.cpp` | `Agent/` | AI Agent 테스트 세트 |
| `Scenario/AgentTestSetSubsystem.h` | `Agent/` | AI Agent 테스트 세트 |
| `Scenario/AgentTestSetTrigger.cpp` | `Agent/` | AI Agent 테스트 세트 |
| `Scenario/AgentTestSetTrigger.h` | `Agent/` | AI Agent 테스트 세트 |
| `Scenario/CoilLoadingAppearance.cpp` | `Coil/` | 코일 적재 모델·시뮬레이션 |
| `Scenario/CoilLoadingGeometry.cpp` | `Coil/` | 코일 적재 모델·시뮬레이션 |
| `Scenario/CoilLoadingMeasurement.cpp` | `Coil/` | 코일 적재 모델·시뮬레이션 |
| `Scenario/CoilLoadingModel.cpp` | `Coil/` | 코일 적재 모델·시뮬레이션 |
| `Scenario/CoilLoadingModel.h` | `Coil/` | 코일 적재 모델·시뮬레이션 |
| `Scenario/CoilLoadingQuayLayout.h` | `Coil/` | 코일 적재 모델·시뮬레이션 |
| `Scenario/CoilLoadingQuayPresentation.cpp` | `Coil/` | 코일 적재 모델·시뮬레이션 |
| `Scenario/CoilLoadingSimulation.cpp` | `Coil/` | 코일 적재 모델·시뮬레이션 |
| `Scenario/CoilLoadingSimulation.h` | `Coil/` | 코일 적재 모델·시뮬레이션 |
| `Scenario/ProjectDeviceAnalysisPolicy.cpp` | `Common/` | 플러그인↔과제 연결 정책 |
| `Scenario/ProjectDeviceAnalysisPolicy.h` | `Common/` | 플러그인↔과제 연결 정책 |
| `Scenario/ProjectDeviceSemanticPolicy.cpp` | `Common/` | 플러그인↔과제 연결 정책 |
| `Scenario/ProjectDeviceSemanticPolicy.h` | `Common/` | 플러그인↔과제 연결 정책 |
| `Scenario/ProjectRunDeliveryAdapter.h` | `Common/` | 플러그인↔과제 연결 정책 |
| `Scenario/ShipHatchCab.cpp` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchGrab.cpp` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchIndustrialDetails.cpp` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchModel.cpp` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchModel.h` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchRayQuery.cpp` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchSensorPresentation.cpp` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchSimulation.cpp` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchSimulation.h` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchUnloader.cpp` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/ShipHatchVessel.cpp` | `Ship/` | 해치 모델·시뮬레이션 |
| `Scenario/SlabExecutionTypes.h` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/SlabRunComparison.cpp` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/SlabRunComparison.h` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/SlabRunDeliveryTypes.h` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/SlabRunResultsSubsystem.cpp` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/SlabRunResultsSubsystem.h` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/SlabScenarioReplaySubsystem.cpp` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/SlabScenarioReplaySubsystem.h` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/SlabSurfaceAppearanceRecord.h` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/VirtualDatasetSubsystem.cpp` | `Dataset/` | 가상 데이터셋 |
| `Scenario/VirtualDatasetSubsystem.h` | `Dataset/` | 가상 데이터셋 |
| `Scenario/VirtualDatasetTypes.cpp` | `Dataset/` | 가상 데이터셋 |
| `Scenario/VirtualDatasetTypes.h` | `Dataset/` | 가상 데이터셋 |
| `Scenario/VirtualSensorSlabAcquisitionProvider.cpp` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/VirtualSensorSlabContextSubsystem.cpp` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/VirtualSensorSlabContextSubsystem.h` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/VirtualSlabFrameContext.h` | `Slab/Scenario/` | 기존 Slab 폴더에 합침 |
| `Scenario/Automation/AutomationControlSubsystem.cpp` | `Manager/Automation/` | 자동화 제어 서버·프로토콜 |
| `Scenario/Automation/AutomationControlSubsystem.h` | `Manager/Automation/` | 자동화 제어 서버·프로토콜 |
| `Scenario/Automation/AutomationLoopbackServer.cpp` | `Manager/Automation/` | 자동화 제어 서버·프로토콜 |
| `Scenario/Automation/AutomationLoopbackServer.h` | `Manager/Automation/` | 자동화 제어 서버·프로토콜 |
| `Scenario/Automation/AutomationProtocol.cpp` | `Manager/Automation/` | 자동화 제어 서버·프로토콜 |
| `Scenario/Automation/AutomationProtocol.h` | `Manager/Automation/` | 자동화 제어 서버·프로토콜 |
| `Scenario/Tests/AutomationControlTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/AutomationOwnershipTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/CoilLoadingModelTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/ShipHatchModelTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/SlabAcquisitionProviderTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/TrainBatchCoreTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/TrainBatchIoTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/TrainBatchProcessTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/TrainBatchRuntimeTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/TrainBatchServiceTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/TrainBatchWorkerSafetyTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Tests/VirtualDatasetTests.cpp` | `(테스트 대상 폴더의 Tests/)` | 대상 파일과 같은 분류로 분산 |
| `Scenario/Training/TrainBatchArchive.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchArchive.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchCodec.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchCodec.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchCoordinatorProbe.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchCoordinatorProbe.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchDelivery.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchDelivery.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchProcessPool.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchProcessPool.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchProcessProtocol.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchProcessProtocol.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchSftp.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchSftp.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchStorage.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchStorage.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchSubsystem.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchSubsystem.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchWorkerSafety.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchWorkerSafety.h` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchWorkerSubsystem.cpp` | `Training/` | 대량 학습 |
| `Scenario/Training/TrainBatchWorkerSubsystem.h` | `Training/` | 대량 학습 |

합계 206개 파일: UI/Freeview/ 28, Training/ 22, UI/Tests/ 17, UI/Common/ 16, UI/Main/Ship/ 13, Slab/Scenario/ 13, (테스트 대상 폴더의 Tests/) 12, Ship/ 11, Diagnostics/ 10, Coil/ 9, UI/Main/Slab/ 8, Agent/ 8, UI/Main/Coil/ 7, Manager/Automation/ 6, UI/Main/Sensor/ 5, Common/ 5, UI/Topview/ 4, Sensor/Tool/ 4, UI/Main/ 4, Dataset/ 4

## 4. DTDevice 폴더

- `Public/Device/<분류>`는 유지를 권장합니다. 이유는 include 이름공간 충돌 방지입니다(`NAMING_01_RULES.md` 1절).
- 파일 이름만 2절의 클래스 이름 변경에 맞춰 바꿉니다. 예: `Device/Sensor/VirtualLidarScanComponent.h` → `Device/Sensor/VirtualLidarScanComp.h`.
- 이 변경은 DTDevice 저장소 커밋과 pin 갱신이 필요합니다. 과제 쪽 include(`Device/…`) 535곳 중 이름이 바뀐 헤더를 쓰는 곳만 갱신합니다.

## 5. 작업 순서(안)

8-1 안에서 단계마다 빌드합니다.

1. 2.1 컴포넌트 이름 변경
   - DTDevice → 과제 순서로 진행합니다.
   - CoreRedirects를 추가하고, 기존 항목도 새 이름으로 갱신합니다.
   - 컴포넌트 멤버 변수 이름(`ScanComponent` 등)이 Blueprint에 저장돼 있으면 PropertyRedirects를 넣습니다.
2. 2.2 Dx 규칙 적용 (사용자 결정 후)
3. 3절 파일 이동
   - `git mv`로 옮기고 include를 스크립트로 일괄 갱신합니다.
   - 클래스 이름은 바뀌지 않으므로 redirect가 필요 없습니다.
4. 빌드 → release → 맵 로드 경고 0건 → 성능 ABAB 확인

## 6. 위험과 확인 사항

- 다음 문자열은 바꾸지 않습니다: SaveGame, layout JSON, 서버 payload에 클래스 이름을 문자열로 저장하는 곳.
  - 8-1 시작 전에 `GetClass()->GetName()`/`GetPathName()`을 쓰는 직렬화 코드를 다시 검사합니다.
- 테스트 이름 `MA0T10.*`는 그대로 둡니다. 측정·검증 스크립트가 이 이름으로 필터링합니다.
- `UShipHatchOverlay`, `UVirtualCameraGroundTruth`는 `ULineBatchComponent`/`UActorComponent`를 상속하지만 이름에 접미사가 없습니다. 규칙대로 `…Comp`를 붙이는 것을 제안합니다.

## 7. 실행 결과 (r52, 2026-11-04)

이 표는 이동 전 경로/이름을 고정한 계획 근거로 유지한다. 실제206개 항목에는 위치 유지21개가 포함된다. 타입 파일명까지 합쳐 host196개, DTDevice49개 경로를 git mv했다. 46개 reflected 타입과45개 컴포넌트 속성의 직접 alias를 확인했다. 테스트530개 경로 누락0, 호환 테스트1개 추가. 세부/검증은 `Saved/Reports/NAMING-01/RESULT.md`.
