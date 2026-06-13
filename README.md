# UE-DT-Project

Unreal Engine 5.3 기반 Digital Twin 프로젝트입니다. 철강 제조 환경을 대상으로 가상 센서와 가상 카메라를 배치하고, 시뮬레이션 또는 실제 센서 입력을 Unreal 환경에 재현한 뒤 판단 서버로 측정 데이터를 전달하는 것을 목표로 합니다.

현재 브랜치는 가상 LiDAR/카메라, point-cloud-only view, Slab 각도 분석 v1, CSV/JSONL point cloud replay source, native monitor fallback UI에 초점을 둡니다. Livox, RealSense, ROS2 직접 입력은 `URealSensorSourceComp` 기반 placeholder까지 준비되어 있으며 실제 SDK/bridge 연동은 후속 작업입니다.

## 목표 기능

1. 시뮬레이션 데이터를 Unreal에 재현하고, 가상 카메라와 가상 LiDAR 측정값을 수집해 판단 서버로 전송합니다.
2. 실제 센서/카메라 데이터를 받아 Unreal 환경에 재현합니다.
3. 철강 공정의 Slab 각도 틀어짐을 LiDAR point cloud 기반으로 분석합니다.

## 저장소 구성

```text
Source/m7at10_dt/M7AT10/Camera   가상 카메라
Source/m7at10_dt/M7AT10/Sensor   가상 LiDAR, 센서 매니저, replay source, transport/recorder
Source/m7at10_dt/M7AT10/UI       센서 모니터 위젯과 HostActor
Source/m7at10_dt/M7AT10/Crane    크레인 예제 구현
Plugins/DTCore                   공통 Core 플러그인 submodule
Content/M7AT10                   맵, 위젯, Blueprint asset
docs                             schema, smoke test, adapter plan, widget setup
Samples                          replay sample data
```

## DTCore Submodule

처음 받은 뒤 반드시 submodule을 초기화합니다.

```powershell
git submodule update --init --recursive
git submodule status
```

기준 commit:

```text
2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7 Plugins/DTCore
```

이번 작업 범위에서는 DTCore 내부 소스를 직접 수정하지 않습니다. 현재 DTCore에서 `EnhancedInput` dependency 경고가 발생할 수 있지만, DTCore 수정 허용 전까지는 DT-Project 쪽에서 우회합니다.

## 빌드

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" m7at10_dtEditor Win64 Development "C:\path\to\m7at10_dt.uproject" -WaitMutex
```

로컬 실행 프로젝트 예시:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" m7at10_dtEditor Win64 Development "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -WaitMutex
```

## LiDAR 데이터 정책

`UVirtualLidarSensorComp`는 한 번의 scan 또는 replay frame에서 생성된 전체 측정값을 `LastPoints`에 유지합니다.

서버/판단 payload:

- `ServerPayloadStride`
- `MaxServerPayloadPoints`
- `bIncludeMissPointsInServerPayload`
- JSON `schemaVersion = virtual-lidar.v1`
- JSON `payloadPolicy`
- JSON `slabAnalysis`

Editor/UI preview:

- `PreviewPointStride`
- `MaxPreviewPoints`
- `bPointCloudPreviewHitOnly`
- JSON `previewPolicy`

기본 방향은 서버에는 원본에 가까운 hit point를 보내고, Unreal Editor 화면에는 제한된 point만 표시하는 것입니다. 기존 `PayloadPointStride`, `MaxPayloadPoints`, `PointCloudPreviewStride`, `MaxPointCloudPreviewInstances`는 Blueprint 호환을 위해 남아 있지만 새 정책 필드를 우선 사용합니다.

## File Replay Source

`ULidarCsvReplaySourceComp`와 `ULidarJsonLinesReplaySourceComp`는 저장된 point cloud를 `UVirtualLidarSensorComp::InjectPointCloudFrame`으로 주입합니다. 이 경로를 타면 replay 데이터도 LiDAR JSON payload, recorder, transport, preview, Slab 분석 경로를 동일하게 통과합니다.

지원 CSV 형식:

```text
row,col,x,y,z
x,y,z
```

지원 JSONL 형식:

```text
{"x":900,"y":-260,"z":0,"distance":936.8,"hit":true,"semanticLabel":"Slab"}
{"worldLocation":[900,-260,0],"localDirection":[1,0,0],"hit":true,"semanticLabel":"Slab"}
```

샘플 파일:

```text
Samples/slab_replay_sample.csv
Samples/slab_replay_sample.jsonl
```

Editor Details 패널 버튼:

```text
PushFrameOnceInEditor
PushFrameOnceNoTransportInEditor
StartReplayInEditor
StopReplayInEditor
```

## 실제 센서 Source Placeholder

현재 준비된 placeholder component:

```text
URos2SensorBridgeSourceComp
ULivoxLidarSourceComp
URealSenseCameraSourceComp
```

이 component들은 아직 SDK/bridge 연결을 수행하지 않고, 설정 필드와 명확한 not-implemented 상태 메시지를 제공합니다. 후속 구현에서도 가능한 한 `InjectPointCloudFrame` 같은 normalized frame 주입 경로를 공유해야 합니다.

## Slab 각도 분석 Workflow

1. Slab actor 또는 component 이름/tag를 `Slab`, `SteelSlab`, `Plate` 중 하나와 매칭되게 설정합니다.
2. `UVirtualLidarSensorComp`에서 `SlabSemanticLabel = Slab`을 유지합니다.
3. 정상 기준 방향에 맞춰 `ReferenceSlabYawDegrees`를 설정합니다.
4. LiDAR scan/replay 후 `FVirtualLidarSlabAnalysisResult` 또는 JSON `slabAnalysis`를 확인합니다.

분석 결과에는 slab hit point count, bounds, center, estimated yaw, reference yaw, angle deviation, confidence, status message가 포함됩니다.

## Widget 조작

권장 Widget parent class:

```text
UVirtualSensorMonitorWidget
```

권장 optional binding 이름:

```text
ViewImage
TitleText
StatusText
ToggleButton
ToggleButtonText
NextCameraButton
NextLidarButton
PointCloudOnlyButton
LidarViewModeButton
LidarViewModeButtonText
LogPointCloudButton
ExportPointCloudButton
LocalSensorCaptureButton
LocalSensorCaptureButtonText
CaptureOnceButton
PreviewMoreButton
PreviewLessButton
```

SensorManager Blueprint API:

```text
CaptureSelectedOnce
CaptureAllOnce
SetSelectedLidarPreviewPolicy
AdjustSelectedLidarPreviewBudget
TogglePointCloudOnlyView
SetPointCloudOnlyMode
```

Level Blueprint 없이 위젯을 자동 생성하려면 맵에 `AVirtualSensorMonitorHostActor`를 배치하고 `MonitorWidgetClass = WBP_VirtualSensorMonitor`를 지정합니다.

`MonitorWidgetClass`가 비어 있고 `bUseNativeMonitorWidgetFallback`이 true이면 HostActor가 native `UVirtualSensorMonitorWidget`를 생성합니다. 이 fallback은 상태 텍스트와 기본 버튼을 보여주는 smoke-test용 UI입니다. 운영자용 UI는 Designer에서 만든 `WBP_VirtualSensorMonitor`를 권장합니다.

## 자동화 테스트

전체 로컬 smoke gate:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1"
```

대표 명령:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorReplay; Quit" -TestExit="Automation Test Queue Empty"
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.RealSensorSource; Quit" -TestExit="Automation Test Queue Empty"
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorMonitor; Quit" -TestExit="Automation Test Queue Empty"
```

## 권장 Smoke Test 설정

```text
AVirtualSensorManager.bDiscoverOnBeginPlay = true
AVirtualSensorManager.bStartSensorsOnBeginPlay = true
SharedSensorTransport.TransportMode = LogOnly
LiDAR SimulationQuality = RealTimePreview
Camera SimulationQuality = RealTimePreview
LiDAR bUseMultiHit = false
LiDAR ExportOnScan = false
LiDAR bDrawDebugRays = false
```

성능 문제가 있으면 다음 순서로 낮춥니다.

```text
1. LiDAR SimulationQuality = Debug
2. PreviewPointStride 증가
3. MaxPreviewPoints 감소
4. Camera CaptureMode = PreviewOnly
5. MultiHit / ExportOnScan / FullSpec 비활성화
```

## 관련 문서

```text
docs/lidar_payload_schema.md
docs/widget_designer_setup.md
docs/editor_smoke_test.md
docs/real_sensor_adapter_plan.md
```

## 알려진 제한

- Livox SDK, RealSense SDK, ROS2 bridge 직접 입력은 아직 구현되지 않았습니다.
- `ExportLastPointCloudLaz()`는 진짜 LAZ 압축이 아닙니다. 현재는 `*_laz_source_*.las` 형식의 LAS 호환 source 파일을 저장하고 warning log를 남깁니다.
- FullSpec, MultiHit, ExportOnScan을 동시에 켜면 editor 성능이 크게 떨어질 수 있습니다.
- 대규모 point cloud rendering은 아직 CPU/instance 기반 preview 한계가 있어 GPU/Niagara 기반 renderer 검토가 필요합니다.
- 실제 맵에서의 PIE smoke test와 WBP Designer 배치 검증은 별도 editor 작업이 필요합니다.
