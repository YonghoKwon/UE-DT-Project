# UE-DT-Project — 가상 센서와 Slab 시뮬레이션

현재 기능 기준: **`59d1027` (RT-02 `d2024bf` 포함) / PR #27 병합 `68f8f2f` 기반, 2026-09-22**. Unreal Engine 5.3 C++ 프로젝트다. Camera·LiDAR 측정/미리보기/송신, Slab 벌크 시나리오의 보간 이동·분석 표시·차트·재실행을 제공한다.

**아직 독립 센서 플러그인이 아니다.** “센서 여러 대를 항상 정격 주기로, 어떤 PC에서도 무부하로 실행”하는 단계도 아니다. 현재 구현과 실측 범위, 다음에 보완할 기능을 구분한다.

## 관리 문서 4개

| 문서 | 읽는 목적 |
|---|---|
| [AGENTS.md](AGENTS.md) | 개발·수정 시 반드시 지킬 경계와 완료 기준 |
| [README.md](README.md) | 현재 기능, 실행·연동 방법, 계약과 제한 |
| [보완 필요 사항](docs/IMPROVEMENTS.md) | 우선 구현할 문제, 근거, 테스트와 완료 조건 |
| [최종 목표 로드맵](docs/ROADMAP.md) | 다중 센서 → 신뢰성 → 설정화 → 플러그인 → 간편 설치 순서 |

기존 문서는 경로를 보존한 **고정 참고자료**다. 아래 “참고자료”에서 찾을 수 있다. 새 기능/동작 변경은 위 네 문서에 반영하며 과거 기록을 현재 보장으로 읽지 않는다.

## 1. 현재 가능한 것과 아닌 것

| 영역 | 현재 구현 | 아직 보장하지 않는 것 |
|---|---|---|
| 배치·조작 | Editor Actor 배치, PIE 선택 센서 기즈모/키보드 이동·회전, 경량 조작 미리보기, 명시적 맵 저장 예약 | 완성된 다중 센서 생성/복제/삭제 UI, 일반 프로젝트 저장 workflow, 모든 종료 경로·대규모 무지연 |
| 측정 | Camera SceneCapture, CPU LiDAR chunk, GPU depth projection, immutable snapshot, 비동기 파생 처리 | 모든 backend의 물리 동등성, 실제 ML-X/D455 패킷·depth stream 동일성 |
| 출력 | JPEG/telemetry/Binary PCD, Raw TCP STOMP worker, 호환 STOMP/HTTP, 비동기 로컬 파일 저장 | 네트워크 장애 중 영구 무손실, 모든 소비자의 업무 처리 완료, 고성능 Raw TCP TLS |
| Slab | 벌크 수신, 보간 이동, 단위·Track 설정, 3D 표시, 3중 점진 차트, 메모리 보관/재실행 | 녹화 영상/과거 월드 복원, 디스크 영구 시나리오 보관, AI 사행 감지 |
| UI | 명시적 소유 패널 6개, 글자 배율·drag·resize·접기·숨김·Main/Viewport 배치 | 타 프로젝트 Widget 전수 검증, 모든 해상도·DPI·3D 라벨 겹침 자동 해결 |
| 이식 | 같은 DTCore 기반 프로젝트에 소스·자산·설정을 함께 옮길 수 있음 | 플러그인 복사 후 클래스 하나만 배치하는 완성된 설치 방식 |

수정 후보와 검증 공백은 [IMPROVEMENTS](docs/IMPROVEMENTS.md)에 있다. 이미 있는 Scheduler·snapshot·비동기 전송을 다시 만드는 대신 경계 조건과 확장성을 보강한다.

## 2. 빌드와 안전한 시작

### 2026-09-29 런처 연계 패키징 점검

- 현재 체크아웃(기준148cc336, DTCore a1b333e)의 Windows Development Build/Cook/Stage/Archive를 실제 완료했다. 최종 성공은 맵 제외 없이 수행했다.
- 게임 빌드를 막던 에디터 전용 테스트11파일의 가드를 보완했다(`760082c`). D3D12 에디터 관련29테스트 실패0/미실행0:12건 정상,17건 격리된 SaveGame 파일 부재 경고 동반. 센서 계산/전송 로직은 변경하지 않았다.
- DT_DxLevel이 참조하는 TestMap의 누락된 구형 모니터 클래스를 사용자 승인 후 현재 WBP_VirtualSensorMonitorPanel로 연결했다. 원본은 Saved/LauncherPackagingBackup에 보존했고 재실행 시 파일 불변을 확인했다. SensorTestMap과 Config/Game.ini의 원래 바이트는 보존했다.
- 패키지328파일/878,409,923바이트. ZIP+외부 release.json을 격리된 런처 배포 서버에 접수·승인하고 사용자 확인 후 실제 GUI로 설치·실행했다. Manifest328파일 해시, SensorTestMap/D3D12 창, 런처 종료/재실행 중 동일 UE bootstrap/자식 생존, 정상 종료 후 Quiescent를 확인했다. 실제 버전 업데이트/복구·Linux/회사 인수까지 완료한 것은 아니다.
- `Scripts/inspect_launcher_package_map.py`는 기본 맵의 출력 설정을 읽기만 한다. 현재 저장된 기본 맵의 센서 출력은 LogOnly이며, 런처 시험은 별도 UserDir와 DTCore 로컬 시험 주소를 사용한다.
- DTCore의 EnhancedInput 플러그인 의존 선언 경고는 남아 있다. DTCore 및 gitlink를 임의 수정하지 않았으며 Shipping/Linux·회사 서버·실장비 검증을 완료로 표시하지 않는다.
- 실제 실행에서 DTCore의 CustomLogs가 설치 폴더에 생성됐다. `-UserDir`만으로 모든 로그가 분리되는 것은 아니며 Program Files·서비스 계정의 로그 쓰기 경로는 후속 점검한다. 이번 시험은 시험 폴더 안에서만 이루어졌다.

에셋 복구 재현: Editor 종료 후 `UnrealEditor-Cmd.exe <uproject> -run=RepairTestMapMonitorReference`로 점검하고, 대상 확인 후에만 `-Apply`를 추가한다. 전체 맵 재생성 스크립트는 실행하지 않는다.

- Engine: **UE 5.3** / Editor target: `ma0t10_dtEditor Win64 Development`.
- 모듈: `ma0t10_dt`, `ma0t10_dtEditor`, 조기 설정용 `ma0t10_dtBootstrap`.
- DTCore는 필수 submodule이다. 현재 parent gitlink는 `2eec1fe`, 최근 검증 환경의 로컬 checkout은 `a1b333e`였다. 두 revision 사이 Source 변경은 없고 문서만 다르지만, pinned clean checkout 검증은 별도로 필요하다. 기존 로컬 checkout을 자동 갱신하지 않는다.
- 기존 worktree의 DTCore·Game.ini·운영맵·PixelStreaming 변경은 보존한다.

Editor/Live Coding을 종료한 뒤 프로젝트 루트에서:

```powershell
& "C:/Program Files/Epic Games/UE_5.3/Engine/Build/BatchFiles/Build.bat" ma0t10_dtEditor Win64 Development "-Project=$($PWD.Path)/ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE
```

빌드 후 **이미 커밋된 테스트맵을 열고** PIE를 시작한다. 테스트를 위해 운영맵을 재생성하거나 덮어쓰지 않는다.

| 맵 | 용도 |
|---|---|
| `/Game/MA0T10/Maps/Tests/SensorRefactorTestMap` | 센서/카메라 V2 UI·조작·출력 확인 |
| `/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap` | synthetic Slab, 차트·진행·PCD 연계 검증 |
| `/Game/MA0T10/Maps/Tests/LidarSemanticValidationMap` | 같은 재질·태그 없는 판/물체의 형상 구분 |
| `/Game/MA0T10/Maps/Tests/SensorScaleStressMap` | 10,000 정적/1,000 이동 proxy 스트레스 fixture; 성능 보증 아님 |
| `/Game/MA0T10/Maps/SensorTestMap` | 기존 운영 테스트맵; 로컬 사용자 변경 보호 |

맵 생성 스크립트는 신규 fixture가 필요할 때만 대상·기존 자산을 확인하고 실행한다. `SlabScenarioValidationRig`는 센서 ID·프로필·로컬 Broker 등을 바꾸는 **테스트 도구**이므로 운영맵에 옮기지 않는다.

## 3. 다른 맵에서 현재 기능 연결

### 센서만 사용

1. `AVirtualSensorCoordinator` 하나와 필요한 `AVirtualCameraSensorActor`/`AVirtualLidarSensorActor`를 배치한다.
2. 고유한 SensorId, Transform, 장비/품질/backend를 지정한다.
3. `AVirtualSensorUiHostActor`를 배치한다. 제공 WBP 또는 native fallback을 사용한다.
4. 외부 입력/자체 수신 진단이 필요할 때만 `AVirtualSensorExternalSourceHostActor`를 추가한다.
5. 서버 송신은 데이터 패널에서 설정을 적용하고 필요한 출력만 시작한다. Widget을 여는 것만으로 송신하지 않는다.

### Slab도 사용

- `ASlabTrackReferenceActor`: 로컬 X 진행, Y 오른쪽, Z 위쪽 및 레일 내부면 기준.
- `ASlabActor`: TrackReference, ReceiverId, 치수/위치 단위, TargetSensorIds, 출력 정책 지정.
- `ASlabSimulationUiHostActor`: 대상 Slab와 차트·진행 WBP 연결.
- 재생 목록은 `ASlabScenarioReplayUiHostActor` 또는 도구 막대의 재생 메뉴로 연다. 운영 Slab가 replay adapter를 등록한다.
- 센서 없는 관찰 실행도 가능하다. 송신을 선택할 때만 센서/Coordinator/STOMP 구성이 필요하다.

현재 맵 저장 예약은 **SensorTestMap 전용 경로가 남아 있다**. 다른 맵의 영구 저장까지 일반화됐다고 생각하지 말고, 대상 검증/비활성화 또는 별도 승인된 이식 작업이 필요하다.

### 이식 시 누락하기 쉬운 항목

- C++ 파일 외에 Build.cs/module API macro/include 경로/`/Script/ma0t10_dt` 경로, DTCore 의존성, TC DataTable 등록을 함께 검사한다.
- Unreal **Migrate**로 WBP/Niagara/material 및 의존 자산을 옮긴다. 수동 파일 복사나 일괄 문자열 치환만으로 완료되지 않는다.
- 센서 자산: 세 센서 WBP, `NS_VirtualLidarPointCloud`, `M_VirtualLidarPointSprite`, GPU 분류용 `M_LidarSemanticId`.
- Slab 자산: 재생·차트·진행 WBP, 기본 표면/분석 재질, `M_SlabAnalysisReadable`, `M_SlabTextReadable`, `F_SlabDiagnostics`.
- 진단 폰트는 offline atlas라 실행 PC에 글꼴 설치가 필요 없다. 제작용 재생성 스크립트는 Malgun Gothic을 사용한다.
- 공용 설정 객체와 단일 Bootstrap 진입점은 아직 구현되지 않았다. 자세한 분리 계획은 [ROADMAP](docs/ROADMAP.md)을 따른다.

## 4. UI와 조작

상단은 **선택 센서 / 센서 도구 / Slab 도구 / UI 표시**다. Host가 명시적으로 등록한 인스턴스만 관리하며 다른 Widget의 스타일·입력·저장값을 건드리지 않는다.

| 패널 / WBP | 현재 역할 |
|---|---|
| 모니터 / `WBP_VirtualSensorMonitorPanel` | 영상 + SensorId/프레임/Hz/경고. 투영·색상·듀얼 카메라·범례·진단은 **보기 설정** |
| 센서 설정 / `WBP_VirtualSensorSettingsPanel` | 장비/품질·측정값, 위치/회전, 고급 식별/Source/디버그/저장 예약 |
| 데이터 / `WBP_VirtualSensorCaptureExportPanel` | **실시간 전송 / 파일 저장 / 연결·진단**. 새 PIE는 시나리오 연동 화면 |
| 재생 / `WBP_SlabScenarioReplayPanel` | 전체 UUID/내부 보관 ID, 선택 항목 재실행·복사·삭제, 출력 선택 |
| 차트 / `WBP_SlabChartsPanel` | 기울기·중심 이탈·Margin 3개를 진행 시점까지만 표시 |
| 진행 / `WBP_SlabProgressPanel` | 상태·진행률·pause/resume/중단, 세부 3D 표시와 단위/외형 |

- 제목 drag, 우하단 resize, 실제 영역 접기, 숨김/다시 열기를 지원한다. 숨김은 측정·파일 저장·송신·재생 중지가 아니다.
- `더보기 → 이 창 위치·크기 초기화`는 해당 패널만, `UI 표시 → 센서 도구 UI 초기화`는 소유 패널 배치·열림·폰트만 초기화한다.
- 글자 배율은 85/100/125/150%. 작은 창은 스크롤하거나 확대한다. 외부 UserWidget 내부를 재귀 변경하지 않는다.
- 듀얼 카메라는 기존 RenderTarget을 공유한다. 보조 카메라는 보기 전용이며 추가 capture/readback을 만들지 않는다.
- Esc 종료 요청·종료 버튼·선택 변경·패널 숨김/파괴는 조작을 시작한 Actor의 설정과 이전 실행 상태를 복원한다. 드래그 완료만으로 조작 모드를 끝내지는 않는다. D3D12 자동 회귀는 통과했으며 실제 키보드 Esc 확인은 데스크톱 접근 오류로 미완료다.

| 저장 파일(`Saved/SaveGames`) | 내용 |
|---|---|
| `MA0T10_SensorToolWorkspace_v1.sav` | 소유 창 배치·크기·접힘·열림·글자 배율 |
| `MA0T10_VirtualSensorUI_v6.sav` | 센서 보기·캡처/출력/연결의 비밀정보 아닌 옵션; 이전 슬롯 migration |
| `MA0T10_SensorToolAppearance_v1.sav` | 기존 폰트 설정 호환용 |
| Slab 시나리오 | **파일 저장 없음**. GameInstance 메모리에 최근 최대 10개 |

센서 Transform/장비 설정을 UI SaveGame에 저장하지 않는다. PIE 변경의 원본 맵 반영은 명시적 저장 예약으로만 수행한다.

### LiDAR 보기의 차이

- **태그 기반 의미 분류:** Actor Tag/클래스/이름 규칙. 태그가 없으면 같은 회색일 수 있다.
- **자동 형상 구분:** XYZ의 기준면/돌출 분석. 파랑/주황 표시이며 Slab 인식 AI나 개별 물체 추적이 아니다. 모호한 기준면은 높이 색상 fallback으로 표시한다.
- **높이 색상:** 센서 로컬 Z 또는 월드 Z의 연속값. 수직 설치에서 현장 높이가 필요하면 월드 Z를 사용한다.
- **월드 XY 조감도:** 설치 회전과 무관한 평면도. 로컬 XY 조감도는 센서 기준이며 수직 센서에서는 다른 단면처럼 보일 수 있다.
- **방사 거리-높이:** X=`sqrt(local X²+Y²)`, Y=local Z. 좌우 방향이 합쳐진다.
- **전방 수직 슬라이스:** 회전한 로컬 X-Z 평면의 설정 두께 내 점. 방향 분석에 사용한다.
- 거리 영상/Split 전용 격자·깊이 경계와 조감도의 축/거리 원은 별개다.
- 표현은 **2D / 2D+월드 포인트 / 포인트 전용**. 전용 보기는 플레이어 뷰만 숨겨 센서 SceneCapture를 보존한다. 현재 진입 이후 새 Actor 자동 추적은 별도 보완 대상이다.
- GPU 측정과 Niagara 표시 renderer는 별개다. Niagara가 실패하면 CPU ISM fallback/사유를 표시하며 자산 로드만으로 성공 판정하지 않는다.

GPU 의미 분류는 별도 proxy 장면으로 불투명 StaticMesh/ISM/HISM을 지원한다. 깊이 일치·설정 revision을 검사한다. 투명/마스크/WPO/미지원 geometry는 추정하지 않는다. “GPU는 무조건 태그 정보를 가질 수 없다”는 오래된 설명은 현재와 다르며, 필터 사용 전 해당 프레임의 semantic 지원 상태를 확인한다.

## 5. Slab 데이터·자동 실행·재실행

- 입력 Topic: **`topic.scenario`**, MESSAGE_ID: **`IFactory-agent`**. DTCore `WebSocketTopics`에 구독되어야 한다. 다른 업무 Topic을 삭제하지 않는다.
- TC는 백그라운드 parse 후 게임 스레드에서 `ReceiverId`로 DataSync를 찾는다. 기본 ID는 `SlabScenario.Main`이며 Actor와 handler가 같아야 한다.
- 수신 경로는 **DTCore→TC→DataSync** 또는 **`ASlabActor::SubmitScenarioJson(Json)`** 하나다. Actor 호출 전에 archive 등록 API를 별도로 호출하면 중복 UUID로 자동 실행이 막힐 수 있다.
- 직접 API는 게임 스레드에서 호출한다. `SubmitScenarioJson`의 `true`는 비동기 파싱 요청 접수이며 실행 성공이 아니다. 실제 결과는 `GetLastScenarioAdmissionStatus()`와 실행 상태·경고로 확인한다.

| 필드 | 현재 해석 |
|---|---|
| frame_no | 원본 Slab 상태 번호. 센서 FrameId/렌더 프레임과 독립 |
| mtl_no | 한 시나리오의 소재 식별자 |
| slab_len/wth/thk | 형상 치수. 입력 단위를 cm로 변환 |
| center_x | Track 로컬 X 위치 |
| left_skew_angle | 실제 회전각(도); right는 비교 차트 |
| elapsed_sec | 시간 기준 보간 |
| mov_pos / center_y | 이동에는 사용하지 않고 원문 보존 |
| slab_wt | 표시 정보; 물리 질량 아님 |
| _meta.UUID | 원본 시나리오 ID. 신규 입력에서 누락하면 내부 ID 생성 |
| _meta.duration_sec | 있으면 종료 시각. 없으면 마지막 시각+sample period(기본 .05초) |

**치수·위치 기본 단위는 cm다.** `slab_len=10830`은 기본값에서 108.3m다. 검증맵은 치수 mm/위치 cm이므로 길이 10.83m이다. 단위를 추정하거나 원문을 변환해 덮어쓰지 않는다. 원본 행이 건너뛰어도 행을 만들어 넣지 않고 자세만 보간한다.

신규 벌크는 보관 후 자동 실행한다. 단, 중복 UUID/잘못된 입력은 거절하고 기존 실행·pause·drain 중 새 벌크는 **보관만** 한다. 나중에 자동 큐로 실행하지 않는다.

- 기본 출력: PCD만. Camera 이미지/LiDAR 정보는 선택 사항이며 이미 시작한 실행에는 소급 적용하지 않는다.
- 송신 구성이 부족하면 **unbound 관찰 fallback**으로 Slab 이동만 진행한다. 기존 독립 스트림을 중지·재예약하지 않고 `TransmissionWarning`을 남긴다.
- 준비 검사는 설정 검사이지 Broker 연결/receipt 보장이 아니다. 실행 중 설정을 복구해도 해당 관찰 실행에서 송신을 저절로 켜지 않는다.
- `GetLastScenarioAdmissionStatus()`는 자동 실행/송신 없이 실행/보관만/중복/거절과 이유·요청/적용 출력을 제공한다.
- 목록 재실행은 원문/시나리오 ID를 유지하고 **새 RunUUID**를 만든다. 기본 관찰 전용이다. 움직임 종료와 승인된 데이터의 송신 drain(최대 10초)은 별도 상태다.
- 준비·재생·pause·drain 항목은 삭제하지 않는다. 목록 삭제는 원본 파일/Broker/Slab Actor를 삭제하는 기능이 아니다.

진행 패널의 3D 분석은 선 기본 4px(2~12), 글자 28px(18~56), 어두운 배경판, signed Margin과 `[침범]`을 제공한다. 음수 Margin은 기하학적 침범이며 안전 기준/AI 판정이 아니다. 보조 표시는 센서 측정에서 제외한다.

테스트 발행(로컬 Broker가 이미 정상 실행되고 입력 Topic을 구독한 경우):

```powershell
node Tools/Artemis/publish_slab_scenario.mjs --topic topic.scenario
node Tools/Artemis/publish_slab_scenario.mjs --input "C:/TestData/scenario.json" --topic topic.scenario
```

## 6. 측정 프로필과 출력 계약

### 프로젝트 프리셋

| FullSpec | 설정된 규격 |
|---|---|
| D455 Camera | 1280×720 / 30Hz / JPEG 기본 품질 80 |
| Livox 계열 기본 LiDAR | 360×60 / 10Hz |
| ML-X(80) Integration 200 | 200×56 / 20Hz / 최대 150m |
| ML-X(80) Native | 576×56 / 20Hz / 최대 150m / 공개 사양 해석 기반 밀도 |

프로필 값과 **실제 완료율**은 다르다. ML-X Native는 제조사 원시 패킷/실장비 보정 완료가 아니며 D455 SceneCapture는 실장비 depth stream이 아니다. CPU/GPU의 geometry·Echo 지원 차이, 캘리브레이션 근거와 fidelity 상태를 함께 확인한다. enum에 HardwareRayTracing/SDK 이름이 있다고 실장비 backend가 완성된 것은 아니다.

### 측정률·정체 진단

- 시작/재등록 후 `max(1초, 실제 적용 주기의 3배)`는 시작 유예다. 이후 진행이 이 시간보다 오래 없으면 과거 양수 Hz가 남아 있어도 유효 측정률은 0Hz다.
- Settings 상세 부하 요약에서 센서별 시작 유예·측정 중·일시정지·조작용 경량 미리보기·정체를 구분한다. 처리 실패 이력이 있는 정체는 별도 문구로 표시한다.
- World 시간으로 신선도를 계산하므로 PIE 일시정지는 정체 시간에 포함하지 않는다. 일시정지 중에는 진단만 갱신하고 측정하지 않는다.
- `SensorRates`와 `StarvedSensorIds`를 공개한다. 기존 공정성 수치를 읽는 Blueprint/C++는 `bCameraFairnessEvaluable`/`bLidarFairnessEvaluable`도 확인해야 한다. 판정 불가 수치 0은 정상 공정성을 뜻하지 않는다. 조작용 임시 주기는 정격 공정성 통과로 처리하지 않는다.
- Camera acquisition 신선도는 기존 SceneCapture 제출 정의이며 실제 GPU 픽셀 완료/FrameId 일치는 별도 RT-04 검증 대상이다. 송신 receipt·소비자 검증 Hz와도 구분한다.
- 성능 보고서는 워밍업 이후의 0Hz 표본 및 요청 센서 누락을 평균으로 숨기지 않고 실패로 판정한다.

### Topic/Body

| 방향·용도 | 기본 Topic | Body / schema |
|---|---|---|
| Slab 입력 | topic.scenario | JSON, MESSAGE_ID=IFactory-agent |
| Camera 출력 | topic.virtual.sensor.camera.0 | 고성능 원본 JPEG / virtual-camera.jpeg.v1 |
| LiDAR 값 출력 | topic.virtual.sensor.lidar.0 | point 배열 없는 통계 JSON / virtual-lidar.telemetry.v1 |
| PCD 출력 | topic.virtual.sensor.export.0 | 완전한 PCD v0.7 DATA binary / virtual-pointcloud.pcd.v1 |

Raw TCP STOMP worker가 binary I/O·receipt·자체 수신 검증을 담당한다. 기존 JSON `virtual-camera.v1`/`virtual-lidar.v1`은 호환 경로에 남는다. HTTP JSON/raw file POST도 별도 지원한다. `wss://`는 Engine STOMP 호환 경로이며 Raw TCP TLS 지원으로 보지 않는다.

**receipt는 Broker 수락이다.** 내부/외부 소비자 수신 검증, 최종 업무 처리 ACK, 로컬 파일 완료는 각각 별개다. 정상 연결 중 FIFO여도 네트워크 단절 중 프레임을 디스크에 영구 보존하는 기능은 아니다. Raw outstanding receipt 상한의 보완은 RT-01에서 추적한다.

### PCD 소비자가 알아야 할 최소 계약

```text
FIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence
SIZE 4 4 4 2 2 2 1 1 8 1 4
TYPE F F F U U U U U I U F
COUNT 1 1 1 1 1 1 1 1 1 1 1
DATA binary
```

- 한 점 33바이트, little-endian. XYZ는 센서 로컬 **m**, X 전방/Y 좌측/Z 위쪽이다. 12/36/40바이트 stride로 읽으면 안 된다.
- `# MA0T10_META {JSON}` 주석에 sensor_id, sensor_frame_id, timestamp_utc 및 선택적 run_uuid/scenario_uuid/mtl_no/frame_no/elapsed_sec가 있다. Slab frame_no와 sensor_frame_id는 다른 번호이며 식별 정수는 문자열로 보존한다.
- snapshot pose가 있으면 `sensor_to_world_m`(row-major 4×4, column vector 적용)로 UE 월드 meter를 복원한다. 센서를 회전한 경우 로컬 Z에 센서 높이만 더하면 틀린다.
- `POINTS 0`도 정상 프레임이다. checksum/content-length는 주석을 포함한 전체 파일 바이트 기준이다. 일반 PCD 라이브러리 재저장은 사용자 주석을 제거할 수 있다.
- 실시간 PCD는 Binary 고정이다. 수동 ASCII/기존 API의 주석 포함 여부를 동일하다고 가정하지 않는다.
- Tag/Semantic/ROI 필터는 측정 출력 조건이다. `PointCloudTarget`/Semantic은 Digital Twin 정보이며 ML-X 고유 기능이라고 표기하지 않는다. `mtl_no`가 붙어도 모든 점이 그 Slab 점이라는 뜻은 아니다.

```powershell
python Scripts/read_pcd_context.py received.pcd
python Scripts/inspect_binary_pcd.py received.pcd --plate-z-m 0
```

### 파일 저장과 외부 입력

- 새 프레임 저장: 실행 중인 센서의 요청 후 완료 프레임을 최대 2초 기다린다. 정지 센서를 몰래 켜거나 이전 프레임으로 대체하지 않는다.
- 현재 프레임 저장: 기존 완료 snapshot을 고정하며 추가 측정하지 않는다. 없는 출력은 비활성/실패 안내한다.
- 주기 저장: .05~3600초, 최신 완료 프레임을 비동기 저장한다. 같은 FrameId·busy·새 프레임 없음은 저장 기회 생략으로 집계한다. Topic 주기와 독립적이다.
- 기본 루트 `Saved/SensorCaptures`, 주기 저장 `LocalTimedCapture/<UTC>/Camera|Lidar`, Recorder `Saved/SensorRecordings`.
- 요청 대상·출력·revision을 고정하고 실제 파일 생성 후 완료 이벤트를 보낸다. LAZ는 외부 compressor가 필요하며 프로세스 timeout/cancel 보완이 남아 있다.
- 외부 Source(CSV/JSONL replay, Camera buffered JSON, LiDAR HTTP/UDP)는 센서 입력이다. 서버 수신 진단은 출력 검사다. ROS2/Livox/RealSense SDK 클래스는 현재 stub/확장 지점이며 실제 SDK 지원 완료가 아니다.

## 7. 구조와 현재 재사용 경계

| 기존 구성 | 책임 |
|---|---|
| VirtualSensorActorBase / Camera·LiDAR Actor | 실행·편집 상태·interaction·외부 프레임 API |
| Capture / Scan / Analysis / Visualization / Export | 측정과 파생 처리 분담 |
| VirtualSensorSchedulerSubsystem | 자동 작업 분배; 센서 수 기반 60/30 FPS 예산은 지원 보증이 아님 |
| IVirtualSensorAcquisitionBackend | 기존 GPU LiDAR 경계; 아직 Camera/SDK 전체의 범용 확장 계약은 아님 |
| Output / StreamPublisher / Transport / Recorder | 프레임 라우팅·직렬화·송수신·기록 |
| FileSave / PeriodicFileSave | Widget 수명과 독립적인 bounded 파일 작업 |
| SlabContext / Replay / SlabActor | 업무 세션·보관·이동; 미래 플러그인에서는 host 경계로 분리 필요 |
| ToolWorkspace / PanelHost | 명시적 소유 UI·Main/Viewport 재호스팅 |

`UVirtualSensorIntegrationSettings`/`UVirtualSensorDtCoreBridgeSubsystem`는 현재 소스에 없다. 예전 제안이 구현 완료된 것으로 오인하지 않는다. 분산된 기본값과 Slab 직접 참조를 먼저 정리해야 한다.

## 8. 최신 검증과 제한

### 센서 안정성 수정 — 2026-09-22 / 59d1027

- UE 5.3 Editor Development 빌드 통과.
- RT-02 수정 전 회귀에서 원래 Camera가 640px·0.2초에 남는 선택/종료 문제 재현. 수정 후 lifecycle·monitor follow·D3D12 PIE runtime **3/3 통과**.
- RT-03 성능 자동화 **5/5**, 실제 성능 판정 스크립트의 정상/0Hz/센서 누락 검사 **3/3 통과**.
- 전체 `MA0T10` NullRHI: **172개 중 실제 통과 156 / 실패 1 / 조건부 skip 15**. 실패는 기존 사용자 변경 `SensorTestMap`의 LiDAR 높이 fixture다. 그중 조작 runtime skip은 별도 D3D12 실행에서 통과했다. 신규 실패는 없으며 전체 green으로 표시하지 않는다.
- Computer Use는 `0x80070057` 화면 캡처 및 `0x80070005` 커서 접근 오류로 실제 마우스·Esc 검증을 수행하지 못했다. RHI 실행 인자는 1280×720이지만 실제 client viewport 크기는 미계측이다.
- 장시간 Artemis·다중 센서 성능, WBP 전수 재컴파일, pinned clean checkout은 이번에 재실행하지 않았다. 기존 개인 UI 저장값은 백업에서 복원했다.
- 증거: `Saved/Reports/SensorStability/report.ko.md`, `reproduction.log`, `rt03-focused`, `full-regression`, `final-rhi`. Saved는 커밋하지 않는다.

### 이전 표시·송신 검증 — f7bdd3b

아래는 기존 실행 증거이며 이번 안정성 수정의 신규 성능 측정 결과가 아니다.

- Development 빌드, 6개 WBP 메모리 컴파일, V2 자산 검사 통과.
- 전체 168개 기록: 실제 통과 157 / 실패 2 / 조건부 skip 9. helper pool의 과거 37개 기대값을 42로 갱신하고 최종 집중 3/3 통과. 남은 운영맵 높이 fixture는 사용자 맵을 수정하지 않았다.
- 실제 RHI 표시 OFF/ON: Camera 변경 0픽셀, GPU depth/semantic 변경 0점. 양성 대조군 Camera는 2,533픽셀 변화.
- ML-X Native 한 대 PCD-only, warmup 10초 후 30초×2, **실제 viewport 753×403**: 제출/receipt/내부/외부 수신 599+600=1,199개 일치, invalid/gap/duplicate/overflow 0, 활성 구간 약20Hz.
- 평균 FPS 59.999/59.799, 1% low 59.940/45.019, frame p95 16.667ms. 후반 1% low 저하를 무시하지 않는다. 다중 센서나 3-stream/1920×1080 보증으로 확대하지 않는다.
- Computer Use 실제 viewport: 내장 988×521, 별도 창 1174×683. 목표 1280×720/1920×1080 전수 검증, 회사 프로젝트, pinned clean checkout, 장시간 soak는 미완료다.
- 증거: `Saved/Reports/SlabVisibility/report.ko.md`, `all.log`, `final_focused.log`, `pcd_external.json`, `pcd_rhi.log`, 실제 밝은 화면 PNG. Saved는 git에 포함되지 않는다.

검증 명령/그룹은 사용 목적에 맞게 선택한다. 기본 quick check가 운영맵 생성이나 전체 성능 테스트를 뜻하지 않는다.

```powershell
& Scripts/validate_sensor_v2_refactor.ps1 -RequireAssets
& "C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$($PWD.Path)/ma0t10_dt.uproject" -d3d12 -unattended -nosplash -nop4 "-ExecCmds=Automation RunTests MA0T10;Quit" "-TestExit=Automation Test Queue Empty"
```

실제 RHI/Artemis 그룹은 해당 환경변수·Broker·외부 구독자가 필요하다. skip을 pass로 세지 않는다. `run_sensor_map_stream_rhi_smoke.ps1`, `run_fullspec_performance_evidence.ps1`, `run_lidar_geometry_stream_test.ps1`의 인자를 먼저 확인한다.

## 고정 참고자료

아래 문서는 이전 상세 명세/작업 기록이며 현재 UI·지원 성능의 기준 문서는 아니다. 경로를 참조하는 스크립트를 위해 보존한다.

- 계약: [Camera v1](docs/camera_payload_schema.md), [LiDAR v1/v2](docs/lidar_payload_schema.md), [전송](docs/server_transport_contract.md), [과거 스트리밍 상세](docs/sensor_streaming.ko.md)
- 센서: [테스트맵](docs/sensor_test_map_setup.ko.md), [높이·좌표](docs/lidar_height_semantic_coordinates.ko.md), [태그 없는 형상](docs/lidar_geometry_validation.ko.md), [V2 migration](docs/sensor_v2_migration.ko.md)
- Slab/UI: [시뮬레이션 상세](docs/slab_simulation.ko.md), [재생 상세](docs/slab_scenario_replay.ko.md), [센서 연동](docs/sensor_slab_integration.ko.md), [Workspace 상세](docs/sensor_tool_workspace.ko.md)
- 이력/범위: [PR16 병합 기록](docs/sensor_pr16_merge_notes.ko.md), [Editor smoke 이력](docs/editor_smoke_test.md), [실장비 adapter 제안](docs/real_sensor_adapter_plan.md), [PixelStreaming 참고](docs/pixel_streaming_setup.md)
