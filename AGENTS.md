# AGENTS.md - UE-DT-Project 작업 가이드

이 파일은 저장소를 수정하는 개발자와 자동화 agent가 지켜야 할 현재 기준입니다. 사용자 기능 설명은 `README.md`, 맵·WBP 조작 절차는 `docs/sensor_test_map_setup.ko.md`에 둡니다.

## 프로젝트 불변 조건

- Unreal Engine: 5.3
- 프로젝트 파일: `ma0t10_dt.uproject`
- Runtime module: `ma0t10_dt`
- Editor module: `ma0t10_dtEditor`
- Content namespace: `/Game/MA0T10`
- 기본 검증 맵: `/Game/MA0T10/Maps/SensorTestMap`
- DTCore: `Plugins/DTCore` git submodule
- DTCore 고정 commit: `2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7`

과거 `/Game/M7AT10` 경로는 새 코드, WBP, redirect 대상에 사용하지 않습니다.

## 작업 경계

- 사용자가 명시적으로 요청하지 않으면 `Plugins/DTCore` 내부와 parent gitlink를 수정하거나 stage하지 않습니다.
- `Config/Game.ini`는 로컬 runtime override로 취급합니다. 명시적 요청 없이 수정·stage하지 않습니다.
- `Binaries/`, `Intermediate/`, `Saved/`, `.vs/`, packaged `Windows/` 결과는 커밋하지 않습니다.
- marketplace/sample content와 대용량 binary asset은 소유권과 배포 허용 여부를 확인하기 전까지 stage하지 않습니다.
- 사용자의 기존 worktree 변경을 되돌리거나 정리하지 않습니다.
- IDE 빌드와 Live Coding을 동시에 사용하지 않습니다. IDE/UBT 빌드 전 Unreal Editor와 game process를 종료합니다.

DTCore guard:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_dtcore_submodule_guard.ps1"
```

## SensorTestMap 소유 규칙

저장소가 소유하는 맵 구성의 source of truth는 `Scripts/setup_sensor_test_map.py`입니다.

- 스크립트는 `SensorTestManaged` 태그 Actor만 제거·재생성합니다.
- 사용자가 배치한 비관리 Actor와 mesh는 건드리지 않습니다.
- Slab mesh는 자동 생성하지 않습니다.
- 누락된 WBP는 올바른 native parent로 생성합니다.
- map/WBP asset 변경은 Python script 또는 Unreal Editor로 수행하고 raw binary patch를 시도하지 않습니다.

영구 반영 대상 Actor 태그:

```text
SensorTestPersistent_PrimaryCamera
SensorTestPersistent_PrimaryLidar
```

이 태그는 Actor 이름과 무관한 불변 식별자입니다. 한 맵에 같은 태그가 둘 이상 존재하면 안 됩니다.

## 런타임 UI 기준

HostActor는 다음 세 패널을 생성합니다.

| WBP | Native parent | 기본 위치 | ZOrder |
|---|---|---|---:|
| `WBP_VirtualSensorMonitor` | `UVirtualSensorMonitorWidget` | 오른쪽 중앙, 약 820×430 | 10 |
| `WBP_VirtualSensorSettings` | `UVirtualSensorSettingsWidget` | 왼쪽 중앙, 약 450×640 | 20 |
| `WBP_VirtualSensorCaptureExport` | `UVirtualSensorCaptureExportWidget` | 하단 중앙, 약 760×280 | 30 |

공통 parent `UVirtualSensorDraggableWidgetBase`가 title drag, DPI 보정, viewport clamp, collapse, reset과 사용자별 normalized 위치 저장을 제공합니다. 1280×720에서는 세 패널 접근성을 위해 compact 크기를 허용합니다. UI 상태는 `Saved/SaveGames/MA0T10_VirtualSensorUI_v1.sav`에만 저장하며 센서 설정이나 맵 영구 반영 데이터와 섞지 않습니다.

WBP Designer가 비어 있어도 native-backed Slate UI가 표시되어야 합니다. Designer를 추가할 때:

- parent class를 바꾸지 않습니다.
- `BindWidgetOptional` 이름과 대소문자를 맞추고 `Is Variable`을 활성화합니다.
- 자동 바인딩되는 Button에 같은 `OnClicked` Event Graph를 중복 연결하지 않습니다.
- HostActor 방식과 Level Blueprint `Create Widget` 방식을 동시에 사용하지 않습니다.

기존 `MonitorWidgetClass`와 monitor Blueprint API는 호환성을 위해 유지합니다.

패널 역할은 중복시키지 않습니다.

- Monitor: 카메라/LiDAR 화면, 센서 전환, LiDAR 표시 방식·범례와 접을 수 있는 상세 진단
- Settings: Transform 기즈모·키보드, 센서 값, preview/payload 정책, 투사 디버그, 외부 source 제어
- CaptureExport: 1회 캡처, payload/point cloud export, timed capture, 저장 폴더와 결과
- native fallback의 사용자 노출 문구는 한글로 작성하고 SensorId, FOV, JSONL 같은 기술 토큰만 원문을 유지합니다.
- 세 native fallback은 공통 어두운 반투명 스타일을 사용합니다. 밝은 맵에서 흰 배경과 흰 글씨 조합을 만들지 않으며 주요 글씨 대비는 4.5:1 이상을 유지합니다.
- `ActorClassColor` enum 이름은 Blueprint 호환성을 위해 유지하지만 UI에는 `의미 분류 색상`으로 표시하고 `SemanticLabel`의 실제 설정 색상을 사용합니다.

## 센서 설정과 맵 저장 규칙

`FVirtualSensorEditableState`는 UI와 PIE Actor 간 허용된 편집 상태입니다.

- 숫자 입력은 commit/slider 종료 시 한 번 적용하고 checkbox·preset은 선택 즉시 적용합니다. SpinBox `OnValueChanged`에서 센서를 재시작하지 않습니다.
- 기즈모 연속 조작은 Transform을 즉시 갱신하되 센서 recapture를 최대 10 Hz로 제한하고 조작 종료 시 최종 캡처합니다.
- 빈 ID, 다른 센서와 중복된 ID, NaN Transform, 지원 범위 밖 값은 거부합니다.
- 거부 시 Actor를 변경하지 않고 현재 runtime 값을 UI에 복구합니다.
- profile/quality preset 적용과 이후 사용자 고급 값 override를 구분합니다.
- `FullSpec` 선택은 명시적이어야 하며 기본값으로 만들지 않습니다.

투사 범위 표시는 선택 센서 하나에만 적용하고 기본값은 false입니다. Camera는 frustum, LiDAR는 최대 64개의 대표 광선만 그리며 legacy `bDrawDebugRays`를 대신 켜지 않습니다. 디버그·기즈모 UI 상태는 맵 저장 스냅샷에 포함하지 않습니다.

`SensorTestMap에 저장 예약`은 자동 저장이 아닙니다.

1. HostActor가 스냅샷을 대기시킵니다.
2. `ma0t10_dtEditor`가 `PrePIEEnded`에서 회수합니다.
3. `EndPIE`에서 열린 맵과 불변 태그, Actor 타입을 전부 검증합니다.
4. Transaction 안에서 Transform과 허용된 센서 속성만 복사합니다.
5. `FEditorFileUtils::SaveLevel` 실패 시 Transaction을 Undo합니다.

다른 맵, 태그 누락·중복, 타입 불일치에서는 원본 맵을 변경하지 않아야 합니다. packaged build에는 맵 저장 기능을 넣지 않습니다.

## 장비 메타데이터와 실행 예산

장비 사양과 runtime simulation budget은 분리합니다.

기본 profile:

```text
Intel RealSense D455
  Depth FOV 87 x 58 deg
  max resolution 1280 x 720
  Min-Z about 0.52 m at max resolution
  recommended range 0.6-6 m

Livox Mid-360S
  FOV 360 x 59 deg
  vertical -7 to 52 deg
  minimum 0.1 m
  40 m at 10% reflectivity, 100 m cutoff
  200,000 points/s, 10 Hz
```

기본 `RealTimePreview`:

```text
Camera 640 x 360, 10 Hz
LiDAR 120 x 24 rays, 4 Hz, 40 m
```

D455 preview는 calibrated depth stream이 아니라 SceneCapture 결과라는 문구를 UI와 문서에서 유지합니다.

## LiDAR 데이터 정책

한 scan/replay frame의 전체 측정값은 `LastPoints`에 유지합니다.

Server payload 정책:

```text
ServerPayloadStride
MaxServerPayloadPoints
bIncludeMissPointsInServerPayload
```

Editor preview 정책:

```text
PreviewPointStride
MaxPreviewPoints
bPointCloudPreviewHitOnly
```

preview 조정, point-cloud-only mode, renderer fallback은 server payload 정책을 변경하면 안 됩니다. 기존 legacy property는 Blueprint 호환성을 위해 남기되 새 정책 필드를 우선 사용합니다.

## Replay와 real source

CSV/JSONL replay와 live adapter는 가능하면 normalized frame 주입 경로를 공유합니다.

```text
ULidarCsvReplaySourceComp / ULidarJsonLinesReplaySourceComp
  -> UVirtualLidarSensorComp::InjectPointCloudFrame
  -> payload / recorder / transport / preview / slab analysis
```

실제 Livox, RealSense, ROS2 연결을 구현했다고 주장하려면 SDK/bridge 코드뿐 아니라 장비 기반 acceptance evidence가 있어야 합니다. Placeholder 또는 deployment-evidence-pending 상태를 성공으로 바꾸지 않습니다.

## 저장 경로 계약

```text
Payload              Saved/SensorCaptures/<SensorId>/ServerPayload
Point cloud          Saved/SensorCaptures/<SensorId>/PointCloud
Timed camera         Saved/SensorCaptures/LocalTimedCapture/<UTC>/Camera
Timed LiDAR          Saved/SensorCaptures/LocalTimedCapture/<UTC>/Lidar
Recorder             Saved/SensorRecordings
Transport SaveToFile Saved/SensorCaptures/<SensorId>
```

새 export 기능은 `FVirtualSensorExportResult`의 성공 여부, 형식, sensor id, 절대 경로, 파일 크기와 메시지를 채웁니다. 최근 결과 목록은 8개로 제한합니다.

## 성능 안전 기본값

```text
Camera SimulationQuality = RealTimePreview
LiDAR SimulationQuality = RealTimePreview
SharedSensorTransport.TransportMode = LogOnly
LiDAR bUseMultiHit = false
LiDAR bDrawDebugRays = false
ExportOnScan = false
PointCloudOnly = 필요할 때만 활성화
```

문제 발생 시 `Debug` quality, preview stride 증가, max preview 감소, Camera `PreviewOnly` 순으로 낮춥니다.

## 빌드와 검증

빌드:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" `
  ma0t10_dtEditor Win64 Development `
  "-Project=$PWD\ma0t10_dt.uproject" `
  -WaitMutex -NoHotReloadFromIDE
```

전체 smoke:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1"
```

SensorTestMap/UI 변경 시 최소 실행 그룹:

```text
MA0T10.SensorControl
MA0T10.SensorDebug
MA0T10.SensorExport
MA0T10.SensorMonitor
MA0T10.EditorSmoke
```

개별 명령:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  ".\ma0t10_dt.uproject" `
  -NullRHI -Unattended -NoSplash -NoSound `
  '-ExecCmds=Automation RunTests MA0T10.SensorControl; Quit' `
  '-TestExit=Automation Test Queue Empty'
```

UI 변경은 1920×1080과 1280×720에서 title drag, clamp, collapse, reset과 패널 겹침을 수동 확인합니다. 카메라 JPEG/GPU readback처럼 renderer-dependent 경로는 `-NullRHI`만으로 완료 판정하지 않습니다.

## 문서 관리

- 루트 문서는 `README.md`, `AGENTS.md`만 유지합니다.
- 사용자 기능과 빠른 시작은 README에 둡니다.
- 작업 불변 조건과 안전 경계는 AGENTS에 둡니다.
- 맵/WBP/PIE의 기준 문서는 `docs/sensor_test_map_setup.ko.md`입니다.
- payload와 transport 문서는 구현 계약이므로 UI 가이드에 합치지 않습니다.
- 오래된 문서를 삭제할 때 validator/report script의 하드코딩 경로도 함께 갱신합니다.
- 동일 절차를 여러 문서에 복제하지 말고 기준 문서로 링크합니다.

커밋 전에는 다음을 확인합니다.

```powershell
git diff --check
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_dtcore_submodule_guard.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\invoke_local_decision_precommit_gate.ps1"
```
