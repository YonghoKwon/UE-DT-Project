# DTCore Slab 시뮬레이션과 자체 차트

> **고정 참고자료 (2026-09-22 전환)**: 이 문서는 이전 상세 명세/검증 이력을 보존합니다. 현재 사용법·UI·지원 범위는 [README](../README.md), 작업 규칙은 [AGENTS](../AGENTS.md), 후속 구현은 [보완 사항](IMPROVEMENTS.md)과 [로드맵](ROADMAP.md)을 기준으로 합니다. 아래의 과거 "현재"·성능 수치·맵 생성 절차를 최신 보장이나 실행 승인으로 해석하지 마세요. 기존 링크와 스크립트 참조를 위해 본문/경로를 유지합니다.

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

현재 Slab 입력 Topic은 `topic.scenario`이며 테스트 발행기도 이를 기본값으로 사용합니다. DTCore `UDTCoreSettings.WebSocketTopics`에 이 Topic이 있는지 확인하세요. 다른 업무 구독은 삭제하지 않으며 코드가 공통 설정을 자동 변경하지 않습니다. 다른 Topic을 사용하면 구독 설정과 발행기의 `--topic`을 동일하게 맞추세요. Camera/LiDAR/PCD 송신 Topic과는 별개입니다.

### 목록에는 들어오지만 수신 즉시 움직이지 않을 때

수신 진입점은 **DTCore → `UFactoryAgentScenarioTC` → Slab DataSync** 또는 별도 연동 코드의 **`SlabActor.SubmitScenarioJson(Json)`** 중 하나만 사용합니다. Actor 진입 전에 `RegisterScenarioJson`을 별도로 호출하면 같은 UUID가 이미 보관되어 중복 실행 방지에 걸립니다. 설정을 고쳐 테스트할 때도 새 UUID를 사용하거나 목록의 재생 버튼을 이용하세요.

`GetLastScenarioAdmissionStatus()`에서 자동 실행, 송신 없이 실행, 실행 중 보관, 중복, 거절을 구분할 수 있습니다. 원문을 출력하지 않는 `[SlabAdmission]` 로그에는 ReceiverId·Actor·UUID·요청/적용 출력·사유가 남습니다.

신규 live 시나리오의 송신 구성(Coordinator, STOMP 모드/URL, 필요한 센서/TargetSensorId)이 부족하면 Slab 이동은 **송신 없는 관찰 세션**으로 진행합니다. 이 fallback은 센서를 시작·중지하거나 기존 독립 스트림을 변경하지 않습니다. 요청한 출력은 다음 실행을 위해 유지하며, 실행 중 설정을 복구해도 중간부터 자동 송신하지 않습니다. `TransmissionWarning`은 자세·pause 갱신 후에도 남습니다. 데이터 오류·중복 UUID·진행 중 세션은 이 fallback으로 우회하지 않습니다.

이 준비 검사는 Broker 연결 성공이나 receipt를 보장하지 않습니다. 정상 송신 시작 후에는 제출·Broker 수락·실제 소비자 수신을 별도로 확인하세요. 회사 프로젝트의 실제 원인은 해당 코드/로그가 없으므로 확정하지 않았습니다.

### 밝은 장면의 3D 분석 표시

Slab 진행 → **세부 표시**에서 선 굵기(기본 4px, 2~12px)와 문자 크기(기본 28px, 18~56px)를 조절합니다. `가독성 초기화`는 두 크기만 초기화하며 개별 표시 선택은 유지합니다. 선 두께는 시점에 따라 월드 크기로 환산하며, 진단 재질만 노출 보정을 사용합니다. 맵 조명·Post Process는 변경하지 않습니다.

문자는 Unlit 재질·어두운 배경판을 사용하고, 음수 Margin에는 `[침범]`을 표시합니다. Margin 양끝 눈금은 측정 구간을 구분하며 안전 판정을 추가하지 않습니다. 진단 선·문자·배경판은 충돌과 Camera/GPU LiDAR 캡처에서 제외합니다.

다른 프로젝트로 이식할 때 `M_SlabAnalysisReadable`, `M_SlabTextReadable`, `F_SlabDiagnostics`도 Migrate하세요. 폰트는 한글 두 글자를 포함한 offline atlas이므로 실행 PC에 글꼴 설치가 필요하지 않습니다. 자산 재생성 스크립트 `setup_slab_readability_assets.py`를 실행하는 제작 PC에는 Malgun Gothic이 필요합니다. 기존 사용자 Slab 표면 재질은 덮어쓰지 않습니다.

### 2026-09-22 후속 검증

- 자동 실행/fallback 조건 3개, 최소 UI/소유권 관련 3개, 분석 표시·실제 RHI 격리 9개 집중 검사가 통과했습니다. Camera 픽셀 및 GPU depth/semantic의 표시 OFF/ON 차이는 0이었고 양성 대조군 변화도 확인했습니다.
- 전체 자동화 168개 중 엔진 Success 166(조건부 skip 9 포함), Fail 2였습니다. 실패 중 과거 helper pool 기대값 37을 배경판 포함 42로 갱신하고 관련 3개를 재검증해 통과했습니다. 남은 운영맵 LiDAR 높이 fixture는 사용자 맵을 바꾸지 않고 실패로 기록했습니다.
- 100,000 lux 임시 PIE에서 실제 마우스로 크기 조절·복원·`[침범]` 글자·간단 모니터·시나리오 기본 화면을 확인했습니다. 확보된 viewport는 내장 988×521, 별도 창 1174×683이며 목표 해상도 통과로 표기하지 않습니다.
- 실제 PCD 30초 실행 두 번: 599/600개 제출·receipt·내부/외부 수신 일치, 약 20Hz, invalid/gap/duplicate/overflow 0. 평균 59.999/59.799 FPS, 1% low 59.940/45.019 FPS, p95 16.667ms. 자동화 viewport는 753×403입니다. 두 번째 실행의 1% low 저하는 별도 성능 변동으로 남기며 무회귀라고 단정하지 않습니다.
- 최종 빌드, WBP 6개 메모리 컴파일, V2 자산 검사가 통과했습니다. 회사 프로젝트와 고정 DTCore gitlink의 clean checkout은 별도 미검증 범위입니다. 로컬 증거는 `Saved/Reports/SlabVisibility/report.ko.md`에 있습니다.

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

### 이미 구성된 다른 맵에 적용하기

**`SlabScenarioValidationRig`를 운영맵에 복사하지 마세요.** 이 Actor는 LiDAR/Camera 프로필과 SensorId·시작 상태·입력 모드를 시험용으로 설정하며, 로컬 시험 버튼은 개발용 Broker 설정까지 적용합니다. `setup_slab_scenario_validation.py`도 검증맵 생성기이지 기존 운영맵 자동 설치 도구가 아닙니다.

기존 맵에서는 다음 연결만 추가하고, 이미 배치된 센서·Camera·조명·가드레일·동료 Widget은 유지합니다.

| 설정 | 직접 연결할 내용 | 미지정 시 동작 |
|---|---|---|
| Slab `TrackReference` | 롤러 중심선의 원점·진행 방향·바닥 기준 | Slab 초기 Transform 사용, Margin은 N/A |
| 치수/위치 단위 | 발행 데이터의 단위를 각각 선택 | 둘 다 cm이며 크기를 추측하지 않음 |
| `ReceiverId` | TC의 ReceiverId와 동일한 값, 중복 금지 | 기본 `SlabScenario.Main` |
| UI Host `SlabActor` | 표시할 Slab 직접 지정 권장 | 후보가 정확히 하나일 때만 자동 연결 |
| `TargetSensorIds` | 이번 Slab 실행에 사용할 기존 SensorId | 시작 시 Coordinator의 전체 센서 대상 |
| `SensorOutputs` | PCD/Camera/LiDAR별 다음 실행 정책 | 신규 벌크는 PCD만 기본 활성 |
| Topic·Broker | 기존 DTCore 입력 설정과 기존 센서 출력 설정 | 두 연결을 서로 복사하거나 임의 변경하지 않음 |

이미 실제 가드레일 Mesh가 있다면 Track의 `bShowReference`를 꺼 중복 Mesh 표시를 피하고, `LeftRailYcm`·`RightRailYcm`만 실제 내부면에 맞춥니다. Track의 진단용 레일은 물리 가드레일이 아니며 센서가 감지할 장애물로 사용하지 않습니다.

`ASlabActor::ValidateSlabSetup()`은 설정을 변경하지 않고 연결 상태를 보고합니다. `bCanSimulate`는 Slab 구조/단위의 준비 상태이고 `bCanSendSelectedOutputs`는 선택 출력의 Coordinator·센서·STOMP 설정 상태입니다. 에러/경고에서 중복 ReceiverId, 다른 재생 adapter, TC 연결, UI Host, 레일·단위, 대상 센서를 확인하세요. 연결된 Broker의 실제 수신 성공은 별도의 receipt/소비자 검증이 필요합니다. 관찰만 할 경우 출력 세 개를 모두 끄면 Coordinator나 Broker 없이 움직임을 시험할 수 있습니다.

다른 GameMode가 PlayerController를 늦게 생성하면 Slab UI Host는 최대 10초 동안 0.25초 간격으로만 재시도합니다. 이후에는 `GetInitializationMessage()`로 사유를 확인하고 준비된 뒤 `ShowSimulationPanels()`를 호출하세요. 종료 시 재시도 타이머를 정리하며, 다른 Host가 소유한 패널을 가져오거나 재생성하지 않습니다.

### 가드레일과 외형

- `LeftRailYcm` / `RightRailYcm`은 기준 좌표계에서 레일 **안쪽 면**의 Y 위치입니다. 표시 Mesh 중심이 아닙니다.
- `bRailsConfigured`를 끄거나 값이 잘못되면 Margin은 `N/A`입니다. 안전한 0으로 표시하지 않습니다.
- Margin은 회전된 Slab 꼭지점 중 각 레일에 가장 가까운 점의 부호 있는 간격입니다. **음수는 레일 경계 침범**입니다.
- 이 지표는 설정한 평행한 레일 면에 대한 간격이지 임의 복잡 Mesh의 최단 거리 검사가 아닙니다.
- `center_y`를 이동에 쓰지 않으므로 기준 중심선에 배치한 Slab의 중심 이탈은 보통 0입니다. 회전으로 달라지는 꼭지점 이탈·Margin과 혼동하지 마세요.
- 기본 표면은 **얼룩·Noise가 없는 `M_SlabSurfacePlain`**입니다. `고온 Slab 외형`을 켜면 단순한 고온 발광색, 끄면 단색 냉각 철강으로 표시합니다. Roughness·조명에 의한 음영과 반사는 남으며, 온도 물리 모델이나 실제 장비 표면 캘리브레이션은 아닙니다.
- 새 Actor는 Plain을 기본으로 사용합니다. 기존 소유 기본 재질 `M_SlabSurface` 또는 미지정 상태도 런타임에서 Plain으로 해석하지만, 기존 Map 자산의 값을 재저장하지 않습니다. **사용자가 지정한 다른 `SurfaceMaterial`은 그대로 보존**합니다. `GetEffectiveSurfaceMaterialPath()`로 실제 선택 경로를 확인할 수 있습니다.
- Actor의 재질 속성이 기본값일 때 `SlabMesh`의 Material Slot 0에 직접 지정한 사용자 재질도 그대로 유지하며 고온 토글로 해당 인스턴스의 파라미터를 바꾸지 않습니다. Actor `SurfaceMaterial`에 별도 사용자 재질을 지정했다면 그 명시적 속성이 Slot보다 우선합니다.
- 이전 맵에 저장된 `MaterialInstanceDynamic*` 자동 이름의 MID 중 **Slab 소유이며 부모가 이전/신규 소유 기본 재질인 경우만** 레거시 생성물로 판단하여 런타임에 Plain으로 바꿉니다. 사용자 이름을 붙인 MID나 다른 사용자 graph를 부모로 하는 MID는 보존합니다. 원본 맵을 재저장하는 자동 마이그레이션은 하지 않습니다.
- `Oxidation` 속성은 기존 API/사용자 재질 호환을 위해 남아 있습니다. Plain 재질에는 산화/Noise 계산이 없으므로 이 값으로 얼룩이 생기지 않습니다. 기존 소유 재질의 graph 기본값만 수정하는 방식은 Actor의 MID 값과 충돌할 수 있으므로 사용하지 않습니다.

### 3D 분석 보조 표시

`FSlabAnalysisDisplaySettings` 또는 Actor의 `GetAnalysisDisplaySettings` / `SetAnalysisDisplaySettings`로 항목을 각각 켜고 끕니다.

| 표시 | 의미 |
|---|---|
| 외곽선·십자·중심점 | 현재 Slab의 실제 배치와 중심 |
| 0° 기준 윤곽 | 같은 중심에 yaw=0인 Slab가 놓였을 때의 기준 |
| 중심선·화살표 | Track의 진행 방향과 중심선 |
| Yaw 호·수치 | 적용된 좌측 기준 signed 회전각 |
| 좌우 Margin 선·수치 | 가장 가까운 꼭지점과 설정된 레일 내부면의 간격. 음수는 빨강 |
| 소재·프레임·시간·진행률 | 현재 소재와 원본 Slab frame, 연속 재생 시간 |

월드 수치는 폰트 이식성을 위해 `Yaw`, `L`, `R`, `Center`, `frame`, `cm` 등의 짧은 기술 표기를 사용하고 자세한 상태는 한글 UMG 패널에서 제공합니다. 레일 미설정은 `N/A`로 표시합니다.

월드 텍스트는 관찰 카메라의 거리·FOV·실제 viewport에 맞춰 약 18px 높이를 목표로 조절하되 20~120cm 범위로 제한합니다. 직교 시점은 OrthoWidth를 사용합니다. 각도/중심 상태는 위쪽, 진행 정보는 아래쪽, 좌우 Margin은 Slab의 투영 외곽으로 분산하며, 긴 소재 번호의 전체 값은 UMG 패널에서 확인합니다. 이 크기·배치 보정도 최대 5Hz이며 센서 카메라의 Transform이나 측정값을 바꾸지 않습니다.

텍스트가 Slab나 바닥 뒤에 묻히지 않도록 표시 평면을 관찰 카메라 쪽으로 당깁니다. 원근 시점에서는 화면 위치·글자 픽셀 크기가 유지되도록 위치와 world-size를 함께 보정하고, 직교 시점은 평행 이동만 합니다. 물리 Margin 선과 측정 좌표는 이동시키지 않습니다. Near clip 범위를 넘어 당기지 않으며, 임의의 장면 장애물 전체에 대한 X-ray 표시 기능은 아닙니다.

선과 텍스트는 고정된 최대 **32개 선 Mesh + 5개 TextRender** pool을 재사용합니다. 텍스트는 최대 5Hz로 갱신하며, 보조 표시에는 충돌·SceneCapture·그림자·간접광·거리장 영향을 모두 끕니다. 따라서 보조 선을 실제 Slab의 점군이나 Camera 이미지로 내보내기 위한 기능이 아닙니다. 새로운 센서 캡처/스캔/readback도 만들지 않습니다. 운영맵의 센서 Camera 대신 별도 관찰 시점만 변경해 확인하세요.

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
- `GetAnalysisDisplaySettings`, `SetAnalysisDisplaySettings`: 3D 분석 항목별 표시.
- `GetEffectiveSurfaceMaterialPath`: 사용자 재질을 보존한 최종 표시 재질 경로.
- `ValidateSlabSetup`: 다른 맵 연결의 읽기 전용 사전 점검.

재생 시간·보간·실제 Transform 적용은 `USlabMotionComponent`가 소유합니다. Actor는 센서 세션과 UI 상태를 연결합니다. 외형 상세의 `Roughness`, `EmissiveStrength`는 거칠기·고온 발광 강도이며 `Oxidation`은 이전 사용자 재질 호환 속성입니다.

`USlabScenarioReplaySubsystem`:

- `RequestScenarioReplayWithOutputs(ScenarioUUID, Outputs, TargetSensorIds)`.
- `CanDeleteScenario(UUID, Reason)`, `DeleteScenario(UUID, Reason)`: 현재 메모리 목록에서만 삭제합니다. live/replay 준비·실행·일시정지·센서 drain 중인 항목은 삭제와 자동 정리 모두에서 보호합니다.
- 기존 `RequestScenarioReplay(UUID, bSendPcd, TargetSensorIds)`는 그대로 지원합니다.
- 새 `ASlabActor`가 BeginPlay에 운영 replay adapter를 등록합니다. 이 경로에서는 외부 코드가 센서 세션 Begin/Notify/End를 다시 호출하지 않습니다.

별도 Slab Actor가 여러 개라면 UI Host의 참조를 명시적으로 지정하세요. 데이터 패널은 `BindScenarioSlabActor(Actor)`로 대상을 지정할 수 있습니다. 여러 후보가 있는데 임의의 첫 Actor에 출력 정책을 적용하지 않습니다.

### 재생 목록의 UUID·삭제

각 행에 전체 UUID를 표시합니다. 발행자 UUID가 없는 입력은 **내부 보관 ID**로 표시하며, 실행 UUID와 다릅니다. UUID 복사, 삭제→행 안의 삭제 확인/취소를 사용합니다. 선택 항목을 삭제하면 남은 최신 항목을 선택하고 목록이 비면 재생을 비활성화합니다. 원본 파일·Broker·Slab·현재 표시된 완료 차트는 지우지 않습니다.

삭제 전에 `RegisterScenarioJson` 계열 API로 접수된 비동기 등록 결과는 serial cutoff로 폐기합니다. 삭제 후 새 접수는 같은 UUID라도 다시 등록할 수 있습니다. 외부 DTCore 파서가 작업을 마친 뒤 `RegisterValidatedScenario`를 호출하는 경우에는 **그 게임 스레드 호출이 새 접수 시점**입니다. 삭제가 DTCore의 파싱 작업이나 Topic 수신을 취소하는 것은 아닙니다.

### 3중 점진 차트

Slab 차트 패널은 기울기(left/right), 실제 중심 이탈, 좌우 Margin을 동시에 보여줍니다. 각 카드 드롭다운에서 꼭지점 이탈이나 진행 속도로 바꿀 수 있습니다. 시간/원본 frame 축, 드래그·휠 보기 범위, 진행 커서는 세 카드가 공유합니다. 차트 탐색은 Slab 재생 위치를 바꾸지 않습니다.

전체 시간축은 처음부터 고정하되 곡선·hover·Y축 범위에는 도달한 표본만 사용합니다. 미래 위치값이 현재 속도를 오염시키지 않도록 속도는 직전→현재 표본 차이로 계산합니다(첫 표본 0). 일시정지는 표시도 멈추며, 중단은 마지막 도달 지점까지만 남고 새 실행 UUID에서는 0부터 다시 그립니다. 숨겼다 열어도 현재 진행 시점으로 복원합니다.

세 그래프는 immutable 표본 배열 하나를 공유합니다. generic `USlabScenarioChartWidget`은 기존 전체 표시가 기본이며 소유 Slab 패널만 점진 표시를 켭니다. 기존 `ScenarioChart`, `SetChartMetric`은 첫 카드에 대응하고, `SetChartSlotMetric(0~2, Metric)`, `SetProgressiveReveal`, `SetRevealedTime`을 추가 제공합니다.

### 표시 스타일의 영향 범위

새 어두운 패널·버튼·입력창은 Workspace Host가 등록한 6개 인스턴스에만 적용합니다. 공용 스타일, Main, DTCore 테마, 엔진 DPI, 미등록/중첩 외부 위젯은 변경하지 않습니다. 짧은 버튼 문구를 강제로 줄바꿈하지 않고 긴 설명에만 명시적인 wrap을 사용합니다. 확대 글꼴에서는 본문 스크롤을 사용하며 진행 패널의 일시정지/중단 위치는 긴 상태 문구와 무관하게 고정됩니다.

기존 저장 배치는 보존하며 차트만 3개 카드에 필요한 최소 크기로 보정합니다. 정상 100% 글꼴 기준 3개 차트를 함께 배치하고, 125/150% 또는 작은 화면에서는 스크롤이 필요할 수 있습니다.

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
node Tools/Artemis/publish_slab_scenario.mjs --topic topic.scenario

# 다른 입력 Topic 또는 UUID가 없는 입력 시험
node Tools/Artemis/publish_slab_scenario.mjs --topic topic.virtual.agent.scenario.0 --without-uuid true

# 실제 입력 파일을 그대로 전송
node Tools/Artemis/publish_slab_scenario.mjs --input "C:\TestData\scenario.json" --topic topic.scenario
```

기본 접속은 `127.0.0.1:61616`이며 `--host`, `--port`로 바꿀 수 있습니다. 로컬 개발 기본 인증값 대신 다른 계정을 쓰면 `ARTEMIS_USER`, `ARTEMIS_PASSWORD` 환경변수로 제공합니다. 운영 비밀번호를 명령행·저장소에 넣지 마세요.

생성 데이터는 약 186KB(182KiB)이며 실제 byte 수는 발행기 출력에서 확인합니다. 원본 0·1·580번 값을 유지하고 중간을 보간한 600행(0~599, 0.00~29.95초)입니다. 581~599는 최종 자세를 유지하며 전체 종료는 명시적인 30.00초입니다. 파일 기본 위치는 `Saved/Reports/Slab/synthetic_30s.json`입니다. 발행기 `brokerAccepted:true`는 receipt 확인일 뿐이므로 Editor 진행률 증가도 별도로 확인해야 합니다.

### 회귀 검증 목록

- `MA0T10.Slab` 자동화: 파서·보간·단위·레일·자체 차트 축약·Widget 격리·출력 기본값.
- `MA0T10.SlabAnalysis`: Plain/사용자 재질 경로 보존, 보조 표시 측정 제외와 pool 상한, 일반 맵의 읽기 전용 검증, UI Host 재시도·종료 정리.
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

### 분석·목록·3중 차트 개선 재검증 (2026-09-21)

- 실제 마우스로 3개 카드 동시 표시, 진행 prefix, pause/재개, 새 실행 초기화, 미래 hover 차단, 카드별 지표, 프레임 축, 창 drag/resize/접기/숨김을 확인했습니다. 125% 글꼴의 짧은 문구 분절도 수정 후 재확인했습니다.
- 기존 맵의 레거시 자동 생성 MID를 새 Plain으로 해석하여 냉각/고온 모두 얼룩이 없는 것을 확인했습니다. 사용자 직접 지정 Slot 재질과 Actor 재질 override는 별도 테스트로 보존합니다.
- 최종 Slab 관련 자동화는 24개 실제 실행 통과, 장시간 ProductionRuntime 1개는 별도 실행 대상으로 분리했습니다. 그 별도 시험에서 표시 OFF는 599/600건, ON은 600/600건의 두 30초 PCD 실행을 확인했고 제출=receipt=내부/외부 수신, invalid/gap/overflow=0이었습니다.
- 표시 ON 평균 FPS는 59.98/59.93, 1% low는 57.66/53.53, p95는 모두 16.67ms였습니다. 두 번째 1% low는 OFF 대비 약 7.3% 낮았습니다. 과거 반복 실행 미달의 원인 해결을 입증한 것은 아니므로 이전 기록은 유지합니다.
- RHI GPU 점 32,256개의 XYZ/의미 분류 변화는 0이었습니다. Camera 직접 표시 제외는 결정론적 linear SceneColor fixture에서 OFF/OFF·OFF/ON 변화 0, positive control 312픽셀로 검증했습니다. 이 시험은 postprocess/tonemap/AO/shadow/SSR/Lumen을 fixture에서만 끄며 운영 영상의 모든 명암 픽셀이 항상 동일하다는 보장은 아닙니다.
- 실제 viewport는 1174×731 및 1174×683이었습니다. 정확한 두 목표 해상도는 미완료입니다. 삭제의 API·보호·재수신·빈 목록은 자동 테스트했고, 마우스로 확인/취소까지 확인했습니다. 마지막 삭제 클릭은 action-time 확인 응답 전에는 실행하지 않았습니다.
- 보고서: `Saved/Reports/Slab/analysis_presentation_report.ko.md`, `analysis_verified_regression.log`, `analysis_on/off_external.json`, `Saved/Reports/SlabAnalysis/rhi_isolation.json`.

선택 실행 환경변수는 `MA0T10_SLAB_ANALYSIS_RHI=1`(카메라/GPU 표시 격리), `MA0T10_SLAB_PORTABLE=1`(Rig 없는 임시 GameMode와 지연 PlayerController), `MA0T10_SLAB_ANALYSIS_OFF=1`(전용 ValidationRig의 분석 OFF 비교)입니다. 이 설정을 운영맵에서 자동 적용하지 않습니다.
