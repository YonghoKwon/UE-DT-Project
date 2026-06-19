# AGENTS.md - UE-DT-Project 작업 가이드

## 프로젝트 기준

- Unreal Engine: 5.3
- 메인 프로젝트: `UE-DT-Project`
- Core 플러그인: `Plugins/DTCore` submodule
- 기준 DTCore 브랜치: `claude/digital-twin-core-analysis-1lxlcb`
- 현재 기능 브랜치: `feature/lidar-point-cloud-only-view`

이 프로젝트는 철강 제조 환경 Digital Twin을 목표로 합니다. 현재 DT-Project 코드는 가상 카메라, 가상 LiDAR, 센서 매니저, 센서 데이터 전송/저장, point-cloud-only view, Slab 각도 분석 v1, CSV/JSONL replay source, monitor widget/HostActor를 포함합니다.

DTCore는 공통 API/WebSocket/Data/UI 기반 플러그인으로 사용합니다. 이 저장소 작업에서는 사용자가 명시적으로 허용하기 전까지 DTCore 내부 소스를 직접 수정하지 않습니다.

## 작업 원칙

- DTCore 수정이 필요해 보이면 DT-Project 안에서 우회하거나 별도 이슈로 남깁니다.
- `Plugins/DTCore`는 submodule로 유지합니다. 내장 소스 복사본을 다시 커밋하지 않습니다.
- 커밋 전 `Scripts/validate_dtcore_submodule_guard.ps1`로 `Plugins/DTCore`가
  `2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7`에 고정되어 있고 staged,
  parent, submodule worktree 변경이 없는지 확인합니다.
- DTCore is an external submodule pinned to that commit; this repository must
  not stage gitlink changes or files under `Plugins/DTCore` during local
  decision cleanup.
- `Binaries/`, `Intermediate/`, `Saved/`, `.vs/` 생성물은 커밋하지 않습니다.
- 대용량 marketplace/content asset은 의도적으로 추적 대상인지 확인하기 전까지 커밋하지 않습니다.
- 가상 센서의 서버 전송 데이터와 editor preview 데이터는 분리합니다.
- 무거운 옵션은 기본값으로 켜지 않습니다. FullSpec, MultiHit, ExportOnScan 조합은 검증용으로만 사용합니다.
- 로컬 실행 프로젝트 `C:\Unreal Projects\m7at10_dt`에 반영하는 것과 git commit은 별개 작업입니다. 기능 변경 시 가능한 한 작업 repo와 로컬 프로젝트 양쪽에 같은 내용으로 커밋합니다.

## LiDAR 데이터 정책

`UVirtualLidarSensorComp`는 한 번의 scan 또는 replay frame에서 생성된 전체 측정값을 `LastPoints`에 유지합니다.

서버/판단 payload:

- `ServerPayloadStride`
- `MaxServerPayloadPoints`
- `bIncludeMissPointsInServerPayload`
- `schemaVersion = virtual-lidar.v1`
- `payloadPolicy`
- `slabAnalysis`

Editor preview:

- `PreviewPointStride`
- `MaxPreviewPoints`
- `bPointCloudPreviewHitOnly`
- `previewPolicy`

기본 방향은 서버에는 원본에 가까운 hit point를 보내고, Unreal Editor 화면에는 제한된 point만 표시하는 것입니다. 기존 `PayloadPointStride`, `MaxPayloadPoints`, `PointCloudPreviewStride`, `MaxPointCloudPreviewInstances`는 Blueprint 호환을 위해 남아 있지만 새 정책 필드를 우선 사용합니다.

PointCloudOnly 모드에서도 preview 정책은 서버 payload 정책을 변경하지 않아야 합니다.

## Replay Source

`ULidarCsvReplaySourceComp`와 `ULidarJsonLinesReplaySourceComp`는 저장된 point cloud를 `UVirtualLidarSensorComp::InjectPointCloudFrame`으로 주입합니다.

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

replay 경로는 payload, recorder, transport, preview, Slab 분석을 모두 통과해야 합니다. 실제 Livox/ROS2/RealSense adapter를 추가할 때도 가능한 한 같은 normalized frame 주입 경로를 공유합니다.

## Slab 각도 분석

Slab 분석은 LiDAR semantic label이 `Slab`인 hit point를 기준으로 합니다.

권장 actor 설정:

```text
Actor Tag = Slab
```

분석 결과 `FVirtualLidarSlabAnalysisResult`는 LiDAR JSON payload의 `slabAnalysis`에도 포함됩니다.

포함 값:

- slab hit point count
- bounds min/max
- center
- estimated yaw
- reference yaw
- angle deviation
- confidence
- status message

`ReferenceSlabYawDegrees`는 기준 축을 뜻합니다. 설비 좌표계에서 정상 Slab 진행 방향이 X축이면 `0`으로 둡니다.

## Widget 구성

권장 Widget parent class:

```text
UVirtualSensorMonitorWidget
```

기존 Blueprint 위젯과의 호환을 위해 binding은 optional입니다. 권장 이름:

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

`StatusText`에는 sensor id, frame id, scan interval, ray count, hit count, server payload count, preview count, Slab angle, confidence, performance warning을 표시합니다.

추가 조작:

- `CaptureOnceButton`: SensorManager가 있으면 선택된 카메라/LiDAR 1회 capture, 없으면 바인딩된 카메라/LiDAR 직접 capture
- `PreviewMoreButton`: 서버 payload는 유지하고 editor preview 표시량만 증가
- `PreviewLessButton`: 서버 payload는 유지하고 editor preview 표시량만 감소
- native fallback: `MonitorWidgetClass`가 비어 있고 `bUseNativeMonitorWidgetFallback`이 true일 때 smoke-test용 Slate 패널을 생성

SensorManager Blueprint API:

```text
CaptureSelectedOnce
CaptureAllOnce
SetSelectedLidarPreviewPolicy
AdjustSelectedLidarPreviewBudget
TogglePointCloudOnlyView
SetPointCloudOnlyMode
```

## Editor Smoke Test 권장값

```text
Camera SimulationQuality = RealTimePreview
LiDAR SimulationQuality = RealTimePreview
SharedSensorTransport.TransportMode = LogOnly
LiDAR bUseMultiHit = false
LiDAR bDrawDebugRays = false
ExportOnScan = false
PointCloudOnly = 필요 시에만 On
```

성능 문제가 있으면 다음 순서로 낮춥니다.

```text
1. LiDAR SimulationQuality = Debug
2. PreviewPointStride 증가
3. MaxPreviewPoints 감소
4. Camera CaptureMode = PreviewOnly
5. MultiHit / ExportOnScan / FullSpec 비활성화
```

## 검증 명령

빌드:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" m7at10_dtEditor Win64 Development "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -WaitMutex
```

자동화 테스트:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1"
```

개별 테스트:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorReplay; Quit" -TestExit="Automation Test Queue Empty"
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.RealSensorSource; Quit" -TestExit="Automation Test Queue Empty"
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorMonitor; Quit" -TestExit="Automation Test Queue Empty"
```

## 문서 관리

- 루트 문서는 `README.md`와 `AGENTS.md`만 유지합니다.
- 이전 오타 파일명 `AGETNS.md`는 사용하지 않습니다.
- 기능 설명은 README에, 작업자용 운영 규칙은 AGENTS에 둡니다.
- 세부 실행 문서는 `docs/` 아래에 둡니다.
