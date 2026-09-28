# 최종 목표를 위한 로드맵

기준: **59d1027 / PR #27 병합 68f8f2f 기반, 2026-09-22**.
이 문서는 후속 개발 순서와 승인 기준이다. 플러그인 생성이나 모든 단계의 구현을 지금 승인하는 문서는 아니다.

[AGENTS](../AGENTS.md) · [현재 구현](../README.md) · [개별 보완 백로그](IMPROVEMENTS.md)

## 1. 최종 사용자 경험

2026-09-29 배포 준비 증거: 현재 UE5.3 프로젝트의 Windows Development 패키징을 완료하고 외부 런처의 격리된 ZIP/sidecar 접수·승인을 준비했다. 에디터 테스트 가드와 승인된 TestMap의 누락 위젯 참조만 보완했다. 이는 R5 플러그인화·간편 설치 또는 회사 운영 인수 완료가 아니며, 실제 런처 실행·Shipping/Linux·회사 환경 수용은 각각의 증거로 판단한다. 자세한 범위는 [README의 패키징 점검](../README.md)을 따른다.

| 사용자 목표 | 최종적으로 제공할 동작 | 현재와의 차이 |
|---|---|---|
| 여러 Camera/LiDAR를 자유롭게 배치 | 목록에서 생성·복제·선택, 부드러운 gizmo/키보드 조작, 배치 preset과 명시적 저장 | 현재 Actor 배치·단일 선택 조작 기반; 통합 설치 workflow와 다중 부하 검증 필요 |
| 시나리오에 맞춰 측정·전송 | 준비 상태 확인 후 실행, 독립 센서 주기로 측정, 올바른 소재/프레임 context, 종료 drain과 완전/부분 결과 | 현재 Slab/PCD 동작과 관찰 fallback 존재; 정격 다중 수집·소비자 완료 보장은 추가 단계 |
| 장비별 플러그인 관리 | DTCore + 센서 플러그인, 데이터 기반 ProfileId registry, backend/SDK 확장 경계 | 현재 프로젝트 모듈·enum/resolver·Slab 직접 의존 |
| 다른 프로젝트에 쉽게 설치 | 플러그인 활성화 후 Bootstrap Actor 하나와 setup asset으로 preview/UI 확인 | 현재 여러 Host·Coordinator·자산·TC 설정을 수동 이식 |

“느려짐 없이”는 무제한 센서·어떤 PC에서도 60FPS를 의미하지 않는다. **고정 장비/장면/출력 조합의 지원 매트릭스**를 만들고, 한계 초과를 미리 알리며 요청 규격을 몰래 낮추지 않는 것을 제품 기준으로 삼는다.

DTCore는 필수 의존성으로 사용하지만 **이번 로드맵의 선행 단계에서는 소스/gitlink를 변경하지 않는다.** 공통 플러그인 개발이 완료되면 별도 compatibility gate로 업데이트한다. AI 모델 학습/실장비 SDK 전체 구현은 센서 플랫폼 완성의 자동 포함 범위가 아니다.

## 2. 권장 순서와 단계별 결과물

권장 순서: **R0 기준선 → R1 정확성/한계 → R2 다중 배치/성능 → R3 데이터 수집 신뢰성 → R4 의존성/설정 정리 → R5 플러그인/간편 설치 → R6 장비/운영 확장**.

R4의 인터페이스 설계는 R2/R3와 병행할 수 있으나 실제 클래스·자산 이동은 회귀 기준이 마련된 뒤 수행한다. DTCore 작업 완료를 기다리느라 프로젝트 측 안정화를 멈출 필요는 없다.

### R0 — 재현 가능한 기준선

**현재:** 핵심 문서 4개 체계는 이번 문서 작업 결과다. clean checkout·고정 의존성 재검증은 아직 남아 있다.

- 코드/DTCore SHA, Engine/toolchain/GPU/driver, actual viewport/DPI, profile/backend/output/UI 상태를 manifest로 기록한다.
- 현재 DTCore pin `2eec1fe`와 로컬 `a1b333e`의 Source는 같고 문서만 다르다. API 결함으로 단정하지 말고 깨끗한 checkout에서 확인한다.
- 운영맵 사용자 변경을 검증 fixture에서 분리한다. map regeneration을 quick-start 필수 단계에서 제거하고 임시/커밋된 test scene을 사용한다.
- 최신 단일 ML-X PCD 기준선을 재현하고 각 test의 실제 수행/skip과 원본 보고서를 연결한다.

**완료 gate:** 개인 Game.ini/SaveGame 없이 빌드·WBP·계약·핵심 RHI 재현, 운영맵/보호 경로 hash 불변. 관련 백로그: QA-01.

### R1 — 확장 전에 데이터·수명 정확성 확보

진행: RT-02(`d2024bf`)·RT-03(`59d1027`) 구현·자동 검증 완료. 실제 키보드/화면 검증은 데스크톱 접근 오류로 남아 있다. RT-01/04·LIFE-01과 전체 R1 gate는 미완료다.

1. Esc 종료를 단일 interaction 종료 경로로 통합한다. Drag commit과 mode exit를 구분한다.
2. Raw worker의 receipt 대기 frame/byte 한도와 producer backpressure를 연결한다. 0Hz/starvation을 진단에서 숨기지 않는다.
3. Camera 픽셀 marker로 capture/readback/encode metadata의 동일성을 검증한다. 불일치가 재현될 때만 acquisition-buffer 구조를 고친다.
4. Stop/삭제/설정 변경/PIE 종료/재연결의 generation·cancel·drain 경계를 점검한다.

**완료 gate:** RT-01/02/03/04, LIFE-01의 선택된 회귀가 통과하고 같은 frame에 다른 픽셀이 붙거나 다른 owner 자원을 정리하지 않음. 정상 1+1 출력 성능을 유지한다. 이 단계 전에 새 센서 수를 크게 늘리지 않는다.

### R2 — 다중 센서 배치와 성능 제품화

- 편집용 설치 workflow: 생성/복제/삭제, 고유 SensorId, 검색/선택, transform/snap, 배치 setup asset, Editor Undo/Redo를 추가한다. PIE 임시 배치와 원본 맵 저장을 구분한다.
- 기존 경량 조작 API를 유지하며 조작 중 입력→Transform→화면 지연을 계측한다. 선택 센서의 preview 우선순위가 타 센서의 acquisition을 독점하지 않게 한다.
- 센서 개수만이 아니라 pixels/s·rays/s·echo·CPU/GPU·semantic·encode·네트워크/파일 비용으로 capacity를 계산한다. 현재 프레임 풀/슬롯/worker를 먼저 계측하고 불필요한 재할당·복사를 제거한다.
- 설정할 규격, admission된 규격, 실제 acquisition/output률을 별도로 보여준다. 지원 불가 조합은 사전 경고하고 명시적인 profile/preview 선택으로 해결한다.
- 1+1, 2+2, 4+4 매트릭스를 작성하고 8+8은 탐색값으로만 공개한다. 스트레스 fixture를 실 공장 장면 대표로 자동 간주하지 않는다.
- UI의 실제 해상도/DPI·저장 복원·입력 소유권을 점검하고 3D 라벨 겹침은 측정 파이프라인과 독립적으로 개선한다.

**완료 gate:** RT-05/06/08, UX-01, UI-01. 지원 조합에서 입력 반응·측정 완료율·송수신·자원 상한이 동시에 만족한다. 모든 지원 수치에 실제 조건/보고서를 붙인다.

### R3 — 시나리오 기반 신뢰할 수 있는 수집

- host 실행 정책을 **관찰 가능**과 **데이터 필수**로 분리한다. 현재 기본 관찰 fallback을 유지하고 데이터 필수 모드는 opt-in으로 추가한다.
- preflight는 설정/자산/필요 센서 확인, 연결 준비는 실제 transport 상태 확인, 실행은 acquisition, drain은 승인된 데이터 완료로 구분한다.
- 중립적인 run manifest에 scenario/run ID, target sensors, profile/calibration/backend, 요청/완료/누락/전달 수, 출력 경로·checksum·종료 이유를 기록한다.
- 원본 Slab frame과 센서 frame을 일대일 강제하지 않는다. acquisition 시작 context가 직렬화/전송/저장까지 보존되는 것을 검증한다.
- 소비자 업무 완료가 필요한 배포에만 application ACK·멱등성·누락 대조를 추가한다. Broker receipt로 학습 데이터셋 저장 완료를 선언하지 않는다.
- 네트워크 장애 중 영구 보존은 선택 기능이다. 추가할 경우 bounded 디스크 spool·회수·용량 경고·재전송 규칙을 별도 계약으로 정하며 무제한 backlog를 만들지 않는다.
- LAZ 프로세스 timeout/cancel과 파일 작업의 종료 안전성을 보완한다.

**완료 gate:** DATA-01/RT-07/LIFE-01/IO-01. 오류를 의도적으로 넣어 완전/부분/실패 결과를 판정하고, 선택하지 않은 출력과 비대상 스트림을 침범하지 않는다. PCD/JPEG 기존 소비자 호환 시험 필수.

### R4 — 프로젝트 안에서 분리 준비

**이 단계는 아직 클래스/자산을 플러그인으로 이동하지 않는다.** 경계를 먼저 정리한다.

| 현재 결합 | 프로젝트 내 선행 리팩토링 |
|---|---|
| frame/request의 `FVirtualSlabFrameContext`, Capture/Scan의 SlabContext 호출 | 중립 immutable acquisition context + provider/completion observer. host Slab adapter가 기존 wire metadata 의미를 제공 |
| CaptureExport가 SlabActor 탐색·출력 설정 | host-owned scenario UI extension/policy adapter. 일반 저장/송신 UI와 분리 |
| Workspace의 Slab panel type 검사/Host spawn | 등록형 panel descriptor/factory. 소유권 token과 기존 저장 key adapter 유지 |
| Lidar의 업무용 slabAnalysis/기준 yaw | 일반 분석 extension 또는 host analyzer로 이동; 기존 Payload adapter 유지 |
| Actor가 `UI/VirtualSensorControlTypes.h`에 의존 | 편집 상태·명령 계약을 Runtime public 영역으로 이동, UI→Runtime 단방향 |
| Coordinator의 Monitor 타입·직접 bind/보기 제어·OwningPlayer 조회 | 선택/모드 변경은 Runtime delegate/interface, Widget binding·플레이어 뷰 처리는 UI adapter로 분리 |
| `/Game/MA0T10`, SensorTestMap, MA0T10 저장 슬롯/Topic | settings/soft references와 explicit override 우선순위 도입 |
| device enum/switch | built-in enum adapter + 안정적인 ProfileId registry |

- 현재 이름만 제안됐던 IntegrationSettings/DtCoreBridge를 이미 있는 것으로 가정하지 않는다. 필요한 최소 설정/구독 façade를 구현한다.
- 일반 DTCore JSON 구독은 owner/generation별로 정리하되 공유 연결·다른 Subscription을 끊지 않는다. Binary 경로는 Raw worker로 유지한다.
- context provider는 게임 스레드에서 snapshot을 만들고 worker는 immutable 값만 읽는다. UObject/host 상태를 worker가 역참조하지 않는다.
- serialization byte fixture와 Blueprint compatibility를 경계마다 검증한다. 클래스 이름 변경과 계약 변경을 같은 대규모 작업에 섞지 않는다.

**완료 gate:** PL-01/02/03. 센서 Runtime→Slab/host/UI include·타입·module dependency가 없는지 검사하고, Slab/UI adapter를 연결하면 기존 metadata/동작이 돌아온다. 아직 plugin 설치 완료라고 표시하지 않는다.

### R5 — DTCore 의존 플러그인과 한 클래스 진입점

권장 최종 구조(모두 **신규 제안**):

| 구성 | 포함 | 포함하지 않음 |
|---|---|---|
| `DTVirtualSensorRuntime` | 센서 Actor/Component, scheduler/backend, profile registry, codec, output/transport/record/file, 중립 context/extension 계약 | Slab 클래스·업무 Topic·차트, host module, UI 타입 역참조 |
| `DTVirtualSensorUI` | 센서 Monitor/Settings/Data, gizmo, panel host/workspace core, 소유 UI 상태 | Slab panel factory/업무별 메뉴를 hardcode한 구현 |
| `DTVirtualSensorEditor` | 배치·설정 검증·명시적 맵 반영·자산 제작 도구 | packaged runtime 의존 |
| plugin Content | 센서 WBP/Niagara/semantic material·기본 profile/setup assets | 운영맵·Slab 업무 자산 |
| host `ma0t10_dt` | Slab 시나리오/이동/분석/차트/재생, TC 등록·context/UI adapter | 센서 코어의 사본 |

- `.uplugin`에 DTCore 필수 의존을 명시한다. host Build.cs를 그대로 복사해 PixelStreaming·업무 API·Player/Manager 의존을 끌고 오지 않는다.
- 자산 mount는 `/DTVirtualSensor/...`로 옮기고 Asset Registry·cook 의존성을 검사한다. `M_LidarSemanticId` 같은 간접 로드 자산을 누락하지 않는다.
- 기존 자산을 이어 쓰는 경우에만 정확한 CoreRedirect를 제공한다. enum 순서·Blueprint 함수·SaveGame은 adapter/migration으로 검증하며 파일을 일괄 삭제하지 않는다.
- `ADTVirtualSensorBootstrapActor`(제안)는 기존 구성 요소의 **façade**다. 구현을 한 거대 Actor로 합치지 않는다.

Bootstrap의 확정할 기본 동작:

1. 명시적인 Coordinator/Host가 있으면 재사용, 없으면 소유 인스턴스만 생성한다. 초기 버전은 World당 한 sensor rig/coordinator로 제한하고 중복은 오류로 안내한다.
2. setup DataAsset에서 **기존 센서 발견** 또는 **목록 기반 생성**을 선택한다. 생성 시 ID/설정 검증→output service 연결→등록→UI bind→측정 시작 순서를 제어한다.
3. 초기 기본값은 로컬 preview만이다. Broker credentials/업무 Topic·송신은 explicit opt-in 또는 host 시나리오 정책으로 시작한다.
4. GameMode/GameInstance/Player/Main를 강제 교체하지 않는다. Main Canvas가 있으면 사용하고 없으면 Viewport fallback, 지연 PlayerController는 제한된 재시도로 처리한다.
5. EndPlay는 자신이 만든 Actor·timer·delegate·subscription만 정리한다. UI 숨김과 작업 중단은 별개다. dedicated server에서는 UI를 만들지 않는다.
6. Slab는 Bootstrap을 몰라도 되고 Bootstrap도 Slab 클래스를 몰라야 한다. host가 adapter를 명시적으로 등록한다.

**초기화 시점 주의:** 현재 `ma0t10_dtBootstrap`의 WebSocket 1ms 설정은 PostConfigInit에서 적용된다. 이를 Actor BeginPlay로 옮기는 것은 동등하지 않다. 우선 host 정책으로 남기고 플러그인은 암묵적으로 전역 값을 바꾸지 않는다. 필요하면 별도 opt-in 조기 모듈을 비교 검증한다.

**완료 gate:** PL-04. 빈 UE5.3 consumer 프로젝트에 DTCore+plugin만 넣고 Bootstrap 하나로 실행. host 소스/자산/local Game.ini 없이 Editor와 packaged Development 빌드/cook, UI/미리보기/파일/PCD/RHI/종료 회귀 통과. 다른 GameMode/Main 유무·다중 PIE에서 동료 자원 불변.

### R6 — 장비와 운영 범위 확장

- 새 장비의 물리 규격·품질·지원 capability를 Profile DataAsset으로 추가한다. backend/SDK가 필요하면 해당 extension을 구현하고 unsupported 기능은 비활성+이유를 표시한다.
- enum 기존값은 built-in ProfileId로 매핑한다. ML-X Native/Integration200을 합치지 않는다. 프로필 적용은 하나의 설정 transaction/재예약으로 유지한다.
- 실장비 SDK·패킷/캘리브레이션·raw sample이 확보된 모델만 hardware-calibrated/protocol-verified 단계로 올린다.
- 보안 배포 프로필·TLS 경로·Broker 장애·소비자 ACK·장시간 soak를 지원 조합별로 인증한다.
- DTCore의 별도 개발이 완료되면 **의존성 업데이트 PR**에서 API/수명/Widget/구독 contract 시험을 돌린다. 센서 기능 PR에 gitlink 변경을 묶지 않는다.

**완료 gate:** 신규 profile 등록에 코어 switch 편집이 필요 없고, golden dataset/실측 지원 표/릴리스 노트/호환 매트릭스가 장비별로 존재한다. 아직 자료 없는 장비는 공개 사양 에뮬레이션으로 남긴다.

## 3. 성능·신뢰성 gate 제안

아래는 최종 목표를 수치화하기 위한 **제안값**이며 현재 지원 선언이 아니다. 구현 착수 시 대상 PC·driver·scene·정격·출력을 고정하고 승인한다.

| 구성 | 화면 목표 | 정격/데이터 목표 | 현재 상태 |
|---|---|---|---|
| D455 1 + ML-X Native 1 | 평균≥55 FPS, 1% low≥45, frame p95≤20ms | Camera acquisition/JPEG≥29Hz, LiDAR/PCD≥19Hz(선택 출력) | 최신 PCD-only 단일 LiDAR 근거만 있음; 현재 SHA 전체 3-stream 재검증 필요 |
| 2 + 2 | 같은 60FPS 계열 기준 | 센서별 같은 요청 규격; 편차/정체 없음 | 미검증 |
| 4 + 4 | 평균≥28 FPS, 1% low≥24, frame p95≤36ms | 정격 유지 목표. 불가능하면 지원 한계를 명시, 몰래 15Hz로 바꾸지 않음 | 미검증 |
| 8 + 8 이상 | 탐색 시험 후 등급 설정 | Best Effort/지원/미지원 구분 | 보장 없음 |

공통 gate:

- 같은 모델/설정의 센서 간 완료 횟수 max/min≤1.2, 유예 후 0Hz 센서는 정상 평균에서 제외하지 않음.
- 실제 client 1280×720/1920×1080, 고정 scene 및 preview/표시/출력 조합, 최소10초 warmup(셰이더/자산 준비 포함), 60초 측정.
- 동일한 **승인된 센서 프레임 집합**의 제출/receipt/소비자 검증 일치. Slab 600행=센서600프레임으로 가정하지 않음.
- 정상 지원 조건 invalid/unexpected gap/overflow/silent replacement 0. budget miss와 미확인 소비자 결과는 별도 집계.
- 입력→화면 p95≤50ms, 조작 종료 coherent 프레임≤1.5초를 제안한다. 경량 preview와 물리 output을 섞지 않는다.
- 메모리·큐는 count와 byte 모두 상한, 10분/60분 안정 구간에서 누적 증가가 없어야 한다. 무조건적 0지연/0메모리를 요구하지 않는다.

### 네트워크는 물리적 한계가 있다

현재 33-byte PCD의 Native 32,256점은 header 제외 약1.064MB/frame, 20Hz에서 약21.29MB/s다. 4대면 약85.16MB/s이며 JPEG·telemetry·프로토콜 overhead가 추가된다. 모든 ray가 2 Echo이면 4대 약170.31MB/s로 **1Gbps의 이론적 125MB/s도 초과**한다.

따라서 센서 수 증가에는 측정/직렬화 최적화만이 아니라 NIC/Broker 디스크/소비자 처리량 검증이 필요하다. 가능한 해결은 더 빠른 망, 승인된 ROI/출력 선택, 별도 수집 노드 등이며 PCD 포맷/정격을 몰래 바꾸는 것이 아니다. 현재 코드의 bandwidth budget이 backend별 어느 경로에 적용되는지도 RT-01/08에서 확인한다.

## 4. 단계 운영과 출시 원칙

- 첫 구현 묶음은 **QA-01 + RT-02**, 다음은 **RT-01/03**, 별도 **RT-04**를 권장한다. 작은 정확성 결함을 둔 채 plugin 폴더부터 옮기지 않는다.
- 각 PR은 기능/계약/자산 이동 중 하나를 중심으로 만든다. 관련 빌드/테스트 후 커밋하고, 전체 회귀·실 RHI/네트워크 검증은 통합 gate에서 수행한다.
- schema/enum/SaveGame migration은 old fixture를 유지한다. wire contract를 바꾸는 기능은 별도 opt-in/version으로 제공한다.
- 성능 미달은 기능적 데이터 오류와 구분한다. 요청 규격을 충족하지 못하면 지원 표를 낮추거나 후속 최적화를 하고 달성했다고 표시하지 않는다.
- rollback은 신규 branch/commit으로 계획하고 보호된 사용자 작업을 reset하지 않는다. 주요 module/asset 이동 전에는 별도 worktree/깨끗한 consumer fixture로 검증한다.
- 문서 완료, 코드 완료, 자동화 완료, 실제 현장 완료를 구분한다. 실제 회사 맵/consumer가 제공되지 않으면 현장 완료 gate는 남는다.

### 단계 착수 시 확정할 입력

| 입력 | 필요한 이유 |
|---|---|
| 목표 PC/GPU/driver 및 실제 센서 수 | 지원 매트릭스·GPU 메모리/trace/encode budget |
| 장면 대표성·동적 물체·geometry 종류 | GPU depth/semantic/CPU의 측정 차이와 proxy 비용 |
| 필수 output·소비자 ACK 필요성 | 수집 완료의 의미·네트워크/디스크 budget |
| 배치 작업이 Editor인지 packaged runtime인지 | Undo/영구 저장/권한/Editor module 경계 |
| 대상 consumer 프로젝트·DTCore 승인 revision | 충돌 없는 drop-in 검증과 재현 가능한 릴리스 |
| 장비 SDK/calibration/golden data | 제조사 동일성의 검증 근거 |

모든 입력을 처음부터 받을 필요는 없다. 현재 저장소에서 해결 가능한 R0/R1부터 진행하고, 높은 영향의 미정 사항은 해당 단계 직전에 확정한다.

## 5. 최종 완료의 정의

1. 승인된 지원 구성에서 여러 센서를 설치·이동·복원하고, 무응답/상태 잔류 없이 조작할 수 있다.
2. 시나리오 실행마다 무엇을 측정·전송·소비했는지 데이터와 증거로 확인할 수 있으며, 관찰 fallback이나 부분 실패를 성공으로 오인하지 않는다.
3. 센서·카메라·공통 UI가 DTCore 의존 플러그인으로 배포되고 장비 추가가 데이터/extension 중심으로 이루어진다.
4. 깨끗한 다른 프로젝트에서 Bootstrap 하나로 검증 가능하며, 기존 GameMode/Main/Widget/Subscription/사용자 Actor를 침범하지 않는다.
5. 소스 빌드만이 아니라 자산/cook/실 RHI/외부 소비자/수명/성능/이식 gate를 통과한 범위만 release support로 공개한다.
