# UE-DT-Project

UE 5.3 / Windows / D3D12 기반 가상 센서와 Slab·해치·코일·학습 업무 프로젝트입니다. **#99~#104 완료 상태**, 문서 기준 `master` 80e03ed1(2026-11-05)입니다. DTCore 고정값은 `cb9788a2338ad92e6a651be71330f2abe4b63a90`; DTDevice는 [pin 파일](Scripts/device_plugin_pin.txt)의 정확한 커밋을 사용합니다.

사용법은 이 문서, 작업 권한·보호 규칙은 [AGENTS.md](AGENTS.md), 세부 문서는 [docs 색인](docs/README.md)을 따릅니다. 완료된 계획은 [archive](docs/archive/README.md)에 보존하며 현재 승인이나 보장으로 읽지 않습니다. 저작권 고지는 [LICENSE](LICENSE)를 따릅니다.

## 1. 현재 상태와 검증 범위

- #99 장치 플러그인 분리, #100 에셋·TestHost, #101 이름·폴더, #102 포맷, #103 DTCore 기능 통합, #104 데이터 채널 통합이 반영됐습니다.
- 최근 코드 검증 r55f(`023c2444`, Device `dca79e3`, Core `cb9788a`): release **536 Success / 46 Skip / 0 Fail**, 고유578개·누락0, 테스트 맵10개 읽기 전용 로드·대상 경고0, 두 guard PASS입니다. 문서 작업에서 빌드·Unreal을 다시 실행한 결과는 아닙니다.
- r55e: TestHost 실제43 Success / 1 Skip, Development 패키지 공통·Windows·재시작 설정3/3 및 pak 행 URL 편집 후 재시작 확인이 통과했습니다. 설정 검사는 NullRHI이며 live broker 수용과 구분합니다.
- 메모리 미해결, 장애 전환 중 PCD 유실, loose 설정 적용 범위는 **[알려진 미해결](docs/KNOWN_ISSUES.md)**에서만 현재 상태를 관리합니다. release 통과를 모든 메모리·현장 수용 완료로 해석하지 않습니다.
- 날짜별 원본은 [VALIDATION_HISTORY](docs/VALIDATION_HISTORY.md)에 보존합니다. skipped/미수행·다른 SHA의 결과는 실제 통과로 세지 않습니다.

## 2. 세 저장소의 관계와 받기

| 저장소 | 역할 | 프로젝트 연결 |
|---|---|---|
| UE-DT-Core-Plugin | DTCore: 객체·데이터·위젯·API·채널 연결/등록/상태 기반 | `Plugins/DTCore` 필수 submodule |
| UE-DT-Device-Plugin | 장치 획득·스케줄러·코덱·전송·native RT·공통 UI body | `Plugins/DTDevice` 필수 submodule |
| UE-DT-Project | Slab·해치·코일·크레인·학습·Workspace와 과제 UI/adapter | 이 저장소 |

```powershell
git clone --recursive https://github.com/YonghoKwon/UE-DT-Project.git
# 기존 clone에서는 parent가 고정한 커밋만 초기화:
git submodule update --init Plugins/DTCore Plugins/DTDevice
git submodule status
```

`--remote`로 임의 최신 버전을 고르지 않습니다. 소비 프로젝트는 두 gitlink를 고정하고, 플러그인 내부에 DTCore를 다시 중첩하지 않습니다. 변경 권한·pin 검증은 [AGENTS §2](AGENTS.md#2-보호승인-규칙)를 따릅니다.

## 3. 빌드와 테스트

프로젝트 절대 경로를 사용하고 실행 중 Editor/Live Coding을 먼저 정상 종료합니다. 아래는 저장소 루트 PowerShell 예시입니다. 보호 맵 재생성·resave는 빌드 준비 절차가 아닙니다.

```powershell
$project = Join-Path $PWD 'ma0t10_dt.uproject'
& 'C:/Program Files/Epic Games/UE_5.3/Engine/Build/BatchFiles/Build.bat' ma0t10_dtEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
& 'C:/Program Files/Epic Games/UE_5.3/Engine/Build/BatchFiles/Build.bat' ma0t10_dt Win64 Development "-Project=$project" -WaitMutex
& 'C:/Program Files/Epic Games/UE_5.3/Engine/Build/BatchFiles/Build.bat' ma0t10_dt Win64 Shipping "-Project=$project" -WaitMutex
powershell -ExecutionPolicy Bypass -File Scripts/run_smoke_tests.ps1 -ProjectPath $project -SkipBuild -Rhi D3D12
powershell -ExecutionPolicy Bypass -File Scripts/verify.ps1 -Level quick
powershell -ExecutionPolicy Bypass -File Scripts/verify.ps1 -Level release -BaseCommit <기준-SHA> -Tests 'DTCore.+MA0T10.SensorReceiver'
powershell -ExecutionPolicy Bypass -File Scripts/verify.ps1 -Level perf -BaseCommit <기준-SHA> -PerfRayTracing ProjectDefault
powershell -ExecutionPolicy Bypass -File Scripts/validate_dtcore_submodule_guard.ps1 -RequireConnected
powershell -ExecutionPolicy Bypass -File Scripts/validate_device_plugin_submodule_guard.ps1 -RequireConnected
```

`quick`은 정적 검사, `release`는 Editor 빌드·자동화·실제 D3D12·WBP 메모리 컴파일 등을 포함합니다. `perf`는 별도 RT-on ABAB 상대 2% gate입니다. 명령·판정·환경·메모리 구간은 [검증 기준](docs/BUILD_AND_VALIDATION.md)을 따릅니다. TestHost만 확인할 때는 [DTDevice 시작 가이드](Plugins/DTDevice/Docs/GettingStarted.md)를 사용합니다.

## 4. 과제별 기능과 화면

| 기능 | 사용 흐름과 경계 |
|---|---|
| 공통 장치 | 가상 LiDAR·카메라 배치 → 프로필/출력 선택 → 측정. 설정·수동 캡처·파일 저장·미리보기는 기존 장치 API를 사용합니다. |
| 센서 도구 | `UI/Freeview`의 설정·모니터·데이터·배치 패널을 Workspace가 소유합니다. 숨김은 측정/송신/파일 작업 중지가 아닙니다. |
| Slab | DTCore TC/DataSync 수신 → 원본 시나리오 보관 → 사용자 실행/재실행. 자동 수신 보관과 실행 큐를 구분합니다. [Slab](docs/slab_simulation.ko.md), [재실행](docs/slab_scenario_replay.ko.md). |
| 해치 | 준비·센서 배치·퍼내기·분석 결과를 과제 host/UI가 소유합니다. [직접 점검표](docs/MANUAL_CHECKLIST.md)에서 자동화와 수동 수용을 구분합니다. |
| 코일·크레인 | 제품 출하부두 배치·센서 식별·선적/적재도와 크레인 표현은 프로젝트 업무 코드입니다. 공통 장치 플러그인으로 넘기지 않습니다. |
| 로컬 데이터셋·표준 export | 장면 라벨·manifest/PCD·카메라 결과를 생성하고 COCO/KITTI 형식으로 내보냅니다. 원본 측정과 export 변환을 구분합니다. |
| Agent 테스트셋 | 일반 시나리오 우선 정책을 유지하며 수신/로컬 입력을 테스트셋으로 보관·생성하고 확정 결과를 보기 전용으로 조회합니다. |
| 학습 배치 | 기본 꺼짐. 20Hz `sampled_timeline` Job을 워커 프로세스에 분할하고 확정 결과를 ZIP/tar.gz로 묶어 OpenSSH SFTP로 전달합니다. tar.zst는 미지원입니다. [학습 배치](docs/TRAIN_BATCH.md). |
| 외부 장치 입력 | 정규화된 프레임을 `SubmitExternalFrame` 계열로 전달합니다. 진단 수신을 다시 측정/송신에 주입하지 않습니다. [adapter](docs/SENSOR_ADAPTERS.md). |
| Pixel Streaming | 필요한 선택 기능입니다. 모듈 의존·플러그인 활성화·로컬 `Samples/PixelStreaming`을 사용처 검색만으로 제거하지 않습니다. [설정](docs/pixel_streaming_setup.md). |

일반 시나리오의 기존 입력은 `topic.scenario` / `IFactory-agent`입니다. 새 장치 채널 설정이 이 업무 구독을 자동 교체하지 않습니다.

## 5. 장치와 패널 배치

프로젝트 UI는 DTCore `UDxWidget`과 기존 Workspace 소유권을 유지합니다. 공통 base/body는 DeviceUI에 있고 과제 façade와 WBP는 프로젝트에 남습니다. 현재 예시는 `UDxWidgetVirtualSensorSettingsPanel`, `UDxWidgetVirtualSensorMonitorPanel`, `UDxWidgetVirtualSensorCaptureExportPanel`입니다.

장치만 붙이는 새 소비 프로젝트는 `AVirtualLidarSensorActor`, `AVirtualCameraSensorActor`, `ADeviceSensorMonitorHostActor`와 `UDxDeviceSensorMonitorPanel`을 사용합니다. Host 한 개가 패널을 생성·바인딩하며 Level Blueprint 생성 경로를 중복 추가하지 않습니다. [빈 C++ 프로젝트 절차](Plugins/DTDevice/Docs/GettingStarted.md)는 현재 클래스·include·에셋 경로를 따릅니다.

공통 에셋은 `/DTDevice/Sensor/Materials/M_LidarSemanticId`, `/DTDevice/Sensor/VFX/NS_VirtualLidarPointCloud`, `/DTDevice/Sensor/VFX/M_VirtualLidarPointSprite`입니다. 기존 맵/BP/SaveGame은 최종 이름으로 직접 연결된 redirects로 호환하며 기존 자산을 다시 저장하지 않습니다.

## 6. 데이터 I/F와 현장 설정

**DTCore는 연결·등록·상태·재연결 메커니즘, 프로젝트는 토픽 정책**을 소유합니다.

| 채널 | 구현·용도 |
|---|---|
| `Default` | DTCore 기존 공유 연결. 명시 선택한 상태·수동1회·결과 JSON용 `DTCoreWebSocket` 모드. |
| `sensor` | `UDxDeviceSensorStompChannel : UDxStompChannel`, JSON·수동 binary 및 기존 receipt/ACK 호환. |
| `sensor-bulk` | `UDxDeviceSensorBulkChannel : UDxDataChannelBase`, Worker 정책. 기존 Raw TCP STOMP worker의 immutable PCD/body·reservation·backpressure·receipt에 위임. |

`UDxDeviceSensorChannelSubsystem`은 GameInstance 수명에 등록을 묶습니다. 기본 등록만으로 연결·tick·worker·구독을 시작하지 않습니다. 화면 없는 워커도 DeviceRuntime을 사용할 수 있습니다.

- 연결: `[/Script/DTCore.DTCoreSettings]`의 `DataChannels`(Default는 기존 `WebSocketUrl` 등 공유 설정).
- 토픽: `[/Script/DeviceRuntime.DeviceDataChannelSettings]`의 `Channels` → 채널별 Alias / Destination / bSubscribe / Purpose.
- 공통: `Config/DefaultGame.ini`; 플랫폼: `Config/Windows/WindowsGame.ini`, `Config/Linux/LinuxGame.ini`.
- Windows 패키지: `Windows/ma0t10_dt/Config/DefaultGame.ini`와 그 아래 `Windows/WindowsGame.ini` loose 파일을 수정하고 **재시작**합니다.
- 지원 속성은 플랫폼 loose → 공통 loose → 기존 pak/사용자 설정 순서입니다. 공통 파일에 목록이 있으면 pak 목록을 대체하며, 같은 채널 이름의 뒤 행이 앞 행을 대체합니다.
- 빈 새 항목은 기존 `SensorTransport.ini`/맵/UI profile 대체 경로를 유지합니다. 연결값과 토픽값은 독립적으로 이전할 수 있습니다.
- 비밀번호·토큰을 ini·로그·문서·커밋에 넣지 않습니다. Runtime Override **키 이름**만 지정하고 실제 값은 기존 사용자 override 경로로 관리합니다. URL은 `"ws://…"`처럼 인용합니다.

별칭은 camera/lidar/pointcloud/imu/ack를 사용하며 추가 수신 별칭을 둘 수 있습니다. 채널 내 같은 destination은 중복 구독하지 않고, 채널 간 설정 중복은 시작 경고·상태로 알리되 연결을 막지 않습니다. 상세 목록·변경 절차·수신 의미는 [Artemis 설정](docs/ARTEMIS_PCD_SETTINGS.md)과 [DataChannels](Plugins/DTDevice/Docs/DataChannels.md)를 따릅니다.

## 7. 출력 계약

기본 자동 출력은 PCD이며 미리보기 양을 줄여 서버 payload를 줄이지 않습니다. PCD XYZ는 센서 로컬 meter, X 전방/Y 좌측/Z 위쪽이고 **33-byte little-endian 명시 레코드**입니다. 월드 복원은 획득 snapshot의 `sensor_to_world_m`를 사용합니다. FrameId·Slab frame_no·ScenarioUUID·RunUUID를 구분합니다.

획득/생략, encode, submit, broker receipt, 내부 검증, 외부 소비자 ACK를 별도로 집계합니다. receipt는 소비자 처리 완료가 아닙니다. 정상 연결의 ConnectedNoLoss는 FIFO이며 전역128MiB/스트림64MiB와 Camera8/기타20프레임 한도를 유지합니다. wire 키·schemaVersion·로그 문구·테스트 이름 `MA0T10.*`는 이름 정리와 무관하게 유지합니다.

상세: [LiDAR schema](docs/lidar_payload_schema.md), [Camera schema](docs/camera_payload_schema.md), [전송 계약](docs/server_transport_contract.md), [스트리밍](docs/sensor_streaming.ko.md).

## 8. 저장·안전 상태

Workspace 창 위치, SensorUI 표시·출력, Appearance 폰트와 학습 Job/manifest의 책임을 구분합니다. 기존 SaveGame 슬롯·필드는 새 기능 때문에 삭제하거나 이름을 바꾸지 않습니다. 사용자 설정을 검사할 때는 백업·복원합니다.

native LiDAR는 장치/드라이버 조합별 startup oracle 및 안전 상태를 사용합니다. 비정상 종료 후 복구 실행, 조합 비활성·초기화는 [native 안전 상태](docs/NATIVE_LIDAR_SAFETY.md), 현장 메모리 관찰은 [운영 가이드](docs/SITE_MEMORY_AND_LIDAR_GUIDE.md)를 따릅니다. 정상 장치의 soft disable과 device loss를 구분합니다.

## 9. 이름과 코드 스타일

현재 ActorComponent는 `…Comp`, DTCore 기반 과제 위젯은 `UDxWidget<기능>`, 공통 DeviceUI는 `UDxDeviceSensorPanelBase` / `UDxDeviceSensorMonitorPanel`입니다. 이미 Comp 이름이던 크레인 세 타입, 엔진·DTCore 타입과 동결 계약을 임의로 바꾸지 않습니다.

[현행 규칙](docs/STYLE_AND_NAMING.md), [.clang-format](.clang-format), [.git-blame-ignore-revs](.git-blame-ignore-revs)를 사용합니다. 포맷과 로직 변경은 별도 커밋으로 하고 토큰 동일성 검증을 유지합니다.

## 10. 아키텍처 뼈대

```text
Plugins/DTCore/                         별도 저장소: 기반 객체·UI·데이터·채널
Plugins/DTDevice/                       별도 저장소: 공통 장치
  Source/DeviceNative/                  조기 RT·셰이더·native 안전 상태
  Source/DeviceRuntime/                 Contracts/Camera/Core/Sensor/Transport
  Source/DeviceUI/                      공통 panel base/body·독립 monitor
  Content/Sensor/                       공통 장치 에셋
Source/ma0t10_dt/MA0T10/
  Contracts/ Common/                    과제 값 계약·공용 코드
  Sensor/ Sensor/Tool/ Diagnostics/      host adapter·배치 도구·opt-in probe
  Ship/ Coil/ Crane/ Slab/               과제 업무·장면
  Slab/Scenario/ Training/ Dataset/ Agent/
  Manager/Automation/ WebSocket/ Api/   실행 조율·업무 전문·API
  UI/Common/                           공통 상태·Workspace·appearance
  UI/Freeview/                         이동 가능한 과제 패널
  UI/Topview/                          상위 고정 화면·학습 화면
  UI/Main/{Sensor,Ship,Coil,Slab}/      장면별 고정 화면
Source/ma0t10_dtBootstrap/               host WebSockets 1ms 정책 shim
Source/ma0t10_dtEditor/                  commandlet·Editor 자동화
```

### 10.1 의존 방향

중립 Contracts → DeviceRuntime(장치 L1) → 과제 업무 L2 → UI L3 방향만 허용합니다. DTDevice가 host `ma0t10_dt`를 include하거나 UHT 역참조하지 않습니다. 공통 구현은 plugin base/body 한 곳이 실행 상태를 소유하고 host façade는 기존 BP/저장 호환을 유지합니다. 과제 semantic 표·run 집계·분석은 host hook/provider로 등록합니다.

### 10.2 현재 위반

ARCH layer allowlist는 **0**입니다. [layer 검사](Scripts/validate_architecture_layers.ps1)의 의미와 예외 수를 이름/문서 정리로 늘리지 않습니다. 파일·모듈 이동, 기능 변경, 에셋 이동은 각각 별도 승인을 받습니다.

### 10.3 모듈 책임

DeviceNative는 PostConfigInit, DeviceRuntime은 Default, DeviceUI는 None(명시 로드)입니다. DTCore는 필수이고 DeviceRuntime은 UI 없는 실행을 지원합니다. host 업무·Workspace·WBP·SaveGame은 프로젝트에 남습니다. 구성은 [DTDevice 모듈](Plugins/DTDevice/DTDevice.uplugin) 및 실제 Build.cs를 우선합니다.

## 11. 테스트 opt-in 환경 변수

다음 표는 소스의 명시 opt-in을 감사하는 목록입니다. 비밀값 목록이 아니며 값을 로그에 출력하지 않습니다. 미설정 skip을 성공으로 바꾸지 않습니다.

<!-- MA0T10_ENV_TABLE_BEGIN -->

| 변수 | 활성화/설정 대상 시험·fixture | 값 의미·필요한 환경 |
|---|---|---|
| `MA0T10_AUTOMATION_TOKEN` | AUTO-01 local control API | Optional Bearer token (ASCII 33..126, max 256 chars); does not enable API; never logged or saved |
| `MA0T10_PCD20HZ_CASE` | MA0T10.SensorStream.Pcd20Hz.SocketFaults | healthy/slow/heartbeat/silent/error/reset/observe; Tools/StompFaultServer 로컬 TCP fault 서버 |
| `MA0T10_PCD20HZ_SECONDS` | MA0T10.SensorStream.Pcd20Hz.SocketFaults | 8~600초; 합성 socket 부하 시간 |
| `MA0T10_PCD20HZ_INTERVAL` | MA0T10.SensorStream.Pcd20Hz.SocketFaults | 0.05~0.2초; 합성 프레임 간격 |
| `MA0T10_PCD20HZ_LIDARS` | MA0T10.SensorStream.Pcd20Hz.SocketFaults | 1~2; 합성 스트림 수, 실제 acquisition 검증과 구분 |
| `MA0T10_PCD20HZ_NATIVE_INTERVAL` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | PCD-20HZ 전용 0.05~0.2초; Actor 편집 API로 시험 주기 적용, 상태바·Raw·Engine 시간축 기록 |
| `MA0T10_ACCEPTANCE_LEDGER` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke / RepeatThreeStreamAcceptance | 수용 ledger 출력 경로; 쓰기 가능한 Saved 하위 경로 |
| `MA0T10_ACCEPTANCE_RES_X` | FinalAcceptanceTestSession 사용 RHI/PIE 시험 | client viewport 폭 정수; 실제 RHI/PIE |
| `MA0T10_ACCEPTANCE_RES_Y` | FinalAcceptanceTestSession 사용 RHI/PIE 시험 | client viewport 높이 정수; 실제 RHI/PIE |
| `MA0T10_ARTEMIS_PASSWORD` | SensorStream / SensorV2 / SlabScenarioReplay 및 ExternalSource receiver | 인증 비밀번호; Broker 시험 스크립트·Node probe 필수(없으면 중단), 비밀값은 기록·커밋하지 않음 |
| `MA0T10_ARTEMIS_RAW_URL` | MA0T10.SensorStream.HighThroughput.RawTcpArtemisLoopback | 기존 Raw TCP STOMP URL; Raw Broker |
| `MA0T10_ARTEMIS_URL` | SensorStream / SensorV2 / SlabScenarioReplay 및 ExternalSource receiver | 기존 STOMP/WS URL; 시험에 맞는 Broker·외부 구독자 |
| `MA0T10_ARTEMIS_USER` | SensorStream / SensorV2 / SlabScenarioReplay 및 ExternalSource receiver | 인증 사용자 이름; Broker 시험 스크립트·Node probe 필수(없으면 중단), 비밀값은 기록하지 않음 |
| `MA0T10_CAMERA_MARKER_RHI` | MA0T10.SensorFiles.CameraMarkerRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_DIAGNOSTIC_HIDE_OWNED_UI` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; 진단 fixture 소유 UI 숨김 |
| `MA0T10_DTCORE_SLAB_BROKER` | MA0T10.DTCoreIntegration.SlabBroker | 1; 실제 Broker + 외부 synthetic scenario publisher |
| `MA0T10_GEOMETRY_BASELINE` | MA0T10.LidarGeometry.StreamPerformance | 비교할 이전 geometry 성능 JSON 경로 |
| `MA0T10_GEOMETRY_PERF` | MA0T10.LidarGeometry.StreamPerformance | 1; 실제 RHI + local Artemis + geometry fixture |
| `MA0T10_INPUT_ACCEPTANCE_RHI` | MA0T10.SensorV2.UI.InputAcceptanceRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_LAYOUT_API_FIXTURE` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke / SlabScenario.ProductionRuntime / SlabReliability.RepeatedRuntime | 1; 기존 layout API fixture 설정 |
| `MA0T10_LIDAR_SEMANTIC_BENCHMARK` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; 실제 RHI semantic benchmark fixture |
| `MA0T10_MANIPULATION_RHI` | MA0T10.SensorV2.UI.ManipulationRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_OUTPUT_SENSOR_PAIRS` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke / SlabScenario.ProductionRuntime / SlabReliability.RepeatedRuntime | 1 또는 2; Camera+LiDAR 쌍 수 선택·실제 RHI fixture |
| `MA0T10_OUTPUT_SUBMISSION_BUDGET` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 프레임당 submit 예산 정수; 운영 기본값은 변경하지 않음 |
| `MA0T10_PCD_DEMAND_ACCEPTANCE` | ContinuousThreeStreamSmoke / SlabScenario.ProductionRuntime / SlabScenarioValidationRig | 1; 기존 PCD demand fixture·실제 RHI |
| `MA0T10_PCD_JSON_POLICY` | PCD demand / stream / Slab fixture의 기존 JSON 정책 선택 | 기존 policy selector; 관련 runner의 인자/소스 참조 |
| `MA0T10_PCD_ONLY` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; stream fixture PCD-only 선택 |
| `MA0T10_PLACEMENT_AB_RHI` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_PLACEMENT_PRESENTATION_RHI` | PlacementUi.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_PLACEMENT_RHI` | MA0T10.Placement.InputOwnershipRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_RAW_PROBE` | MA0T10.SensorStream.RawReceiptProbe | 1; Tools/Artemis/raw_receipt_probe.mjs의 local fault endpoint |
| `MA0T10_REPEAT_STREAMS_RHI` | MA0T10.SensorV2.Runtime.RepeatThreeStreamAcceptance | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_REPLAY_REPORT` | MA0T10.ScenarioReplay.Runtime | replay 보고서 경로; 쓰기 가능한 Saved 하위 경로 |
| `MA0T10_REPLAY_RHI` | MA0T10.ScenarioReplay.Runtime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_RESOURCE_MANUAL_FIXTURE` | SensorResource.ManualFixture | 1; 사용자 승인된 직접 조작 세션·화면 필요 |
| `MA0T10_RUN_ARTEMIS_SMOKE` | MA0T10.SensorStream.ArtemisIntegration<br>MA0T10.SensorStream.HighThroughput.RawTcpArtemisLoopback | 1; 실행 중 Broker + 인증 + 외부 구독자 |
| `MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; 실제 RHI + 커밋본 맵 + Broker + 외부 검증 |
| `MA0T10_RUN_SLAB_SCENARIO_SMOKE` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; 실제 RHI + Slab scenario fixture + Broker |
| `MA0T10_SCENARIO_FAULT_CYCLES` | MA0T10.SlabReliability.TcpFaultRuntime | 반복 횟수 정수; local TCP fault fixture |
| `MA0T10_SCENARIO_FAULT_MODE` | MA0T10.SlabReliability.TcpFaultRuntime | 기존 fault 모드 문자열; local TCP fault fixture |
| `MA0T10_SENSOR_FILES_RHI` | MA0T10.SensorFiles.RealRhiWorkflow | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SENSOR_RESOURCE_DIAGNOSTICS` | 기존 Slab/수용 fixture (관련 runner 참조) | 1; 기존 fixture 진단 수집 |
| `MA0T10_SENSOR_RESOURCE_MEMREPORT` | 기존 Slab/수용 fixture (관련 runner 참조) | 1; 기존 fixture 메모리 보고 수집 |
| `MA0T10_SENSOR_RESOURCE_PREVIEW_OFF` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; resource fixture 미리보기 숨김 |
| `MA0T10_SENSOR_RESOURCE_REPORT_DIR` | 기존 Slab/수용 fixture (관련 runner 참조) | resource 보고서 폴더; 쓰기 가능한 Saved 하위 경로 |
| `MA0T10_SLAB_ANALYSIS_OFF` | 기존 Slab/수용 fixture (관련 runner 참조) | 1; Slab fixture 분석 표시 숨김 |
| `MA0T10_SLAB_ANALYSIS_RHI` | MA0T10.SlabAnalysis.CameraGpuIsolationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_EXACT_VIEWPORT` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; 설정한 acceptance 해상도를 엄격히 사용 |
| `MA0T10_SLAB_LOCAL_BROKER` | 기존 Slab/수용 fixture (관련 runner 참조) | 1; Slab fixture의 기존 local Broker 선택 |
| `MA0T10_SLAB_OUTPUT_ALL` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; Slab fixture 추가 출력 선택 |
| `MA0T10_SLAB_PORTABLE` | MA0T10.SlabScenario.PortableMapAndLatePlayer | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_REPEAT_COUNT` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 실행 반복 횟수 정수; Slab fixture |
| `MA0T10_SLAB_REPEAT_RHI` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_REQUIRE_DATA` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; Slab fixture 데이터 필수 출력 선택 |
| `MA0T10_SLAB_RESET_RHI` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_RHI` | MA0T10.SlabScenario.ProductionRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_RUNTIME` | MA0T10.SlabScenario.ProductionLifecycle | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_STREAM_MEASURE_SECONDS` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 측정 시간(초); 실제 RHI stream fixture |
| `MA0T10_STREAM_WARMUP_SECONDS` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 준비 시간(초); 실제 RHI stream fixture |
| `MA0T10_UXR01_PRESENTATION_RHI` | UXR01.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR02_MANUAL` | UXR02.Manual | 1; Computer Use 직접 조작 세션·화면 필요 |
| `MA0T10_UXR02_RENDER` | UXR02.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR03_RENDER` | UXR03.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR04_RENDER` | UXR04.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR05_RENDER` | UXR05.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR06_RENDER` | UXR06.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR08_RENDER` | UXR04.PresentationRhi<br>UXR08.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR_PERF` | UXR02.Performance | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR_PERF_NAME` | UXR02.Performance | 성능 비교 run 이름 |
| `MA0T10_UXR_PERF_VIEW` | UXR02.Performance | preview/transfer/settings/scenario/scenario-monitor; 미설정은 기본 배치 화면 |
| `MA0T10_UXR_REPORT_ROOT` | UXR02.Performance / UXR02~06/08 렌더 fixture | UXR 증거 출력 폴더; 쓰기 가능한 Saved 하위 경로 |
| `MA0T10_WORKSPACE_RHI` | MA0T10.SensorWorkspace.Runtime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_WORKSPACE_SCREENSHOT` | MA0T10.SensorWorkspace.Runtime | screenshot 파일 경로; 실제 RHI + 쓰기 권한 |
| `MA0T10_DATASET_SCENES` | MA0T10.Dataset.RealRhiGeneration | 명시한 10–100장면 실제 RHI 생성; Saved/Datasets에 시험 파일을 만드는 opt-in |
| `MA0T10_TRAIN_WORKERS` | MA0T10.TrainRuntime.ProcessRhi | Editor 자동화의 워커 요청 상한1~4. 실제 수는 GPU 예산으로 제한하며 일반 제품 실행에서는 읽지 않는다. |
| `MA0T10_SFTP_LIVE_HOST` | 기본 미지정 · 사용자 승인 수신 서버 테스트 전용 | `MA0T10.TrainBatch.SftpLive`; 대상 호스트; HOST 미지정은 semantic skip. 서버 설치·시스템 설정 자동 변경 없음. |
| `MA0T10_SFTP_LIVE_PORT` | 기본 미지정 · 사용자 승인 수신 서버 테스트 전용 | `MA0T10.TrainBatch.SftpLive`; 대상 포트; HOST 미지정은 semantic skip. 서버 설치·시스템 설정 자동 변경 없음. |
| `MA0T10_SFTP_LIVE_USER` | 기본 미지정 · 사용자 승인 수신 서버 테스트 전용 | `MA0T10.TrainBatch.SftpLive`; SSH 사용자; HOST 미지정은 semantic skip. 서버 설치·시스템 설정 자동 변경 없음. |
| `MA0T10_SFTP_LIVE_KEY` | 기본 미지정 · 사용자 승인 수신 서버 테스트 전용 | `MA0T10.TrainBatch.SftpLive`; 개인키 경로(값/내용 로그·커밋 금지); HOST 미지정은 semantic skip. 서버 설치·시스템 설정 자동 변경 없음. |
| `MA0T10_SFTP_LIVE_DIR` | 기본 미지정 · 사용자 승인 수신 서버 테스트 전용 | `MA0T10.TrainBatch.SftpLive`; 허용된 원격 테스트 디렉토리; HOST 미지정은 semantic skip. 서버 설치·시스템 설정 자동 변경 없음. |
| `MA0T10_DATASET_PYTHON` | EXPORT-01 선택적 오프라인 변환 | requirements를 설치한 python.exe의 절대 경로. export_format 기본 none에서는 읽거나 실행하지 않는다. |
| `MA0T10_STOMP_USER` | IMU-01 Editor acceptance fixture only | Existing broker session username, read only with explicit `-ImuAutomated -ImuBroker`; never logged/saved; does not enable production output |
| `MA0T10_STOMP_PASSWORD` | IMU-01 Editor acceptance fixture only | Existing broker session passcode, inherited process environment only with explicit `-ImuAutomated -ImuBroker`; never logged/saved; no broker configuration change |
<!-- MA0T10_ENV_TABLE_END -->

## 12. 세부 문서

[문서 색인](docs/README.md) · [현재 작업 항목](docs/IMPROVEMENTS.md) · [완료 단계/후속 범위](docs/ROADMAP.md) · [알려진 미해결](docs/KNOWN_ISSUES.md) · [이력 원문](docs/VALIDATION_HISTORY.md).

이전 README의 세부 기능·실행 예시·날짜별 서술은 [원문 보관](docs/archive/README_BEFORE_R56.md)에 남겨 두었습니다. 현행 설명과 충돌하면 이 README와 현재 코드가 우선합니다.
