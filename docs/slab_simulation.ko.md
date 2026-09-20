# DTCore Slab 시뮬레이션과 자체 차트

이 기능은 벌크 JSON을 한 번 받아 현재 맵의 Slab를 시간 기준으로 다시 움직입니다. 영상이나 과거 맵의 복원이 아닙니다. 실행 중 시점을 바꾸어 자유롭게 관찰할 수 있습니다. 차트는 프로젝트의 Slate/UMG 구현이며 외부 Chart 플러그인이나 WebBrowser를 사용하지 않습니다.

## 데이터 의미와 단위

| 입력 | 사용 방식 |
|---|---|
| `MESSAGE_ID` | 반드시 `IFactory-agent`. DTCore 전문 분배에 사용 |
| `frame_no` | 원본 시나리오 프레임. Unreal 렌더 프레임·센서 FrameId와 독립 |
| `mtl_no` | 소재 식별자. 한 실행에 하나의 소재 |
| `slab_len`, `slab_wth`, `slab_thk` | 길이·폭·두께. 선택한 치수 단위를 cm로 변환해 Mesh 구성 |
| `center_x` | 진행 기준 Actor의 로컬 X 방향 위치. 선택한 위치 단위 사용 |
| `left_skew_angle` | Slab 회전각(도). 음수는 진행 방향 기준 왼쪽 |
| `right_skew_angle` | 원본 우측 각도 차트에 표시. 이동에 중복 적용하지 않음 |
| `elapsed_sec` | 표본 시각 및 보간 기준 |
| `mov_pos`, `center_y` | 이동에 사용하지 않으며 원본에 보존 |
| `slab_wt` | 원본 정보 표시만 수행. 물리 시뮬레이션 질량 아님 |
| `_meta.UUID` | 발행자 시나리오 UUID. 없으면 내부 보관 ID를 생성하며 원본 JSON은 유지 |
| `_meta.duration_sec` | 전체 재생 길이(선택). 없으면 마지막 표본 시각+표본 주기 |
| `_meta.sample_period_sec` | 마지막 표본 유지 시간(선택). 기본 0.05초 |

**운영 클래스의 기본 단위는 치수·위치 모두 cm**입니다. 예시 `slab_len=10830`은 기본값 그대로라면 **108.3m**입니다. 실제 길이 10.83m를 뜻하는 입력이면 **치수 단위만 mm**로 선택하세요. 위치 단위는 별도로 설정합니다. 전용 검증맵은 치수 mm / 위치 cm를 명시적으로 설정하므로 길이 10.83m, 폭 1.1m, 두께 0.25m, `center_x=909`의 이동 위치는 9.09m입니다.

입력은 JSON 한 건당 UTF-8 8MiB까지입니다. 비정상 JSON, 유한하지 않은 숫자, 잘못된 치수, 실행 중 치수·소재 불일치, 역순 프레임/시각을 거부합니다. 원본 행이 빠져 있어도 행을 임의 생성하지 않습니다. 행 사이 자세만 보간합니다.

## 구조와 DTCore 연결

```text
기존 DTCore WebSocket Topic 구독
  → UDxDataSubsystem 전문 큐
  → UFactoryAgentScenarioTC::ParseToStruct (백그라운드)
  → UFactoryAgentScenarioTC::ProcessStructData (게임 스레드)
  → UDxProcessSubsystem의 ReceiverId 조회
  → USlabDataSyncComponent
  → ASlabActor / 이동·시각화·지표 Component
  → 차트·진행률·시나리오 보관·센서 세션
```

- `ASlabActor : AFacilityBase`는 Crane과 같이 DataSync / MechDriver / StatusVisualizer 계열 Component를 조립합니다.
- `USlabMotionComponent` 하나가 재생 시간을 소유합니다. `TG_PrePhysics`에서 위치를 선형 보간하고 각도는 최단 경로로 보간합니다. 600개 타이머를 만들지 않습니다.
- `USlabMetricsComponent`가 중심 이탈, 꼭지점 이탈, 레일 간격을 계산합니다.
- `USlabVisualizationComponent`는 외곽선·중심점·십자선 등 보조 표시를 담당합니다. 보조 표시는 충돌과 SceneCapture에서 제외합니다. 실제 Slab Mesh는 센서 측정 대상입니다.
- DTCore 플러그인 소스·연결 소유권·동료 Subscription은 변경하지 않습니다.

### 전문 등록과 Editor 재시작

프로젝트 DataTable `/Game/MA0T10/Common/DataTables/DT_TransactionCode`에 `IFactory-agent` → `UFactoryAgentScenarioTC` 행이 필요합니다. 아래 commandlet은 기존 행을 삭제하지 않고 추가합니다. 같은 전문이 다른 처리기에 등록되어 있으면 덮어쓰지 않고 실패합니다.

Editor와 Live Coding을 종료한 후 프로젝트 폴더에서 실행합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" `
  ma0t10_dtEditor Win64 Development "-Project=$PWD\ma0t10_dt.uproject" `
  -WaitMutex -NoHotReloadFromIDE

& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\ma0t10_dt.uproject" -run=EnsureSlabScenarioTransaction -Unattended -NoSplash -NullRHI

& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\ma0t10_dt.uproject" `
  "-ExecutePythonScript=$PWD\Scripts\setup_slab_scenario_validation.py" -Unattended -NoSplash
```

그 다음 **Editor를 새로 실행**하세요. DTCore가 초기화 시 읽은 전문 테이블을 이전 Editor 세션이 계속 사용하게 두지 않습니다. 맵 스크립트는 이미 있는 검증맵을 덮어쓰지 않습니다. 운영 `SensorTestMap`도 저장하지 않습니다.

입력 Topic은 고정 계약이 아닙니다. 기존 DTCore `UDTCoreSettings.WebSocketTopics`의 업무 Topic을 사용하세요. 테스트 발행기의 기본값은 `topic.cep.output.0`입니다. 다른 Topic을 사용하면 DTCore 구독 설정과 발행기의 `--topic`을 동일하게 맞추세요. 센서 출력용 Camera/LiDAR/PCD Topic 설정과는 별개입니다.

## 맵에서 배치하는 방법

새 맵이나 작업용 맵에 아래 Actor를 명시적으로 배치합니다.

1. **SlabTrackReferenceActor**: 롤러 중심선의 기준 위치·회전입니다. 로컬 X=진행, Y=오른쪽, Z=위쪽이며 Scale은 이동 단위로 사용하지 않습니다.
2. **SlabActor**: `TrackReference`에 위 Actor를 지정합니다. `ReceiverId` 기본값 `SlabScenario.Main`은 TC의 `ReceiverId`와 같아야 합니다. 같은 ID를 두 Actor에 등록하지 마세요.
3. `DimensionUnit`, `PositionUnit`, 초기 치수와 `SurfaceMaterial`을 확인합니다. 단위는 실행 전에 선택합니다.
4. **SlabSimulationUiHostActor**: `SlabActor` 참조를 지정합니다. `ChartsWidgetClass`, `ProgressWidgetClass`에 아래 WBP를 연결합니다.
5. PCD 송신이 필요하면 기존 **VirtualSensorCoordinator**, LiDAR/Camera, **VirtualSensorUiHostActor** 및 Broker 설정을 사용합니다. 관찰 전용 재생에는 센서가 없어도 됩니다.

| WBP | Native Parent |
|---|---|
| `WBP_SlabChartsPanel` | `USlabChartsPanelWidget` |
| `WBP_SlabProgressPanel` | `USlabProgressPanelWidget` |

생성 스크립트의 WBP는 빈 Designer 상태에서 native fallback UI를 사용하므로 Event Graph에 버튼 호출을 중복 연결하지 않습니다. 자체 Designer를 작성한다면 `USlabScenarioChartWidget`을 **ScenarioChart**라는 이름으로 배치해 optional binding을 사용할 수 있습니다. 부모의 `BindSlabActor`, `SetChartMetric`, `RefreshChartData`와 chart의 Blueprint API로 연결할 수 있습니다. 외부 UserWidget 내부를 재귀 변경하지 않습니다.

상단 도구 막대의 **Slab 차트 / Slab 진행**으로 패널을 엽니다. 처음 등록된 새 패널은 기본 닫힘입니다. Host가 등록한 인스턴스만 Main Canvas 또는 Viewport에 배치하며, 기존 drag·resize·접기·글자 배율을 지원합니다. 숨겨도 Slab·측정·송신을 중지하지 않습니다.

### 가드레일과 외형

- `LeftRailYcm` / `RightRailYcm`은 기준 좌표계에서 레일 **안쪽 면**의 Y 위치입니다. 표시 Mesh 중심이 아닙니다.
- `bRailsConfigured`를 끄거나 값이 잘못되면 Margin은 `N/A`입니다. 안전한 0으로 표시하지 않습니다.
- Margin은 회전된 Slab 꼭지점 중 각 레일에 가장 가까운 점의 부호 있는 간격입니다. **음수는 레일 경계 침범**입니다.
- 이 지표는 설정한 평행한 레일 면에 대한 간격이지 임의 복잡 Mesh의 최단 거리 검사가 아닙니다.
- `center_y`를 이동에 쓰지 않으므로 기준 중심선에 배치한 Slab의 중심 이탈은 보통 0입니다. 회전으로 달라지는 꼭지점 이탈·Margin과 혼동하지 마세요.
- 진행 패널의 `고온 Slab 외형`을 켜면 발광 외형, 끄면 냉각된 산화 철강입니다. `M_SlabSurface`의 색·거칠기·금속성·산화 정도·발광 파라미터를 사용할 수 있습니다. 온도 물리 모델이나 실제 장비 표면 캘리브레이션은 아닙니다.

## 차트와 진행률 조작

| 조작 | 동작 |
|---|---|
| 기울기 / 중심 이탈 / Margin / 꼭지점 이탈 / 속도 | 차트 지표 선택 |
| X축 체크 | 원본 frame 번호 ↔ 시나리오 시간(초) |
| 좌/값·우 체크 | 계열 표시/숨김 |
| 마우스 이동 | 가까운 원본 표본의 값 조회 |
| 좌클릭 드래그 | 차트 보기 범위 이동 |
| 휠 | 마우스 위치 중심 확대·축소 |
| 보기 초기화 | 전체 시나리오 범위 복원 |

차트는 원본 표본을 유지하고 그리기만 화면 폭에 맞춰 min/max 방식으로 축약합니다. 좁은 침범이나 각도 피크를 단순한 N번째 표본 추출로 없애지 않습니다. 녹색 세로선은 현재 재생 시각입니다. **차트 확대·이동은 시뮬레이션 seek가 아닙니다.** 각도 단위는 도, 위치·Margin은 cm, 속도는 cm/s입니다.

표시 갱신은 최대 10Hz, 상세 문자열은 최대 5Hz이며 숨겨진 차트의 표시 계산은 생략합니다. 진행 패널은 원본 프레임/행, 경과·전체 시간, 내부 보관 ID 또는 시나리오 UUID, 이번 실행 UUID, 움직임 상태와 센서 drain 상태를 분리합니다. 일시정지·재개·중단은 실제 이동기와 센서 세션에 전달합니다.

## 자동 송신과 재실행

| 실행 | PCD 기본 | Camera 이미지 기본 | LiDAR 정보 기본 |
|---|---:|---:|---:|
| 신규 벌크 자동 실행 | 켜짐 | 꺼짐 | 꺼짐 |
| 저장 목록 재생 | 꺼짐 | 꺼짐 | 꺼짐 |

데이터 패널의 **신규 Slab 시나리오 자동 송신**에서 세 종류를 개별 선택합니다. 변경값은 다음 실행에 적용되며 이미 시작된 실행의 정책은 바뀌지 않습니다. Camera 체크는 **Camera Topic 이미지 전송**이며 로컬 JPEG 파일 캡처를 자동으로 켜지 않습니다. LiDAR 정보는 기존 backend에 따라 telemetry/호환 Payload를 보냅니다.

재생 패널의 세 체크박스는 그 재실행에만 적용됩니다. 모두 끄면 관찰 전용입니다. 기존 `bSendPcd`와 bool 기반 replay API도 유지됩니다.

송신은 첫 Slab 자세 적용 뒤 시작하며 움직임 종료 후 새 세션 데이터는 받지 않습니다. 이미 승인한 데이터만 최대 10초 drain합니다. Slab가 표본을 적용했다고 추가 센서 스캔을 호출하지 않습니다. 센서는 자기 주기로 측정하며 acquisition 시점에 보관한 Slab 정보를 붙입니다.

- Slab `frame_no`와 센서 `FrameId`는 독립적입니다. 600행 입력과 PCD 600개를 항상 같다고 가정하지 마세요.
- PCD의 기존 본문 메타데이터에 소재 번호, Slab frame, 시나리오 보관 ID/UUID, 이번 실행 UUID가 유지됩니다.
- Broker receipt는 Broker 수락입니다. 소비자 검증 완료나 시뮬레이션 종료가 아닙니다.
- 크기 제한·큐 초과·연결 오류 때문에 송신이 실패해도 Slab 이동이 네트워크 완료를 동기 대기하지 않습니다. 진행/데이터 패널의 실패와 미완료 수를 확인하세요.
- 시나리오 ID가 없는 경우 내부 보관 ID를 생성하되 원문은 바꾸지 않습니다. 같은 원문은 digest로 중복 확인합니다. 기존 `RegisterScenarioJson`은 UUID 필수 동작을 유지하고, 새 `RegisterScenarioJsonWithMissingUuidPolicy(Json, true)`가 누락 UUID 정책을 제공합니다.
- 같은 시나리오를 재실행하면 보관 ID는 같고 실행 UUID는 새로 생성됩니다. 최대 10개는 현재 GameInstance 메모리에만 있으며 프로그램/PIE 종료 후 사라집니다.
- 실행 중 새 벌크는 보관만 합니다. 자동 교체/자동 다음 실행은 하지 않으며 정리 후 재생 목록에서 선택합니다.

### C++ / Blueprint 공개 진입점

`ASlabActor`:

- `SubmitScenarioJson(Json)`: 비동기 처리 접수. `true`는 파싱·재생 완료를 뜻하지 않습니다. 상태를 별도로 확인합니다.
- `GetSimulationStatus()`, `GetCurrentMetrics()`: 상태·진단 조회.
- `SetSimulationPaused(bool)`, `StopSimulation()`: 움직임과 센서 세션 제어.
- `SetDimensionUnit`, `SetPositionUnit`: 실행 전 설정. 실행 중에는 거부.
- `GetSensorOutputs`, `SetSensorOutputs`: 다음 신규 벌크 실행의 출력 정책.
- `SetHotAppearance`, `SetDiagnosticHelpersVisible`: 외형/보조 표시.

`USlabScenarioReplaySubsystem`:

- `RequestScenarioReplayWithOutputs(ScenarioUUID, Outputs, TargetSensorIds)`.
- 기존 `RequestScenarioReplay(UUID, bSendPcd, TargetSensorIds)`는 그대로 지원합니다.
- 새 `ASlabActor`가 BeginPlay에 운영 replay adapter를 등록합니다. 이 경로에서는 외부 코드가 센서 세션 Begin/Notify/End를 다시 호출하지 않습니다.

별도 Slab Actor가 여러 개라면 UI Host의 참조를 명시적으로 지정하세요. 데이터 패널은 `BindScenarioSlabActor(Actor)`로 대상을 지정할 수 있습니다. 여러 후보가 있는데 임의의 첫 Actor에 출력 정책을 적용하지 않습니다.

## 30초 벌크 테스트

전용 맵: `/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap`.

아래 두 경로는 검증 범위가 다릅니다.

1. 맵 하단 `검증: 30초 관찰` / `검증: 30초 PCD / 로컬 Broker`: DTCore 큐→worker→TC→DataSync 전체 처리와 이동을 확인하지만 **외부 Broker에서 벌크가 들어오는 수신 socket 자체의 증거는 아닙니다.** PCD 버튼은 테스트맵의 로컬 Broker 설정을 적용합니다.
2. 외부 Node 발행기: Artemis Topic으로 벌크를 실제 발행합니다. DTCore가 같은 Topic을 구독한 PIE에서 실행하세요.

프로젝트 폴더에서:

```powershell
# 파일만 생성 (네트워크 전송 없음)
node Tools/Artemis/publish_slab_scenario.mjs --generate-only true

# DTCore가 구독 중인 로컬 Artemis Topic으로 전송
node Tools/Artemis/publish_slab_scenario.mjs --topic topic.cep.output.0

# 다른 입력 Topic 또는 UUID가 없는 입력 시험
node Tools/Artemis/publish_slab_scenario.mjs --topic topic.virtual.agent.scenario.0 --without-uuid true

# 실제 입력 파일을 그대로 전송
node Tools/Artemis/publish_slab_scenario.mjs --input "C:\TestData\scenario.json" --topic topic.cep.output.0
```

기본 접속은 `127.0.0.1:61616`이며 `--host`, `--port`로 바꿀 수 있습니다. 로컬 개발 기본 인증값 대신 다른 계정을 쓰면 `ARTEMIS_USER`, `ARTEMIS_PASSWORD` 환경변수로 제공합니다. 운영 비밀번호를 명령행·저장소에 넣지 마세요.

생성 데이터는 약 185KiB이며 실제 byte 수는 발행기 출력에서 확인합니다. 원본 0·1·580번 값을 유지하고 중간을 보간한 600행(0~599, 0.00~29.95초)입니다. 581~599는 최종 자세를 유지하며 전체 종료는 명시적인 30.00초입니다. 파일 기본 위치는 `Saved/Reports/Slab/synthetic_30s.json`입니다. 발행기 `brokerAccepted:true`는 receipt 확인일 뿐이므로 Editor 진행률 증가도 별도로 확인해야 합니다.

### 회귀 검증 목록

- `MA0T10.Slab` 자동화: 파서·보간·단위·레일·자체 차트 축약·Widget 격리·출력 기본값.
- 기존 replay/PCD/session 자동화: bool API, 관찰/PCD/Camera/LiDAR/혼합 정책, 실행 UUID, drain, wildcard 제한.
- 실제 Editor: 냉각/고온, 자유 시점, 드래그/접기/resize/폰트, 차트 hover/pan/zoom, pause/재개/중단, 목록 재실행.
- 실제 Artemis: 신규 벌크 수신, PCD 메타데이터·checksum·FrameId, 선택하지 않은 Topic 미송신, 제출/receipt/실제 소비자 수신 수.
- 동일 조건 차트 on/off 성능 비교: FPS뿐 아니라 LiDAR acquisition·PCD 수신 Hz·queue·invalid·gap을 같이 기록.

증거는 `Saved/Reports/Slab` 및 실행한 자동화 보고서에 저장합니다. 화면 크기는 실제 확보한 값을 기록하며, 자동화 캡처를 수동 마우스 검증으로 표시하지 않습니다. 목표값은 검증 기준이지 실행 전에 보장된 결과가 아닙니다.

### 2026-09-21 검증 상태와 남은 제한

- UE 5.3 Development 빌드, 신규 단위/격리 테스트, 실제 PIE lifecycle, WBP 6개 컴파일과 V2 자산 검사를 실행했습니다.
- 실제 DTCore Topic 수신으로 600행 시나리오가 실행되고, UI에서 선택한 JPEG / LiDAR telemetry / Binary PCD가 독립 외부 구독자에 도착하는 것을 확인했습니다.
- 보이는 PIE의 한 30초 PCD 실행은 제출 600 = receipt 600 = 자체 수신 600이며 외부 수신도 해당 실행의 600건이 모두 유효했습니다. 이것만으로 반복 실행 성능까지 통과했다고 판단하지 않습니다.
- 반복 30초 시험에서는 첫 실행 599건, 두 번째 528건으로 낮아지는 현상이 남았습니다. 총 1,127건은 제출·receipt·실제 소비자 수신이 일치하고 invalid/gap/overflow는 0이었지만, 두 번째 실행 19Hz 및 1% low 45 FPS 목표는 미달입니다. 원인 확정을 위한 추가 성능 분석이 필요합니다.
- 실제 마우스 검증은 약 1180×684 및 1920×998 viewport에서 진행했습니다. 정확한 1280×720 / 1920×1080 검증으로 표기하지 않습니다.
- 전체 회귀에서 운영 `SensorTestMap`의 기존 로컬 LiDAR 높이와 고정 fixture 기대값이 달라 `MA0T10.EditorSmoke.MapSensorComposition`이 실패합니다. 사용자 맵은 덮어쓰지 않았습니다.

실제 RHI 시험은 아래 환경변수로 명시적으로 활성화합니다. 로컬 Artemis가 먼저 실행되어 있어야 합니다.

```powershell
$env:MA0T10_SLAB_RUNTIME = '1' # ProductionLifecycle: 가속 경계·pause/replay 시험
$env:MA0T10_SLAB_RHI = '1'     # ProductionRuntime: 실제 30초 두 번 및 송신율 검사
$env:MA0T10_SLAB_LOCAL_BROKER = '1' # 검증맵에만 로컬 개발 Broker 적용
```

`ProductionRuntime`은 테스트 프로세스 내 Editor background throttle/performance monitor 값을 저장·해제·복원하며 사용자 설정 파일에 저장하지 않습니다. UI 캡처는 성능 측정 구간 밖에서 수행합니다. 이 테스트의 처리율 실패를 통과로 바꾸기 위해 기준을 낮추지 않았습니다.

독립 구독자는 `Tools/Artemis/stomp_probe.mjs --scenario-replay true --expected-scenario-uuid any --quiet true`를 사용할 수 있습니다. `any`는 동일 보관 UUID가 서로 다른 실행 UUID로 재생되는 것을 검증하며, 서로 다른 시나리오를 하나의 재생 시험에 섞으면 실패하는 것이 정상입니다.
