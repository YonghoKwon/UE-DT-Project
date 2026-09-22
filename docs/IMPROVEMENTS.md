# 보완 필요 사항 — 구현 백로그

기준: **f7bdd3b / PR #26 병합 0db2f63, 2026-09-22**. 이번 문서 작업에서는 코드를 수정하거나 새 성능 시험을 실행하지 않았다.

[작업 지침](../AGENTS.md) · [현재 기능/사용법](../README.md) · [단계별 로드맵](ROADMAP.md)

## 읽는 방법

- **정적 위험:** 실제 소스의 연결/조건을 확인했다. 이 문서만으로 런타임 재현 완료를 뜻하지 않는다.
- **검증 공백:** 구현이 있으나 해당 조건의 근거가 부족하다. 먼저 재현/계측하고 실패가 확인되면 고친다.
- **신규 기능:** 현재 없는 목표 기능이다. 기존 결함으로 취급하지 않는다.
- **P0:** 다중 센서 확대 전에 데이터 정확성·상태 복원·자원 한계를 확인/수정. **P1:** 다음 안정화 단계. **P2:** 제품화·선택 기능.
- 상태는 모두 **미착수**다. 선택한 항목의 조사→재현→수정→관련 테스트→문서/증거→커밋을 마친 뒤에만 완료로 바꾼다.
- 미래 요구를 현재 기능이라고 설명하지 않는다. 항목 ID는 커밋/PR/시험 보고서에서 유지한다.

## 우선순위 요약

| ID | 분류 | 우선 | 핵심 | 로드맵 |
|---|---|---:|---|---|
| RT-02 | 조작 / 정적 위험 | P0 | Esc의 Gizmo·Widget·Actor 종료 상태 통일 | R1 |
| RT-01 | 성능·전송 / 정적 위험 | P0 | Raw receipt 보관량의 authoritative 상한 | R1 |
| RT-04 | 데이터 / 검증 공백 | P0 | Camera 픽셀과 FrameId/context 동일성 | R1 |
| RT-03 | 진단 / 정적 위험 | P1 | 0Hz 센서가 최소율·공정성에서 빠지는 문제 | R1 |
| QA-01 | 품질 / 검증 공백 | P0 | 고정 의존성·깨끗한 checkout·fixture 분리 | R0 |
| RT-05 | 성능 / 검증 공백 | P1 | 최신 SHA 다중 센서 지원 매트릭스 | R2 |
| RT-06 | 사용자 / 검증 공백 | P1 | 측정 중 배치 입력 지연·원설정 복원 | R2 |
| RT-08 | 성능 / 신규 기능 | P1 | 작업량 기반 capacity/admission | R2 |
| UX-01 | 사용자 / 신규 기능 | P1 | 생성·복제·삭제·배치 preset·Undo/Redo | R2 |
| DATA-01 | 사용자·신뢰성 / 신규 기능 | P1 | 관찰 실행과 데이터 필수 실행 정책 구분 | R3 |
| RT-07 | 전송 / 신규 기능 | P2 | 최종 소비자 ACK와 실행 manifest | R3 |
| LIFE-01 | 안정성 / 검증 공백 | P1 | 종료·취소·재연결 fault matrix | R1/R3 |
| UI-01 | UI / 검증 공백 | P1 | 실제 해상도·DPI·동료 Widget 회귀 | R2/R5 |
| UI-02 | UI / 신규 기능 | P2 | 3D 라벨 겹침·거리/가림·접근성 | R2 |
| IO-01 | 파일 / 알려진 제한 | P1 | LAZ 외부 프로세스 timeout/cancel | R3 |
| PL-01 | 구조 / 신규 기능 | P1 | Slab/host 의존성 역전 | R4 |
| PL-02 | 이식 / 신규 기능 | P1 | 설정·자산 경로·저장 namespace 중앙화 | R4 |
| PL-03 | 확장 / 신규 기능 | P1 | ProfileId 기반 장비 registry | R4/R6 |
| PL-04 | 설치 / 신규 기능 | P1 | 단일 Bootstrap과 clean consumer gate | R5 |
| SEC-01 | 운영 / 검증 공백·신규 기능 | P2 | 보안 transport·Broker 운영 프로필 | R3/R6 |
| HW-01 | 충실도 / 검증 공백 | P2 | 실장비 golden dataset·SDK adapter | R6 |

## 1. 성능·측정·전송

### RT-01 — Raw TCP receipt 대기 상한 (정적 위험, P0)

**근거:** [StreamPublisher](../Source/ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.cpp)의 `RefreshQueueTelemetry`는 호환 경로 `WaitingReceipts`를 집계하고 `PumpPreparedMessages`가 이 값으로 cap을 검사한다. Raw 제출은 이 map에 넣지 않는다. [고성능 worker](../Source/ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.cpp)의 `SendFrame`은 body를 `PendingReceipts`에 유지하지만 명시적인 outstanding frame/byte cap이 보이지 않는다.

- 위험: socket이 빨리 받지만 receipt가 늦을 때 실제 보관량과 UI/admission 기준이 어긋날 수 있다. 무한 증가가 실측됐다는 뜻은 아니다; timeout/retry와 함께 계측해야 한다.
- 수정: worker가 센서별/전역 미확인 프레임 수·바이트의 기준값을 소유하고 producer admission까지 backpressure를 전달한다. 큐·receipt·retry 소유권을 중복 세지 않는다.
- 완료: receipt만 지연/누락하는 fake Broker 시험에서 모든 단계의 상한 유지, silent replacement 0, 명시적 과부하/timeout, 해제 뒤 메모리 복귀. 정상 PCD/JPEG FIFO·checksum 계약 유지.

### RT-02 — Esc 조작 종료 연결 (정적 위험, P0)

**근거:** [Gizmo](../Source/ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.cpp)의 Esc 처리는 Gizmo `SetManipulationEnabled(false)`만 호출한다. 실제 Actor `EndInteractiveManipulation`은 [Settings](../Source/ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.cpp)의 `SetSensorManipulationEnabled`에 있으며 transform commit handler는 Transform만 갱신한다. Settings NativeTick은 Gizmo의 false를 Widget bool에 복사하지만 Actor 종료는 호출하지 않는다.

- 위험: Gizmo/Widget은 종료로 보이지만 Actor가 경량 preview·파생 출력 억제 상태에 남는 경로. false가 복사된 뒤 Widget의 종료 setter도 early-return할 수 있다. 문서 작업에서는 실제 PIE 재현하지 않았다.
- 수정: Esc 종료 요청을 소유 controller/Settings의 단일 종료 경로로 보낸다. 정상 drag commit과 조작 모드 종료를 구분하고 중복 종료는 idempotent하게 만든다.
- 완료: Camera/LiDAR에서 시작→이동→Esc, 버튼 종료, 선택 변경, 패널 숨김, Actor 삭제, EndPlay를 각각 확인. 세 상태 모두 종료, 이전 규격/주기/실행 상태 복원, 최종 coherent frame 갱신, 불필요한 sync 측정 0.

### RT-03 — 0Hz 최소율/공정성 (정적 위험, P1)

**근거:** [Scheduler](../Source/ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.cpp)의 sensor min/max 집계는 `AcquisitionHz > SMALL_NUMBER`인 대상만 포함한다. 모두 0Hz인 상태나 한 센서의 완전 정체를 평균이 숨길 수 있다. 외부 성능 판정 스크립트는 0Hz를 실패로 보는 부분이 있어 UI와 차이가 있다.

- 수정: configured/running/warmup/paused/starved/failed를 분리하고 첫 완료 후 경과시간을 포함한다. 시작 유예가 끝난 활성 센서는 0Hz도 min/fairness에 포함한다.
- 완료: 두 센서 중 하나의 완료를 중단하면 유예 후 min=0·starved ID가 표시되고 정상 판정을 막는다. pause/의도된 interaction 제한은 이유를 구분한다.

### RT-04 — Camera FrameId와 실제 픽셀 (검증 공백, P0)

**근거:** [Camera Capture](../Source/ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.cpp)는 단일 RenderTarget에 deferred capture하며, readback slot이 없으면 metadata 요청을 보관하고 나중에 현재 target을 읽는 경로가 있다. 파일 전용 readback의 자기 capture hook 검증은 이미 있으나 고성능 스트림의 포화 조건과 동일 시험은 아니다.

- 먼저 매 프레임 색/번호 marker와 GPU/readback/encode 지연을 주입한다. 아직 픽셀 불일치가 입증된 것은 아니다.
- 재현되면 capture+copy를 같은 frame에 고정하거나 풀링된 frame buffer를 유지한다. 보존할 수 없는 경우 명시적 정책으로 실패/생략 처리하고 옛 ID에 새 픽셀을 붙이지 않는다.
- 완료: JPEG marker, SensorFrameId, SlabFrameNo, acquisition UTC/pose가 일치. worker 완료 역순·설정 변경·취소에도 순서/소유권 보존.

### RT-05 — 다중 센서 실측 지원 범위 (검증 공백, P1)

- 최근 근거는 ML-X Native **한 대 PCD-only, 753×403, 30초×2**다. 1,199건 송수신 일치, 후반 1% low 45.019 FPS. 옛 2+2/4+4 저출력률 시험을 현재 정격 지원 근거로 쓰지 않는다.
- 1+1 → 2+2 → 4+4를 동일 SHA/고정 fixture에서 검증한다. 8+8은 탐색용 best effort이며 자동 지원으로 분류하지 않는다.
- 각 단계에서 실제 client 해상도, profile/backend/echo, preview/출력 on/off, FPS·1% low·p95/p99, 센서별 acquisition/encode/submit/receipt/내외부 receive, queue/bytes/memory를 기록한다.
- 완료: 60초 회귀를 통과한 구성만 10분 안정성, release 후보는 60분 soak로 확장한다. 요청값을 낮춰놓고 원래 FullSpec 통과로 발표하지 않는다. 목표 수치는 ROADMAP의 제안 gate를 승인·고정한 뒤 적용한다.

### RT-06 — 조작 중 입력 지연 (검증 공백, P1)

- 기본 Camera 640×360/5Hz, LiDAR 120×24/4Hz의 경량 조작 API는 이미 있다. 이는 interaction request 기본값이지 API의 고정 최대치가 아니다. 새로 만드는 작업이 아니라 다중 실행 경계 검증이다.
- 입력 timestamp→Transform 적용→화면 표시를 측정한다. 평균 FPS만으로 조작감을 평가하지 않는다. 기존 world/local, Shift/Ctrl, 최종 품질 복원 유지.
- 완료: 2+2/4+4에서 30초 연속 drag/rotate, 키보드, 선택 전환, Esc/hide/end를 실행. 제안 목표는 input-to-visible p95 ≤50ms, 종료 후 coherent 최종 프레임 ≤1.5초. 불가능한 구성은 지원 밖/제한 원인을 공개한다.

### RT-08 — 작업량 기반 admission (신규 기능, P1)

- 현재 FPS tier는 센서 **개수** 중심이다. 같은 4대라도 해상도·rays·echo·GPU/CPU·JPEG·PCD·semantic proxy 비용은 다르다.
- pixels/s, rays/s, 측정/파생 처리 EWMA, GPU/readback/encode 슬롯, queued bytes·네트워크 예산으로 사전 capacity 표시와 공정 배분을 보완한다.
- Requested/Admitted/Acquired/Published Hz를 분리한다. preview 품질과 물리 측정 규격도 분리하며 사용자 모르게 resolution/주기를 낮추지 않는다.
- 완료: 동일 센서 수의 경량/고부하 혼합에서 starvation 0, bounded 자원, 경고와 실측 결과 일치. 추가 센서가 기존 센서의 완료율을 독점하지 않음.

## 2. 사용자 workflow·데이터 신뢰성

### UX-01 — 센서 설치 도구 (신규 기능, P1)

- 현재는 Actor를 배치하고 선택/조작하는 기능이다. Runtime 생성·복제·삭제·배치 preset을 완성된 한 workflow로 제공하지 않는다.
- Editor 설치와 PIE 임시 설치를 명확히 구분한다. 고유 ID 생성/중복 검사, 이름/종류별 목록, transform 입력·snap·복제, 배치 preset 저장/불러오기, Editor Undo/Redo를 제공한다.
- Actor API와 setup descriptor를 사용하고 측정 Component 필드 직접 변경은 금지한다. Editor 배치의 영구 저장은 명시적 동작으로만 수행한다.
- 완료: 10개 생성·복제·이름 변경·삭제·Undo/Redo·재로드, 소유하지 않은 Actor 불변, duplicate ID 0, 송신/비동기 작업 중 삭제 안전. 센서 없는 레벨도 빈 상태 안내로 시작.

### DATA-01 — 관찰 가능한 실행과 데이터 필수 실행 (신규 기능, P1)

- 현재 live 설정 부족은 관찰 fallback으로 이동한다. 좋은 개발 편의지만 학습 데이터 수집 작업에서는 “움직였으니 데이터도 수집됐다”는 오해를 막아야 한다.
- 기본 fallback은 유지하고 opt-in **데이터 필수 실행**을 제안한다. preflight→연결 준비→실행→drain→완전/부분/실패 결과를 분리하고 요구 SensorId/출력의 준비 상태를 확인한다.
- 실행 중 Broker 장애는 움직임 중지 여부를 host 정책으로 명시한다. 자동 fallback/중간 재송신을 몰래 추가하지 않는다.
- 완료: 설정 누락·실제 연결 거절·용량 초과·중도 단절에서 실행 정책과 결과가 일치. 원본 시나리오/실행 UUID·선택 센서·profile revision·승인/완료 수를 작은 manifest에 기록한다.

### RT-07 — 소비자 업무 ACK (신규 기능, P2)

- 현재 drain의 Raw 대기는 Broker receipt 중심이다. 자체 parser의 성공도 최종 AI/업무 소비자의 저장·처리 완료는 아니다.
- 필요 시 `(RunUUID, Segment, SensorId, SensorFrameId, checksum)` 기반 application ACK/누락 대조/멱등 재전송을 별도 버전 계약으로 추가한다. 기존 PCD body를 묵시적으로 변경하지 않는다.
- 완료: 소비자가 중단되어도 BrokerAccepted와 ConsumerProcessed를 혼동하지 않음. 중복 메시지 재처리 방지, 종료 결과에 미확인 목록, timeout/재시도/보존 정책이 명시됨.

### LIFE-01 — 중도 종료와 재연결 (검증 공백, P1)

- weak pointer/revision·세션 drain·file writer 취소는 이미 있으므로 전체 재작성하지 않는다.
- acquisition/readback/encode/직렬화/socket/receipt/파일 저장 각 단계에서 Actor 삭제, Stop, 품질 변경, PIE 종료, world 이동, 동일 시나리오 재실행을 주입한다.
- 완료: 늦은 결과 적용·double complete·old generation 수신 0, 종료 시간 상한, thread/handle/메모리 복귀, 다른 owner 연결 유지. 각 경로 테스트가 어느 수명 경계를 증명하는지 기록한다.

### IO-01 — LAZ 프로세스 제한 (알려진 제한, P1)

- 외부 compressor 미설정 시 성공을 가장하지 않으며 실제 timeout/cancel 및 장시간 hang 정리는 아직 보완 대상이다.
- 자신이 시작한 자식 프로세스만 추적해 timeout/cancel, 제한된 stdout/stderr, 임시 파일 cleanup, 오류 코드를 제공한다. 무관한 프로세스를 이름으로 종료하지 않는다.
- 완료: fake compressor 성공/오류/hang/취소 시험, worker slot 복귀, 부분 파일을 완료로 표시하지 않음. CSV/PCD/JPEG 작업이 함께 굶지 않음.

## 3. UI·가독성

### UI-01 — 실제 해상도/DPI/소유권 (검증 공백, P1)

- 최근 수동 크기는 988×521/1174×683이다. 요청했던 1280×720/1920×1080과 다르다. 실제 동료 프로젝트는 미제공이다.
- 두 목표 client 크기, 100/125/150% OS DPI 및 네 폰트 배율의 대표 조합을 검사한다. 창 경계/메뉴 overflow/입력 clipping/접기 후 hit-test/Tab focus/숨김 중 작업/재호스팅을 확인한다.
- 완료: 실제 크기 증거, 기존 저장값 roundtrip, 미등록/중첩 Widget의 font·geometry·입력·파일 hash 불변. 확보 못한 조합은 명시적으로 미완료.

### UI-02 — 3D 라벨 배치 (신규 기능, P2)

- 현재 큰 문자 배경판이나 도구 창이 다른 라벨을 가릴 수 있다. 선/글자 크기, 대비, 한글 표시 자체는 구현·수동 확인했다.
- 시야 밖/뒤쪽 점을 숨기거나 안전 위치로 안내하고, 라벨별 priority·최대 겹침·leader line/거리 fading을 추가할 수 있다. 숫자/단위/좌우 의미는 유지한다.
- 완료: 원근/직교, 근접/원거리, roll, 태양광/고온 Slab 조건의 읽기 시험. 진단이 Camera/LiDAR를 오염시키지 않고 5Hz 표시 budget 유지. 전역 occlusion/visibility를 바꾸지 않는다.

## 4. 플러그인·프로필·운영

### PL-01 — host 의존성 역전 (신규 기능, P1)

- 현재 센서 frame/request가 `FVirtualSlabFrameContext`를 포함하며 Camera/Scan이 SlabContext Subsystem을 직접 호출한다. 데이터 UI는 SlabActor를, Workspace는 Slab panel/Host를 직접 안다.
- 중립적 frame context provider/completion observer, output-session policy, host UI extension으로 역전한다. Slab codec/업무 원본/차트/재생은 host adapter에 남긴다.
- 완료: 센서 Runtime이 host 모듈을 include/dependency하지 않는 static/build gate. Slab 없는 fixture + Slab adapter fixture에서 기존 wire bytes/metadata 의미 회귀 통과. provider UObject를 worker에서 직접 읽지 않음.

### PL-02 — 기본값·설정 namespace (신규 기능, P1)

- `UVirtualSensorIntegrationSettings`와 `UVirtualSensorDtCoreBridgeSubsystem`은 현재 소스에 없다. 경로·맵 저장 대상·슬롯·Topic이 여전히 여러 곳에 있다.
- soft references/설정 객체를 도입하되 명시적 Actor/Transport 설정을 우선하고 없을 때만 기본값을 사용한다. 새 값으로 기존 Map/WBP를 자동 재저장하지 않는다.
- Frame/편집 상태 타입은 Runtime public 계약에 두고 UI→Runtime 단방향을 만든다. 저장 namespace는 plugin/rig/user 기준으로 설계하되 기존 v6/Workspace v1은 opt-in migration으로 보존한다.
- Coordinator의 Monitor 타입/직접 bind 호출도 Runtime delegate/interface와 UI adapter로 분리한다. 편집 타입 이동만으로 Runtime→UI 역의존이 해소되지는 않는다.
- 완료: host 이름 일괄 치환 없이 자산/저장/Topic 변경 가능, 없는 자산의 명확한 진단, 다른 rig/Widget 저장값 불변, Editor 저장 allowlist 유지.

### PL-03 — 장비 registry (신규 기능, P1)

- 현재 장비 enum·resolver는 존재한다. 프로필을 또 다른 switch에 계속 추가하는 방식 대신 안정적인 ProfileId+DataAsset/registry를 제안한다.
- 물리 규격·quality preset·capability·calibration provenance와 acquisition backend/실장비 SDK adapter를 구분한다. 프로필 데이터 추가만으로 SDK가 구현되는 것은 아니다.
- 완료: 테스트 장비 하나를 핵심 switch 수정 없이 등록/선택/검증. 기존 enum→built-in ProfileId 호환, 원본 ML-X/Integration200 구분, 단일 설정 transaction·한 번의 재예약 유지.

### PL-04 — 한 클래스 진입점·배포 (신규 기능, P1)

- Bootstrap façade가 기존 Coordinator/UI Host/선택 SourceHost를 조합한다. 기존 인스턴스를 우선 사용하고 자신이 만든 것만 정리한다. 새 거대 manager로 통합하지 않는다.
- host GameMode/GameInstance/Main를 강제 교체하지 않는다. 기본은 로컬 preview, 외부 송신은 명시적 설정 또는 host 시나리오 정책이다.
- 완료: 빈 UE5.3 프로젝트에 DTCore+센서 plugin만 넣고 Bootstrap 하나로 preview/UI 확인. 다른 GameMode/Main 유무, 지연 PlayerController, 다중 PIE, packaged Development, cook/asset migration 통과.

### SEC-01 — 보안/연결 운영 (검증 공백·신규 기능, P2)

- Raw TCP는 평문이다. wss 호환 경로·HTTP·자체 진단·실장비 입력은 동일 기능이 아니며 연결을 임의 통합하면 안 된다.
- Broker 인증/권한·주소 유형·message limit·receipt timeout·재연결·TLS 성능을 배포별 설정으로 관리한다. 개발 계정/endpoint를 release 기본값으로 복사하지 않는다.
- 현재 조기 Bootstrap의 WebSocket 1ms 설정은 프로세스 전체 영향이 있다. 레벨 BeginPlay로 옮기면 초기화 시점이 늦다. host 정책으로 유지하거나 명시적 opt-in 조기 설정으로 별도 검증한다.
- 완료: 암묵적 전역 설정/다른 Subscription 종료 0, secret 미저장, wss가 평문으로 바뀌지 않음, 오류/인증/연결 상태가 UI에 정확히 표시됨.

### HW-01 — 실장비 충실도 (검증 공백, P2)

- ML-X Native는 공개 사양 기반, ROS2/Livox/RealSense는 stub/확장 지점이다. 실제 장비 동일성을 주장할 자료가 없다.
- SDK/패킷·calibration·raw capture golden dataset을 확보한 뒤 좌표·거리·intensity·echo·timestamp·miss를 대조한다. HW Ray Tracing/GPU physical 모델은 capability별 별도 승인 작업이다.
- 완료: profile마다 구현/미구현·단위·오차·지원 backend·근거 dataset을 공개. synthetic truth와 보정된 센서 관측, Digital Twin semantic을 구분한다.

## 5. QA-01 — 재현 가능한 기준선 (검증 공백, P0)

- 현재 parent DTCore는 `2eec1fe`, 로컬 검증은 `a1b333e`다. 두 revision의 **Source 차이는 없고 문서만 다름**을 확인했다. API 불일치가 입증된 것으로 쓰지 않는다.
- 그럼에도 개인 Game.ini/SaveGame/수정 운영맵에 의존하지 않는 clean checkout build/asset load 기준선이 필요하다.
- 운영맵 Z=150cm fixture를 통과시키기 위해 사용자 맵을 수정하지 않는다. 커밋된 스냅샷 검사 또는 임시 검증 scene으로 fixture와 실사용 편집을 분리한다.
- 보고서에 commit/submodule SHA, Engine/toolchain/GPU/driver, 실제 viewport/DPI, 설정 hash, 실행한 tests/skip, 원본 count를 남긴다. 문서 요약과 원본 JSON을 자동 대조한다.
- 완료: clean setup 명령 한 묶음으로 빌드·WBP·계약·핵심 RHI 재현, 운영 asset hash 불변, 실패·skip이 집계에서 숨겨지지 않음.

## 다음 착수 순서

1. QA-01의 현재 baseline을 고정하고 **RT-02**를 작은 기능 커밋으로 해결한다.
2. **RT-01 + RT-03**, **RT-04**를 각각 재현/시험 단위로 처리한다.
3. 이후 RT-05/06과 UX-01로 실제 다중 배치 workflow를 확장한다.
4. 데이터 정확성 기준이 정해진 뒤 PL-01/02/03 → PL-04 순으로 플러그인화한다.

완료 이력은 각 항목에 검증 SHA·보고서 경로·남은 제한을 붙인다. 이 문서 작성은 해당 기능의 구현 승인이나 실행 완료가 아니다.
