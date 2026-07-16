# UE-DT-Project

Unreal Engine 5.3 기반 Digital Twin 센서 실험 프로젝트입니다. 가상 카메라와 LiDAR를 배치하고, 런타임 UI에서 센서 설정을 바꾸며, payload·point cloud·timed capture를 저장하고 테스트할 수 있습니다.

현재 기본 검증 맵은 `/Game/MA0T10/Maps/SensorTestMap`입니다. PIE를 시작하면 별도 Level Blueprint 없이 모니터, 센서 설정, 캡처·내보내기 패널이 자동으로 표시됩니다.

## 빠른 시작

1. `ma0t10_dt.uproject`를 Unreal Editor 5.3으로 엽니다.
2. `/Game/MA0T10/Maps/SensorTestMap`을 엽니다.
3. PIE를 시작합니다.
4. 화면의 세 패널에서 카메라/LiDAR 전환, 위치 변경, 캡처와 export를 시험합니다.

기본 패널:

| 패널 | 기본 위치 | 역할 |
|---|---|---|
| `WBP_VirtualSensorMonitor` | 오른쪽 중앙 | 카메라/LiDAR 화면, 센서 전환, 표시 모드 |
| `WBP_VirtualSensorSettings` | 왼쪽 중앙 | 기즈모·키보드 Transform 조작, 센서 값, 투사 디버그 |
| `WBP_VirtualSensorCaptureExport` | 하단 중앙 | payload·point cloud·timed capture와 저장 결과 |

세 패널은 밝은 맵에서도 읽기 쉬운 어두운 반투명 테마를 사용하며 제목 표시줄 드래그, 화면 경계 제한, `접기`, `위치 초기화`를 지원합니다. 패널 위치·접힘 상태와 LiDAR 표시 옵션은 `Saved/SaveGames/MA0T10_VirtualSensorUI_v1.sav`에 사용자별로 저장되고 다음 PIE에서 복원됩니다. Settings의 `전체 UI 초기화`는 이 UI 전용 상태만 기본값으로 되돌리며 센서 Transform이나 SensorTestMap 값은 변경하지 않습니다.

자세한 맵/WBP/PIE 절차는 [가상 센서 테스트 맵과 Widget Blueprint 설정](docs/sensor_test_map_setup.ko.md)을 참고합니다.

## 현재 구현된 기능

### 가상 카메라

- `AVirtualCameraAct` / `UVirtualCameraComp`
- `virtual-camera.v1` JSON payload
- RenderTarget preview와 JPEG 저장
- 1회 캡처 및 timed local capture
- server payload export
- Intel RealSense D455 장비 메타데이터
- `Debug`, `RealTimePreview`, `Balanced`, `FullSpec`, `Custom` 실행 품질

D455 기본 실행은 실제 depth stream을 재현하는 것이 아니라 `SceneCapture` preview입니다. 장비 메타데이터와 렌더링 예산은 분리되어 있습니다.

### 가상 LiDAR

- `AVirtualSensorAct` / `UVirtualLidarSensorComp`
- `virtual-lidar.v1` JSON payload
- server payload와 editor preview 정책 분리
- hit/miss, semantic label, grid 좌표와 Slab 분석
- point-cloud-only view
- CSV, JSONL, PCD, LAS, LAZ export
- CSV/JSONL replay 및 normalized frame 주입
- Livox Mid-360S 장비 메타데이터

기본 `RealTimePreview` 예산은 120×24 rays, 4 Hz, 40 m입니다. `FullSpec`, MultiHit, ExportOnScan 조합은 성능 검증용이며 기본값으로 사용하지 않습니다.

Monitor의 `LiDAR 표시 방식` 드롭다운은 다음 네 가지입니다.

| 방식 | 의미 |
|---|---|
| 거리 색상 | 가까운 점은 자홍·주황, 중간은 노랑, 먼 점은 청록·파랑, 미검출은 검정 |
| 검출 마스크 | 검출 성공은 흰색, 미검출은 검정 |
| 의미 분류 색상 | Actor 태그·클래스에서 계산한 `SemanticLabel`별 설정 색상, 미분류는 기본 회색 |
| 거리 회색조 | 가까운 점은 밝게, 먼 점과 미검출은 어둡게 표시 |

`적응형 거리`는 현재 프레임의 검출 범위로 색상 대비를 조정하고, `깊이 경계`는 거리 변화가 큰 윤곽을 흰색으로 강조하며, `격자`는 LiDAR 행·열 기준을 겹쳐 표시합니다. 세 옵션은 표시 방식과 독립적입니다. 모니터는 SensorId·프레임·주기·측정/검출 수를 먼저 보여주며 Payload·Preview·Slab·전송 정보는 `상세 진단 펼치기`에서 확인합니다.

### 런타임 센서 설정

Settings 패널에서 다음 값을 변경할 수 있습니다.

- Camera/LiDAR 선택과 다음 센서 전환
- SensorId 중복 검사
- 위치 XYZ와 Pitch/Yaw/Roll
- 마우스 3축 이동 기즈모와 Pitch/Yaw/Roll 회전 링
- 로컬/월드 좌표계, 이동·회전 단위, 키보드 연속 조작
- 선택 센서의 Camera frustum 또는 LiDAR 스캔 범위 표시
- Device Profile과 Simulation Quality
- 카메라 해상도, 캡처 주기, FOV, JPEG 품질, CaptureMode
- LiDAR 거리, 주기, sample/channel, 수평·수직 FOV
- server payload와 preview stride/max/hit 정책
- multi-hit, max hits, 자동 CSV/JSONL/PCD export

숫자는 Enter 또는 입력 포커스 해제, SpinBox 드래그 종료 시 한 번 적용됩니다. 체크박스·프로필·품질은 선택 즉시 반영합니다. 기즈모 조작 중 Transform과 디버그 형상은 매 프레임 움직이고 센서 미리보기는 최대 10Hz로 갱신되며 조작 종료 시 최종 프레임을 즉시 생성합니다. 잘못된 범위, 중복 SensorId, NaN Transform은 거부하고 마지막 런타임 값을 다시 표시합니다.

고급 설정의 각 행에는 `ⓘ` 버튼과 한글 툴팁이 있습니다. 버튼을 누르면 패널 아래 도움말 카드에 의미, 단위, 성능 영향, 권장값과 주의사항이 표시됩니다. 같은 카드의 `현재 예상 부하`는 카메라 MP/s 또는 LiDAR 광선/스캔·광선/s와 Preview/Payload 최대 점 수를 계산하고, 실행 중인 공용 스케줄러의 자동 60/30 FPS 단계·활성 센서 수·대기/생략 수·실측 FPS도 함께 보여줍니다.

`센서 조작 시작` 후 키보드는 다음과 같습니다.

```text
W/S 전후, A/D 좌우, Q/E 아래/위
방향키 Pitch/Yaw, Z/C Roll
Shift 5배, Ctrl 0.2배, Esc 조작 종료
```

버튼 의미:

| 버튼 | 동작 |
|---|---|
| `PIE 시작값으로 되돌리기` | PIE 시작 시 맵에서 읽은 값으로 복구 |
| `SensorTestMap에 저장 예약` | PIE 종료 시 원본 SensorTestMap에 저장할 스냅샷 등록 |

`SensorTestMap에 저장 예약`은 Editor PIE에서만 활성 의미가 있습니다. 투사 범위 토글과 기즈모 표시 상태는 런타임 UI 설정이므로 맵에 저장하지 않습니다. Editor 모듈은 PIE 종료 시 불변 관리 태그로 원본 Actor를 찾고 허용된 센서 설정만 Transaction으로 복사합니다. 다른 맵, 누락·중복 태그, 잘못된 Actor 타입이면 저장하지 않습니다. 파일 저장 실패 시 방금 적용한 Transaction을 Undo합니다.

### FullSpec 자동 실행 최적화

FullSpec 규격은 Camera 1280×720·30Hz, LiDAR 360×60(21,600 rays)·10Hz입니다. `StartCapture()`와 `StartScan()` 자동 실행은 `UVirtualSensorPerformanceSubsystem`이 관리합니다.

- Camera≤2·LiDAR≤2: 자동 60 FPS 단계, LiDAR game-thread trace 예산 4ms/frame
- 각 종류 4대 이하: 자동 30 FPS 단계, trace 예산 8ms/frame
- 어느 종류든 4대 초과: 30 FPS 최선 실행과 지원 범위 초과 경고
- 카메라 GPU readback은 비동기이며 JPEG/Base64/JSON은 worker thread에서 처리
- FullSpec 설정의 명목 30Hz는 유지하되, 여러 카메라에서는 GPU 장면 캡처를 전체 15Hz 상한으로 최신 프레임 분배하고 Monitor 선택 카메라를 격회 우선 처리
- LiDAR는 128~1024 ray 적응형 chunk로 time-slice하고 스캔 중 Transform·설정을 고정
- 센서별 acquisition 1개·derived 작업 1개만 유지하며 밀리면 오래된 파생 프레임을 누적하지 않고 생략 수를 기록
- `PreviewOnly` 카메라는 GPU readback/JPEG를 생략하고, LiDAR texture는 부분 갱신, ISM은 수량 차이만 반영

수동 `Capture Once`, `CaptureAndSendImage()`, `ScanAndSend()`는 생략하지 않는 기존 동기 계약을 유지합니다. 따라서 여러 카메라의 `실측 Hz`는 명목 30Hz보다 낮을 수 있으며, 이는 렌더링 FPS를 지키기 위한 최신 프레임 정책의 정상 결과입니다. 선택된 카메라 preview가 우선되고 비선택 LiDAR의 texture/ISM 갱신은 생략하지만 측정·Payload 순서는 바뀌지 않습니다. FullSpec에서 MultiHit와 스캔별 자동 export는 규격 밖의 추가 부하이므로 기본적으로 끄고 수동 export를 권장합니다. Payload 스키마에는 성능 필드를 추가하지 않았으며 Monitor `상세 진단`과 Settings 부하 카드에서만 확인합니다.

관리 태그:

```text
SensorTestPersistent_PrimaryCamera
SensorTestPersistent_PrimaryLidar
```

### 캡처와 내보내기

Capture & Export 패널 기능:

- `선택 센서 1회 캡처`
- `서버 Payload 내보내기`
- point cloud CSV/JSONL/PCD/LAS/LAZ 선택 export
- timed local capture 시작/정지
- 저장 루트와 최근 결과 폴더 열기
- 최근 저장 경로를 클립보드에 복사
- 비동기 진행 상태와 최근 결과 8개 표시

저장 위치:

```text
Saved/SensorCaptures/<SensorId>/ServerPayload
Saved/SensorCaptures/<SensorId>/PointCloud
Saved/SensorCaptures/LocalTimedCapture/<UTC>/Camera
Saved/SensorCaptures/LocalTimedCapture/<UTC>/Lidar
Saved/SensorRecordings
Saved/SensorCaptures/<SensorId>                 # Transport SaveToFile
```

외부 LAZ compressor가 설정되지 않은 경우 LAZ 요청은 실제 압축 완료로 표시하지 않으며 LAS-compatible source와 경고 상태를 남깁니다.

### Sensor Manager와 데이터 경로

`AVirtualSensorManager`가 카메라, LiDAR, transport, recorder, real-source placeholder와 UI 선택 상태를 연결합니다.

```text
Virtual/Replay/Live Source
        -> normalized camera or LiDAR frame
        -> sensor component cache
        -> payload / recorder / transport / preview / UI
```

LiDAR 서버 정책과 preview 정책은 독립적입니다. preview 밀도나 point-cloud-only mode를 바꿔도 server payload 정책이 바뀌면 안 됩니다.

세부 payload 계약:

- [LiDAR Payload Schema](docs/lidar_payload_schema.md)
- [Camera Payload Schema](docs/camera_payload_schema.md)
- [Server Transport Contract](docs/server_transport_contract.md)

실장비 입력은 공통 source interface와 JSON/HTTP/UDP/WebSocket bridge 기반 골격까지 구현되어 있습니다. 실제 Livox, RealSense, ROS2 SDK 연동과 장비 acceptance evidence는 아직 필요합니다. 상세 범위는 [Real Sensor Adapter Plan](docs/real_sensor_adapter_plan.md)을 참고합니다.

## SensorTestMap 재생성

`Scripts/setup_sensor_test_map.py`는 다음 작업을 수행합니다.

- 누락된 세 WBP를 올바른 native parent로 생성
- `SensorTestManaged` 태그 Actor만 교체
- 카메라/LiDAR/Manager/HostActor와 테스트 타깃 배치
- 약 16×12×4m의 밝은 개방형 실내 테스트 홀과 천장 Rect Light 4개 배치
- HostActor에 세 WBP와 ZOrder 연결
- 현실적인 장비 메타데이터와 `RealTimePreview` 기본값 적용

사용자가 배치한 비관리 Actor는 삭제하지 않으며 Slab mesh는 생성하지 않습니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  ".\ma0t10_dt.uproject" `
  -ExecutePythonScript=".\Scripts\setup_sensor_test_map.py" `
  -Unattended -NoSplash -NoSound
```

## 빌드

Editor와 게임을 종료한 뒤 실행합니다. Live Coding이 켜진 Editor가 열려 있으면 UBT가 빌드를 거부합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" `
  ma0t10_dtEditor Win64 Development `
  "-Project=$PWD\ma0t10_dt.uproject" `
  -WaitMutex -NoHotReloadFromIDE
```

Editor 안에서 반복 개발 중이면 `Ctrl+Alt+F11`로 Live Coding을 사용하고, IDE 빌드를 할 때는 Editor를 종료합니다.

## 테스트

전체 smoke script:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1"
```

이미 Editor target을 빌드했다면:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1" -SkipBuild
```

현재 주요 automation 그룹:

| 그룹 | 범위 |
|---|---|
| `MA0T10.SensorControl` | 패널 clamp, UI 상태 직렬화, 상태 검증, 기즈모 좌표 계산, map apply queue, 현실적 프로필 |
| `MA0T10.SensorPerformance` | 60/30 FPS 단계, FullSpec 규격, 부하 계산과 도움말 coverage |
| `MA0T10.SensorDebug` | 선택 센서 투사 범위의 성능 예산과 기본값 |
| `MA0T10.SensorExport` | 저장 경로 요약, 최근 경로 복사 오류와 export 계약 |
| `MA0T10.SensorMonitor` | native fallback, 테마 대비, LiDAR 모드·의미 색상, status, payload export |
| `MA0T10.EditorSmoke` | SensorTestMap, 센서, HostActor, WBP 구성 |
| `MA0T10.SensorReplay` | CSV/JSONL replay, payload, export |
| `MA0T10.SensorManager` | shared service와 point-cloud-only 정책 |
| `MA0T10.SensorTransport` | HTTP loopback, retry와 backpressure |
| `MA0T10.SensorRecorder` | recording save/load |
| `MA0T10.RealSensorSource` | live JSON/HTTP/UDP/WebSocket source 경로 |
| `MA0T10.Sensor.CsvPointCloudPreview` | 대용량 CSV preview fallback |

개별 실행 예시:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  ".\ma0t10_dt.uproject" `
  -NullRHI -Unattended -NoSplash -NoSound `
  '-ExecCmds=Automation RunTests MA0T10.SensorControl; Quit' `
  '-TestExit=Automation Test Queue Empty'
```

긴 acceptance·성능 검증 명령은 [Editor Smoke/Automation 문서](docs/editor_smoke_test.md)에 정리되어 있습니다.

실제 RHI FullSpec 2+2 측정은 다음 명령으로 실행합니다. 스크립트는 런타임에만 추가 센서를 생성하고 맵 asset은 변경하지 않으며, 10초 워밍업 뒤 60초 데이터를 `Saved/Reports`의 JSON/Markdown으로 저장합니다.

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_fullspec_performance_evidence.ps1" -CameraCount 2 -LidarCount 2
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_fullspec_performance_evidence.ps1" -CameraCount 4 -LidarCount 4
```

2026-07-17 현재 개발 PC(Ryzen 7 7800X3D, RTX 4070 Ti)의 D3D12 측정값은 2+2에서 평균 58.94 FPS / 1% low 49.96 / p95 18.41ms, 4+4에서 평균 53.16 FPS / 1% low 33.91 / p95 27.37ms였습니다. 이는 해당 실행의 증거이며 다른 맵·드라이버·장비에서의 보장값은 아닙니다.

## 저장소 구조

```text
Content/MA0T10/                 Unreal assets, maps, WBP
Source/ma0t10_dt/MA0T10/       runtime module
Source/ma0t10_dtEditor/        Editor-only PIE map persistence
Plugins/DTCore/                external git submodule
Scripts/                       map setup, validation, report helpers
Samples/                       replay/live payload samples
docs/                          contracts and operating guides
```

C++ module namespace는 `ma0t10_dt`, Content package namespace는 `/Game/MA0T10`입니다. 과거 `/Game/M7AT10` 경로는 새 코드나 asset 참조에 사용하지 않습니다.

## DTCore submodule

`Plugins/DTCore`는 외부 submodule입니다. 이 저장소 기능 작업에서 임의로 내부 소스나 gitlink를 변경하지 않습니다.

```powershell
git submodule status
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_dtcore_submodule_guard.ps1"
```

현재 작업 규칙과 고정 commit은 [AGENTS.md](AGENTS.md)를 따릅니다.

## 문서

현재 유지하는 문서는 다음과 같습니다.

- [SensorTestMap과 WBP 설정](docs/sensor_test_map_setup.ko.md): 기본 사용자 가이드
- [Editor Smoke/Automation](docs/editor_smoke_test.md): 상세 테스트와 acceptance 절차
- [LiDAR Payload Schema](docs/lidar_payload_schema.md)
- [Camera Payload Schema](docs/camera_payload_schema.md)
- [Server Transport Contract](docs/server_transport_contract.md)
- [Real Sensor Adapter Plan](docs/real_sensor_adapter_plan.md)
- [Local Asset Report](docs/local_asset_report.md): 로컬 asset 판정 도구 설명
- [Remaining Work](docs/remaining_work.md): 외부 결정·장비 증거가 필요한 장기 항목
- [Pixel Streaming Setup](docs/pixel_streaming_setup.md): 현재 센서 범위 밖인 sample 보존 정책

모니터 Designer hierarchy와 Optional Bindings는 SensorTestMap/WBP 가이드에서 함께 관리합니다.

## 로컬 변경과 커밋 전 점검

`Config/Game.ini`, marketplace content, packaged output, WBP `.uasset`, DTCore submodule 변경은 자동으로 커밋 대상으로 간주하지 않습니다.

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\invoke_local_decision_precommit_gate.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_goal_progress_blocker_report.ps1"
```

이 도구들은 기본적으로 상태와 evidence gap을 보고하며 자동 staging을 수행하지 않습니다.

## 알려진 제한

- 실제 D455 depth calibration/stream이 아니라 SceneCapture 기반 preview입니다.
- 실제 Livox/RealSense/ROS2 SDK adapter와 배포 장비 검증이 필요합니다.
- 실제 LAZ 압축은 외부 compressor 설정과 검증이 필요합니다.
- 고밀도 실시간 point cloud는 현재 CPU fallback 중심이며 GPU/Niagara backend는 후보 단계입니다.
- 실제 judging server endpoint, 인증, 응답 schema와 운영 backpressure 정책은 환경 소유자 결정이 필요합니다.
- DTCore는 일부 USTRUCT 기본 초기화 및 `EnhancedInput` dependency 경고를 출력할 수 있습니다.
