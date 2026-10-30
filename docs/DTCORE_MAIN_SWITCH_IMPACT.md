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
