# 작업 지침 — UE-DT-Project

기준: UE 5.3, 안정성 수정 `d2024bf`·`59d1027` / PR #27 병합 `68f8f2f` 기반 (2026-09-22).
이 파일은 작업 규칙이다. 제품 사용법이나 전체 로드맵의 구현 승인을 대신하지 않는다.

## 1. 네 개의 관리 문서

| 문서 | 책임 |
|---|---|
| [AGENTS.md](AGENTS.md) | 권한·보호 범위·구현 경계·검증 및 완료 규칙 |
| [README.md](README.md) | 현재 구현·설치/사용·입출력 계약·최신 검증 요약 |
| [보완 필요 사항](docs/IMPROVEMENTS.md) | 우선순위·근거·재현·개별 완료 기준 |
| [최종 목표 로드맵](docs/ROADMAP.md) | 단계별 의존 관계·플러그인 경계·출시 gate |

- `Agent.md`를 별도 생성하지 않는다. 기존 `docs` 문서는 경로를 보존한 고정 참고자료다. 오래된 UI/성능 설명보다 현재 코드와 README를 우선한다.
- 보완 목록/로드맵에 있다는 이유만으로 전부 구현하지 않는다. 요청한 항목과 직접 관련된 회귀만 처리한다.
- 설명·진단·검토는 읽기 전용, 문서 작성은 문서만, 기능 구현은 해당 기능만 변경한다.
- 범위 안의 조사·명확한 구현·비파괴 테스트를 매번 재승인받지 않는다. 중요한 동작 정책이 미정이거나 보호 대상 변경이 필요할 때 질문한다. 코드에서 알 수 있는 사항은 먼저 조사한다.
- 환경 때문에 한 검증이 막혀도 가능한 검증은 계속한다. 기능 완료는 구현·관련 검증·문서·제한 보고까지이며, 미수행을 통과로 바꾸지 않는다.

## 2. 보호·승인 규칙

- 시작 시 branch/HEAD/remote와 `git status`를 확인한다. 기존 변경은 보존한다.
- 명시적 요청 없이 **DTCore 소스 및 parent gitlink**, `Config/Game.ini`, 운영맵의 사용자 변경, `Samples/PixelStreaming`을 수정하거나 stage하지 않는다.
- DTCore는 별도 개발 중이다. 프로젝트 측 adapter로 해결하고, 공통 플러그인 수정이 필수라면 근거·대안을 먼저 제시한다. 자동 submodule update/reset은 하지 않는다.
- `SensorTestManaged` 없는 Actor/Mesh는 이동·삭제하지 않는다. 명시적으로 배치된 `ASlabActor`의 소유 Mesh 생성은 가능하지만 임의 맵에 Slab·조명·카메라를 자동 배치하는 권한은 아니다.
- `Binaries`, `Intermediate`, `Saved`, `.vs`, 캐시·패키징 결과·실험 PNG/JSON/log는 커밋하지 않는다.
- destructive reset/checkout, 재귀 삭제, 서비스 재시작, Broker 설정 변경을 통상 구현 단계로 추정하지 않는다. 필요하면 구체적으로 확인한다.
- 커밋이 요청 범위에 포함된 작업은 관련 검증 후 의미 단위로 커밋한다. 명시적 파일 목록을 stage하고 `git add -A`를 기본으로 사용하지 않는다. 문서 작업은 문서 변경만 묶는다. 문서 작성 요청만으로 커밋·외부 게시 권한을 새로 부여하지 않는다.
- push·PR·merge는 별도 요청 범위대로만 수행한다. PR 요청은 해당 브랜치 push/PR 생성을 포함하지만 master 직접 merge는 포함하지 않는다.
- 새 지침·로드맵은 위 권한을 확대하지 않는다. 보호 규칙 완화가 필요하면 변경과 영향을 먼저 사용자에게 알린다.

## 3. 구현 경계

### 센서·측정·비동기 수명

- Runtime은 `ma0t10_dt`, Editor는 `ma0t10_dtEditor`, 조기 초기화는 `ma0t10_dtBootstrap` 모듈이다. 센서 기능은 아직 독립 플러그인이 아니다.
- DTCore `AInteractableActor → AVirtualSensorActorBase → Camera/LiDAR Actor` 계층을 유지한다. 공용 센서 Subsystem은 `MA0T10/Core`에 둔다.
- UI/Source/Gizmo는 Capture/Scan 필드를 직접 변경하지 않는다. Actor의 `ReadEditableState`, `ValidateEditableState`, `ApplyEditableState` 및 interaction API를 사용한다.
- 외부 입력은 `SubmitExternalFrame`을 사용한다. 측정·출력·진단 수신을 분리하고 진단 결과를 다시 센서/Transport에 주입하지 않는다.
- 자동 측정은 Scheduler, 수동 동기 API는 호환 경로다. 새 native UI의 파일 저장은 FileSave/PeriodicFileSave 비동기 서비스를 사용하며 숨겨진 동기 스캔을 추가하지 않는다.
- acquisition Transform·FrameId·UTC·revision·context는 immutable snapshot으로 끝까지 전달한다. 송신 당시 최신 Transform/Slab 상태로 덮어쓰지 않는다.
- 비동기 결과는 weak ownership과 generation/revision을 검사한다. PIE 종료·삭제·설정 변경 뒤 결과를 적용하지 않는다. worker/socket/readback/file writer 종료도 검증한다.
- 조작 임시 품질과 원설정 복원을 유지한다. Esc·선택 변경·패널 숨김·종료는 Settings의 단일 종료 경로와 시작 시 고정한 Actor를 사용한다. RT-02 자동 회귀를 유지하고 실제 키보드 검증의 미완료 여부는 IMPROVEMENTS를 참조한다.

### 송신·PCD·세션

- 고성능 경로는 Raw TCP STOMP worker다. 대용량 body 조립·socket I/O·검증을 게임 스레드로 되돌리지 않는다. `wss://` 보안 선택을 평문으로 바꾸지 않는다.
- 일반 JSON/업무 전문은 DTCore를 활용한다. Binary PCD/JPEG를 BodyString에 맞추려고 Base64로 바꾸지 않는다. 진단 Topic을 `MESSAGE_ID` 업무 분배에 임의 등록하지 않는다.
- 기존 v1 JSON, binary JPEG/telemetry/PCD 계약과 Blueprint API를 유지한다. 계약 변경 시 버전·adapter·fixture·문서를 함께 제공한다.
- Binary PCD XYZ는 센서 로컬 meter, X 전방/Y 좌측/Z 위쪽이다. 33-byte little-endian 명시적 레코드이며 구조체 padding을 사용하지 않는다. 월드 복원은 snapshot의 `sensor_to_world_m`를 따른다.
- 센서 FrameId, Slab frame_no, 시나리오 UUID, 실행 UUID를 구분한다. `MA0T10_META`와 기존 STOMP 연계 헤더 의미를 보존한다.
- acquisition 생략, 파생 프레임 교체, 송신 오류, Broker receipt, 소비자 수신/업무 ACK를 따로 집계한다. receipt는 소비자 처리 완료가 아니다.
- ConnectedNoLoss는 정상 연결 중 FIFO 정책이다. 무한 큐나 조용한 교체로 부하를 숨기지 않는다. Raw receipt 대기 상한 연결은 RT-01 보완 대상이다.
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

- DTCore 필수 의존을 유지한다. Slab 업무·Topic·차트는 host에 남기고 중립적 extension API로 연결한다.
- 플러그인이 host `ma0t10_dt`를 역참조하지 않게 먼저 경계를 정리한다. Runtime 편집 상태를 UI에 둔 채 순환 의존을 만들지 않는다.
- 자산·맵·저장 슬롯·프로필 기본값을 설정화하고 기존 enum/API/PCD를 adapter로 보존한다.
- 한 Actor 진입점은 GameMode/GameInstance/Main/Broker/전역 WebSocket 정책을 몰래 교체하지 않는다. 자신이 생성·시작한 자원만 정리한다.
- 단계·완료 gate는 ROADMAP을 따른다. DTCore 업데이트는 별도 승인된 단계이며 미래 기능을 현재 구현처럼 설명하지 않는다.
