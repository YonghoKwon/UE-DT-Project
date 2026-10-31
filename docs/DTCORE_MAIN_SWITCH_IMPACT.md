# DTCore main 전환 1단계 영향도

2026-10-31 사용자 승인 범위: `chore/dtcore-main-switch`, parent 기준 `d8048bfaf2bc5107189fce666b3111ea9ff8e974`(시작 시 `origin/master` 기준), 작업 위치 `C:\Unreal Projects\wt_dtcore_main`.
DTCore는 `b2504d1dfea5659f832ff0d750515edee146a28e` → **`bc60f05e2d1e64f4ffe1410cf3239fe24cb4d4c9`**로 고정한다. `git ls-remote`로 원격 `refs/heads/main` tip을 확인했고 worktree 전용 submodule을 초기화했다. `.gitmodules`의 `branch = main`은 추적 브랜치 정보이며 재현 기준은 parent의 정확한 gitlink다.

**코드 적용·정적 점검만 수행한다. 빌드·Editor·PIE·패키징·실행·성능·실제 Broker 검증은 미수행이며 다음 단계다.** 메인 프로젝트의 파일·프로세스·DTCore 작업 트리와 별도 `UE-DT-Core-Plugin` 작업 트리는 수정하지 않는다. worktree Git 메타데이터는 Git이 지정한 `.git/worktrees/wt_dtcore_main/` 아래에 생성된다.

## 조사 범위와 근거

- DTCore PR #3·#4를 포함한 `b2504d1..bc60f05`의 28개 변경 파일, [QUALITY_90.md](../Plugins/DTCore/docs/QUALITY_90.md), 공개 헤더·구현·설정 redirect·이관 시험/fixture를 대조했다. 별도 DTCore 저장소의 문서와 새 gitlink의 문서도 확인했다.
- parent `Source/` 전체의 WebSocket/STOMP/receipt·DTCore 설정/위젯/데이터 사용처를 검색했다. 적용 전 검색 결과는 114파일·923행이며 행 번호 목록은 `Saved/Reports/DTCORE-MAIN-SWITCH/source-usage-inventory.json`이다. 직접 DTCore 호출과 프로젝트 자체 STOMP 구현을 구분했다.
- 과거 README/IMPROVEMENTS/ROADMAP/검증 이력의 `b2504d1`은 일괄 치환하지 않았다. 과거 결과를 새 의존성의 통과로 취급하지 않는다. 이번 고정값은 `.gitmodules`, parent gitlink, AGENTS, guard 및 이 문서를 따른다.

## 컴파일·동작 영향과 적용

| 변경 | 프로젝트 사용처·영향 | 이번 적용/남은 확인 |
|---|---|---|
| `bPresistent` → `bPersistent` | `UI/CoilLoadingMonitor.cpp`, `UI/ShipHatchMonitorWidget.cpp`가 이전 필드를 직접 설정하므로 컴파일 수정 필수 | 두 이름만 수정. DTCore `Config/DefaultDTCore.ini`의 `/Script/DTCore.DxWidget.bPresistent` → `bPersistent` PropertyRedirect 존재 확인. 기존 BP/저장 자산은 수정하지 않았고 로드 확인은 후속 |
| 소비 프로젝트 friend 제거 | `DTCoreSlabBrokerTests.cpp`의 `bConnectedBroadcast`, `PendingSubscribeCount`, `StompClient`, `bWantsConnection` 접근 불가 | 공개 `GetConnectionState()`로 Ready/Disconnected 확인. Ready 시 실패 구독 수·재연결 횟수 0을 조회/검증하여 기존 시험을 완화하지 않음 |
| 이관 시험의 클래스·등록 중복 | 프로젝트와 플러그인에 동일한 `FDTCore*Test` 11개가 남으면 중복 구현/등록 위험 | 아래 이름·본문 대조 후 프로젝트 측 11개와 전용 fake/fixture/include만 제거. Camera/Crane/SlabBroker 유지 |
| 상태 API/이벤트 | `Disconnected/Connecting/Subscribing/Ready/Reconnecting/GaveUp`, 상태 변경 시 GameThread 이벤트. 기존 `OnConnected`는 구독 callback batch 완료 알림 유지 | 새 프로젝트 구독 루프/관리자를 만들지 않음. Ready는 모든 구독 성공·소비자 업무 ACK를 의미하지 않음 |
| 구독 receipt timeout | `SubscribeReceiptTimeoutSeconds` 양수일 때 미회수 batch receipt를 실패 수에 더하고 Ready/기존 OnConnected 1회 통지. 늦은/중복/이전 generation callback 무시 | 기본 0 유지. 프로젝트의 추가 `Subscribe()`는 완료 callback이 DTCore 자체 batch callback일 때만 이 timeout 추적에 포함됨 |
| Topic 라우팅 | `SlabActor.cpp`, `SensorMapCheckWidget.cpp`, `WebSocket/TC/VirtualSensorExternalSourceReceiverAdapter.cpp`가 설정·Subscribe/Unsubscribe 사용. WebSocketTopics/ApiTopics 모두 기존처럼 WebSocket 큐의 `MESSAGE_ID` dispatch | Slab `topic.scenario`/`IFactory-agent`, TC→DataSync 경로 유지. 토픽별 최초 수신 로그만 추가됨 |
| 데이터 통계 | `UDxDataSubsystem::GetStatistics()` 추가. API/WS 적용 수, JSON 실패, dispatch 누락, 미등록 키, 큐 대기를 독립 원자 카운터로 관찰 | 업무 핸들러/처리 순서·배치/5ms 추출 예산 유지. 기존 `TotalProcessedCount` 의미 유지. 파서가 반환한 null은 공통 JSON 실패와 다름. 새 UI 폴링 없음 |
| 위젯 prune | Workspace `BringOwnedPanelToFront()`의 임시 등록→BringToFront→등록 해제, Coil/ShipHatch host 등록, 외부 입력 blocker와 접점 | DTCore가 중복 검사/BringToFront/hover/Main 교체에서 화면 트리에서 떨어진 항목 제거. 화면에 붙은 정적 자식 보존, 추가 close hook 없음. 숨김·접기·Main/Viewport 재호스팅·중복 열기·Z-order 직접 수용은 후속 |
| HTTP/로그/입력 설정 | `UDTCoreSettings`, `DxApiSubsystem`, `DxLogSubsystem`, `ADxPlayerControllerBase`, `AInteractableActor` 사용/상속 | 기본값은 아래와 같이 기존 동작 유지. HTTP Body/URL 우선순위·세대 수명 유지. CCTV/Ship 설정은 Legacy 표시만 바뀌고 키는 유지 |
| 빌드 의존 | DTCore 자체 계약/품질 시험이 Runtime 모듈에 추가됨 | 실행 시험 코드는 `WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR`. UnrealEd/UMGEditor/Kismet/BlueprintGraph/KismetCompiler는 Editor target의 Private 의존. 실제 Editor/게임 빌드 확인은 후속 |
| 고정 SHA 진단 | guard, large-content 정책의 guard 문자열 검사, precommit 요약, `UxrP1P2CaptureTests.cpp`/`UxrP2bCaptureTests.cpp` 메타데이터 | 모두 새 전체 SHA로 갱신. AGENTS의 의존성 기준을 main/bc60f05로 갱신. 허용 경로/테스트 기대 수치·성능 기준은 완화하지 않음 |

## PCD receipt·재연결·메모리

PCD 고성능 경로는 프로젝트의 `Core/VirtualSensorHighThroughputTransportSubsystem` Raw TCP STOMP worker다. `SubscribeReceiptTimeoutSeconds`는 **DTCore의 구독 batch receipt** 설정이며 Raw worker의 **SEND frame receipt** 설정이 아니다. 새 Ready/실패 구독 수를 PCD 송신 성공이나 소비자 처리 완료로 대체하지 않는다.

Raw worker는 기존처럼 `PendingReceipts`에서 제출 후 5초를 검사하고 retry attempt 3까지 재송신한 뒤 최종 실패로 처리한다. 동일 프레임의 reservation을 재사용하며 접수부터 receipt/실패/종료까지 worker가 소유한다. 전역 128MiB/스트림 64MiB, Camera 8/기타 20프레임 한도·ConnectedNoLoss FIFO·압축·33-byte PCD·frame/run 식별자는 이번에 수정하지 않았다. 재연결/backoff·소켓 송수신·파일 저장·스캔·GPU readback을 게임 스레드로 이동하지 않았다.

`Sensor/VirtualSensorTransportComponent`의 Engine STOMP 호환 송신도 프로젝트가 만든 별도 `IStompClient`를 사용한다. `SendStompBinaryStreamRequest()`의 binary body, broker receipt callback, `OnDataSent`와 업무 ACK 구독은 DTCore 구독 timeout과 별개다. `VirtualSensorExternalSourceReceiverAdapter`의 자체 5초 구독 대기/재시도, Raw/shared 수신 경로도 그대로 둔다.

DTCore 업무 연결의 재연결은 기존 지수 backoff/최대 재시도 설정을 유지하며, retry timer 예약 시 횟수가 증가하고 전송 연결 성공 때 0으로 돌아간다. 수동 Disconnect는 의도를 지우고 Disconnected가 된다. 최대 횟수 도달은 GaveUp이며 `ConnectWebSocket()`으로 재시도한다. 기본 timeout 0이면 미회수 구독 receipt로 Subscribing에 계속 머물 수 있다. 이 상태를 수신 준비 완료로 표시하거나 조용히 timeout을 활성화하지 않는다.

메모리 관점에서는 atomic 카운터가 고정 크기로 추가되고, batch receipt 추적은 구독 수에 비례하며 완료·새 연결·disconnect·종료 때 정리된다. 미등록 handler 경고 키 집합은 Subsystem 수명 중 서로 다른 키 수에 비례해 증가하고, 최초 수신 로그 집합은 서로 다른 수신 토픽 수에 비례한다. 큐 대기 통계에는 이미 추출된 batch가 포함되지 않으며 여러 카운터의 snapshot은 동일 시점을 보장하지 않는다. PCD 바이트/큐 한도를 새 통계로 대신하거나 변경하지 않는다.

위젯 hover/prune는 트리를 따라가며 임시 방문 집합을 만들고, 입력 hover는 기존 정적 충돌 object query를 인스턴스 목록에서 구성하는 방식으로 바뀐다. 기본 결과가 같아도 CPU/할당 비용의 동일성은 아직 측정하지 않았다. 기존 프로젝트 #26 메모리 증가의 해결 또는 악화를 정적 조사로 단정하지 않는다. QUALITY_90의 compat host 수치·시험은 이 소비 프로젝트의 수용/장기 누수 부재 증거가 아니다.

## 새 설정 기본값

아래는 DTCore C++ 기본값이며 이번에 프로젝트 Config/개인 설정을 저장하거나 덮어쓰지 않았다. 새 숫자/경로 항목은 `[/Script/DTCore.DTCoreSettings]`이며 기존 `[DTCoreRuntimeOverride]` 접속 문자열 reader에 새 키가 추가된 것은 아니다. 배포 환경별 override/BP의 실제 유효값은 다음 실행 단계에서 확인한다. 비밀값은 문서/증거에 옮기지 않는다.

| 항목 | 기본값 | 영향 |
|---|---|---|
| SubscribeReceiptTimeoutSeconds | 0초 | 기존 무기한 구독 receipt 대기 |
| HttpRequestTimeoutSeconds | 0초 | SetTimeout 미호출, 엔진 기본값 |
| HttpMaxAttempts / HttpInitialRetryDelaySeconds | 3 / 1초 | 최초 포함 3회, 기존 지수 2 유지. 코드 최소 1회/0.001초 |
| LogDirectory / LogRetentionDays | 빈 값 / 7일 | LaunchDir/Logs/CustomLogs, 최소 1일. 경로는 Initialize 때 고정 |
| HighlightStencilValue / ErrorStencilValue | 100 / 254 | 기존 하이라이트 값. 일반 Actor 소유 메시에는 Interactable CDO 기본값 |
| PriorityTraceChannel | ECC_GameTraceChannel1 | 기존 우선 채널 |
| TraceObjectTypes | WorldStatic, WorldDynamic, Pawn, PhysicsBody, GameTraceChannel1 | 기존 5개 대상 |

## 11개 이관 시험의 동등성 대조

원본은 parent `d8048bf`의 `Source/ma0t10_dtEditor/Private/DTCoreContractTests.cpp`, 대상은 DTCore `bc60f05`의 `Source/DTCore/Private/Tests/DTCoreContractTests.cpp`다. 표의 이름은 원본 `MA0T10.DTCoreIntegration.<이름>` → 대상 **`DTCore.Contract.<이름>`**이며 동일한 테스트 클래스가 유지된다. 줄 번호는 이 두 고정 SHA 기준이다.

| 이름 | 원본/대상 줄 | assertion 수(양쪽) | 내용 대조 |
|---|---|---|---|
| BlueprintCloseDispatch | 142 / 108 | 2 | 실제 BP close override 값 77, 재닫기 값 19 유지 |
| ClickPolicy | 172 / 150 | 6 | 중복 press/release, double/single, UI 차단, 외부 blocker 비소유 |
| ParseShutdown | 205 / 190 | 4 | 실제 worker 시작·join, 늦은 적용 0, handler 소유 해제 |
| LogFlush | 228 / 216 | 4 | 40개 로그 순서, IO 완료·강제 쓰기 실패 노출 |
| RegistryGC | 253 / 245 | 8 | GC 추적·무효 정리·별칭 교체·stream-out 해제 |
| SubscriptionLifecycle | 284 / 281 | 11 | 혼합 성공/실패·중복/이전 세대 callback, Topic 0, 미회수 대기, 수동 종료 |
| HttpBodyAndFallback | 332 / 341 | 12 | URL 우선순위, UTF-8/빈 Body, parameter, 종료 후 callback/새 요청 차단 |
| WidgetIdentifiers | 416 / 412 | 2 | 기존 enum 7개 값/순서와 byte WidgetFlag |
| QueueSurvivesWorldCleanup | 427 / 424 | 1 | World cleanup 뒤 GI 큐 업무 적용 1회 |
| RegistryEngineStreamOut | 451 / 451 | 3 | 실제 World/Actor EndPlay, 모든 별칭 해제·GC |
| WidgetCloseOrder | 476 / 479 | 5 | 자식→부모 hook, remove 선행, 재진입/반복 닫기 중복 없음 |

총 **58개 assertion 호출의 기대값·인자·표현식이 fixture 이름 치환 후 동일**하다. 본문 토큰도 대조했으며 차이는 Blueprint 컴파일 helper를 동일 CompileBlueprint 호출/성공 조건으로 인라인한 것과 로그 출력 디렉터리뿐이다. private fixture는 `UDxCoreTest*`/`FDxCoreTest*`, fake는 `FDxCoreContractFakeStomp`로 이름만 격리했으며 slow parser·callback 증가·close trace의 내용도 확인했다. 원본·대상 본문과 비교 JSON은 `Saved/Reports/DTCORE-MAIN-SWITCH/`에 보존하며 커밋하지 않는다. 이는 정적 대조이며 58개 실행 통과를 뜻하지 않는다.

제거한 프로젝트 전용 항목은 fake STOMP, slow transaction/parse probe, close-order widget, Listener의 message/ready/http callback·카운터와 전용 include다. `UDTCoreMainAcceptanceWidget`은 `SensorInputAcceptanceTests.cpp`에서 쓰므로 보존했다. `CompileBlueprintForValidation`은 `Scripts/validate_dtcore_integration_assets.py`가 쓰므로 보존했고, 준비/상태 조회 helper도 유지했다. CameraAcquisitionSnapshot/CraneDispatch 본문은 원본과 동일하게 유지한다. SlabBroker는 업무 검증·맵·opt-in 조건을 유지하고 공개 상태 API로만 교체하며 실패 구독 수/재연결 초기화 assertion 2개를 추가한다.

## 정적 gate와 후속 단계

- `git diff --check`, PowerShell 변경 스크립트 구문 검사, 고정 SHA/삭제 fixture 잔여 참조·보호 경로·유지 시험 본문 대조를 수행한다. 레이어 allowlist는 **0→0**, 위반 0이다. 상세 증거/결과는 `Saved/Reports/DTCORE-MAIN-SWITCH/result.json`과 정적 점검 JSON에 둔다.
- 기존 guard는 parent gitlink staged/unstaged 변경 자체를 금지한다. 이번 승인된 전환 전에는 이 이유로 위반을 보고하며 기대값/내부 worktree 청결은 별도로 확인한다. 규칙을 완화하지 않고 커밋 뒤 새 고정 SHA에서 guard 전체를 다시 실행한다.
- 기존 `merge_check.ps1`은 승인 예외 인자가 없어 gitlink 변경을 `Protected path changed: Plugins/DTCore`로 BLOCK한다. 테스트/guard 변경과 미수행 gate도 독립 검수 대상이다. 판정기를 수정하거나 이 결과를 자동 머지 허용으로 바꾸지 않는다. 이번 요청 자체가 gitlink 전환 권한의 근거이며 후속 빌드·회귀 지시와 Claude 독립 검수는 별도다.
- 다음 단계는 동일 parent 커밋/DTCore bc60f05에서 Editor Development 및 게임 빌드, `DTCore.Contract` 11개·`DTCore.Quality` 7개와 유지된 Camera/Crane/SlabBroker 관련 회귀, BP redirect/자산 읽기 검증이다. 실제 Broker·PCD 송신 receipt/재연결·누락/큐/메모리, 숨김/재호스팅 직접 UI, 단독 실행 ABAB 성능·회사 수용은 각각 별도로 기록해야 한다. 이 단계에서는 실행하지 않는다.

`Config/Game.ini`, `Config/SensorTransport.ini`, `Samples/PixelStreaming`, `CLAUDE.md`, `docs/IMPROVEMENTS_CLAUDE.md`, 운영맵/자산은 변경·stage하지 않는다. DTCore 소스 수정/커밋, push/PR/merge도 수행하지 않는다.

## 2단계 검증 결과와 미해결 사항 (2026-10-31)

실행 소스는 parent `5c35b854331765fa07b8fdf272393052b6a40adf` / DTCore `bc60f05`다. 소스 변경 없이 UE 5.3 `ma0t10_dtEditor Win64 Development` 빌드를 91.08초에 완료했다. 이 절을 추가한 문서 커밋은 실행 소스를 바꾸지 않는다. 실행은 한 번에 Editor 1개로 진행했으며 증거는 `Saved/Reports/DTCORE-MAIN-SWITCH/stage2/`에 보존한다.

| 검증 | 결과 | 증거/조건 |
|---|---|---|
| quick/layer/guard | PASS | `verify.ps1 -Level quick -BaseCommit 5c35b85` 9단계. 전환 완료 커밋 이후의 보호 상태 검사이며 d8048bf 대비 gitlink 변경 승인 자체를 대신하지 않음. allowlist/위반 0/0 |
| MA0T10 + DTCore.Contract + DTCore.Quality | 최초 444 성공 / 44 skip / 6 실패 | D3D12 offscreen, 총 494개. DTCore 18개 전부 성공. 실패 6개는 Coil 화면 캡처의 Saved fixture 부재 |
| Coil fixture 준비 후 재실행 | 6 성공 / 0 실패 | 기존 `capture_coil_environments.py prepare`와 제품 시험이 생성한 seed 사용. 기대값/제품 코드 변경 없음. 이 시점의 고유 결과는 450 성공 / 44 skip |
| 실제 기본값 / BP 읽기·메모리 컴파일 | PASS | receipt timeout 0, reconnect 10회, HTTP timeout 0/최대 3회/초기 1초, 로그 빈 경로/7일. BP 17개 컴파일, 저장 패키지 0. helper의 Python 노출 이름 오류 2회는 별도 로그에 보존하고 reflection 이름으로 수정 후 통과 |
| Raw socket healthy 120초 | PASS | 1 synthetic 576×56×33-byte 스트림, 0.1초 간격, NullRHI(socket 전용), 로컬 TCP fixture. 서버 최종 SEND/receipt 1,189/1,189, 거절·과부하·송신 실패 0, 종료 reservation 0 |
| Raw socket reset 20초 | PASS | 8초마다 연결 reset 주입, 연결 3회/끊김 2회와 재연결 확인, 종료 reservation 0. 실제 Artemis/회사 Broker 시험이 아님 |
| native PCD 120초 | **FAIL** | D3D12, SensorTestMap 보관 사본, client 1920×1080, LiDAR 1대/0.1초, 로컬 receipt+echo. 아래 실패를 유지 |

native `MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke`는 전체 입력 1,300건에 대해 직렬화·제출·receipt가 각각 1,299건이었다. 측정 ledger 1,200건은 모두 receipt와 loopback 소비가 존재하고 invalid·송신 실패·과부하는 0이지만, 전체 입력 대비 부족 1건을 성공으로 바꾸지 않는다. 평균 46.20FPS(기준 ≥55), 1% low 33.18FPS(≥45), engine frame p95 29.58ms(≤20)로 기존 절대 성능 assertion도 실패했다. callback wall p95는 34.19ms이며 engine frame과 구분한다. ABAB 비교를 수행하지 않았으므로 이 실패를 DTCore 전환에 귀속하거나 기존 문제라고 단정하지 않는다.

추가 opt-in 실행까지 같은 시험 이름의 최신 결과를 합치면 **451 성공 / 42 skip / 1 실패**다. healthy/reset은 같은 socket 시험을 서로 다른 조건으로 각각 실행했다. native 실패의 기대값·시험 수치·센서/PCD 계약을 완화하거나 범위 밖 Runtime을 수정하지 않았다. 장시간 #26 메모리 검증, 표준 RT on ABAB, 회사/실장비 수용과 직접 마우스 수용은 미수행이다.

### 실행 범위 사고와 복원 제한

최초 smoke wrapper 호출에서 Codex가 PowerShell 인자를 `-ProjectPath=...` 형태로 잘못 전달하여 스크립트 기본값의 메인 프로젝트 Editor를 1회 실행했다. 프로세스는 exit 255로 종료했다. 이는 사용자 지시의 메인 파일·프로세스 보호 위반이며 즉시 보고했다. 이후 명시적 argv와 worktree 경로 assertion으로 전환했다.

읽기 전용 확인에서 메인 `Saved/Config/WindowsEditor`의 `EditorPerProjectUserSettings.ini`, `Engine.ini`, `GameUserSettings.ini`, 새 crash 설정 및 `Saved/Logs/ma0t10_dt.log`의 갱신을 확인했다. 사전 메인 개인 설정 백업이 없어 이전 내용으로 복원했음을 보장할 수 없다. 이를 추정 덮어쓰기하지 않았다. 메인의 기존 Git 변경 파일 중 사고 시작 이후 갱신된 파일은 0개였지만, 이 시각 검사를 사전 전체 해시 증명으로 취급하지 않는다. `launch-incident.json`에 값/설정 원문 없이 경로·시각·제한을 기록했다.

승인 worktree는 사전 개인 설정 파일 0개와 보호 파일 191개를 기록했고 검증 후 동일 해시/초기 부재를 복원했다. 기존 설정 시험이 자동 생성한 worktree `Config/Game.ini`는 증거 사본을 Saved에 보존한 뒤 초기 부재로 복원했으며 stage하지 않았다. DTCore 내부, SensorTransport 템플릿, PixelStreaming, Claude 문서, 운영맵/자산은 최종 불변이다. 2단계 결과의 `personal_settings_restored`는 당시 메인 사고의 미확인 상태 때문에 false로 보고했다.

**2단계 종료 시점에는 native 실패와 보호 범위 사고를 보존하며 push/PR을 보류했다.** 당시 사용자 지시의 “통과 시 push 후 PR” 조건을 충족하지 못했다. 후속 사용자 지시는 아래 3단계 비교 및 기존 동일 실패 판정 시 PR 생성을 승인했다. 대용량 생성물 정리 후보는 `stage2/CLEANUP.md`에 있으며 삭제하지 않았다.

## 3단계 재현성·구버전 비교 판정 (2026-10-31)

**판정: 이번 조건에서 기존(master) 동일 실패.** 각 버전에서 셰이더/DDC 준비용 15초 실행을 먼저 하고, 동일 native 시험을 새 버전 2회 → 구버전 2회 순서로 측정했다. 본 실행은 모두 D3D12, ProjectDefault RT, 실제 client 1920×1080, LiDAR 1대/0.1초, Camera preview, warmup 10초+측정 120초, 로컬 TCP receipt+echo다. 맵 payload와 native 시험의 해시는 네 실행에서 동일하다.

| DTCore / 실행 | 입력 | 직렬화=제출=receipt | 평균 FPS | frame p95 ms | 결과 |
|---|---:|---:|---:|---:|---|
| bc60f05 / new-1 | 1300 | 1299 | 46.18 | 29.55 | 동일 6개 assertion 실패 |
| bc60f05 / new-2 | 1300 | 1299 | 45.98 | 29.59 | 동일 6개 assertion 실패 |
| b2504d1 / old-1 | 1299 | 1298 | 46.08 | 29.57 | 동일 6개 assertion 실패 |
| b2504d1 / old-2 | 1300 | 1299 | 46.11 | 29.59 | 동일 6개 assertion 실패 |

각 실행의 측정 ledger 1,200건은 모두 receipt/loopback이 있고, 재시도·과부하는 0이다. 종료 시점에 따른 전체 입력 수 차이는 있지만 부족분은 네 실행 모두 1건이다. 평균 FPS 중앙값은 새/구버전 46.080/46.095(-0.033%), p95 중앙값은 29.57/29.58ms(-0.034%)다. 네 본 실행의 정기 감시에서 GPU VideoDecode, 외부 Unreal, ShaderCompileWorker가 모두 0으로 관측됐다. 준비 후에도 재현됐으므로 최초 셰이더/DDC 준비만으로 설명되는 실패는 아니다. 이 순차 AABB 진단을 표준 출시 ABAB 2% gate 통과로 취급하지 않는다.

구버전은 승인된 동일 worktree의 로컬 임시 브랜치 `tmp/dtcore-main-ab-20261031`, 커밋 `973f683`에서 실행했다. DTCore 소스를 편집하지 않고 b2504d1로 체크아웃했으며, 소비 측은 두 UI의 필드 이름과 SlabBroker의 구버전 friend 접근만 빌드 호환 상태로 돌렸다. 이 baseline의 Runtime 전체와 native 시험은 전환 기준 master `d8048bf`와 바이트 단위로 동일함을 확인했다. 임시 커밋은 최종 작업 브랜치에 합치거나 push하지 않는다.

동일 누적 집계/종료 drain 및 절대 성능 실패는 이번 DTCore 전환 PR 범위 밖으로 기록한다. receipt timeout·통계·위젯 prune·설정 기본값에 새 원인 징후가 없어 프로젝트 Runtime·설정 대응을 추가하지 않았고 native 기대값도 변경하지 않았다. 실패의 상세 수정 또는 모든 환경에서의 무회귀를 증명한 것은 아니다. 원본 실패 report는 그대로 유지한다.

비교 후 `chore/dtcore-main-switch`와 bc60f05로 복귀하고 Editor를 재빌드했다. 구버전 빌드 65.69초, 복귀 빌드 69.89초 모두 exit 0이다. 실행 전마다 `C:\Unreal Projects\wt_dtcore_main\ma0t10_dt.uproject`를 출력하고 절대경로를 검증했다. 이번 단계에서는 메인 프로젝트를 실행·수정하지 않았다.

후속 사용자 통보에 따르면 Claude가 메인 `EditorPerProjectUserSettings.ini`, `Engine.ini` 두 파일을 **r21 백업으로 복원 완료**했다. 이는 복원 주체의 통보이며 Codex가 메인을 다시 실행하거나 파일을 재검증한 결과가 아니다. 이전 사고와 이 복원 사실을 모두 PR에 남긴다.

증거는 `Saved/Reports/DTCORE-MAIN-SWITCH/stage3/comparison.json`, `classification.json`, `old-baseline-identity.json`, 각 실행의 `environment.jsonl`/`automation/index.json`, 빌드 로그 및 최종 `result.json`이다. 기존 동일 판정 후 push/PR은 후속 사용자 지시 범위에서 수행하며, native FAIL 및 일반 merge_check의 보호 gitlink 차단을 숨기지 않는다. 장시간 #26·표준 ABAB·직접 UI·회사 수용·PR merge는 별도다.
