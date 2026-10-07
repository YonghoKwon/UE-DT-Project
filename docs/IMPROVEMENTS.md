# 보완 필요 사항 — 구현 백로그

기준: **프로젝트 `c4ffe92` 기반 Slab 초기 배치 복귀 / 센서 Runtime 기반 `2eb30f0` / DTCore 최소 수정 `b2504d1`, 확인일 2026-10-07**. 진행률은 이 기준의 소스와 확인된 검증 증거를 반영한다. 전체 R1·다중 센서·플러그인화가 완료된 상태는 아니다.

[작업 지침](../AGENTS.md) · [현재 기능/사용법](../README.md) · [단계별 로드맵](ROADMAP.md)

## 읽는 방법

**Slab 초기 배치 복귀 — 2026-10-07: 구현·관련 검증 완료.** 종료·송신 정리 후 진행 패널의 `초기 위치로 복귀` 또는 Actor API로 PIE 시작 당시의 월드 위치·회전에 즉시 복귀한다. 현재 형상/재질/스케일과 보관 목록은 유지하고 진행률·현재 프레임·세 차트·이전 3D 분석 라벨을 초기화한다. 실행·pause·파싱·재생/센서 준비·drain 중 거절, 실패/중단 후 복귀, cm/mm/m와 회전된 Track, 반복 호출·지연 적용 차단을 검사했다. 실제 Artemis 두 실행의 PCD 1,200건 제출/receipt/내부·외부 수신 일치와 복귀 추가 제출 0을 확인했다. 전체 D3D12 자동화 194 Success(실제 185·skip 9), Slab 집중 12 Success(실제 11·skip 1), WBP 6개 메모리 컴파일과 직접 1286×760/100% 조작을 수행했다. 숨김 중 차트 초기화와 동일 시나리오 새 RunUUID 재생도 직접 확인했다. DTCore·운영맵·기존 UI 저장값은 보존했다. `Saved/Reports/SlabResetPlacement/FINAL_STATUS.md`를 따른다. 이는 개별 복귀 기능의 완료이며 RT-05/QA-01 또는 기존 재생 p95 목표를 완료로 올리지 않는다.

**최소 DTCore 적용 검증 — 2026-10-06:** 별도 체크아웃에서 main 기반 최소 수정본을 연동했다. 1차 D3D12 전체161개 Success에는 조건부skip5개가 포함되며 실제156개다. 추가 공통 수명 검증은 13개 Success(실제 수행 12개·Slab Broker opt-in skip 1개), 실제 topic.scenario 수신은 별도 전체 시험에서 통과했다. PCD 전용 및 세 스트림의 1920×1080·10초 준비+60초 시험에서 승인 집합의 내부/외부 누락 0, invalid/gap/duplicate 0을 확인했다. PCD 20Hz, Camera 30Hz, 평균 60FPS·프레임 p95 16.67ms. 단순 누적 카운터가 아니라 고정 측정 request 집합을 대조했다. 최종 소스 전체 자동화는184개 Success(실제179·조건부skip5), 신규 실패0이다. 직접1286×760에서 네 글자 배율·패널·Slab 실행/재생·입력창 Esc 복구를 확인하고 최종 Shipping/Development 패키징을 통과했다. 재생 데이터1200건은 일치하지만 p95는기존24.908ms/최소수정24.925ms로 두 버전 모두20ms 목표가 실패다. Geometry 성능·10회 stream 반복은 미수행으로 유지한다. 기능 회귀와 성능을 포함한 최종 검수 완료를 구분한다. `Saved/Reports/DTCoreMinimalAcceptance/FINAL_STATUS.md`를 따른다. 상세 원인은 이 문서의 부록으로 추가하지 않는다.

**이전 이력 — 2026-09-29 런처 실행:** 격리된 관리형 런처 GUI에서328파일 설치/해시·UE 창·런처 종료 후 동일 UE 프로세스 유지·정상 종료를 확인했다. DTCore CustomLogs가 설치 경로에 쓰이는 점을 발견했으므로 회사 Program Files 권한/로그 위치는 미검증으로 남긴다. 실제 버전 변경/복구·Linux·운영망 검증은 이 결과로 완료되지 않는다.

**이전 이력 — 2026-09-29 패키징:** Runtime의 에디터 전용 테스트11파일을 WITH_EDITOR로 제한하고, 승인된 TestMap의 누락 모니터 참조를 복구했다. 당시 Windows Development 전체 Build/Cook/Stage/Archive와 관련 D3D12 자동화29건을 확인했다. 이후 현재 기준에서는 EnhancedInput 의존 선언, 프로젝트·격리 host의 Editor/Shipping 빌드와 Development 패키징을 검증했다. Linux·회사 운영망 및 실제 소비 프로젝트의 검증은 별도다.

**이전 검수 이력 — 2026-10-03:** `Saved/Reports/DTCoreFinalAcceptance/FINAL_STATUS.md`와 README의 최신 검수 요약을 우선한다. 현재 전체 자동화는190건 Success/실패0이며 실제 수행185건과 opt-in skip5건을 구분한다. 현재 source의10분 연속·10회 반복 송수신은 통과했지만 최종 성능 비교·일부 opt-in·전체 직접 조작 검수는 남아 있다.

- **정적 위험:** 실제 소스의 연결/조건을 확인했다. 이 문서만으로 런타임 재현 완료를 뜻하지 않는다.
- **검증 공백:** 구현이 있으나 해당 조건의 근거가 부족하다. 먼저 재현/계측하고 실패가 확인되면 고친다.
- **신규 기능:** 현재 없는 목표 기능이다. 기존 결함으로 취급하지 않는다.
- **P0:** 다중 센서 확대 전에 데이터 정확성·상태 복원·자원 한계를 확인/수정. **P1:** 다음 안정화 단계. **P2:** 제품화·선택 기능.
- 구현 상태와 검증 상태를 따로 읽는다. 실제 화면 검증 미수행을 완료로 바꾸지 않는다.
- 미래 요구를 현재 기능이라고 설명하지 않는다. 항목 ID는 커밋/PR/시험 보고서에서 유지한다.

### 진행률 기준

진행률은 **해당 개선항목의 완료조건에 대한 단계 지표**다. 작업시간·남은 사용량·제품 전체 완성도·성능 수치의 비율을 뜻하지 않는다.

| 진행률 | 의미 |
|---:|---|
| 0% | 제안·미착수. 기존 기반 기능만 있는 경우 포함 |
| 10% | 문제와 구현 공백 조사 완료, 해결 구현은 미착수 |
| 40% | 일부 구현 또는 일부 목표 조건의 검증 완료 |
| 70% | 핵심 구현·집중 자동검증 완료, 주요 수용검증이 남음 |
| 90% | 핵심 조건 통과, 제한된 최종 확인·완료 범위 정리가 남음 |
| 100% | 해당 항목의 명시적 완료조건·검증·문서 정리까지 완료 |

- 신규 기능의0%는 현재 센서·카메라 기능 자체가 없다는 뜻이 아니다. 기존 기반 기능을 신규 목표의 구현 진척으로 합산하지 않는다.
- 크기가 다른22개 항목의 단순 평균으로 프로젝트 전체 진행률을 만들지 않는다. 요약표와 상세 항목의 진행률은 함께 갱신한다.
- RT-01·RT-04의 일반적인 장기·복합 장애 시험은 LIFE-01·RT-05와 연결한다. 해당 항목의 최종 증거 대조와 완료 범위가 닫히면100%로 갱신하며, 전체 R1 완료로 확대하지 않는다.

## 우선순위 요약

| ID | 분류 | 우선 | 진행률 | 핵심 | 남은 핵심 조건 | 로드맵 |
|---|---|---:|---:|---|---|---|
| RT-02 | 조작 / 구현·자동검증 완료 | P0 | **70%** | 종료 상태 통일·원설정 복귀 | 해상도별 전체 직접 조작 체크리스트 | R1 |
| RT-01 | 성능·전송 / 구현·집중검증 완료 | P0 | **90%** | frame/byte reservation·과부하 진단 | 상한·재시도·종료 증거의 최종 연결 및 완료 범위 정리 | R1 |
| RT-04 | 데이터 / 구현·집중검증 완료 | P0 | **90%** | capture 직후 readback·immutable metadata | 현재 증거와 수용 목록의 최종 대조·종료 정리 | R1 |
| RT-03 | 진단 / 구현·자동검증 완료 | P1 | **70%** | 0Hz·신선도·공정성 판정 | 실제 정체·일시정지·복귀 화면 확인 | R1 |
| QA-01 | 품질 / 검증 공백 | P0 | **70%** | 고정 의존성·clean checkout·fixture 분리 | 실행 manifest·새 성능 계측·최종 비교 | R0 |
| RT-05 | 성능 / 검증 공백 | P1 | **40%** | 최신 SHA 다중 센서 지원 매트릭스 | 최종 성능 비교,2+2·4+4 지원 매트릭스 | R2 |
| RT-06 | 사용자 / 검증 공백 | P1 | **0%** | 다중 센서 배치 입력 지연 | input-to-visible 계측·수용시험 | R2 |
| RT-08 | 성능 / 신규 기능 | P1 | **0%** | 작업량 기반 capacity/admission | 작업량·예산 기반 접수 및 공정 배분 | R2 |
| RT-09 | 성능 / 구현 공백 | P1 | **10%** | Camera staging 재사용 | 자원별 지연 인과 확인·필요 시 구현 | R1/R2 |
| UX-01 | 사용자 / 신규 기능 | P1 | **0%** | 센서 설치 workflow | 생성·복제·삭제·preset·Undo/Redo 통합 | R2 |
| DATA-01 | 사용자·신뢰성 / 신규 기능 | P1 | **0%** | 데이터 필수 실행 정책 | 실행 정책·연결 준비·완전성 manifest | R3 |
| RT-07 | 전송 / 신규 기능 | P2 | **0%** | 소비자 업무 ACK | ACK 계약·멱등 처리·미확인 결과 관리 | R3 |
| LIFE-01 | 안정성 / 검증 공백 | P1 | **40%** | 종료·취소·재연결 | 파이프라인별 fault matrix·잔존 자원 조사 | R1/R3 |
| UI-01 | UI / 검증 공백 | P1 | **40%** | 해상도·DPI·소유권 | 1920×1080 직접 조작·DPI·외부 Widget 입력 | R2/R5 |
| UI-02 | UI / 신규 기능 | P2 | **0%** | 3D 라벨 배치 | 겹침 제어·우선순위·거리 표시 정책 | R2 |
| IO-01 | 파일 / 알려진 제한 | P1 | **10%** | LAZ 외부 프로세스 제한 | timeout·cancel·hang 회수 구현과 시험 | R3 |
| PL-01 | 구조 / 신규 기능 | P1 | **0%** | host 의존성 역전 | 중립 인터페이스·Slab 의존 분리 | R4 |
| PL-02 | 이식 / 신규 기능 | P1 | **0%** | 기본값·설정 namespace | 기본값·자산·Topic·저장 경계 중앙화 | R4 |
| PL-03 | 확장 / 신규 기능 | P1 | **0%** | 장비 registry | ProfileId 기반 등록·선택·호환성 | R4/R6 |
| PL-04 | 설치 / 신규 기능 | P1 | **0%** | Bootstrap·배포 | 센서 플러그인·단일 진입점·소비 프로젝트 검증 | R5 |
| SEC-01 | 운영 / 검증 공백·신규 기능 | P2 | **10%** | 보안·연결 운영 | 배포별 인증·권한·TLS·설정 검증 | R3/R6 |
| HW-01 | 충실도 / 검증 공백 | P2 | **0%** | 실장비 충실도 | SDK·실측 데이터·캘리브레이션 비교 | R6 |

## 1. 성능·측정·전송

### RT-01 — Raw TCP receipt 대기 상한 (구현·집중검증 완료, P0)

**진행률: 90%** · 구현: 완료 · 검증: 상한/receipt 누락 집중시험 및 실제 FIFO 송수신 통과.

- **남은 조건:** 상한·동일 body 재시도·종료 후 반환·UI 보관량 증거를 최종 검수 목록에 연결하고 이 항목의 완료 범위를 닫는다. 복합 장애는 LIFE-01, 구성별 장기 지원은 RT-05에서 관리한다.

**수정 전 근거:** Raw body는 PendingReceipts에 보관됐지만 호환 WaitingReceipts와 UI admission 기준이 분리돼 있었다.

- 구현 `a120ba6`: worker가 접수부터 receipt/최종 실패/종료까지 request reservation을 소유한다. 전역128MiB/스트림64MiB, Camera8/기타20프레임. 같은 body retry는 중복 집계하지 않는다. UI는 worker 실제 보관량을 조회한다.
- 집중 검증 3/3: cap/terminal ledger 및 실제 TCP fake Broker receipt 누락(20 SEND/60 bytes 후 21번째 거부, 종료 후 reservation0). 실제 Artemis PCD/세 스트림 정상 FIFO·checksum·외부 수신도 검사했다. 전체 네트워크 장애 조합과60분 soak는 후속이다.
- 증거: `Saved/Reports/DTCoreSync/raw-fault`, `receipt-probe.json`, `dtcore_after_pcd.json`, `dtcore_after_three.json`. 전역상한은 특정 구성의 지원 보장이 아니다.

### RT-02 — Esc 조작 종료 연결 (구현·자동검증 완료, 수동 검증 대기)

**진행률: 70%** · 구현: 완료 · 검증: lifecycle/Slate 자동시험 및 일부 실제 조작 통과.

- **남은 조건:** Camera 회전·LiDAR 숫자 변경·포커스 이탈·선택 변경·종료 등 해상도별 전체 직접 조작 목록을 완료한다. 현재 증거는 아래 최신 기록을 따른다.

2026-10-03 검수: 소유 패널/PIE viewport에 한정된 Slate 입력 중재기, 첫 Esc·repeat/release, 원설정 복귀 및 Main/외부 Widget 격리 시험을 통과했다. 직접 마우스로 Camera/LiDAR 이동·LiDAR 회전·첫 Esc의 PIE 유지도 확인했다. 숫자 Transform 편집이 전체 측정 설정을 재시작하던 회귀와 빠른 pointer drag 누락을 수정했다. Camera 회전·LiDAR 숫자 변경 등 해상도별 전체 직접 조작과1920×1080 수동 gate는 아직 미완료다. Camera 숨김 복귀의 추가 증거는 아래 최신 기록을 따른다.

추가 회귀 `2eb30f0`: 이전 커서 위치의 global UI hover가 실제 viewport 클릭을 거부했다. 우리 기즈모의 event-position hit path만 사용하도록 수정하고, 이전 패널 hover를 강제로 남긴 시험에서 viewport drag 수락·실제 패널 클릭 거부를 확인했다. 공통 DTCore hover·다른 Widget 정책은 변경하지 않았다.

현재 source의 직접 조작 추가 증거: Camera Z170→180 숫자 변경 후5Hz 경량 조작 유지, Settings 숨김으로 종료 후 원래640×360/10Hz 미리보기 복귀, 실제 client1280×720의150% 제목/버튼 가독성. 전체 두 해상도/모든 경계의 수동 완료로 확대하지 않는다.

**수정 전 근거:** Gizmo Esc가 Actor 종료와 분리되어 있었고 Settings가 종료할 때 현재 선택 Actor를 조회했다. 새 회귀 테스트에서 선택 변경 후 원래 Camera의 640px·0.2초·미리보기 모드가 복원되지 않는 것을 재현했다.

- 구현 `d2024bf`: 종료 요청 이벤트 → Settings 단일 정리 → 시작 때 고정한 약한 Actor 참조. 일반 drag commit과 종료를 분리하고 직접 Gizmo 비활성도 정리한다. 선택 변경은 새 센서 조작을 자동 시작하거나 이전 monitor 선택을 덮어쓰지 않는다.
- 검증: 단위 lifecycle/monitor follow 및 D3D12 PIE runtime 3/3. Camera/LiDAR FullSpec 복원·후속 프레임·숨김·Gizmo EndPlay·반복 종료, 정지 상태·삭제된 대상·Widget 파괴를 검사했다. 동기 측정을 추가하지 않았다.
- 이전 검증 당시 미수행: Computer Use 오류 `0x80070057`/`0x80070005` 때문에 물리 Esc와 실제 viewport 측정이 막혔다. 이후2026-10-03에 첫 Esc·이동 및 실제1280×720 일부 직접 검증을 수행했다. 이 과거 제한을 현재 모든 Esc 검증이 미수행이라는 뜻으로 읽지 않는다.
- 증거: `Saved/Reports/SensorStability/reproduction.log`, `final-rhi/index.json`, `report.ko.md`.

이전 이력 — 2026-10-02: 실제1606×728에서 조작 시작/preview·Settings drag/resize·LiDAR 설정 숨김 후4Hz 복귀를 확인했다. 당시 Esc는 Editor Stop PIE를 실행했다. 이후 수정·검증은 위2026-10-03 기록을 우선한다. 당시 증거 `Saved/Reports/DTCoreSync/manual-camera.png`, `manual-lidar.png`, `gui-final.log`는 실패/제한 이력으로 보존한다.

### RT-03 — 0Hz 최소율/공정성 (구현·자동검증 완료, 화면 확인 대기)

**진행률: 70%** · 구현: 완료 · 검증: 정체·유예·복귀·pause·공정성 자동시험 통과.

- **남은 조건:** 실제 화면에서 정체·일시정지·재개 및 정상 복귀 표시를 확인한다.

**수정 전 근거:** Scheduler min/max가 양수 Hz만 포함해 완전히 멈춘 센서를 숨길 수 있었다. 마지막 acquisition-rate 값도 자동 만료되지 않았다.

- 구현 `59d1027`: World-time 진행 시각, 시작 유예 `max(1초, 실제 주기×3)`, 오래된 양수 Hz→0Hz, 정체 ID 및 공정성 유효성 flag. pause는 측정 없이 진단만 갱신하고 조작용 임시 주기는 정격 공정성 통과에서 제외한다. 센서 주기는 변경하지 않았다.
- 검증: SensorPerformance 5/5, 보고서 판정식 정상/0Hz/센서 누락 3/3. 두 센서 중 하나/모두 정체, 복구·시작/재등록 유예·pause·저주기·조작·실패 이력·빈 그룹을 검사했다. 기존 전체 회귀에서 신규 실패 0.
- 주의: 기존 fairness 수치 0은 판정 불가 sentinel이다. 외부 Blueprint는 추가된 `bCameraFairnessEvaluable`/`bLidarFairnessEvaluable`을 확인해야 한다. Camera는 기존 acquisition 정의이며 GPU 픽셀 완료 검증은 RT-04다.
- 남음: 실제 화면에서 정체/일시정지 표시 확인은 미수행. 증거 `Saved/Reports/SensorStability/rt03-focused`, `full-regression`, `report.ko.md`.

### RT-04 — Camera FrameId와 실제 픽셀 (구현·집중검증 완료, P0)

**진행률: 90%** · 구현: 완료 · 검증: 고유 token/CRC990개 완료 JPEG·metadata 및 음성 대조군 통과.

- **남은 조건:** 현재 source의 증거를 Camera 수용 목록과 최종 대조해 항목을 닫는다. 전체 복합 수명 fault는 LIFE-01, 전후 성능·다중 구성은 RT-05/QA-01에 연결한다.

2026-10-03 검수: 완료 이벤트의 immutable JPEG/metadata를 bounded decoder에 전달해 고유 acquisition token+CRC16을 990개 모두 대조했다. 포화·encode 역순·pose/revision/Slab context·Stop/재시작·삭제와 이전 이미지 음성 대조군을 포함한다. 현재 source의 전체 자동화와 독립 host 빌드는 통과했다. LiDAR 측정 시작 UTC 보정 후 성능 비교에서 간헐적인 기준선 실패가 있어 재분석 중이며, 예전 12회 결과를 최종 source 통과로 사용하지 않는다.

**수정 전 근거:** 스트리밍 readback slot 부족 시 metadata를 보관하고 나중의 현재 RenderTarget을 읽을 수 있었다.

- 구현 `017d20f`: 자연 capture hook에서 pose/UTC/context를 고정하고 해당 capture 직후 readback copy를 예약한다. coalesced frame/slot 부족은 명시적 파생 실패이며 나중 픽셀을 옛 ID에 붙이지 않는다. Actor의 프레임 UTC도 encode 완료 시각이 아니라 acquisition 시각을 사용한다.
- 이전 안정화 단계 기록: CameraAcquisitionSnapshot 및 D3D12의 red/blue marker·encode 지연·포화 복귀를 JPEG25개 폴링으로 확인했다. 이 제한된 시험은 위의 최종 고유 token/CRC990개 완료 이벤트 검증과 구분한다. 초기 harness의 ShowOnly/cold shader 문제와 최종 통과 로그를 모두 보존한다.
- 증거: `Saved/Reports/DTCoreSync/camera-unit`, `camera-rhi4`, marker JPEG. 같은 장면의 출력 성능은 10+60초 전후 비교했다. 모든 이동/context/삭제/설정 변경 조합의 stress 인증은 후속이다.

### RT-05 — 다중 센서 실측 지원 범위 (검증 공백, P1)

**진행률: 40%** · 구현: 기존 측정/송신 기반 존재 · 검증: 현재1+1 구성 실측 확보, 다중 지원 범위 미확정.

- **남은 조건:** 새 성능 계측과 최종 비교를 완료하고2+2·4+4 구성별 정격·공정성·장기 지원 매트릭스를 검증한다.
- 최신 근거: Runtime `2eb30f0`의 실제1920×1080 세 스트림10분은 Camera29.998Hz·LiDAR/PCD20Hz, 승인 요청/receipt/내부·외부 수신 대조 오류0으로 통과했다.10회 반복과 일부1280×720 직접 조작도 확인했다. 최종12회 비교·새1% low 판정·다중 센서 지원을 완료한 결과는 아니다.

- 이전 근거는 ML-X Native **한 대 PCD-only, 753×403, 30초×2**다. 1,199건 송수신 일치, 후반1% low45.019FPS. 이후 실제1280×720/1920×1080 및1+1 정격 시험은 README의 source별 기록을 따른다. 옛2+2/4+4 저출력률 시험을 현재 정격 지원 근거로 쓰지 않는다.
- 이전 이력 — 2026-10-02: 실제1274×680 PCD-only/1+1 시험과 재생 p9525.07ms 미달을 기록했다. 이후 재생 시험의 scoped background throttle 보정으로 통과한 기록과 현재 source에서 갱신해야 할 opt-in을 README에서 구분한다. 과거 해상도·지연값을 최신 판정으로 사용하지 않는다.
- 1+1 → 2+2 → 4+4를 동일 SHA/고정 fixture에서 검증한다. 8+8은 탐색용 best effort이며 자동 지원으로 분류하지 않는다.
- 각 단계에서 실제 client 해상도, profile/backend/echo, preview/출력 on/off, FPS·1% low·p95/p99, 센서별 acquisition/encode/submit/receipt/내외부 receive, queue/bytes/memory를 기록한다.
- 현재 코드의 `1% low`는 frame-time p99의 역수다. 남은 최종 검수에서는 가장 느린1% 프레임 평균을 주 판정으로 사용하고 기존 p99 역수도 함께 기록하기로 확정했다. 원시 배열·중복 방지·긴 프레임 보존 계측은 아직 보강 대상이다. engine delta·실제 wall pacing·trace의 대기를 합쳐 하나의 FPS로 표시하지 않는다.
- 완료: 60초 회귀를 통과한 구성만 10분 안정성, release 후보는 60분 soak로 확장한다. 요청값을 낮춰놓고 원래 FullSpec 통과로 발표하지 않는다. 목표 수치는 ROADMAP의 제안 gate를 승인·고정한 뒤 적용한다.

### RT-09 — Camera staging texture 재사용 및 지연 원인 분리 (구현 공백, P1)

**진행률: 10%** · 구현: Camera 재사용 미착수 · 검증: 반복 생성 경로와 trace 한계 조사 완료.

- **남은 조건:** Camera 자원별 지연 인과를 확인하고 성능 실패의 관련 원인으로 확인될 때 재사용 구현·정합성·전후 시험을 진행한다. 현재 최종 검수 범위에서는 조건부 수정이다.

- 2026-10-03 trace에서 game-thread render 대기와 D3D12 texture 생성 지연을 관측했다. LiDAR의 매 프레임 readback 생성은 `0c502af/e6afe4f`에서 재사용·현재 제출 fence 검사로 수정했다. 개별 trace 자원 이름이 없어 모든 생성 지연이 LiDAR라고 단정하지 않는다.
- Camera `QueueScheduledGpuReadback`은 현재도 슬롯을3개로 제한하지만 프레임마다 `FRHIGPUTextureReadback`을 생성하고 완료 후 해제한다. **슬롯 수 제한과 staging GPU 자원 재사용은 다른 기능**이다. Camera 재사용 최적화는 이번에 구현하지 않았다.
- 후속 변경은 동일 크기/format 슬롯 자원 보존, 새로운 copy의 fence 확인, resize/Stop/삭제 시 render-thread 해제, 고유 token/CRC 회귀로 구성한다. 이미지·metadata 정합성이나 출력 주기를 희생하지 않는다.
- 완료: 동일 조건의 전후3회 trace에서 해당 readback allocation 감소를 확인하고, 실제 제출/receipt/수신Hz·지연·FPS·990개 identity·장시간 반환을 재검증한다. 외부 CPU 부하와 엔진 자원 지연은 인과 증거 없이 하나의 원인으로 합치지 않는다.

### RT-06 — 조작 중 입력 지연 (검증 공백, P1)

**진행률: 0%** · 구현: 경량 조작은 기반 기능 · 검증: 이 항목의 다중 센서 입력 지연 수용시험 미착수.

- **남은 조건:** 2+2/4+4 input-to-visible 계측과30초 연속 조작·최종 프레임 복귀 시험을 수행한다.

- 기본 Camera 640×360/5Hz, LiDAR 120×24/4Hz의 경량 조작 API는 이미 있다. 이는 interaction request 기본값이지 API의 고정 최대치가 아니다. 새로 만드는 작업이 아니라 다중 실행 경계 검증이다.
- 입력 timestamp→Transform 적용→화면 표시를 측정한다. 평균 FPS만으로 조작감을 평가하지 않는다. 기존 world/local, Shift/Ctrl, 최종 품질 복원 유지.
- 완료: 2+2/4+4에서 30초 연속 drag/rotate, 키보드, 선택 전환, Esc/hide/end를 실행. 제안 목표는 input-to-visible p95 ≤50ms, 종료 후 coherent 최종 프레임 ≤1.5초. 불가능한 구성은 지원 밖/제한 원인을 공개한다.

### RT-08 — 작업량 기반 admission (신규 기능, P1)

**진행률: 0%** · 구현: 신규 작업량 기반 정책 미착수 · 검증: 전용 수용시험 없음.

- **남은 조건:** 작업량·EWMA·자원/대역폭 예산 기반 접수와 공정 배분을 구현하고 혼합 부하를 검증한다.

- 현재 FPS tier는 센서 **개수** 중심이다. 같은 4대라도 해상도·rays·echo·GPU/CPU·JPEG·PCD·semantic proxy 비용은 다르다.
- pixels/s, rays/s, 측정/파생 처리 EWMA, GPU/readback/encode 슬롯, queued bytes·네트워크 예산으로 사전 capacity 표시와 공정 배분을 보완한다.
- Requested/Admitted/Acquired/Published Hz를 분리한다. preview 품질과 물리 측정 규격도 분리하며 사용자 모르게 resolution/주기를 낮추지 않는다.
- 완료: 동일 센서 수의 경량/고부하 혼합에서 starvation 0, bounded 자원, 경고와 실측 결과 일치. 추가 센서가 기존 센서의 완료율을 독점하지 않음.

## 2. 사용자 workflow·데이터 신뢰성

### UX-01 — 센서 설치 도구 (신규 기능, P1)

**진행률: 0%** · 구현: 통합 설치 workflow 미착수 · 검증: 전용 수용시험 없음.

- **남은 조건:** 생성·복제·삭제·preset·Undo/Redo·재로드와 삭제 중 비동기 안전성을 통합한다.

- 현재는 Actor를 배치하고 선택/조작하는 기능이다. Runtime 생성·복제·삭제·배치 preset을 완성된 한 workflow로 제공하지 않는다.
- Editor 설치와 PIE 임시 설치를 명확히 구분한다. 고유 ID 생성/중복 검사, 이름/종류별 목록, transform 입력·snap·복제, 배치 preset 저장/불러오기, Editor Undo/Redo를 제공한다.
- Actor API와 setup descriptor를 사용하고 측정 Component 필드 직접 변경은 금지한다. Editor 배치의 영구 저장은 명시적 동작으로만 수행한다.
- 완료: 10개 생성·복제·이름 변경·삭제·Undo/Redo·재로드, 소유하지 않은 Actor 불변, duplicate ID 0, 송신/비동기 작업 중 삭제 안전. 센서 없는 레벨도 빈 상태 안내로 시작.

### DATA-01 — 관찰 가능한 실행과 데이터 필수 실행 (신규 기능, P1)

**진행률: 0%** · 구현: 데이터 필수 실행 정책 미착수 · 검증: 전용 수용시험 없음.

- **남은 조건:** 실행 정책·실제 연결 준비·장애 처리·완전성 manifest를 구현한다. 기존 관찰 fallback을 이 신규 정책의 완료로 세지 않는다.

- 현재 live 설정 부족은 관찰 fallback으로 이동한다. 좋은 개발 편의지만 학습 데이터 수집 작업에서는 “움직였으니 데이터도 수집됐다”는 오해를 막아야 한다.
- 기본 fallback은 유지하고 opt-in **데이터 필수 실행**을 제안한다. preflight→연결 준비→실행→drain→완전/부분/실패 결과를 분리하고 요구 SensorId/출력의 준비 상태를 확인한다.
- 실행 중 Broker 장애는 움직임 중지 여부를 host 정책으로 명시한다. 자동 fallback/중간 재송신을 몰래 추가하지 않는다.
- 완료: 설정 누락·실제 연결 거절·용량 초과·중도 단절에서 실행 정책과 결과가 일치. 원본 시나리오/실행 UUID·선택 센서·profile revision·승인/완료 수를 작은 manifest에 기록한다.

### RT-07 — 소비자 업무 ACK (신규 기능, P2)

**진행률: 0%** · 구현: 업무 ACK 계약 미착수 · 검증: 전용 수용시험 없음.

- **남은 조건:** ConsumerProcessed·멱등 처리·재전송·미확인 결과와 보존 정책을 버전 계약으로 정의하고 구현한다.

- 현재 drain의 Raw 대기는 Broker receipt 중심이다. 자체 parser의 성공도 최종 AI/업무 소비자의 저장·처리 완료는 아니다.
- 필요 시 `(RunUUID, Segment, SensorId, SensorFrameId, checksum)` 기반 application ACK/누락 대조/멱등 재전송을 별도 버전 계약으로 추가한다. 기존 PCD body를 묵시적으로 변경하지 않는다.
- 완료: 소비자가 중단되어도 BrokerAccepted와 ConsumerProcessed를 혼동하지 않음. 중복 메시지 재처리 방지, 종료 결과에 미확인 목록, timeout/재시도/보존 정책이 명시됨.

### LIFE-01 — 중도 종료와 재연결 (검증 공백, P1)

**진행률: 40%** · 구현: 기존 generation/종료 안전 장치 존재 · 검증: Camera/LiDAR 수명 경계와10회 반복 일부 확인.

- **남은 조건:** 전체 파이프라인 fault matrix와 thread/handle·잔존 객체·메모리 복귀를 조사한다. 현재 반복 정리 후 private bytes 약32MB 증가는 원인 확인이 남아 있다.

- weak pointer/revision·세션 drain·file writer 취소는 이미 있으므로 전체 재작성하지 않는다.
- acquisition/readback/encode/직렬화/socket/receipt/파일 저장 각 단계에서 Actor 삭제, Stop, 품질 변경, PIE 종료, world 이동, 동일 시나리오 재실행을 주입한다.
- 완료: 늦은 결과 적용·double complete·old generation 수신 0, 종료 시간 상한, thread/handle/메모리 복귀, 다른 owner 연결 유지. 각 경로 테스트가 어느 수명 경계를 증명하는지 기록한다.

### IO-01 — LAZ 프로세스 제한 (알려진 제한, P1)

**진행률: 10%** · 구현: timeout/cancel 보강 미착수 · 검증: 외부 실행 공백 조사 완료.

- **남은 조건:** timeout·cancel·hang 회수·부분 파일 정리와 fake compressor 시험을 구현한다.

- 외부 compressor 미설정 시 성공을 가장하지 않으며 실제 timeout/cancel 및 장시간 hang 정리는 아직 보완 대상이다.
- 자신이 시작한 자식 프로세스만 추적해 timeout/cancel, 제한된 stdout/stderr, 임시 파일 cleanup, 오류 코드를 제공한다. 무관한 프로세스를 이름으로 종료하지 않는다.
- 완료: fake compressor 성공/오류/hang/취소 시험, worker slot 복귀, 부분 파일을 완료로 표시하지 않음. CSV/PCD/JPEG 작업이 함께 굶지 않음.

## 3. UI·가독성

### UI-01 — 실제 해상도/DPI/소유권 (검증 공백, P1)

**진행률: 40%** · 구현: 소유 패널 격리·배율·창 조작 존재 · 검증: 일부 실제1280×720 및 Main/외부 Widget 자동시험 통과.

- **남은 조건:** 1920×1080 전체 직접 조작·DPI 대표 조합·외부 Widget 입력·전체 checklist를 완료한다.
- 최신 직접 근거: 실제client1280×720,150% 제목/버튼 가독성, 숫자 Transform·Settings 숨김 복귀를 확인했다. 일반 PIE의 앞선 최대client는1920×998였고 Alt+Enter로 크기가 바뀌지 않았다.1920×1080 자동화 결과는 직접 마우스 검수와 구분한다.
- 이전 이력: 988×521/1174×683은 과거 수동 시험 크기다. 실제 동료 프로젝트는 미제공이다.
- 두 목표 client 크기, 100/125/150% OS DPI 및 네 폰트 배율의 대표 조합을 검사한다. 창 경계/메뉴 overflow/입력 clipping/접기 후 hit-test/Tab focus/숨김 중 작업/재호스팅을 확인한다.
- 완료: 실제 크기 증거, 기존 저장값 roundtrip, 미등록/중첩 Widget의 font·geometry·입력·파일 hash 불변. 확보 못한 조합은 명시적으로 미완료.

### UI-02 — 3D 라벨 배치 (신규 기능, P2)

**진행률: 0%** · 구현: 신규 자동 배치 정책 미착수 · 검증: 전용 수용시험 없음.

- **남은 조건:** 겹침 제어·우선순위·leader line·거리 표시 정책을 구현하고 여러 시점에서 검증한다. 기존 문자 크기/대비 개선은 기반 기능이다.

- 현재 큰 문자 배경판이나 도구 창이 다른 라벨을 가릴 수 있다. 선/글자 크기, 대비, 한글 표시 자체는 구현·수동 확인했다.
- 시야 밖/뒤쪽 점을 숨기거나 안전 위치로 안내하고, 라벨별 priority·최대 겹침·leader line/거리 fading을 추가할 수 있다. 숫자/단위/좌우 의미는 유지한다.
- 완료: 원근/직교, 근접/원거리, roll, 태양광/고온 Slab 조건의 읽기 시험. 진단이 Camera/LiDAR를 오염시키지 않고 5Hz 표시 budget 유지. 전역 occlusion/visibility를 바꾸지 않는다.

## 4. 플러그인·프로필·운영

### PL-01 — host 의존성 역전 (신규 기능, P1)

**진행률: 0%** · 구현: 중립 경계 전환 미착수 · 검증: 무Slab/adapter fixture 게이트 없음.

- **남은 조건:** 중립 provider/observer·출력 정책·UI extension으로 Slab/host 직접 의존을 분리한다.

- 현재 센서 frame/request가 `FVirtualSlabFrameContext`를 포함하며 Camera/Scan이 SlabContext Subsystem을 직접 호출한다. 데이터 UI는 SlabActor를, Workspace는 Slab panel/Host를 직접 안다.
- 중립적 frame context provider/completion observer, output-session policy, host UI extension으로 역전한다. Slab codec/업무 원본/차트/재생은 host adapter에 남긴다.
- 완료: 센서 Runtime이 host 모듈을 include/dependency하지 않는 static/build gate. Slab 없는 fixture + Slab adapter fixture에서 기존 wire bytes/metadata 의미 회귀 통과. provider UObject를 worker에서 직접 읽지 않음.

### PL-02 — 기본값·설정 namespace (신규 기능, P1)

**진행률: 0%** · 구현: 제안 중앙 설정/bridge 미착수 · 검증: 전용 이식 게이트 없음.

- **남은 조건:** 기본값·자산·Topic·저장 namespace와 Runtime/UI 경계를 중앙화하고 기존 값을 보존한다.

- `UVirtualSensorIntegrationSettings`와 `UVirtualSensorDtCoreBridgeSubsystem`은 현재 소스에 없다. 경로·맵 저장 대상·슬롯·Topic이 여전히 여러 곳에 있다.
- soft references/설정 객체를 도입하되 명시적 Actor/Transport 설정을 우선하고 없을 때만 기본값을 사용한다. 새 값으로 기존 Map/WBP를 자동 재저장하지 않는다.
- Frame/편집 상태 타입은 Runtime public 계약에 두고 UI→Runtime 단방향을 만든다. 저장 namespace는 plugin/rig/user 기준으로 설계하되 기존 v6/Workspace v1은 opt-in migration으로 보존한다.
- Coordinator의 Monitor 타입/직접 bind 호출도 Runtime delegate/interface와 UI adapter로 분리한다. 편집 타입 이동만으로 Runtime→UI 역의존이 해소되지는 않는다.
- 완료: host 이름 일괄 치환 없이 자산/저장/Topic 변경 가능, 없는 자산의 명확한 진단, 다른 rig/Widget 저장값 불변, Editor 저장 allowlist 유지.

### PL-03 — 장비 registry (신규 기능, P1)

**진행률: 0%** · 구현: ProfileId registry 미착수 · 검증: switch 수정 없는 신규 등록 시험 없음.

- **남은 조건:** DataAsset/registry 등록·선택·capability·기존 enum 호환성을 구현한다.

- 현재 장비 enum·resolver는 존재한다. 프로필을 또 다른 switch에 계속 추가하는 방식 대신 안정적인 ProfileId+DataAsset/registry를 제안한다.
- 물리 규격·quality preset·capability·calibration provenance와 acquisition backend/실장비 SDK adapter를 구분한다. 프로필 데이터 추가만으로 SDK가 구현되는 것은 아니다.
- 완료: 테스트 장비 하나를 핵심 switch 수정 없이 등록/선택/검증. 기존 enum→built-in ProfileId 호환, 원본 ML-X/Integration200 구분, 단일 설정 transaction·한 번의 재예약 유지.

### PL-04 — 한 클래스 진입점·배포 (신규 기능, P1)

**진행률: 0%** · 구현: 센서 플러그인/단일 설치 진입점 미착수 · 검증: 빈 소비 프로젝트 설치 게이트 없음.

- **남은 조건:** 센서 플러그인·Bootstrap을 구성하고 빈 프로젝트·다른 GameMode/Main·패키징을 검증한다. DTCore 최소 host 빌드와 이 항목의 센서 설치 시험은 다르다.

- Bootstrap façade가 기존 Coordinator/UI Host/선택 SourceHost를 조합한다. 기존 인스턴스를 우선 사용하고 자신이 만든 것만 정리한다. 새 거대 manager로 통합하지 않는다.
- host GameMode/GameInstance/Main를 강제 교체하지 않는다. 기본은 로컬 preview, 외부 송신은 명시적 설정 또는 host 시나리오 정책이다.
- 완료: 빈 UE5.3 프로젝트에 DTCore+센서 plugin만 넣고 Bootstrap 하나로 preview/UI 확인. 다른 GameMode/Main 유무, 지연 PlayerController, 다중 PIE, packaged Development, cook/asset migration 통과.

### SEC-01 — 보안/연결 운영 (검증 공백·신규 기능, P2)

**진행률: 10%** · 구현: 배포별 보안 보강 미착수 · 검증: 평문/전역 설정 등 정적 위험 조사 완료.

- **남은 조건:** 배포별 인증·권한·TLS·secret·연결 상태 및 프로젝트 전역 설정을 검증한다.

- Raw TCP는 평문이다. wss 호환 경로·HTTP·자체 진단·실장비 입력은 동일 기능이 아니며 연결을 임의 통합하면 안 된다.
- Broker 인증/권한·주소 유형·message limit·receipt timeout·재연결·TLS 성능을 배포별 설정으로 관리한다. 개발 계정/endpoint를 release 기본값으로 복사하지 않는다.
- 현재 조기 Bootstrap의 WebSocket 1ms 설정은 프로세스 전체 영향이 있다. 레벨 BeginPlay로 옮기면 초기화 시점이 늦다. host 정책으로 유지하거나 명시적 opt-in 조기 설정으로 별도 검증한다.
- 완료: 암묵적 전역 설정/다른 Subscription 종료 0, secret 미저장, wss가 평문으로 바뀌지 않음, 오류/인증/연결 상태가 UI에 정확히 표시됨.

### HW-01 — 실장비 충실도 (검증 공백, P2)

**진행률: 0%** · 구현: 실장비 근거에 맞춘 보정/adapter 미착수 · 검증: golden dataset 없음.

- **남은 조건:** SDK·실측 캡처·캘리브레이션을 확보하고 센서 관측/패킷을 대조한다. 공개 사양 프로필은 실장비 동일성 검증 진척으로 세지 않는다.

- ML-X Native는 공개 사양 기반, ROS2/Livox/RealSense는 stub/확장 지점이다. 실제 장비 동일성을 주장할 자료가 없다.
- SDK/패킷·calibration·raw capture golden dataset을 확보한 뒤 좌표·거리·intensity·echo·timestamp·miss를 대조한다. HW Ray Tracing/GPU physical 모델은 capability별 별도 승인 작업이다.
- 완료: profile마다 구현/미구현·단위·오차·지원 backend·근거 dataset을 공개. synthetic truth와 보정된 센서 관측, Digital Twin semantic을 구분한다.

## 5. QA-01 — 재현 가능한 기준선 (검증 공백, P0)

**진행률: 70%** · 구현: 의존성 고정·격리 host/맵 fixture·검수 도구 일부 완료 · 검증: 현재 빌드·패키징·자산·회귀 통과, 최종 비교 미완료.

- **남은 조건:** 실행 manifest·원시 프레임/새1% low 계측·요약 자동 대조·최종 기준선 비교를 완성한다.
- 현재 상태: 동기화본 `b22af0b`의 새 공개 API를 유지하고 parent pin을 검증한 `067195b`로 고정했다. 현재 프로젝트와 별도 plugin 복사본의 `Tools/DTCoreCompatHost` 최소 host는 검증했다. 다른 실제 소비 프로젝트의 호환성을 대신 보장하지 않는다.
- 그럼에도 개인 Game.ini/SaveGame/수정 운영맵에 의존하지 않는 clean checkout build/asset load 기준선이 필요하다.
- 운영맵 Z=150cm fixture를 통과시키기 위해 사용자 맵을 수정하지 않는다. 커밋된 스냅샷 검사 또는 임시 검증 scene으로 fixture와 실사용 편집을 분리한다.
- 보고서에 commit/submodule SHA, Engine/toolchain/GPU/driver, 실제 viewport/DPI, 설정 hash, 실행한 tests/skip, 원본 count를 남긴다. 문서 요약과 원본 JSON을 자동 대조한다.
- 완료: clean setup 명령 한 묶음으로 빌드·WBP·계약·핵심 RHI 재현, 운영 asset hash 불변, 실패·skip이 집계에서 숨겨지지 않음.

## 다음 착수 순서

1. RT-02/03의 남은 수동 화면 검증과 QA-01의 고정 baseline/운영맵 fixture 분리를 마무리한다.
2. RT-01/04 집중 검증을 유지하면서 LIFE-01의 나머지 fault 조합과 QA-01 clean-consumer 범위를 보완한다. 전체 R1 완료로 확대하지 않는다.
3. 이후 RT-05/06과 UX-01로 실제 다중 배치 workflow를 확장한다.
4. 데이터 정확성 기준이 정해진 뒤 PL-01/02/03 → PL-04 순으로 플러그인화한다.

완료 이력은 각 항목에 검증 SHA·보고서 경로·남은 제한을 붙인다. 이 문서 작성은 해당 기능의 구현 승인이나 실행 완료가 아니다.
