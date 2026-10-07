# UE-DT-Project — 가상 센서와 Slab 시뮬레이션

현재 기능 기준: **`3c9c95c` 기반 시나리오 송신 신뢰성·센서 설정 적용 개선, DTCore `b2504d1`, 2026-10-07**. Unreal Engine 5.3 C++ 프로젝트다. Camera·LiDAR 측정/미리보기/송신, Slab 벌크 시나리오의 보간 이동·분석 표시·차트·재실행을 제공한다.

**아직 독립 센서 플러그인이 아니다.** “센서 여러 대를 항상 정격 주기로, 어떤 PC에서도 무부하로 실행”하는 단계도 아니다. 현재 구현과 실측 범위, 다음에 보완할 기능을 구분한다.

## 2026-10-06 main 기반 최소 DTCore 적용

DTCore는 main `b22af0b`에서 필요한 오류·종료 안전성만 적용한 `b2504d1`을 사용한다. 기존 센서·Slab 기능을 유지하는 별도 연동 브랜치에서 검증했다. `OnConnected`는 성공/실패를 포함한 구독 완료 callback 집계이며 모든 구독 성공을 보증하지 않는다. custom 수신 binding을 보존하고, 공통 5초 timeout·all-ready 상태기계·World cleanup의 GI 전역 큐 폐기는 포함하지 않는다. 이전 안정화 기록의 해당 API 설명은 현재 계약으로 사용하지 않는다.

로그 쓰기·parse 종료 안전성, Registry EndPlay/stream-out 정리와 Widget 제거 후 Blueprint hook을 검증했다. SingleRelease와 FPS 숨김은 프로젝트 Controller가 선택하며, 미등록 Widget 정책은 유지한다. 공통 변경 이유와 소비자 이관은 [최소 수정 계약](Plugins/DTCore/docs/MINIMAL_MAIN_MIGRATION.md)을 참조한다. 검증 결과는 `Saved/Reports/DTCoreMinimalAcceptance/FINAL_STATUS.md`에 있다. 최종 `443b48a` 전체 RHI 자동화184 Success(실제179·opt-in skip5), 신규 실패0이며 독립 host/프로젝트 Editor·Shipping과 Development 패키징을 통과했다. 직접1286×760 PIE에서 조작·숫자 입력창 첫 Esc·폰트·패널·Slab 실행/재생을 확인했다. PCD 전용과 세 stream의 별도1920×1080·10+60초 시험은20/30Hz·평균60FPS·p9516.67ms, 승인 request 집합의 내부/외부 누락0이다. **재생 성능20ms 조건은 기존24.908ms/최소수정24.925ms로 두 버전 모두 실패**하므로 성능을 포함한 최종 검수 완료로 표시하지 않는다. Geometry 성능·10회 stream 반복의 최종 버전 시험은 미수행이다. 이후 발견한 소유 숫자 편집창 Esc는 프로젝트에서 수정했고 전송 계약은 동일하다.

## 2026-10-07 Slab 초기 배치 복귀 검증

- 진행 패널에서 종료·정리 후 초기 위치/회전으로 복귀한다. 형상·보관 목록은 유지하고 진행·차트·3D 분석을 대기로 초기화한다. 사용법은 아래 Slab 절을 따른다.
- UE 5.3 Editor Development 빌드, 전체 `MA0T10` 194건 Success/실패 0(실제 185·조건부 skip 9), 별도 Slab 12건 Success(실제 11·PCD opt-in skip 1), 소유 WBP 6개 메모리 컴파일(저장 0)을 확인했다. 전체 시험의 skip은 실제 통과로 합산하지 않는다.
- 실제 Artemis의 `30초 PCD → drain → 복귀 → 같은 시나리오 30초 재실행`은 각 600건, 총 1,200건 제출/receipt/내부·외부 수신이 일치했고 invalid/gap/duplicate/overflow 0이다. 복귀 후 대기 1초의 추가 제출은 0건이다. 센서와 Slab 프레임 개수가 항상 같다는 보장은 아니다.
- Computer Use로 실제 client **1286×760, 글자 100%**에서 실행/pause 비활성 사유, 완료·중단 후 복귀, 동일 목록 재생, 숨긴 차트 재표시와 3D 라벨 제거를 확인했다. 임시 GUI 관찰 스크립트의 미노출 Python API 오류는 보정 후 재실행했으며 production 결함으로 집계하지 않는다. DTCore·운영맵은 수정하지 않았고 개인 UI 저장값은 복원했다.
- 증거: `Saved/Reports/SlabResetPlacement/FINAL_STATUS.md`. PCD 시험의 실제 viewport는 1068×360이다. 이번 시험을 목표 해상도 전체 성능 검수나 기존 재생 p95 문제 해결로 확대하지 않는다. push/PR은 별도 요청 전까지 하지 않는다.

## 관리 문서 4개

2026-10-07 송신 신뢰성 검증: Editor Development 빌드, 최종 D3D12 `MA0T10` **198 Success/실패0(실제188·opt-in skip10)**, 소유 WBP6개 메모리 compile/save0. 별도 TCP fixture의 인증 거절·연결 지연·단절·receipt 누락·과부하·센서 삭제 6종을 통과했다. 실제 Artemis 30초 두 실행에서 PCD 전용1,200건 및 세 출력 Camera1,800/LiDAR정보1,200/PCD1,200건의 제출·receipt·내부 수신이 일치하고 외부 PCD1,200건도 검증했다. 실제 자동화 viewport1068×360, 직접 client1286×760/100%다. 세 출력 평균59.85~59.91FPS·p9516.67~16.69ms는 작은 viewport의 집중 결과이며 목표해상도·기존 replay p95 문제 해결 인증이 아니다. 직접 필수 선택·준비 대기·실패·결과 조회·복귀 후 기록 보존을 확인했다. 상세 증거는 `Saved/Reports/ScenarioDataReliability/FINAL_STATUS.md`. Engine STOMP/WSS 필수 모드의 별도 실사용 시험과 장시간 수집·외부 업무 ACK는 남아 있다.

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
- DTCore는 필수 submodule이다. 이번 승인된 작업은 동기화본 `b22af0b`의 새 API를 유지하고 공통 안정화 커밋을 별도 생성한다. parent gitlink는 검증한 plugin 커밋으로 고정한다. enum 반환형 소비 코드는 이관이 필요하며 무수정 호환이라고 설명하지 않는다. [main 최소 수정 계약](Plugins/DTCore/docs/MINIMAL_MAIN_MIGRATION.md)를 따른다.
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
- Esc 종료 요청·종료 버튼·선택 변경·패널 숨김/파괴는 조작을 시작한 Actor의 설정과 이전 실행 상태를 복원한다. 드래그 완료만으로 조작 모드를 끝내지는 않는다. D3D12 자동 회귀는 통과했다. 2026-10-02 직접 Esc 입력은 Editor의 기본 Stop PIE 동작으로 재생 창을 종료했다. PIE 유지 상태에서 조작만 종료하는 물리 키 검증은 별도로 남는다.

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

### 데이터 필수 실행과 실행 결과

데이터 패널의 신규 시나리오 설정 또는 재생 패널에서 **`데이터 필수 실행 · 연결 준비 후 시작`**을 선택한다. 기본값 `ObservationAllowed`는 기존 관찰 fallback이고, 선택값 `RequireData`는 대상 센서/출력 검사와 실제 STOMP CONNECTED 확인 후 움직임을 시작한다. 준비 시간은 기본 10초(1~120초), 준비 중에는 현재 자세를 유지하며 `중단`으로 취소할 수 있다. 출력이 하나도 선택되지 않은 필수 실행은 거절한다.

- 연결 끊김·최종 receipt 실패·과부하·센서 삭제/정체·필수 스트림 중지 때 움직임과 신규 접수를 중단하고 최대 10초 동안 승인된 데이터를 정리한다. 자동 재개하지 않으며 재실행은 새 RunUUID다.
- 준비/실행/drain 동안 해당 센서의 규격 편집과 경량 조작 모드는 잠근다. 센서 설정 적용은 원래 실행/정지 상태를 유지하고 Scheduler로 갱신하며, 설정 적용 때문에 동기 캡처·스캔·파일 저장을 추가하지 않는다. NaN/무한대는 거절한다.
- 결과는 `준비 실패 / 사용자 중단 / 부분 전송 / Broker 수락 완료 / 관찰 완료`로 표시한다. Broker 수락은 해당 실행의 승인 프레임 receipt 기준이며 측정률·자체 검증·외부 업무 처리는 별개다.
- 진행 패널과 데이터의 `연결·진단 → 실행별 전송 결과`에서 최근 20개 요약을 확인한다. 초기 위치 복귀·새 실행·목록 삭제에도 이전 요약을 유지한다. 결과 폴더에는 작은 JSON만 저장하며, 기본 100개를 넘으면 이 기능의 schema/파일명에 해당하는 파일만 정리한다. 저장 실패는 전송 실패와 구분한다.
- 파일: `Saved/Reports/ScenarioRuns/slab-run-<RunUUID>.json`, schema `ma0t10.slab-run.v1`. 원본 PCD/JPEG·인증 정보는 포함하지 않는다. 실행 중 진단 식별자는 65,536건으로 제한하며 초과를 명시적 오류로 처리한다. 장시간 수집 지원 인증은 별도다.
- API: `FSlabExecutionOptions`, `ASlabActor::SetExecutionOptions`, `RequestScenarioReplayWithExecutionOptions`, `USlabRunResultsSubsystem::GetRecentRunResults/GetRunResult/OpenRunReport`. 기존 재생 API는 기본 관찰 정책으로 유지한다. 사용자 WBP는 데이터 패널의 `SetLiveScenarioExecutionOptions/GetScenarioRunResults/OpenScenarioRunReport`를 사용할 수 있다.

### 초기 배치로 복귀

진행 패널의 **`초기 위치로 복귀`**를 누르면 PIE 시작 시 레벨에 배치되어 있던 Slab의 월드 위치·회전으로 즉시 돌아간다. 시나리오 첫 행이나 Track 원점으로 돌아가는 기능이 아니다.

- 현재 형상·스케일·재질과 최대 10개의 보관 목록을 유지한다. 진행률은 0%, 현재 프레임은 `—`, 세 차트와 이전 3D 분석 라벨은 대기로 초기화한다. 표시 선택·선/문자 크기·창 배치는 유지한다.
- 실행·일시정지·입력 파싱·재생 준비·센서 송신 정리 중에는 비활성화되며 같은 사유를 API에서도 반환한다. 먼저 중단하거나 완료를 기다리고 송신 정리까지 끝낸 뒤 사용한다.
- 복귀 자체는 새 RunUUID, 센서 세션, 캡처/스캔이나 자동 송신을 만들지 않는다. 이전 receipt·실패 기록은 연결·진단에 남는다. 이후 신규 UUID는 정상 자동 실행하며 기존 목록의 같은 시나리오도 재실행할 수 있다.
- C++/Blueprint: `ASlabActor::CanResetToInitialPlacement(OutReason)` / `ResetToInitialPlacement(OutError)`. 사용자 WBP는 `USlabProgressPanelWidget::CanResetSlabToInitialPlacement` / `ResetSlabToInitialPlacement`에 버튼을 연결한다. Designer와 기존 binding은 자동 변경하지 않는다.

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
- Camera acquisition 신선도는 SceneCapture 제출 정의다. 스트리밍 readback은 해당 capture 직후의 복사와 고정 snapshot을 사용한다. slot 부족/합쳐진 capture는 명시적 파생 실패로 기록하며 나중의 픽셀을 옛 FrameId에 붙이지 않는다. acquisition·JPEG 완료·receipt·소비자 검증 Hz는 서로 구분한다.
- 성능 보고서는 워밍업 이후의 0Hz 표본 및 요청 센서 누락을 평균으로 숨기지 않고 실패로 판정한다.

### Topic/Body

| 방향·용도 | 기본 Topic | Body / schema |
|---|---|---|
| Slab 입력 | topic.scenario | JSON, MESSAGE_ID=IFactory-agent |
| Camera 출력 | topic.virtual.sensor.camera.0 | 고성능 원본 JPEG / virtual-camera.jpeg.v1 |
| LiDAR 값 출력 | topic.virtual.sensor.lidar.0 | point 배열 없는 통계 JSON / virtual-lidar.telemetry.v1 |
| PCD 출력 | topic.virtual.sensor.export.0 | 완전한 PCD v0.7 DATA binary / virtual-pointcloud.pcd.v1 |

Raw TCP STOMP worker가 binary I/O·receipt·자체 수신 검증을 담당한다. 기존 JSON `virtual-camera.v1`/`virtual-lidar.v1`은 호환 경로에 남는다. HTTP JSON/raw file POST도 별도 지원한다. `wss://`는 Engine STOMP 호환 경로이며 Raw TCP TLS 지원으로 보지 않는다.

**receipt는 Broker 수락이다.** 내부/외부 소비자 수신 검증, 최종 업무 처리 ACK, 로컬 파일 완료는 각각 별개다. 정상 연결 중 FIFO여도 네트워크 단절 중 프레임을 디스크에 영구 보존하는 기능은 아니다. Raw worker reservation은 입력부터 receipt/최종 실패까지 유지된다. 기본 전역 128MiB·스트림 64MiB 및 Camera 8/기타 20프레임을 넘으면 명시적 과부하로 거부한다. 재시도는 같은 body를 중복 집계하지 않는다.

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

### 최종 검수 진행 — 2026-10-03

현재 production source는 `2eb30f0`, DTCore는 `067195b`다. DTCore 공통 소스·wire schema·센서 규격은 이번 최종 검수에서 바꾸지 않았다. 숫자 Transform 편집의 경량 조작 유지, 실제 Slate 포인터 경로, 큰 글자에서 소유 패널 최소 폭을 보완했다.

현재 시험의 `1% low` 표기는 `1000 / frame-time p99` 계산값이다. 최악1% 프레임의 평균을 사용하는 다른 벤치마크 정의와 같다고 해석하지 않는다. engine FApp delta와 실제 callback wall pacing도 별도로 기록한다.

- LiDAR envelope UTC가 측정 완료 시점이던 결함을 `c1b86eb`에서 실제 측정 시작 시점으로 수정했다. 이전 12회 비교와 장시간 시험의 PCD 지연 27~36ms는 completion-to-consumer 기록이며 acquisition-to-consumer 통과 근거가 아니다. 전달 건수·checksum·FrameId 증거와 시각 기준을 분리한다. 독립 baseline에도 같은 시각 변환만 적용해 비교를 정규화했다.
- 정규화 후 기준선에서 간헐적인 성능 실패가 발생했다(1280×720 세 스트림: 평균 56.90FPS/1% low 33.70FPS/p95 28.61ms, 1920×1080 PCD: 평균 59.08FPS/1% low 34.40FPS). 동일 조건의 추적 재시험 3회는 통과했지만 원인은 아직 확정되지 않았다. 실패를 정상 재시험으로 지우지 않으며, 최종 12회 비교·전후 5% gate는 미완료다. 사용자 요청에 따라 다른 앱·서비스를 중지하지 않고 현재 환경에서 분석 중이다.
- `117e6e3`의 1920×1080 세 스트림 집중 시험은 평균 59.79FPS/1% low 59.57FPS/p95 16.67ms, 전체 구간 acquisition-to-consumer p95 Camera 49ms·PCD 66ms·LiDAR telemetry 72ms였다. 제출·receipt·내부 검증·외부 MESSAGE 집합을 대조했다. 한 번의 집중 시험이며 전체 비교 또는 다른 맵/다중 센서 지원 인증이 아니다.
- 현재 환경의 10분 trace에서 렌더 작업 대기와 D3D12 텍스처 생성 지연을 관측했다. 매 LiDAR 프레임의 staging readback 생성을 `0c502af`에서 재사용으로 변경했고, 동일 크기·해상도 변경·진행 중 취소/재시작의 실제 깊이와 generation 시험을 포함한 관련 6개 테스트를 통과했다. 수정 후 1920×1080 세 스트림60초는 평균59.75FPS/1% low55.28FPS/p9516.7ms, PCD19.963Hz·gap/invalid/duplicate0으로 통과했다. 외부 CPU 부하도 함께 관측됐고 개별 RHI 자원 이름은 trace에 없어, 간헐적 지연의 전체 인과 또는 완전 해결을 단정하지 않는다.
- `e6afe4f`는 재사용 버퍼의 이전 GPU fence를 새 완료로 읽지 않도록 nonblocking render 제출 fence를 추가했다. `2eb30f0`는 기즈모 클릭 위치가 viewport인데 전역의 이전 UI hover 때문에 거부되던 회귀를 수정했다. 실제 클릭 위치의 소유 viewport hit path를 사용하고 실제 패널 위 클릭은 거부한다. 의도적으로 이전 패널 hover를 남긴 Slate 회귀를 통과했으며 DTCore의 공통 hover API는 변경하지 않았다.
- Camera는 실제 장면의 64bit acquisition token+CRC16 격자를 완료 JPEG마다 독립 해독해 990개 프레임을 검사했다. 이전 JPEG/새 metadata 음성 대조군, readback 포화·설정 변경·Stop/재시작·삭제를 포함한다. 관측한 JPEG만 폴링하던 이전 시험과 구분한다.
- 재생은 관찰 1회와 30초 PCD 2회, 1,200건 제출/receipt/소비자 검증 일치·context 오류 0·평균 59.93FPS·p95 16.67ms다. 시험 프로세스의 Editor background throttle을 명시적으로 해제하고 복원했으며 사용자 설정은 저장하지 않았다. 이전 25.07ms 결과를 숨기거나 production 최적화 효과로 표시하지 않는다.
- 현재 `2eb30f0`의 전체 `MA0T10` 보고서는190건 Success/실패0이다. 그중185건을 실제 수행했고 geometry 성능·replay runtime·continuous stream·repeat·production Slab의5건은 opt-in skip이다. continuous/repeat는 아래 현재 source의 집중 시험으로 갱신하고, 다른3건의 이전 source 증거는 최신 통과로 합산하지 않는다. 앞선 입력 hover 회귀1건의 실패 원본도 보존했다. 커밋된 운영맵은 Saved에 격리해 기존 assertion을 그대로 검사하고 사용자 맵은 보존한다.
- Slate 입력 시험은 첫 Esc/키 반복 후 PIE 유지, 원설정 복구, Data 패널 geometry, Main 재호스팅과 미등록 Widget font 불변을 확인했다. 이 결과를 직접 마우스 검증으로 대신 표시하지 않는다.
- 현재 source의 실제1920×1080 10분 실행은 PCD/telemetry 각12,000건(20Hz), Camera17,999건(29.998Hz)의 승인 요청/제출/receipt/내부·외부 검증 집합 일치와 missing/invalid/gap/duplicate0을 확인했다. 평균59.99FPS·1% low59.96FPS·frame p9516.67ms, 외부 acquisition-to-consumer p95 PCD73ms·Camera47ms였다. callback wall p9522.75ms는 별도로 기록한다. 첫1분 이후 private bytes8.03~8.13GB에는 전체 측정 ledger 보관 비용도 포함되며, 장기 무누수 인증은 아니다. 호환 JSON 파생 출력 생략·acquisition deadline miss는 승인된 PCD 전달 누락과 별도로 기록한다.
- 현재 source의 45초 실행+정리10회를 통과했다. 모든 회차의 승인 요청/제출/receipt/내부·외부 검증 집합과 missing/invalid/gap/duplicate0, outstanding frame/bytes0 반환을 확인했다. Camera29.956~30Hz·PCD19.956~20Hz, 외부 p95 Camera48~49ms·PCD71~73ms였다. 정리503~507ms, 정리 후 private bytes8.12~8.15GB로 약32MB 증가했으며 이 결과를 장기 무누수 또는 완전한 메모리 plateau로 표현하지 않는다. 중단된 초기 fixture·외부 probe timeout은 별도 보존했다.
- 운영 Slab adapter의 신규 실행+같은 시나리오 재생 2회 및 LiDAR geometry 표시/PCD 전송 시험도 실제 Artemis·외부 구독자로 통과했다. 출력 선택과 서로 다른 RunUUID·원본 scenario UUID·Slab frame의 계약을 유지한다. 전체 시험의 조건부 항목은 이러한 집중 실행 증거와 이름별로 연결하며 skip 자체를 통과로 세지 않는다.
- 현재 source의 프로젝트·독립 최소 host Editor Development/Shipping 빌드, Windows Development Build/Cook/Stage/Archive, 16개 Blueprint 메모리 컴파일(저장0), V2 자산 검사를 통과했다. 독립 host는 별도 복사한 검증된 DTCore source/DLL을 사용하며 다른 실제 소비 프로젝트의 검증을 대신하지 않는다.
- 직접 마우스로 Camera/LiDAR 이동, LiDAR 회전, 첫 Esc의 PIE 유지, 네 글자 배율과 패널 resize/접기를 확인했다. 현재 source에서는 Camera Z170→180 입력 후5Hz 경량 조작 유지, Settings 숨김 후 기존640×360/10Hz 미리보기 복귀, 실제 client1280×720의150% 제목/버튼 가독성을 추가 확인했다. 일반 PIE 전체화면 단축키는 크기를 바꾸지 않았고 앞선 최대 client는1920×998였으므로1920×1080 직접 조작은 환경상 미완료다. 자동화의 정확한1920×1080 측정과 혼동하지 않는다. 남은 직접 조작·최종3개 opt-in 회귀·12회 비교 gate 때문에 **최종 검수 완료가 아니다**. 증거는 `Saved/Reports/DTCoreFinalAcceptance`에 둔다. 개인 UI9개와 테스트 fullscreen 설정은 복원했다.

### DTCore 동기화·센서 출력 안정화 — 2026-10-02

아래는 이전 안정화 단계의 기록이다. 최종 검수의 신규 증거와 제한은 위 항목을 우선한다.

- 동기화본의 `GetType(): int32`, Widget byte API를 유지한다. 구형 Widget 식별자 0~6은 Hidden 호환 항목으로 보존한다. 현재 Crane 생산/소비 코드를 함께 이관했다.
- HTTP UTF-8 Body와 URL 우선순위, 수동 Disconnect 의도·generation·구독 receipt 5초 timeout, parse/log worker 종료와 registry GC 보유를 검증한다. 전송 연결 이벤트와 전체 구독 준비 이벤트를 구분한다.
- 공통 클릭 기본값은 DoublePress, 현재 프로젝트 Controller는 SingleRelease를 명시한다. 소유 패널은 약한 입력 차단자로 등록하며 이 등록은 다른 Widget의 style/close/z-order 소유권을 이전하지 않는다. Blueprint close dispatch는 한 번만 실행한다.
- 로그 `LogDirectory`가 비어 있으면 기존 경로를 보존한다. 상대 경로는 Project Saved 기준이다. `FlushLogs()` 결과를 확인한다. `bPresistent`의 모드 유지 정책은 아직 연결되지 않았다.
- RT-01 fake Broker 실제 SEND/receipt 누락 시험과 RT-04 D3D12 JPEG 색상 marker·pose/UTC/포화 시험을 통과했다. marker 검사는 폴링으로 관측한 완료 JPEG에 대한 검사이며 모든 프레임을 독립 검증했다고 확대하지 않는다.
- 10초 warmup+60초 실제 Artemis 외부 구독: PCD 전용 19.93Hz, 세 스트림 Camera 29.95Hz·LiDAR 19.98Hz·PCD 19.96Hz, invalid/gap/duplicate 0. 엔진 frame 평균 60FPS·1% low 59.98 이상·p95 16.67ms. 세 스트림 callback wall p95는 21.97ms로 별도다.
- RT-01/04 전후 기준선은 공통 DTCore 안정화 이후 `8c04c5d`다. 동기화본 자체가 컴파일되지 않았으므로 동기화 전후 전체 성능 비교로 표시하지 않는다. 실제 PIE viewport는 1274×680이며 요청 1920×1080 성능 인증이 아니다.
- 세부 결과·미수행·보호 파일 hash는 `Saved/Reports/DTCoreSync`에 남긴다. 다른 실제 소비 프로젝트, Linux, 회사 PC, 60분 soak 및 센서 플러그인화는 검증/완료 범위가 아니다.
- 10분 세 스트림 연속 시험: Camera29.99Hz·LiDAR/PCD20.00Hz, PCD 입력/제출/receipt/내부검증12,197건 일치, 외부 측정구간11,999건·invalid/gap/duplicate0. 외부 구독자의 측정구간은 내부 전체 실행구간과 다르므로 두 raw count를 직접 같다고 주장하지 않는다. 평균59.99FPS·1% low59.97·p9516.67ms. 메모리58표본의 private bytes는7.56~7.60GB; 60분 leak 인증은 아니다.
- 최종 전체 MA0T10 D3D12:186개 중 실제통과174/조건부skip11/실패1. 실패는 보호된 운영맵의 기존 LiDAR 높이 fixture다. SlabBroker와 RawReceiptProbe·ContinuousThreeStreamSmoke 등 일부 skip은 별도 opt-in 집중 실행에서 통과했으나 전체 green으로 표시하지 않는다. 마지막 구조체 default 보강 후 공통10개·소유Blueprint16개 메모리 컴파일을 다시 통과했다(저장0).
- 현재 프로젝트/격리 호스트 Editor Development·Shipping 컴파일, 현재 프로젝트 Windows Development Build/Cook/Stage/Archive 통과. 중간 DLL 잠금 및 중단된 재생 시험 로그와 단독 재실행 성공 로그를 함께 보존한다. 반사 구조체의 미초기화7필드도 기본값을 명시했고 저장된 자산은 재저장하지 않았다.
- 재생 실통합은 관찰1회+PCD30초2회, 제출/receipt/내부검증1,200건·invalid context0·외부검증 통과. 평균 engine55.52FPS/p9525.07ms로 **20ms 목표 미달**이며 고정3stream 시험과 별도 조건이다.
- Computer Use 직접 확인: Camera/LiDAR 선택, 조작 시작·경량 preview, Settings drag/resize/hide 후 LiDAR4Hz 복귀. 실제 viewport1606×728. 물리 기즈모 이동·PIE 유지 Esc, Data 메뉴/연결 제어, Main 빈 영역, 전체 목표해상도 검증은 미완료다. 캡처0x80070057·활성화 오류 후 추가 입력을 중단했다. 자동화 UI 통과를 해당 마우스 검증의 대체로 표기하지 않는다.
- 마지막 HTTP/default 보강 포함 재측정(`dtcore_final_*`): PCD-only19.92Hz; 세 스트림 Camera29.96Hz·LiDAR/PCD19.98Hz, 엔진평균59.99FPS·1%low59.99·p9516.67ms, callback wall p9522.04ms. PCD 직렬화p953.39ms(기준선3.44ms), 끝queue0/0/0·invalid/gap/overflow0. 전송 end-to-end p95의 전후5% 비교 전체값은 별도 계측 공백으로 남긴다.

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
