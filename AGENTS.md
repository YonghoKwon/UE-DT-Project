# 작업 지침 — UE-DT-Project

기준: UE 5.3, 브랜치 `codex/architecture-skeleton`(분기 `fda25e0`), DTCore `b2504d1`, 2026-10-11. 작업 폴더는 `C:\Unreal Projects\ma0t10_dt` 하나다(`m7at10_dt`는 무관한 다른 프로젝트). 검증 범위와 제한은 README를 따른다.
이 파일은 작업 규칙이다. 제품 사용법이나 전체 로드맵의 구현 승인을 대신하지 않는다.

**현재 단계: ARCH-01~04 → UXR-01~07 → ARCH-05~08.** 동작·계약을 바꾸지 않는 구조 정리와 사용자 흐름 중심 UI 재정비가 최우선이며, 이번 버전에서는 **센서 플러그인 제작을 시작하지 않는다**(§5). UXR은 기능·API·저장값·기본 정책을 바꾸지 않고 화면 위치·문구·노출 수준만 바꾼다(IMPROVEMENTS 0-U절의 공통 규칙과 용어표).

## 1. 네 개의 관리 문서

| 문서 | 책임 |
|---|---|
| [AGENTS.md](AGENTS.md) | 권한·보호 범위·구현 경계·레이어 규칙·검증 및 완료 규칙·에이전트 협업 |
| [README.md](README.md) | 현재 구현·설치/사용·입출력 계약·아키텍처 뼈대(§10)·현재 검증 요약 |
| [보완 필요 사항](docs/IMPROVEMENTS.md) | 우선순위·근거·재현·개별 완료 기준(ARCH 포함) |
| [최종 목표 로드맵](docs/ROADMAP.md) | 단계별 의존 관계(A → R0~R6)·플러그인 경계·출시 gate |

- [검증 이력 보관](docs/VALIDATION_HISTORY.md)은 과거 날짜별 검증 기록의 원문 보관본이다. 규칙·현재 상태 문서가 아니다. 새 날짜별 상세 기록은 README/IMPROVEMENTS 본문에 쌓지 말고 이 파일에 추가하며, README §1과 IMPROVEMENTS 표에는 현재 요약만 둔다.
- `CLAUDE.md`, `docs/IMPROVEMENTS_CLAUDE.md`는 Claude 전용 진입·제안 메모다. 관리 문서가 아니며 권한을 확대하지 않는다. 다른 에이전트는 이 두 파일을 수정하지 않는다.
- `Agent.md`를 별도 생성하지 않는다. 기존 `docs` 문서는 경로를 보존한 고정 참고자료다. 오래된 UI/성능 설명보다 현재 코드와 README를 우선한다.
- 보완 목록/로드맵에 있다는 이유만으로 전부 구현하지 않는다. 요청한 항목과 직접 관련된 회귀만 처리한다.
- 설명·진단·검토는 읽기 전용, 문서 작성은 문서만, 기능 구현은 해당 기능만 변경한다.
- 범위 안의 조사·명확한 구현·비파괴 테스트를 매번 재승인받지 않는다. 중요한 동작 정책이 미정이거나 보호 대상 변경이 필요할 때 질문한다. 코드에서 알 수 있는 사항은 먼저 조사한다.
- 환경 때문에 한 검증이 막혀도 가능한 검증은 계속한다. 기능 완료는 구현·관련 검증·문서·제한 보고까지이며, 미수행을 통과로 바꾸지 않는다.

## 2. 보호·승인 규칙

- 시작 시 branch/HEAD/remote와 `git status`를 확인한다. 기존 변경은 보존한다.
- 명시적 요청 없이 **DTCore 소스 및 parent gitlink**, `Config/Game.ini`, 운영맵의 사용자 변경, `Samples/PixelStreaming`을 수정하거나 stage하지 않는다.
- **PixelStreaming은 필요한 기능이다(사용자 결정 2026-10-12).** `ma0t10_dt.Build.cs`의 PixelStreaming 모듈 의존, `.uproject` 플러그인 활성화, `Samples/PixelStreaming`을 C++ 사용처가 없다는 이유로 제거·비활성화하지 않는다.
- 커밋 전 `Scripts/validate_dtcore_submodule_guard.ps1`로 gitlink가 고정값(`b2504d1`)인지, DTCore staged/worktree 변경이 없는지 확인한다(ARCH-01에서 기대값을 현재 고정값으로 맞춘다).
- DTCore는 여러 소비 프로젝트가 사용하는 별도 저장소다. 명시적으로 승인된 동기화 안정화 작업에서는 plugin/parent를 각각 검증·커밋하고 검증한 gitlink만 고정한다. 그 외 작업은 프로젝트 측 adapter를 우선하며 공통 변경이 필수이면 근거·대안을 먼저 제시한다. 자동 submodule update/reset은 하지 않는다.
- `SensorTestManaged` 없는 Actor/Mesh는 이동·삭제하지 않는다. 명시적으로 배치된 `ASlabActor`의 소유 Mesh 생성은 가능하지만 임의 맵에 Slab·조명·카메라를 자동 배치하는 권한은 아니다.
- `Binaries`, `Intermediate`, `Saved`, `.vs`, 캐시·패키징 결과·실험 PNG/JSON/log는 커밋하지 않는다.
- destructive reset/checkout, 재귀 삭제, 서비스 재시작, Broker 설정 변경을 통상 구현 단계로 추정하지 않는다. 필요하면 구체적으로 확인한다.
- 커밋이 요청 범위에 포함된 작업은 관련 검증 후 의미 단위로 커밋한다. 명시적 파일 목록을 stage하고 `git add -A`를 기본으로 사용하지 않는다. 문서 작업은 문서 변경만 묶는다. 문서 작성 요청만으로 커밋·외부 게시 권한을 새로 부여하지 않는다.
- push·PR·merge는 별도 요청 범위대로만 수행한다. PR 요청은 해당 브랜치 push/PR 생성을 포함하지만 master 직접 merge는 포함하지 않는다.
- 새 지침·로드맵은 위 권한을 확대하지 않는다. 보호 규칙 완화가 필요하면 변경과 영향을 먼저 사용자에게 알린다.

## 3. 구현 경계

### 아키텍처 레이어 (A단계부터 적용)

- 레이어 정의와 현재 위반 목록의 원본은 [README §10](README.md#10-아키텍처-뼈대)이다. `Source/ma0t10_dt/MA0T10/` 기준 **L0 `Contracts/` → L1 `Sensor/` `Camera/` `Core/` → L2 `Scenario/` `Slab/` `WebSocket/TC/`·기존 업무 폴더 → L3 `UI/`**, 의존은 위 레이어가 아래 레이어만 include하는 방향으로만 허용한다.
- 새 코드는 처음부터 이 방향을 지킨다. 센서 Runtime(L1)에 UI 타입·Slab 타입·업무 TC를 새로 참조시키지 않는다. 필요하면 L0 계약/interface/delegate를 추가하고 위 레이어가 구현·구독한다.
- 센서 편집 상태 타입은 기존 L1 enum에 의존하므로 `Sensor/VirtualSensorEditableTypes.h`(L1)에 둔다. `Contracts/`(L0)에는 리터럴 기본값과 L1 타입에 의존하지 않는 중립 계약만 둔다. ARCH-06의 중립 acquisition context는 계속 L0 대상이며 이 경계를 지킨다.
- `Scripts/validate_architecture_layers.ps1`(ARCH-01)이 생긴 뒤에는 C++ 변경 커밋마다 실행한다. allowlist는 **줄어들기만** 한다. 파일 이동으로 바뀐 경로는 같은 커밋에서 갱신할 수 있으나 항목 수를 늘리지 않는다. 예외가 꼭 필요하면 근거를 보고하고 사용자 승인을 받는다.
- 구조 정리는 같은 모듈 안에서 한다. 새 UE 모듈·`.uplugin`·자산 mount 변경을 만들지 않는다. 이동 커밋(`git mv`+include 수정)과 로직 변경 커밋을 분리한다.
- UCLASS/USTRUCT/UENUM 이름, enum 값 순서, UPROPERTY, Blueprint 함수 시그니처, SaveGame 슬롯/버전, wire bytes는 구조 정리 중 바꾸지 않는다. 같은 모듈 안의 파일 이동은 `/Script/ma0t10_dt` 경로를 바꾸지 않으므로 CoreRedirect를 추가하지 않는다.

### DTCore 기반 구현 (2026-10-12 사용자 강조)

- 기능은 **DTCore가 제공하는 기반 위에** 만든다. 새 패널/화면은 `UDxWidget` 계열과 `UDxWidgetSubsystem`(열기·닫기·BringToFront·입력 차단 등록)을 사용하고, 3D 객체는 `AInteractableActor`/`AFacilityBase`, 데이터 반영은 `UDataSyncCompBase`, 상태 표시는 `UStatusVisualizerCompBase`, 업무 전문은 `UDxWebSocketSubsystem`+TC, API는 `UDxApiSubsystem`, 조회·검색은 `UDxDataSubsystem`/`UDxObjectSubsystem`/`UDxSearchSubsystem`을 먼저 검토한다. 같은 역할의 병렬 메커니즘(별도 위젯 관리자·별도 구독 루프 등)을 새로 만들지 않는다.
- DTCore로 해결되지 않으면 프로젝트 쪽 adapter로 감싸고 근거를 보고하며, IMPROVEMENTS의 **"DTCore 변경 후보"** 표에 한 줄로 기록한다(후속 DTCore 수정 단계의 입력, ROADMAP "개편 이후 예정 단계"). DTCore 소스·gitlink 수정은 여전히 금지이며, 공통 변경이 꼭 필요하면 재현 근거와 최소 변경안을 별도로 제안한다(§2).
- 기존 프로젝트 고유 구조(Workspace 소유 패널, Raw TCP STOMP worker 등)가 DTCore와 겹치는 지점은 이유를 문서화된 범위 안에서 유지하고, 새 기능에서 확장하지 않는다.

### 성능을 기능과 함께 설계 (2026-10-12 사용자 강조)

- 최적화는 나중 작업이 아니다. 기능을 구현할 때마다 비용을 함께 설계하고 측정한다.
- UI 문자열·목록 갱신은 5Hz 이하, 숨김·접힌 영역은 갱신·문자열 조립을 멈춘다. 매 Tick의 Actor 순회·할당·`FString::Printf` 누적을 피하고 변경 감지(revision) 기반으로 다시 그린다.
- 화면 기능이 측정·송신 경로에 추가 캡처·readback·동기 스캔·게임 스레드 직렬화를 만들지 않는다. 머티리얼·외형 변경은 변경 시 1회 적용한다.
- UI·조작 항목은 같은 조건(1920×1080, 1 Camera+1 LiDAR 측정 중, 10초 준비+30초)의 평균 FPS·frame p95·game thread 시간을 변경 전/후로 보고한다. 2% 넘게 나빠지면 원인을 고친 뒤 완료한다.

### 센서·측정·비동기 수명

- Runtime은 `ma0t10_dt`, Editor는 `ma0t10_dtEditor`, 조기 초기화는 `ma0t10_dtBootstrap` 모듈이다. 센서 기능은 아직 독립 플러그인이 아니며 이번 버전에서 플러그인화를 시작하지 않는다.
- DTCore `AInteractableActor → AVirtualSensorActorBase → Camera/LiDAR Actor` 계층을 유지한다. 센서 공용 Subsystem은 `MA0T10/Core`에 둔다. Slab 업무 서비스는 `MA0T10/Scenario`(L2), UI 조립 Subsystem은 `MA0T10/UI`(L3)에 둔다.
- UI/Source/Gizmo는 Capture/Scan 필드를 직접 변경하지 않는다. Actor의 `ReadEditableState`, `ValidateEditableState`, `ApplyEditableState` 및 interaction API를 사용한다.
- 외부 입력은 `SubmitExternalFrame`을 사용한다. 측정·출력·진단 수신을 분리하고 진단 결과를 다시 센서/Transport에 주입하지 않는다.
- 자동 측정은 Scheduler, 수동 동기 API는 호환 경로다. 새 native UI의 파일 저장은 FileSave/PeriodicFileSave 비동기 서비스를 사용하며 숨겨진 동기 스캔을 추가하지 않는다.
- acquisition Transform·FrameId·UTC·revision·context는 immutable snapshot으로 끝까지 전달한다. 송신 당시 최신 Transform/Slab 상태로 덮어쓰지 않는다.
- 비동기 결과는 weak ownership과 generation/revision을 검사한다. PIE 종료·삭제·설정 변경 뒤 결과를 적용하지 않는다. worker/socket/readback/file writer 종료도 검증한다.
- 조작 임시 품질과 원설정 복원을 유지한다. Esc·선택 변경·패널 숨김·종료는 Settings의 단일 종료 경로와 시작 시 고정한 Actor를 사용한다. 첫 Esc의 PIE 유지 처리는 프로젝트 전용 입력 중재기의 소유 패널/viewport 범위에 한정한다. 실제 Slate 이벤트 회귀와 직접 마우스 검증을 구분하고 미완료 여부는 IMPROVEMENTS를 참조한다.

### 송신·PCD·세션

- 고성능 경로는 Raw TCP STOMP worker다. 대용량 body 조립·socket I/O·검증을 게임 스레드로 되돌리지 않는다. `wss://` 보안 선택을 평문으로 바꾸지 않는다.
- 일반 JSON/업무 전문은 DTCore를 활용한다. Binary PCD/JPEG를 BodyString에 맞추려고 Base64로 바꾸지 않는다. 진단 Topic을 `MESSAGE_ID` 업무 분배에 임의 등록하지 않는다.
- 기존 v1 JSON, binary JPEG/telemetry/PCD 계약과 Blueprint API를 유지한다. 계약 변경 시 버전·adapter·fixture·문서를 함께 제공한다.
- Binary PCD XYZ는 센서 로컬 meter, X 전방/Y 좌측/Z 위쪽이다. 33-byte little-endian 명시적 레코드이며 구조체 padding을 사용하지 않는다. 월드 복원은 snapshot의 `sensor_to_world_m`를 따른다.
- 센서 FrameId, Slab frame_no, 시나리오 UUID, 실행 UUID를 구분한다. `MA0T10_META`와 기존 STOMP 연계 헤더 의미를 보존한다.
- acquisition 생략, 파생 프레임 교체, 송신 오류, Broker receipt, 소비자 수신/업무 ACK를 따로 집계한다. receipt는 소비자 처리 완료가 아니다.
- ConnectedNoLoss는 정상 연결 중 FIFO 정책이다. 무한 큐나 조용한 교체로 부하를 숨기지 않는다. Raw worker가 접수부터 receipt/종료까지 프레임·바이트 reservation을 소유하고 재시도에서 중복 집계하지 않는다. 기본 전역 128MiB/스트림 64MiB 상한과 Camera 8/기타 20프레임 한도를 유지한다.
- 비밀번호·토큰·원본 대용량 body를 로그/SaveGame에 남기지 않는다. 미지원 TLS/영구 보존/업무 ACK를 지원 완료로 표시하지 않는다.

### Slab

- 수신은 DTCore → `UFactoryAgentScenarioTC` → DataSync → Actor 또는 `SubmitScenarioJson` 한 경로다. archive 등록을 선행 중복 호출하지 않는다.
- 현재 입력 Topic은 `topic.scenario`, MESSAGE_ID는 `IFactory-agent`다. 다른 업무 구독을 제거하거나 공유 설정을 자동 덮어쓰지 않는다.
- `USlabMotionComponent`가 World time/보간을 소유한다. 자세 적용 뒤 원본 행 경계에서 context를 알리며 행마다 센서 스캔을 호출하지 않는다.
- 신규 벌크는 기본 PCD만, replay는 기본 관찰 전용이다. 명시적 선택은 보존한다. 준비 실패 fallback은 센서/독립 스트림을 건드리지 않는 unbound observation이다.
- 중복/오류/busy/drain을 fallback으로 우회하지 않는다. 실행 중 새 벌크는 보관만 하고 자동 큐에 넣지 않는다. 중간부터 자동 송신하지 않는다.
- 원본 JSON·최대 10개 메모리 보관·삭제 보호를 유지한다. 재실행은 새 RunUUID를 사용한다. 영상 복원·영구 replay와 혼동하지 않는다.

### UI·자산

- Host가 명시적으로 등록한 6개 패널만 스타일·폰트·배치·초기화 대상이다. 공통 부모 상속만으로 등록하지 않는다.
- 동료 Widget·중첩 외부 UserWidget·Main·엔진 Slate 스타일/DPI를 일괄 변경하지 않는다. Designer·optional binding·기존 enum 순서를 보존한다.
- 숨김은 파괴나 송신/저장/재생 중지가 아니다. 설정 숨김은 조작 모드만 종료한다. Main Canvas/Viewport 재호스팅은 같은 인스턴스를 유지한다.
- Workspace v1(창), SensorUI v6(센서 표시/출력), Appearance v1(이전 폰트)의 책임을 구분한다. 새 기능을 위해 기존 저장값을 삭제하지 않는다.
- resize는 DPI·경계·최소 크기·접기 전 크기 복원을 검증한다. 진단 문자열은 최대 5Hz, 숨긴 상세 문자열 조립은 줄인다.
- 포인트 전용은 선택 플레이어 뷰만 숨긴다. 전역 visibility로 Camera/GPU LiDAR를 오염시키지 않는다.
- Slab 진단 선·문자·배경판은 충돌/SceneCapture/GPU 분류/그림자·반사 측정에서 제외한다.
- `.umap`/`.uasset`은 Unreal API로 작업한다. 생성 스크립트는 필요할 때만 대상/관리 범위를 확인해 실행한다. 일반 빌드·검증을 위해 운영맵을 매번 재생성하지 않는다.

## 4. 검증과 완료 보고

- Runtime 모듈의 에디터 전용 자동화는 `WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR`로 보호한다. EditorContext 플래그만으로 게임 빌드에서 에디터 헤더가 제외되지는 않는다.
- 패키징은 직접 선택한 맵뿐 아니라 DataTable의 간접 맵 참조도 쿠킹한다. 승인된 참조 복구는 대상 패키지만 백업/저장하고 사용자 변경 맵을 재생성하지 않는다. `RepairTestMapMonitorReference`는 기본 dry-run, `-Apply`에서 TestMap만 저장한다.

1. 문서 변경은 링크·경로·코드/증거 일치와 `git diff --check`를 검사한다. 문서만 바꿨다는 이유로 전체 빌드를 반복하지 않는다.
2. C++ 변경은 Editor/Live Coding 정상 종료 후 UE 5.3 Development를 빌드한다. 관련 자동화를 먼저, 전체 회귀는 통합 시 수행한다.
3. WBP·자산 검사는 가능한 읽기/메모리 컴파일로 한다. 테스트를 맞추려고 보호된 맵을 저장하지 않는다.
4. RHI/GPU/성능은 실제 RHI로 검사한다. NullRHI skip, Success 안의 skip, 다른 SHA의 옛 결과를 실제 통과로 세지 않는다.
5. UI는 Computer Use로 직접 조작하고 실제 client viewport/DPI·폰트 배율을 기록한다. 자동 캡처와 마우스 검증을 구분한다.
6. 동일 SHA/의존성/맵/프로필/센서 수/backend/출력/UI/warmup을 기록한다. engine frame과 callback wall pacing을 구분하고 0Hz 센서를 평균에서 빼지 않는다.
7. acquisition/encode/submit/receipt/내부 검증/외부 수신 수·Hz·gap·invalid·overflow·메모리/큐를 대조한다. 운영맵 fixture와 신규 회귀를 구분한다.
8. 종료·취소·선택 변경·반복 실행·재호스팅·미등록 Widget 불변을 검사한다. 테스트한 개인 UI 저장값은 백업 후 복원한다.
9. 수행·실패·skip·미수행, 커밋, 보호 경로, 다음 필요한 작업을 보고한다. 증거는 `Saved/Reports`, 재현 조건/요약은 관리 문서에 남긴다.

## 5. 플러그인화 후속 규칙

**이번 버전에서는 이 절의 작업(센서 플러그인 생성, Bootstrap Actor, 자산 mount 이동)을 시작하지 않는다.** A단계와 남은 R4가 끝난 뒤 별도 요청으로 착수한다. 아래는 그때 적용할 규칙이다.

- DTCore 필수 의존을 유지한다. Slab 업무·Topic·차트는 host에 남기고 중립적 extension API로 연결한다.
- 플러그인이 host `ma0t10_dt`를 역참조하지 않게 먼저 경계를 정리한다. Runtime 편집 상태를 UI에 둔 채 순환 의존을 만들지 않는다.
- 자산·맵·저장 슬롯·프로필 기본값을 설정화하고 기존 enum/API/PCD를 adapter로 보존한다.
- 한 Actor 진입점은 GameMode/GameInstance/Main/Broker/전역 WebSocket 정책을 몰래 교체하지 않는다. 자신이 생성·시작한 자원만 정리한다.
- 단계·완료 gate는 ROADMAP을 따른다. DTCore 업데이트는 별도 승인된 단계이며 미래 기능을 현재 구현처럼 설명하지 않는다.

## 6. 에이전트 협업 (계획·검토 Claude / 구현 Codex)

현재 이 저장소는 **운영 방식 v2(사용자 승인 2026-10-22)**를 따른다. Claude는 계획(예비안 포함)·머지 전 독립 검수·승인 범위의 머지, Codex는 실행·보고, 사용자는 결정·현장 검증을 맡는다. 기존 보호·승인 규칙을 우선하며 이 절은 기능 구현이나 push/PR/merge 권한을 새로 부여하지 않는다.

**역할**

| 업무 | 담당·책임 |
|---|---|
| 계획 | Claude: 한 항목의 목표·범위·기준 SHA·게이트·예비안을 지시서에 명시 |
| 실행 | Codex: 승인된 항목의 구현·빌드·테스트·결과 작성 |
| 병렬 | Claude 조율 / Codex 실행: 명시적으로 승인·지정한 worktree에서 측정 없는 일만 |
| 보고 | Codex: 20분마다 진행, 변화 없으면 한 줄; 결정·이미지·완료·실패는 즉시 |
| 검수 | Claude: 머지 전 diff·게이트·증거 독립 재확인 |
| 머지 | Claude: 독립 검수 통과 후 별도 승인 범위 안에서 수행 |
| 결정 | 사용자: 정책·보호 경로·범위 변경 등 최종 결정 |
| 현장 검증 | 사용자: 현장/실장비 수용, 로컬 자동화 결과와 구분 |

- Claude: 관리 문서 갱신, 단계별 작업 요청 작성, Codex 결과의 diff·gate·빌드/테스트 재확인, 수정 요청. Claude는 기본적으로 production 코드를 직접 고치지 않는다. 사용자 요청(2026-10-13)에 따라 Unreal을 쓰지 않는 검증 도구·문서(Scripts, docs)는 별도 작업 복사본에서 Claude가 직접 맡을 수 있고, 같은 gate(quick 등)를 통과시킨 뒤 Codex 작업 브랜치에 합친다.
- Codex: 요청받은 **한 항목(ARCH-0N 등)**만 구현한다. 범위 밖 문제를 발견하면 고치지 말고 보고한다. 관리 문서는 해당 항목의 진행률·결과·README §10.2 표 갱신 범위에서만 수정한다.

**요청 단위와 진행**

1. 기본 작업 폴더는 `C:\Unreal Projects\ma0t10_dt`이며 §2 시작 점검(branch/HEAD/remote/status)을 한다. 명시적으로 승인된 측정 없는 병렬 지시서는 지정 worktree에서만 수행한다(아래 병렬 규칙). 승인 없는 다른 폴더·worktree 작업은 금지한다.
2. 한 요청 = 한 항목. 의미 단위로 여러 커밋을 만들 수 있다. 명시 파일만 stage하고 커밋 메시지에 항목 ID를 넣는다(예: `refactor(ARCH-02): move slab services to Scenario`).
3. 지시서의 예비안은 범위·계약·보호 규칙 안에서 재질문 없이 적용하고 선택·근거·검증 한계를 결과에 기록한다. 예비안으로 해결할 수 없는 진짜 차단(계약·보호 경로 변경 필요 등)만 해당 지점에서 멈추고 근거와 선택지를 즉시 보고한다. 추정으로 권한이나 계약을 바꾸지 않는다.
4. push·PR·merge·worktree 생성·submodule update는 하지 않는다.
5. **Computer Use(마우스·키보드) 기능 점검은 Codex가 한다(2026-10-12 사용자 요청).** UI·조작에 영향을 주는 항목은 자동화와 별도로 PIE에서 주요 동작을 직접 눌러 확인하고, 단계별 스크린샷·체크리스트(누른 것 / 기대 / 실제)를 `Saved/Reports/<그룹>/<ID>/manual/`에 남긴다. 화면 충돌을 막기 위해 **Codex 작업 중에는 Claude와 사용자가 화면을 조작하지 않는다.** Claude는 요청 전송 때만 Codex 앱을 잠깐 조작하고, 완료 감지는 화면이 아닌 로그로 한다. Codex는 점검을 끝내면 자신이 띄운 Editor/PIE를 정상 종료하고, 사용자 창 배치·개인 UI 저장값은 백업 후 복원한다. 구조 전용 항목(ARCH)은 기존대로 명령줄 검증으로 충분하다.

**지시서·보고·머지 게이트 (v2)**

- [Tools/agent](Tools/agent/README.md)의 작업 지시서·공통 규칙·병렬 추가 제한을 사용한다. 예비안은 조건·대안·검증 방법을 표로 적는다. 템플릿과 기존 규칙이 충돌하면 기존 규칙을 우선하고 충돌 지점을 보고한다.
- 진행은 20분마다 보고하며 변화가 없으면 한 줄로 충분하다. 결정 필요·이미지 산출·완료·실패는 즉시 보고한다.
- 완료 시 **`Saved/Reports/<ID>/result.json` 작성 의무**: [schema](Tools/agent/schema/result.schema.json)를 준수해 커밋·gate·측정 조건/수치/기준·미수행·한계·결정 필요·증거 경로·개인 설정 복원·비밀 노출 여부를 기록한다. `secrets_exposed`는 bool, 설명은 `secrets_note`에 쓰며 비밀 원문을 옮기지 않는다. Saved 결과는 커밋하지 않는다.
- `Scripts/validate_agent_result.ps1 -ResultJson <path>` 후 `Scripts/merge_check.ps1 -ResultJson <path> -BaseCommit <sha> [-Head <ref>]`를 사용한다. merge_check는 Editor/빌드 없이 결과·manifest·커밋 diff만 판정하고 result 옆 `merge-check.json`을 남긴다. exit 0=머지 가능 후보 / 1=사용자 확인 필요 / 2=차단이다. 자동 머지 승인이 아니며 Claude의 독립 검수와 기존 권한이 최종 기준이다.

**완료 보고 형식 (Codex → Claude)**

```text
항목: ARCH-0N
커밋: <sha> <subject> (순서대로)
변경 요약: 이동/수정 파일 수, 핵심 설계 결정
검증: 명령 / 결과(Success·실패·skip 수와 이름) / 보고서 경로
결과: Saved/Reports/<ID>/result.json / merge-check.json 판정
layer gate: 이전 allowlist 수 → 현재 수
보호 경로: DTCore gitlink·worktree, Config/Game.ini, Samples/PixelStreaming, 운영맵, .uasset 변경 여부
미수행·제한: (없으면 "없음")
질문·결정 필요: (없으면 "없음")
```

**작업 환경 운영 (2026-10-13, v2 병렬 규칙 2026-10-22 추가)**

- **측정 없는 병렬 worktree:** 사용자가 승인·지정한 경로/branch에서만 도구·문서 등 측정 없는 일을 수행한다. 메인 폴더는 읽기 전용이며 다른 작업의 Editor·화면·GPU를 건드리지 않는다. Unreal Editor·UBT/Build.bat·성능 측정·GPU 작업 금지. quick/layer도 Editor/빌드 없는 스크립트 경로인지 확인한 뒤 실행한다. DTCore 미초기화는 한계로 기록하고 자동 update하지 않는다.
- 증거 원본은 해당 worktree `Saved/Reports/<ID>/`에 둔다. 메인 쓰기의 유일한 예외는 지시서가 명시한 사용자 확인용 문서 사본을 메인 `Saved/Reports/<ID>/` 한 경로 아래에 복사하는 것이다. 허용 파일만 복사하고 다른 메인 소스·자산·설정·인덱스를 수정하지 않는다. 이 규칙은 worktree 생성·push·PR·merge의 자동 승인이 아니다.


- 한 Codex 대화가 너무 길어지면(기록 파일 수백 MB 이상) 새 대화로 넘긴다. 새 대화 첫 요청은 AGENTS.md·README §10·IMPROVEMENTS 요약표를 읽게 하고, 이전 대화 ID와 마지막 커밋을 적는다.
- 사용자가 한글을 입력하면 Windows `TextInputHost`가 전경을 잡아 Computer Use가 막힐 수 있다. 마우스 점검은 사용자가 PC를 쓰지 않는 시간에 몰아서 하고, 막히면 원인(전경 프로세스)을 기록한 뒤 2회 재시도 후 해당 행만 보류한다.
- 모니터가 꺼져 화면이 없거나 앱 조작이 불가능하면 Claude는 Codex CLI(`codex exec`, 새 대화)로 화면 없는 작업만 요청하고, 마우스 점검은 보류로 남긴다.
- 모델 용량 오류(`server_overloaded`)는 코드 문제가 아니다. 자동 재시도를 기다리고, 중단되면 같은 범위를 다시 요청한다. 부분 커밋·미커밋 상태를 먼저 검토하게 한다.
- **병행 작업용 복사본 (2026-10-13 사용자 승인):** 개발은 `C:\Unreal Projects\ma0t10_dt`, 마우스 점검은 검수 전용 복사본 `C:\Unreal Projects\ma0t10_dt_qa`(일반 git clone, worktree 아님)에서 동시에 할 수 있다. 검수 복사본에서는 소스·자산 수정·커밋·push를 하지 않고(push URL 비활성), 결과는 그 폴더의 `Saved/Reports` 아래에 남긴다. 새 커밋을 검수하려면 검수 복사본에서 `git pull` 후 다시 빌드한다. 두 Editor가 동시에 도는 동안 성능 측정은 하지 않는다.
- 성능 gate는 같은 방식의 변경 전/후 **ABAB 교차 측정 중앙값**으로 판정하고, 마우스 점검·다른 Editor 세션 중에는 측정하지 않는다. 상대 비교와 별도로 절대 수치(표시형/오프스크린 구분)를 함께 기록한다. 표준 실행은 `Scripts/verify.ps1 -Level perf -BaseCommit <기준> -PerfRayTracing ProjectDefault`(QA-02, RT on 기준)이다.

**검토와 재요청**

- Claude는 보고만 믿지 않고 `git diff`, result.json, merge_check, layer gate, 해당 범위의 빌드·관련 테스트 증거를 머지 전에 독립 재확인한다. 측정 없는 병렬 작업에서는 Editor·빌드를 새로 실행하지 않는다.
- 같은 문제의 수정 재요청은 최대 3회다. 3회 후에도 해결되지 않으면 Claude가 사용자에게 상황과 선택지를 보고한다.
- 검토 통과 후에만 다음 항목을 요청한다.
