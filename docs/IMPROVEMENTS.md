# 보완 필요 사항 — 구현 백로그

현재 기준: **`codex/pcd-demand-placement-acceptance` (기준 `a78b443`) / DTCore `b2504d1`**. 2026-10-09 새 배치 LiDAR의 명시적 JSON 수요 정책과 센서별 수용 판정을 추가했다. 과거 검증은 원래 조건을 유지하며, 이번 증거는 `Saved/Reports/PcdDemandPlacementAcceptance`에서 관리한다.

[작업 지침](../AGENTS.md) · [현재 기능/사용법](../README.md) · [단계별 로드맵](ROADMAP.md)

## 현재 개발 초점과 착수 순서

현재 목적은 **여러 센서·카메라를 쉽게 배치해 관측 위치를 비교하고, Slab 시나리오에서 생성한 PCD를 외부 수신 프로그램에 안정적으로 전달하는 것**이다. Camera 배치·방향·미리보기는 계속 개선한다. 송신 고도화와 성능 판정의 주 대상은 Binary PCD다.

**① 센서·카메라 추가 배치와 저장 위치 전용 패널 → ② 작업 흐름 중심 UI/UX 재구성 → ③ PCD 송신 사전 점검·예산 안내 → ④ 다중 센서 PCD 장기 검증·조작 성능 → ⑤ 플러그인 분리 전 의존성 정리**

| 순서 | 사용자가 할 수 있게 될 일 | 연결 항목 | 첫 완료 기준 |
|---:|---|---|---|
| 1 | `배치`에서 LiDAR/Camera를 추가하고 목록에서 선택·이동·저장 위치 복원 | UX-01, RT-02/06, UI-01 | Slab 검증맵 PIE에서 추가 장비가 즉시 목록에 보이고, 위치 저장/이동을 도움 없이 찾으며 기존 센서·Slab는 유지됨 |
| 2 | 배치 → 미리보기 → 시나리오 → PCD 전송 → 결과를 일관된 메뉴로 이용 | UI-03, UI-01 | 자주 쓰는 동작과 현재 선택·실행 상태가 보이며, 좁은 창에서도 주요 동작에 접근 가능 |
| 3 | PCD 시작 전에 대상·준비 상태·예상 전송량·처리량 부족과 해결 방법 확인 | RT-08 1차, RT-01/03, DATA-01 | PCD 중심으로 선택된 출력만 계산하고 미리보기 전용 Camera의 JPEG 송신량은 제외; 부족한 설정을 송신 전 안내 |
| 4 | 실제로 추가한 여러 LiDAR의 PCD를 반복 수집하면서 Camera 시점을 빠르게 조절 | RT-05/06, LIFE-01, QA-01 | 지원할 센서 수·미리보기 조건별 장기 송수신/입력 반응/자원 상한 증거 확보 |
| 5 | 다른 프로젝트로 옮길 수 있도록 센서와 Slab·프로젝트 설정의 결합 축소 | PL-01/02/03 → PL-04 | 중립 adapter와 설정 경계를 먼저 검증한 뒤 플러그인·간편 설치로 진행 |

- RT-01/02/03/04, DATA-01, LIFE-01, QA-01의 관련 회귀는 각 변경과 함께 수행한다. 기존 장기 검수 전체 종료를 배치 UX 개선 착수의 일괄 선행 조건으로 삼지 않는다.
- **현재 보류:** CSV/JSONL/LAZ 등 다른 파일 형식의 신규 개선, JPEG·호환 JSON 송신의 추가 최적화, Camera staging 재사용 최적화. 기존 API·기능·계약은 유지하며 공통 경로 변경으로 발생한 회귀만 확인한다. IO-01·RT-09는 이력과 진행률을 보존하고 보류 상태로 관리한다.
- 소비자 업무 ACK(RT-07), 운영 보안 확장(SEC-01), 실장비 보정(HW-01), 4+4 이상 확대는 실제 필요 조건에 따라 후속으로 착수한다. UE 로컬 학습 데이터 묶음 저장은 현재 범위에 없다.
- DTCore `b2504d1`을 유지하고 프로젝트 Actor·Subsystem·소유 UI에서 우선 해결한다. 공통 변경이 필요한 재현 사례는 별도 근거와 최소 변경안으로 다룬다.

### 확인된 접근성 문제와 설계 판단

- 위치 저장의 현재 진입점은 **`배치 → 저장 위치 · 선택 센서`**다. 이름 입력·현재 위치 저장·목록 이동·Undo/Redo를 먼저 배치하고 숫자 Transform은 접힌 영역에 둔다. 과거 Settings 하단 경로는 소유 native UI에서 배치 창 안내로 바꿨으며 Blueprint API는 유지한다.
- 이 native 북마크 영역은 우리 Workspace가 소유한 Settings에만 붙는다. 별도 Designer Root가 있는 사용자 WBP는 native 구성을 자동으로 받지 않으므로 모든 WBP에서 보인다고 가정하지 않는다.
- Slab 검증맵 생성 코드와 최근 실제 실행 기록에는 **LiDAR 1대와 Camera 1대**가 있다. 검증 Rig는 LiDAR를 시작하고 Camera는 정지시켜 등록한다. Camera가 정지 상태로 보이는 것과 Camera Actor가 없는 것은 구분한다. 개별 수정 맵의 실제 구성은 해당 맵에서 다시 확인해야 한다.
- **현재 PIE 배치 UI/API가 추가됐다.** 관찰 시점에 Camera/LiDAR를 생성·선택·미리보기하고, 측정 설정만 복제한다. 도구 장비만 삭제하며 수집 잠금과 wildcard 자동 편입을 차단한다. 레벨 Actor를 삭제하거나 맵을 저장하지 않는다.
- 별도 `센서 배치` 패널에서 기존 북마크 서비스와 전체 배치 서비스를 연결한다. Settings는 장비 규격·품질, Data는 PCD 전송·결과를 담당한다. 기존 위치 관련 Blueprint API와 사용자 Designer는 유지한다. 구현 완료와 두 해상도의 전체 직접 수용 검증 완료는 구분한다.

근거: `VirtualSensorSettingsPanelWidget.cpp`의 `RebuildWidget`/`BuildPoseBookmarkControls`, `SensorToolToolbarWidget.cpp`, `VirtualSensorLayoutSubsystem.cpp`, `SlabScenarioValidationRig.cpp`, `Saved/Reports/SensorPlacementWorkspace/FINAL_STATUS.md`. 검증맵은 PIE에서 사용했고 운영맵은 재저장하지 않았다.

### 2026-10-09 PCD 수요 정책 검증 — 현재

- 새 배치 LiDAR의 WhenRequested를 추가하고 기존 레벨/BP/구형layout은LegacyAlways로 유지했다. actual compatibility routing·BP/native 소비자·recording·수동 요청이 있을 때만 JSON을 생성한다. 정책 변경은 측정/조작/파일 snapshot을 유지하며, 수동 JSON과 전달 metadata도 같은 완료 프레임을 사용한다.
- 같은 표시형1920×1080 PIE의2+2 교차비교3회에서 새 정책 low59.17~59.80, 기존 전체JSON low29.74~36.02다. 새 정책의10회 반복11,995개request 및600초23,997개고정승인집합은내부/UI/외부대조누락0, JSON 작업0으로 통과했다. 반복최소low55.966,600초low59.58이다. PCD 점/주기/제출예산을 낮추지 않았다.
- 센서별 속도·기대 장비 수·실행 수·성능이 누락되거나 실패하면 수용 실패를 반환하도록 시험을 보강했다. 최종 전체211 Success 중 실제208·skip3·실패0, 별도입력RHI와WBP7개/marker987개를 확인했다. 숨긴/offscreen모드와기존Legacy전체JSON의성능제한은유지한다.
- 새 정책 UI의 직접 검수는Windows커서접근0x80070005로미완료다. 이전직접조작은과거이력으로만읽는다. 현재증거는 `Saved/Reports/PcdDemandPlacementAcceptance/FINAL_STATUS.md`다.

### 2026-10-09 배치 작업공간 수용 이력 — a78b443

- 전용 배치 패널·런타임 추가/복제/소유 삭제·북마크·전체 배치 명시적 복원·PCD 사전 점검을 구현했다. 자동화는 최신 전체209 Success 중 실제206·opt-in skip3·실패0, 배치 입력 RHI는 별도 통과다. WBP7개 메모리 컴파일 및 자산 검사 통과, 기존 자산 저장0이다.
- 실제1920×1080에서 새 배치 API의1+1/2+2 PCD-only 60초를 통과했다. 2+2 기본 제출예산2, 각PCD20Hz·Camera acquisition30Hz다. 같은 연결10분은 각PCD19.993Hz·평균59.98FPS·low58.27·p9516.67ms이며두Camera모두실행한최종30초×10회에서11,938개PCD의승인/제출/receipt/내부·UI/외부 request 대조를 통과했다. 정리 후body/events/active0, 복귀 추가제출0이다. 이 조건을4+4·60분·다른공장장면으로 확대하지 않는다.
- **반복 성능은 실패/후속:** 최종10회 반복의1% low38.42~40.25FPS는45FPS 목표에 미달한다. 초기첫Camera정지시험11,982건·low36.84~43.96은이력이다. 연속 성능 fixture의 미송신 호환JSON은stride32/max1024, 반복 신규LiDAR는기본stride1/무제한이므로 동일한 후처리 조건이 아니다. PCD 검출점은 줄이지 않았다. 후보 비용을 분리해 동일 조건으로 재시험해야 하며, 자동화 Success를 전체 성능 통과로 해석하지 않는다.
- 직접1280×720의추가·북마크·Undo/Redo·전체배치저장과 재시작후1920×1080의복원·ID/북마크연결·PCD/결과진입·150%창조작을 확인했다. 전체배율/DPI·첫사용자발견성·UI직접복제/삭제는 남는다. 마지막간결한PCD요약 재확인은Windows캡처0x80070057로미완료다. 자동화를 직접 마우스 검증으로 집계하지 않는다.
- 최초 부하환경에서기존/신규모두약47FPS·2+2 PCD약15.7Hz로실패한원문과사용자GPU작업중지후통과를보존한다. 특정앱원인이나무조건적무지연을주장하지 않는다. 세부증거·환경제한은 `Saved/Reports/SensorPlacementWorkspace/FINAL_STATUS.md`를 따른다.

## 읽는 방법

**출력 신뢰성 보완 — 2026-10-08:** 느린 LiDAR JSON이 새 측정마다 폐기되던 경로를 수정해 정상 완료를 전달하고 측정 당시 metadata를 보존한다. 50ms 입력에 200/700ms 생성 지연, 전역 4개 작업 상한과 다섯 번째 센서의 진행, Stop/삭제 후 callback 폐기를 집중 검증했다. PCD·Raw telemetry 중복 발행을 막고 선택된 호환 JSON만 drain에 포함한다. 실행 상세와 A/B에 센서·출력별 전달 수를 추가했으며 wildcard의 센서 간 FrameId 간섭·카운터 덮어쓰기·round-robin 순회 오류를 보완했다. `MeasuredOutputRateHz=0`만으로 PCD 실패를 판단하지 않는다. 전송 계약·센서 규격·DTCore·외부 Widget은 그대로이며 UE 학습 데이터 묶음 저장은 추가하지 않았다. 실제 다중 센서 수용 범위와 남은 검증은 RT-05 및 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`를 따른다.

최종 검증: D3D12 전체222 Success/실패0 중6개 opt-in skip(실제216), 별도30초 A/B 실제송수신, WBP6개 메모리컴파일·저장0. 직접1920×1080 Camera기즈모·첫Esc/정지유지와LiDAR정밀이동·10→20Hz복귀, 직접1280×720 PCD재실행·센서별600건A/B·패널resize/scroll을확인했다. PCD전용에서비선택Camera0Hz를측정률실패로오판하던경고를수정했다. 입력fixture는개인의열린Data패널에의존하지않게우리보조패널만숨긴다. 광범위한배치/모든폰트/RMB직접체크리스트와장기지원은별도다. RT-05·RT-06·UI-01 전체완료로확대하지않는다.

**배치·실행 결과 비교 — 2026-10-08:** 맵 값 복원의 실패 안내·Undo, SensorId 변경의 북마크 캐시 및 이전 저장 결과 적용을 보완했다. 새 위치 미리보기는 측정 시작 시각·자세·규격·generation과 실제 완료를 확인하며 Camera 확인에는 JPEG/readback을 추가하지 않는다. 실제 1920×1080 RHI에서 원규격 새 자세 프레임 복귀는 Camera 약31ms·LiDAR 약67ms, 입력→viewport GPU 완료 p95는 약34ms였다. 현재 PIE의 확정 결과 A/B에 배치·설정·기록 시점 Hz와 실행 전체 전송 카운터를 표시한다. 원문 hash·조건 차이·확인 불가를 구분하고 배치 품질 점수로 확대하지 않는다. 실제 30초 A/B의 PCD600/599개 및 모든 acquisition 행렬을 독립 대조했고 비교 UI 표시 전후 frame p95는16.67ms로 같았다. 최종 근거와 직접 조작 범위는 `Saved/Reports/SensorPlacementComparison/FINAL_STATUS.md`를 따른다. UX-01·RT-06의 생성/복제/삭제·다중 구성 완료율은 이번 기능으로 올리지 않는다.

**Slab 초기 배치 복귀 — 2026-10-07: 구현·관련 검증 완료.** 종료·송신 정리 후 진행 패널의 `초기 위치로 복귀` 또는 Actor API로 PIE 시작 당시의 월드 위치·회전에 즉시 복귀한다. 현재 형상/재질/스케일과 보관 목록은 유지하고 진행률·현재 프레임·세 차트·이전 3D 분석 라벨을 초기화한다. 실행·pause·파싱·재생/센서 준비·drain 중 거절, 실패/중단 후 복귀, cm/mm/m와 회전된 Track, 반복 호출·지연 적용 차단을 검사했다. 실제 Artemis 두 실행의 PCD 1,200건 제출/receipt/내부·외부 수신 일치와 복귀 추가 제출 0을 확인했다. 전체 D3D12 자동화 194 Success(실제 185·skip 9), Slab 집중 12 Success(실제 11·skip 1), WBP 6개 메모리 컴파일과 직접 1286×760/100% 조작을 수행했다. 숨김 중 차트 초기화와 동일 시나리오 새 RunUUID 재생도 직접 확인했다. DTCore·운영맵·기존 UI 저장값은 보존했다. `Saved/Reports/SlabResetPlacement/FINAL_STATUS.md`를 따른다. 이는 개별 복귀 기능의 완료이며 RT-05/QA-01 또는 기존 재생 p95 목표를 완료로 올리지 않는다.

**최소 DTCore 적용 검증 — 2026-10-06:** 별도 체크아웃에서 main 기반 최소 수정본을 연동했다. 1차 D3D12 전체161개 Success에는 조건부skip5개가 포함되며 실제156개다. 추가 공통 수명 검증은 13개 Success(실제 수행 12개·Slab Broker opt-in skip 1개), 실제 topic.scenario 수신은 별도 전체 시험에서 통과했다. PCD 전용 및 세 스트림의 1920×1080·10초 준비+60초 시험에서 승인 집합의 내부/외부 누락 0, invalid/gap/duplicate 0을 확인했다. PCD 20Hz, Camera 30Hz, 평균 60FPS·프레임 p95 16.67ms. 단순 누적 카운터가 아니라 고정 측정 request 집합을 대조했다. 최종 소스 전체 자동화는184개 Success(실제179·조건부skip5), 신규 실패0이다. 직접1286×760에서 네 글자 배율·패널·Slab 실행/재생·입력창 Esc 복구를 확인하고 최종 Shipping/Development 패키징을 통과했다. 재생 데이터1200건은 일치하지만 p95는기존24.908ms/최소수정24.925ms로 두 버전 모두20ms 목표가 실패다. Geometry 성능·10회 stream 반복은 미수행으로 유지한다. 기능 회귀와 성능을 포함한 최종 검수 완료를 구분한다. `Saved/Reports/DTCoreMinimalAcceptance/FINAL_STATUS.md`를 따른다. 상세 원인은 이 문서의 부록으로 추가하지 않는다.

**이전 이력 — 2026-09-29 런처 실행:** 격리된 관리형 런처 GUI에서328파일 설치/해시·UE 창·런처 종료 후 동일 UE 프로세스 유지·정상 종료를 확인했다. DTCore CustomLogs가 설치 경로에 쓰이는 점을 발견했으므로 회사 Program Files 권한/로그 위치는 미검증으로 남긴다. 실제 버전 변경/복구·Linux·운영망 검증은 이 결과로 완료되지 않는다.

**이전 이력 — 2026-09-29 패키징:** Runtime의 에디터 전용 테스트11파일을 WITH_EDITOR로 제한하고, 승인된 TestMap의 누락 모니터 참조를 복구했다. 당시 Windows Development 전체 Build/Cook/Stage/Archive와 관련 D3D12 자동화29건을 확인했다. 이후 현재 기준에서는 EnhancedInput 의존 선언, 프로젝트·격리 host의 Editor/Shipping 빌드와 Development 패키징을 검증했다. Linux·회사 운영망 및 실제 소비 프로젝트의 검증은 별도다.

**이전 검수 이력 — 2026-10-03:** `Saved/Reports/DTCoreFinalAcceptance/FINAL_STATUS.md`와 README의 최신 검수 요약을 우선한다. 현재 전체 자동화는190건 Success/실패0이며 실제 수행185건과 opt-in skip5건을 구분한다. 현재 source의10분 연속·10회 반복 송수신은 통과했지만 최종 성능 비교·일부 opt-in·전체 직접 조작 검수는 남아 있다.

- **정적 위험:** 실제 소스의 연결/조건을 확인했다. 이 문서만으로 런타임 재현 완료를 뜻하지 않는다.
- **검증 공백:** 구현이 있으나 해당 조건의 근거가 부족하다. 먼저 재현/계측하고 실패가 확인되면 고친다.
- **신규 기능:** 현재 없는 목표 기능이다. 기존 결함으로 취급하지 않는다.
- **P0:** 데이터 정확성·상태 복원·자원 한계의 중요도. **P1:** 기능·사용성의 주요 개선. **P2:** 선택적 제품화. 이 중요도와 위의 현재 착수 순서를 구분한다. 보류 항목의 과거 중요도는 착수 지시가 아니다.
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
- 크기가 다른23개 항목의 단순 평균으로 프로젝트 전체 진행률을 만들지 않는다. 새 UI/UX 재구성 항목 UI-03은 이번 일부 구현·검증에 맞춰40%다. 보류는 완료가 아니며 요약표와 상세 진행률은 일치해야 한다.
- RT-01·RT-04의 일반적인 장기·복합 장애 시험은 LIFE-01·RT-05와 연결한다. 해당 항목의 최종 증거 대조와 완료 범위가 닫히면100%로 갱신하며, 전체 R1 완료로 확대하지 않는다.

## 항목별 중요도와 진행률

실제 착수 순서는 문서 상단의①~⑤를 따른다. 아래 P0/P1/P2는 문제의 중요도이며, 보류·유지 항목이 현재 배치/UI 작업을 모두 앞선다는 뜻은 아니다.

| ID | 분류 | 우선 | 진행률 | 핵심 | 남은 핵심 조건 | 로드맵 |
|---|---|---:|---:|---|---|---|
| RT-02 | 조작 / 구현·자동검증 완료 | P0 | **70%** | 종료 상태 통일·원설정 복귀 | 해상도별 전체 직접 조작 체크리스트 | R1 |
| RT-01 | 성능·전송 / 구현·집중검증 완료 | P0 | **90%** | frame/byte reservation·과부하 진단 | 상한·재시도·종료 증거의 최종 연결 및 완료 범위 정리 | R1 |
| RT-04 | 데이터 / 유지·회귀 | P0 | **90%** | capture 직후 readback·immutable metadata | Camera 배치/미리보기 변경의 정합성 회귀; 송신 고도화 보류 | R1 |
| RT-03 | 진단 / 구현·자동검증 완료 | P1 | **70%** | 0Hz·신선도·공정성 판정 | 실제 정체·일시정지·복귀 화면 확인 | R1 |
| QA-01 | 품질 / 검증 공백 | P0 | **70%** | 고정 의존성·clean checkout·fixture 분리 | 실행 manifest·새 성능 계측·최종 비교 | R0 |
| RT-05 | 성능 / 기본2+2 검증 | P1 | **70%** | 새 배치 PCD 기본값·센서별 수용 | 대표 장면·4+4·숨김/운영 환경·60분 | R2 |
| RT-06 | 사용자 / 집중 구현·검증 | P1 | **40%** | 연속 배치·입력 지연 계측 | 2+2/4+4·직접 조작·지원 매트릭스 | R2 |
| RT-08 | 성능 / 일부 구현 | P1 | **40%** | PCD 사전 예산 안내 → 작업량 기반 접수 | 다양한 echo/backend 실측 대조·동적 접수/공정 배분 | R2 |
| RT-09 | 성능 / 현재 보류 | 보류 | **10%** | Camera staging 재사용 | Camera 배치/미리보기의 확인된 병목일 때 재검토 | R1/R2 |
| UX-01 | 사용자 / 핵심 구현 | P1 | **70%** | 센서 배치 전용 패널·추가·저장 위치 | 첫 사용자 수용·전체 직접 workflow·Editor 영구 설치 | R2 |
| DATA-01 | 사용자·신뢰성 / 구현·집중검증 | P1 | **90%** | 데이터 필수 실행 정책 | Engine/WSS 필수 모드·장시간/운영 환경 검수 | R3 |
| RT-07 | 전송 / 신규 기능 | P2 | **0%** | 소비자 업무 ACK | ACK 계약·멱등 처리·미확인 결과 관리 | R3 |
| LIFE-01 | 안정성 / 구현·집중검증 | P1 | **70%** | 실행별 회수·종료 snapshot·결과 보존 | 전체 fault matrix·자원 인과·장시간 범위 | R1/R3 |
| UI-01 | UI / 검증 공백 | P1 | **40%** | 해상도·DPI·소유권 | 1920×1080 직접 조작·DPI·외부 Widget 입력 | R2/R5 |
| UI-03 | UI/UX / 일부 구현 | P1 | **40%** | 배치부터 PCD 결과까지 작업 흐름 재구성 | 해상도·배율별 전체 직접 workflow와 첫 사용자 수용 | R2 |
| UI-02 | UI / 신규 기능 | P2 | **0%** | 3D 라벨 배치 | 겹침 제어·우선순위·거리 표시 정책 | R2 |
| IO-01 | 파일 / 현재 보류 | 보류 | **10%** | LAZ 외부 프로세스 제한 | PCD 외 형식의 개선 재개 요청 시 착수 | 후속 |
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

- **남은 조건:** 포커스 이탈·선택 변경·종료 등 해상도별 전체 직접 조작 목록을 완료한다. Camera 회전·LiDAR 숫자 변경의 최신 직접 증거는 아래 기록을 따른다.

2026-10-08 추가 직접 검수: 실제1920×1080 client에서 Camera 기즈모 이동·회전으로 월드 Transform 변경과 플레이어 view rotation 불변, 정지 Camera 첫 Esc·Settings 숨김 종료를 확인했다. LiDAR Z1800→1850 입력 후 첫 Esc로4Hz→20Hz 복귀와 PIE 유지도 확인했다. bounded capture1180×771 때문에 하단 일부와 전체 경계 checklist는 여전히 미완료다.

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

- **남은 조건:** 두 센서·두 해상도의 전체 표시/입력 경계 확인을 완료한다. 실제1280×720 LiDAR 정체·pause·재개는 아래 최신 증거로 확인했다.

2026-10-08 직접 정체 시험에서 Scheduler는0Hz였지만 소유 모니터가 마지막20Hz를 표시하는 불일치를 재현했다. `1d17016`은 소유 모니터만 현재 SensorId·종류에 맞는 Scheduler 상태를 표시한다. 실제1280×720의0Hz·정체 경고→20Hz 복귀, PIE pause의일시정지→재개 화면을 확인했다. 준비·경량 조작·정지·정상 복귀 formatter 자동시험도 통과했다. 측정 이력·주기·전송·미등록 Widget은 변경하지 않는다.

**수정 전 근거:** Scheduler min/max가 양수 Hz만 포함해 완전히 멈춘 센서를 숨길 수 있었다. 마지막 acquisition-rate 값도 자동 만료되지 않았다.

- 구현 `59d1027`: World-time 진행 시각, 시작 유예 `max(1초, 실제 주기×3)`, 오래된 양수 Hz→0Hz, 정체 ID 및 공정성 유효성 flag. pause는 측정 없이 진단만 갱신하고 조작용 임시 주기는 정격 공정성 통과에서 제외한다. 센서 주기는 변경하지 않았다.
- 검증: SensorPerformance 5/5, 보고서 판정식 정상/0Hz/센서 누락 3/3. 두 센서 중 하나/모두 정체, 복구·시작/재등록 유예·pause·저주기·조작·실패 이력·빈 그룹을 검사했다. 기존 전체 회귀에서 신규 실패 0.
- 주의: 기존 fairness 수치 0은 판정 불가 sentinel이다. 외부 Blueprint는 추가된 `bCameraFairnessEvaluable`/`bLidarFairnessEvaluable`을 확인해야 한다. Camera는 기존 acquisition 정의이며 GPU 픽셀 완료 검증은 RT-04다.
- 이전 시험 당시에는 직접 정체/일시정지 확인이 미수행이었으며, 이후2026-10-08 실제1280×720 LiDAR 확인을 추가했다. Camera·두 해상도의 전체 경계 조합은 남아 있다. 이전 근거: `Saved/Reports/SensorStability/rt03-focused`, `full-regression`, `report.ko.md`.

### RT-04 — Camera FrameId와 실제 픽셀 (구현·집중검증 완료, P0)

**진행률: 90%** · 구현: 완료 · 검증: 고유 token/CRC990개 완료 JPEG·metadata 및 음성 대조군 통과.

**현재 착수 상태:** Camera 배치·미리보기·공통 자원 변경에 필요한 정합성 회귀를 유지한다. JPEG 송신률/encode 고도화와 신규 파일 형식 개선을 이번 PCD 우선 작업의 선행 조건으로 두지 않는다.

- **남은 조건:** 현재 source의 증거를 Camera 수용 목록과 최종 대조해 항목을 닫는다. 전체 복합 수명 fault는 LIFE-01, 전후 성능·다중 구성은 RT-05/QA-01에 연결한다.

2026-10-03 검수: 완료 이벤트의 immutable JPEG/metadata를 bounded decoder에 전달해 고유 acquisition token+CRC16을 990개 모두 대조했다. 포화·encode 역순·pose/revision/Slab context·Stop/재시작·삭제와 이전 이미지 음성 대조군을 포함한다. 현재 source의 전체 자동화와 독립 host 빌드는 통과했다. LiDAR 측정 시작 UTC 보정 후 성능 비교에서 간헐적인 기준선 실패가 있어 재분석 중이며, 예전 12회 결과를 최종 source 통과로 사용하지 않는다.

**수정 전 근거:** 스트리밍 readback slot 부족 시 metadata를 보관하고 나중의 현재 RenderTarget을 읽을 수 있었다.

- 구현 `017d20f`: 자연 capture hook에서 pose/UTC/context를 고정하고 해당 capture 직후 readback copy를 예약한다. coalesced frame/slot 부족은 명시적 파생 실패이며 나중 픽셀을 옛 ID에 붙이지 않는다. Actor의 프레임 UTC도 encode 완료 시각이 아니라 acquisition 시각을 사용한다.
- 이전 안정화 단계 기록: CameraAcquisitionSnapshot 및 D3D12의 red/blue marker·encode 지연·포화 복귀를 JPEG25개 폴링으로 확인했다. 이 제한된 시험은 위의 최종 고유 token/CRC990개 완료 이벤트 검증과 구분한다. 초기 harness의 ShowOnly/cold shader 문제와 최종 통과 로그를 모두 보존한다.
- 증거: `Saved/Reports/DTCoreSync/camera-unit`, `camera-rhi4`, marker JPEG. 같은 장면의 출력 성능은 10+60초 전후 비교했다. 모든 이동/context/삭제/설정 변경 조합의 stress 인증은 후속이다.

### RT-05 — 다중 센서 실측 지원 범위 (검증 공백, P1)

**진행률: 70%** · 구현: 새 배치 PCD 중심 JSON 수요 정책·센서별 수용 gate · 검증: 기본생성1+1/2+2 PCD60초교차비교, 2+2 PCD10분·30초×10회 정격/정합성/회수 통과. 대표장면·4+4·숨김/운영환경·60분은 남는다.

**현재 검증 순서:** 새 배치 장비의 WhenRequested와 LegacyAlways를 같은 전체 JSON 설정·표시된 PIE·맵/pose에서 비교한다. Camera는 배치·미리보기 부하를 포함하고 이미지 송신은 기본 꺼짐이다. 센서별 실제 속도·request 집합·성능·회수를 모두 판정하며, 특정 센서 누락이나 성능 미달은 합산 수가 충분해도 실패다. 4대는 수요·환경을 정한 후속이다.

- **남은 조건:** 대표 장면·4+4·숨김/운영 환경·60분 지원 매트릭스다. 기본생성WhenRequested의표시형2+2 반복low미달은이번조건에서해결됐으며LegacyAlways전체JSON과offscreen의실패를지우지않는다. 전체출력이나다른PC의무조건적지원으로확대하지않는다.
- 2026-10-08 추가 범위: 실제1920×1080 D3D12·10초 준비+60초, Camera1280×720/30Hz·ML-X Native576×56/20Hz를 유지했다. 2+2 PCD 전용은 평균59.85FPS·1% low51.8·p9516.69ms, 각 PCD 약19.9Hz와 선택하지 않은 Topic0건이다. 2+2 전체 출력은 기존 제출 예산2건/프레임에서 Camera 큐 과부하로 실패했다. 명시적4건/프레임에서는 각 Camera29.883Hz·각 LiDAR/PCD19.917Hz, 평균59.76FPS·1% low51.81·p9517.04ms, 측정 구간 승인 request의 receipt·내부·UI·외부 누락0으로 통과했다. **기본2건 설정의2+2 전체 출력 정격 지원은 선언하지 않는다.** 운영맵·Actor 기본값을 자동 변경하지 않았다.
- 2026-10-08 당시1% low는 가장 느린1% 엔진 프레임 평균의 역수다. 이전p99역수와직접비교하지않는다. 당시보고서는 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`이고진행률40%였다. 현재70%의근거는이번기본생성PCD교차/반복/600초검증이며4+4/60분은남는다.
- 이전 장기 근거: Runtime `2eb30f0`의 실제1920×1080 세 스트림10분은 Camera29.998Hz·LiDAR/PCD20Hz, 승인 요청/receipt/내부·외부 수신 대조 오류0으로 통과했다.10회 반복과 일부1280×720 직접 조작도 확인했다. 최신2+2 장기 지원을 완료한 결과는 아니다.

- 이전 근거는 ML-X Native **한 대 PCD-only, 753×403, 30초×2**다. 1,199건 송수신 일치, 후반1% low45.019FPS. 이후 실제1280×720/1920×1080 및1+1 정격 시험은 README의 source별 기록을 따른다. 옛2+2/4+4 저출력률 시험을 현재 정격 지원 근거로 쓰지 않는다.
- 이전 이력 — 2026-10-02: 실제1274×680 PCD-only/1+1 시험과 재생 p9525.07ms 미달을 기록했다. 이후 재생 시험의 scoped background throttle 보정으로 통과한 기록과 현재 source에서 갱신해야 할 opt-in을 README에서 구분한다. 과거 해상도·지연값을 최신 판정으로 사용하지 않는다.
- 1+1 → 2+2 → 4+4를 동일 SHA/고정 fixture에서 검증한다. 8+8은 탐색용 best effort이며 자동 지원으로 분류하지 않는다.
- 각 단계에서 실제 client 해상도, profile/backend/echo, preview/출력 on/off, FPS·1% low·p95/p99, 센서별 acquisition/encode/submit/receipt/내외부 receive, queue/bytes/memory를 기록한다.
- Slab 검증맵의 현재 `one_percent_low`는 가장 느린1% 프레임 시간의 평균으로 계산하며 원시 `engine_frame_ms`를 보존한다. 기존 scheduler/UI의 p99 역수 기반 최근 통계와 구분한다. engine delta·실제 wall pacing·trace의 대기를 합쳐 하나의 FPS로 표시하지 않는다.
- 완료: 60초 회귀를 통과한 구성만 10분 안정성, release 후보는 60분 soak로 확장한다. 요청값을 낮춰놓고 원래 FullSpec 통과로 발표하지 않는다. 목표 수치는 ROADMAP의 제안 gate를 승인·고정한 뒤 적용한다.

### RT-09 — Camera staging texture 재사용 및 지연 원인 분리 (구현 공백, P1)

**진행률: 10%** · 구현: Camera 재사용 미착수 · 검증: 반복 생성 경로와 trace 한계 조사 완료.

**현재 보류:** JPEG 출력 전용 최적화는 진행하지 않는다. 다중 Camera 배치·미리보기 또는 PCD 공통 자원을 실제로 방해하는 원인이 확인될 때 범위를 정해 재검토한다.

- 2026-10-08 자원 진단: readback 생성 횟수, 슬롯·render consume·callback·encode·완료 순서 버퍼와 알려진 CPU 용량을 분리했다. 실제 지연 시험에서 확인한 누적은 staging 누수로 단정하지 않고 완료 순서 창8개로 제한했다(`89bb327`). 픽셀/metadata987개와 포화·복귀·Stop/삭제를 검사했다. readback 재사용은 구현하지 않았으며, 정상 반복의 메모리 증가 인과는 별도 확인한다.

- **남은 조건:** Camera 자원별 지연 인과를 확인하고 성능 실패의 관련 원인으로 확인될 때 재사용 구현·정합성·전후 시험을 진행한다. 현재 최종 검수 범위에서는 조건부 수정이다.

- 2026-10-03 trace에서 game-thread render 대기와 D3D12 texture 생성 지연을 관측했다. LiDAR의 매 프레임 readback 생성은 `0c502af/e6afe4f`에서 재사용·현재 제출 fence 검사로 수정했다. 개별 trace 자원 이름이 없어 모든 생성 지연이 LiDAR라고 단정하지 않는다.
- Camera `QueueScheduledGpuReadback`은 현재도 슬롯을3개로 제한하지만 프레임마다 `FRHIGPUTextureReadback`을 생성하고 완료 후 해제한다. **슬롯 수 제한과 staging GPU 자원 재사용은 다른 기능**이다. Camera 재사용 최적화는 이번에 구현하지 않았다.
- 후속 변경은 동일 크기/format 슬롯 자원 보존, 새로운 copy의 fence 확인, resize/Stop/삭제 시 render-thread 해제, 고유 token/CRC 회귀로 구성한다. 이미지·metadata 정합성이나 출력 주기를 희생하지 않는다.
- 완료: 동일 조건의 전후3회 trace에서 해당 readback allocation 감소를 확인하고, 실제 제출/receipt/수신Hz·지연·FPS·990개 identity·장시간 반환을 재검증한다. 외부 CPU 부하와 엔진 자원 지연은 인과 증거 없이 하나의 원인으로 합치지 않는다.

### RT-06 — 조작 중 입력 지연 (검증 공백, P1)

**진행률: 40%** · 구현: 소유 PIE의 연속/정밀 이동·RMB 관찰자 분리·선택 센서 15/10Hz preview · 검증: 1+1 실제 Slate/PIE와 프레임률 독립성 집중 통과.

후속 새 자세 미리보기 시험: Camera PreviewOnly의 render 완료 fence와 LiDAR 완료 snapshot을 사용해 이전 자세 프레임을 배제했다. 실제 두 해상도의 원규격 복귀는 약31/67ms, viewport GPU end-frame p95는 약34~43ms로 1+1 목표를 통과했다. 전체 회귀210개 수행 통과·skip6·실패0이며, 이 버전의 직접 마우스는0x80070005로 환경상 미완료다. 다중 구성 및 물리 RMB/전체 drag 검수는 계속 남는다.

- **남은 조건:** 2+2/4+4 input-to-visible 계측과30초 연속 조작·최종 프레임 복귀 시험을 수행한다.

- 기본 Camera 640×360/5Hz, LiDAR 120×24/4Hz의 경량 조작 API는 이미 있다. 이는 interaction request 기본값이지 API의 고정 최대치가 아니다. 새로 만드는 작업이 아니라 다중 실행 경계 검증이다.
- 2026-10-08 소유 배치 도구는 기본 Camera 640×360/15Hz·LiDAR 120×24/10Hz를 선택 실행 센서에만 사용한다. 기존 request 기본값·미등록 Widget·정지 센서 정책은 유지했다. 1280×720 및 1920×1080 실제 Slate/PIE의 입력→Transform 1프레임·viewport end-frame GPU p95 약 31~34ms(센서별 각 32개)를 확인했다. 30/60/120FPS 이동·회전과 hitch/대각선/Shift/Ctrl, 소유 입력/외부 입력창/정밀 반복 차단을 검사했다. 두 해상도 직접 조작에서 정밀 이동·Undo/Redo·Esc와 북마크 재실행 보존을 확인했으나, 물리 RMB 연속 탐색·전체 drag·1.5초 최종 프레임 복귀 수용시험은 남아 있다. 물리 입력·실제 모니터 및 다중 정격 지원 수치로 확대하지 않는다.
- 입력 timestamp→Transform 적용→화면 표시를 측정한다. 평균 FPS만으로 조작감을 평가하지 않는다. 기존 world/local, Shift/Ctrl, 최종 품질 복원 유지.
- 완료: 2+2/4+4에서 30초 연속 drag/rotate, 키보드, 선택 전환, Esc/hide/end를 실행. 제안 목표는 input-to-visible p95 ≤50ms, 종료 후 coherent 최종 프레임 ≤1.5초. 불가능한 구성은 지원 밖/제한 원인을 공개한다.

### RT-08 — 작업량 기반 admission (신규 기능, P1)

**진행률: 40%** · 구현: 읽기 전용 PCD 사전 점검·예산 안내 · 검증: 40/140건/s·wildcard 중복·미리보기 Camera 제외·원규격/부작용 불변 집중 시험. 동적 작업량 기반 admission은 미착수다.

**1차 범위 — PCD 사전 점검:** 선택된 LiDAR 수·요청 주기·실제 body 크기 추정·송신 예산·연결 준비를 시작 전에 보여준다. 미리보기 전용 Camera를 JPEG 송신량에 포함하지 않는다. 현재2+2 전체 출력의140건/s과 기본 제출 예산2건×60FPS=120건/s 사례는 원인 설명용이며 PCD-only의40건/s도 부족하다고 일반화하지 않는다. 관측 FPS에 따른 계산은 추정임을 표시하고 실제 수신률로 확인한다. 한도 부족 시 이유·대상·명시적 설정 방법을 안내하며 자동 주기 저하·숨은 설정 변경을 하지 않는다. 전체 동적 작업 배분 정책은 이후 구현한다.

- **남은 조건:** 작업량·EWMA·자원/대역폭 예산 기반 접수와 공정 배분을 구현하고 혼합 부하를 검증한다.

- 현재 FPS tier는 센서 **개수** 중심이다. 같은 4대라도 해상도·rays·echo·GPU/CPU·JPEG·PCD·semantic proxy 비용은 다르다.
- pixels/s, rays/s, 측정/파생 처리 EWMA, GPU/readback/encode 슬롯, queued bytes·네트워크 예산으로 사전 capacity 표시와 공정 배분을 보완한다.
- Requested/Admitted/Acquired/Published Hz를 분리한다. preview 품질과 물리 측정 규격도 분리하며 사용자 모르게 resolution/주기를 낮추지 않는다.
- 완료: 동일 센서 수의 경량/고부하 혼합에서 starvation 0, bounded 자원, 경고와 실측 결과 일치. 추가 센서가 기존 센서의 완료율을 독점하지 않음.

## 2. 사용자 workflow·데이터 신뢰성

### UX-01 — 센서 추가·배치와 저장 위치 전용 패널 (현재 최우선, P1)

**진행률: 70%** · 구현: 개별 북마크·Undo/Redo에 장비 수명·전체 배치 저장/복원·전용 패널 추가 · 검증: lifecycle/저장/보호 자동화·WBP7개, 직접 두 해상도의 핵심 저장/복원 및2+2 PCD10분/10회. 첫 사용자·전체직접목록은 남는다.

- **남은 조건:** 첫 사용자 발견성, UI 직접 복제/삭제·모든 배율과 입력 경계, Editor 영구 설치를 검증한다. 현재 PIE의 추가→선택→북마크/전체 배치 복원→PCD/결과 연결은 구현·핵심 검증됐다.

- 현재 native/WBP `센서 배치` 패널이 장비 목록과 저장 위치를 함께 제공하고 툴바에서 한 번에 열린다. 기존 Settings의 위치 관련 API는 같은 구현을 재사용한다. 최초 사용자 발견성 시험은 아직 별도다.
- 패널의 기본 화면: `LiDAR 추가`·`Camera 추가`, 장비 목록(종류·이름·SensorId·측정/정지·수집 잠금), 선택 장비의 위치·회전/조작, `현재 위치 저장`과 저장 위치 목록이다. 이름 입력·저장 상태·`이동` 버튼을 고정된 위치에 두고 속도·단위·고급 설정 뒤에 숨기지 않는다. 장비 규격은 Settings로 연결한다.
- 기존 Actor 클래스/API·Coordinator 등록·공유 서비스 연결을 재사용하는 생성 서비스를 프로젝트에 추가한다. UI가 Scan/Capture 필드를 직접 수정하지 않는다. 고유 ID 생성과 중복 검사를 하고, 생성 위치·방향은 명시적으로 선택·미리 확인하게 한다. 배치/미리보기 상태와 PCD 수집 상태를 구분하며 추가만으로 Topic 송신을 시작하지 않는다.
- 1차는 PIE/runtime 추가·복제·우리 도구가 생성한 장비 삭제다. 기존 레벨 Actor는 출처를 표시하고 삭제 대상으로 자동 편입하지 않는다. 대상 센서의 수집/준비/drain/미완료 출력 중에는 추가된 장비라도 이동·삭제를 거절하고 이유를 표시한다. 새 장비가 진행 중 세션의 고정 대상에 자동 합류하지 않게 한다.
- 저장은 두 종류를 명확히 구분한다. **저장 위치**는 선택 센서 하나의 pose(기존20개)다. **전체 배치 저장**은 별도 버전 JSON에 장비 종류·stable ID·프로필·pose·측정 설정·미리보기 상태를 보관하고 명시적으로 복원한다. 송신 활성 상태·인증·프레임 본문은 제외한다. 기존 pose 파일만으로 새 Actor가 생성되지는 않는다. 전체 배치는 충돌/잠금/맵/정확한 대응을 먼저 검사하고 변경 미리보기 이후 적용하며 운영맵을 묵시적으로 수정하지 않는다.
- 기존 북마크·Undo/Redo 서비스와 저장 파일을 재사용한다. 새 패널은 별도 소유 Role을 추가 등록하고 기존 enum 값·저장 키·Blueprint 전달 API를 보존한다. Settings에는 배치 패널로 이동하는 안내를 두어 중복 편집 화면을 줄인다. 사용자 WBP Designer와 미등록 Widget을 일괄 변경하지 않는다.
- 첫 사용자 수용시험: 기본 Slab 검증맵에서 도움 없이 LiDAR/Camera 추가, 목록/3D 대상 일치, pose 저장·즉시 이동·Undo, 잠금 사유 확인을 수행한다. 재실행 후 북마크 조회는 우선 stable ID가 유지되는 기존 레벨 센서로 검사한다. 새 런타임 장비의 재생성/북마크 연결은 전체 배치 저장과 함께 명시적 ID 복원·중복 검사로 검증하며 이름으로 추측하지 않는다. 운영맵에 검증 Rig를 옮기거나 기존 맵 배치를 자동 덮어쓰지 않는다.

- PIE runtime 생성·복제·소유 삭제·개별 위치·전체 배치 저장/복원은 연결했다. Editor 영구 설치 도구나 모든 장비 수의 정격 지원으로 확대하지 않는다.
- 2026-10-08 개별 센서의 이름 있는 20개 위치·회전과 현재 PIE의 50개 gesture 이력을 추가했다. 별도 `Saved/SensorPlacements` 파일이며 목록 수동 이동·쓰기 실패 보존·ID/맵 검증과 잠금을 적용한다. 기존 설치 Actor를 새로 만들거나 맵을 저장하지 않는다. 같은 30초 시나리오의 배치 A/B는 각 600/599개 외부 PCD의 좌표 행렬·RunUUID별 승인/receipt/수신 ID를 대조했고 시작 snapshot과 북마크 정보를 실행 요약에 보존한다. 전체 배치 세트·생성/복제/삭제·Editor Undo는 남는다.
- Editor 설치와 PIE 임시 설치를 명확히 구분한다. 고유 ID 생성/중복 검사, 이름/종류별 목록, transform 입력·snap·복제, 배치 preset 저장/불러오기, Editor Undo/Redo를 제공한다.
- Actor API와 setup descriptor를 사용하고 측정 Component 필드 직접 변경은 금지한다. Editor 배치의 영구 저장은 명시적 동작으로만 수행한다.
- 완료: 10개 생성·복제·이름 변경·삭제·Undo/Redo·재로드, 소유하지 않은 Actor 불변, duplicate ID 0, 송신/비동기 작업 중 삭제 안전. 센서 없는 레벨도 빈 상태 안내로 시작.

### DATA-01 — 관찰 가능한 실행과 데이터 필수 실행 (구현·집중검증, P1)

**진행률: 90%** · 구현: 선택형 준비 gate·오류 중단·RunUUID별 receipt 집계/요약 저장 · 검증: 실제 PCD/세 출력, TCP 장애6종, 최종 자동화198 Success(실제188·skip10), WBP6개 및 직접1286×760.

- **남은 조건:** Engine STOMP/WSS 필수 모드의 별도 실사용 시험, 회사 운영 환경과 장시간 수집 범위를 검수한다. Broker 수락을 외부 소비자의 저장/처리 완료로 세지 않는다. `Saved/Reports/ScenarioDataReliability/FINAL_STATUS.md`의 수행/미수행을 따른다.

- 현재 live 설정 부족은 관찰 fallback으로 이동한다. 좋은 개발 편의지만 학습 데이터 수집 작업에서는 “움직였으니 데이터도 수집됐다”는 오해를 막아야 한다.
- 기본 fallback을 유지하고 opt-in **데이터 필수 실행**을 구현했다. preflight→실제 STOMP 연결 준비→실행→최대10초 drain→실행별 결과를 구분한다. worker 생성만으로 준비 완료라 하지 않는다.
- 필수 실행의 연결 끊김·최종 receipt 실패·과부하·대상 삭제/정체에서 움직임과 신규 접수를 중단한다. 재실행은 새 RunUUID이며 자동 fallback/중간 재개는 하지 않는다.
- 실행 요약은 원본 시나리오/실행 UUID·선택 센서·설정 revision·승인/제출/receipt/수신 검증·실패/미완료를 기록한다. 메모리20개·소유 JSON100개, 실행 중 식별자65,536건 상한이다. 준비 실패·취소·중복/재시도 집계·보관·초기 복귀 후 결과 보존을 검사한다.

### RT-07 — 소비자 업무 ACK (신규 기능, P2)

**진행률: 0%** · 구현: 업무 ACK 계약 미착수 · 검증: 전용 수용시험 없음.

- **남은 조건:** ConsumerProcessed·멱등 처리·재전송·미확인 결과와 보존 정책을 버전 계약으로 정의하고 구현한다.

- 현재 drain의 Raw 대기는 Broker receipt 중심이다. 자체 parser의 성공도 최종 AI/업무 소비자의 저장·처리 완료는 아니다.
- 필요 시 `(RunUUID, Segment, SensorId, SensorFrameId, checksum)` 기반 application ACK/누락 대조/멱등 재전송을 별도 버전 계약으로 추가한다. 기존 PCD body를 묵시적으로 변경하지 않는다.
- 완료: 소비자가 중단되어도 BrokerAccepted와 ConsumerProcessed를 혼동하지 않음. 중복 메시지 재처리 방지, 종료 결과에 미확인 목록, timeout/재시도/보존 정책이 명시됨.

### LIFE-01 — 중도 종료와 재연결 (검증 공백, P1)

**진행률: 70%** · 구현: 같은 worker의 실행 소유권/회수·종료 전 식별자 대조·확정 결과/저장 재시도 · 검증: 집중 자동화와 동일 연결의 30초×10회 PCD/세 출력 기능·요청 ID 대조 통과.

- `be2b418`은 시험용 실제 수신 ID 집합을 실행별 대조·저장 후 회수하고, 기록 상한 초과를 명시적 실패로 처리한다. 요청 시 자원별 snapshot과 종료 후 weak owner·Camera callback/encode 회수를 검사한다. `89bb327`에서 지연된 앞 JPEG 때문에 완료물이 계속 쌓이는 경로를 실제 재현하여8개 접수 창으로 제한하고 실패한 앞 항목의 후속 결과를 해제했다. `cd28373`의 SceneCapture 격리도 실제 Camera/GPU 깊이 양·음성 대조군으로 검사했다. 전체 프로세스 메모리 누수 검수 완료로 확대하지 않는다.
- 최신30회 근거: PCD17,989/세 출력62,979 request ID집합의 제출·receipt·내외부 수신 대조, 동일 worker/connection,20/21/25/30회 결과20개·body/events 반환 및 Stop/PIE weak owner12개를 확인했다. 이번 최소FPS59.82/59.94,low47.80/55.32,p95≤16.67ms, 변경 전후 각3회5% 이내. 실행1→30 private memory 증가15/44MiB의 전체 owner는 미확정이므로 진행률70%와 남은 인과/fault 범위를 유지한다. 증거는 `Saved/Reports/SensorResourceLifecycle/FINAL_STATUS.md`.

- **남은 조건:** 전체 파이프라인 fault matrix와 thread/handle·잔존 객체·메모리 복귀, 장시간/Engine STOMP/WSS를 조사한다. 과거 private bytes 약32MB 증가를 이번 실행 상태 회수만으로 해결했다고 표시하지 않는다. 반복 후반 성능과 실제 마우스 검수는 `Saved/Reports/SlabRepeatRunStability/FINAL_STATUS.md`의 최종 판정을 따른다.
- `b9b4369/73c71e1/a0f24ba`: 실행 상세 진단과 수신 연속성 상태를 회수하고 종료 집계20개를 유지한다. 늦은 프레임을 막는 최소 UUID/generation 기록은 별도다. PIE 즉시 종료도 정리 전 단계별 대기와 고유 미완료 수를 보존한다. 결과 목록 선택·요약 저장 재시도는 현재 PIE20개에 한정한다.
- 실제 1920×1080에서 변경 전후 PCD/세 스트림 각3회는 FPS·p95 변화5% 이내였지만, 동일 연결10회 반복 후반에 별도의 p95/1% low 실패를 재현했다. trace에서 CPU 미리보기 ISM 갱신 비용 증가를 확인해 `21f7b32`에서 진단 인스턴스의 collision/physics/navigation을 제외했다. 점 수·측정 주기는 유지하며 전후 반복 결과를 별도로 판정한다.
- 최종 physics 제외 소스의1920×1080 동일 연결30초×10회는 PCD/세 출력 모두 성능·identity·회수 통과. 외부 PCD6,000, 세 출력 Camera8,999/LiDAR6,000/PCD6,000; 종료 bytes/sequence0. 최종 전후3회 비교도5% 이내. fault6종×2와 실제198개 자동화 수행, WBP6개 및 직접1174×683 결과 조회/재시도/중단/복귀를 확인했다. Source·보고서별 원본 실패와 opt-in skip5를 보존한다. 모든 pipeline·회사 환경·장시간 또는 목표 두 크기의 전체 mouse gate 완료로 확대하지 않는다.

- weak pointer/revision·세션 drain·file writer 취소는 이미 있으므로 전체 재작성하지 않는다.
- acquisition/readback/encode/직렬화/socket/receipt/파일 저장 각 단계에서 Actor 삭제, Stop, 품질 변경, PIE 종료, world 이동, 동일 시나리오 재실행을 주입한다.
- 완료: 늦은 결과 적용·double complete·old generation 수신 0, 종료 시간 상한, thread/handle/메모리 복귀, 다른 owner 연결 유지. 각 경로 테스트가 어느 수명 경계를 증명하는지 기록한다.

### IO-01 — LAZ 프로세스 제한 (알려진 제한, P1)

**진행률: 10%** · 구현: timeout/cancel 보강 미착수 · 검증: 외부 실행 공백 조사 완료.

**현재 보류:** PCD 중심 정책에 따라 LAZ 및 다른 확장자 개선을 현재 착수 목록에서 제외한다. 아래 제한과 이력은 보존하며 기존 LAZ가 개선 완료된 것으로 표시하지 않는다.

- **남은 조건:** timeout·cancel·hang 회수·부분 파일 정리와 fake compressor 시험을 구현한다.

- 외부 compressor 미설정 시 성공을 가장하지 않으며 실제 timeout/cancel 및 장시간 hang 정리는 아직 보완 대상이다.
- 자신이 시작한 자식 프로세스만 추적해 timeout/cancel, 제한된 stdout/stderr, 임시 파일 cleanup, 오류 코드를 제공한다. 무관한 프로세스를 이름으로 종료하지 않는다.
- 완료: fake compressor 성공/오류/hang/취소 시험, worker slot 복귀, 부분 파일을 완료로 표시하지 않음. CSV/PCD/JPEG 작업이 함께 굶지 않음.

## 3. UI·가독성

### UI-03 — 작업 흐름 중심의 소유 UI/UX 재구성 (신규 기능, P1)

**진행률: 40%** · 구현: 배치/미리보기/시나리오/PCD 전송/결과 진입점과 전용 배치 패널 · 검증: 소유권·선택 동기화·저장 호환 자동화. 전체 직접 workflow와 첫 사용자 수용은 남는다.

- **남은 조건:** 새 배치 진입점과 기존 소유6개 패널을 `배치 → 미리보기 → 시나리오 → PCD 전송 → 결과` 흐름으로 정돈한다. UI-01의 DPI/격리 검증과 구분해 관리한다.
- 툴바에 `배치` 직접 진입점을 제공하고 현재 선택 장비를 분명히 표시한다. 목록 선택·3D 강조·설정·미리보기의 대상이 항상 일치해야 한다. 장비가 없거나 Camera가 정지됐을 때 추가/미리보기 시작 방법을 안내한다.
- 센서 배치 패널은 UX-01, Settings는 장비 규격, Monitor는 큰 미리보기, Data는 PCD 준비/송신/결과, Slab 패널은 시나리오·차트·진행을 담당한다. 반복된 조작과 전문 진단은 정리하고 주 동작·선택 대상·잠금 이유는 즉시 보이게 한다.
- `위치 저장`, `전체 배치 저장`, `PCD 파일 저장`, `PCD 송신`, `창 배치 저장`을 같은 '저장'으로 섞지 않는다. 저장 요청 접수/완료/실패와 Broker 수락/실제 수신을 분명히 표시한다. CSV/LAZ/JPEG 등 기존 선택 기능은 고급/호환 영역으로 두고 신규 포맷 개선은 추가하지 않는다.
- 기존 다크 스타일을 바탕으로 글자 역할·대비·여백·버튼 위치·상태 표시를 통일한다. 기본 동작은 스크롤 깊은 곳에 숨기지 않고 상세 내용만 접기/스크롤을 사용한다. 위험 조작은 인접한 실행 버튼과 구분한다. 키보드 focus·한글 줄바꿈·큰 글꼴·빈 목록·실패 복구도 설계에 포함한다.
- 소유 등록 인스턴스에만 적용한다. 새 배치 패널 추가 시 전용 Role/저장/입력/폰트 등록을 검증한다. 기존 소유6개 패널은 화면을 개편하되 저장값·동작·API 호환을 유지하고, 동료·미등록 Widget의 배치·스타일·저장값은 보존한다. UI 변경으로 측정·전송을 추가 실행하지 않는다.
- 완료: 1280×720·1920×1080에서 처음 사용하는 사람이 추가→배치→위치 저장/불러오기→PCD 실행→결과 확인을 수행한다. 도움 없이 찾을 수 있는지, 조작 횟수·되돌아가기·오류 복구를 기록한다. 숨김/resize/접기/네 글자 배율과 미등록 Widget 불변도 확인한다.

### UI-01 — 실제 해상도/DPI/소유권 (검증 공백, P1)

**진행률: 40%** · 구현: 소유 패널 격리·배율·창 조작 존재 · 검증: 일부 실제1280×720 및 Main/외부 Widget 자동시험 통과.

- **남은 조건:** 1920×1080 전체 직접 조작·DPI 대표 조합·외부 Widget 입력·전체 checklist를 완료한다.
- 이전 직접 근거: 실제client1280×720,150% 제목/버튼 가독성, 숫자 Transform·Settings 숨김 복귀를 확인했다. 당시 일반 PIE의 최대client는1920×998였다. 이후의1920×1080 직접 검수와 아래 최신 기록을 함께 읽는다.
- 2026-10-08 직접 client1280×720에 이어 세션 복구 후1920×1080에서 Camera gizmo 이동·회전·정지 첫Esc·Settings 숨김,LiDAR 숫자Z1800→1850·4Hz→20Hz 복귀,데이터 패널 열기·85/100/125/150%·resize/scroll/collapse를 확인했다.125/150%에서 좁은 Settings의 SensorId가 잘렸지만 resize 후 전체가 표시됐다.1280×720의 정체/pause/복귀는 수정 후 실제 확인했다. bounded capture1180×771로 전체1920 checklist·직접 준비 취소·Main/외부 Widget 입력 전수는 남긴다. 앞선0x80070005 실패는 원본 증거로 보존한다.
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
- 현재 상태: 프로젝트 `2ec67a2`는 최소 수정 DTCore `b2504d1`을 고정한다. `067195b`는 이전 동기화 안정화 이력이다. 현재 프로젝트와 별도 plugin 복사본의 최소 host 검증은 실제 다른 소비 프로젝트의 호환성을 대신 보장하지 않는다. 현재 작업에서는 DTCore를 수정하지 않는다.
- 그럼에도 개인 Game.ini/SaveGame/수정 운영맵에 의존하지 않는 clean checkout build/asset load 기준선이 필요하다.
- 운영맵 Z=150cm fixture를 통과시키기 위해 사용자 맵을 수정하지 않는다. 커밋된 스냅샷 검사 또는 임시 검증 scene으로 fixture와 실사용 편집을 분리한다.
- 보고서에 commit/submodule SHA, Engine/toolchain/GPU/driver, 실제 viewport/DPI, 설정 hash, 실행한 tests/skip, 원본 count를 남긴다. 문서 요약과 원본 JSON을 자동 대조한다.
- 완료: clean setup 명령 한 묶음으로 빌드·WBP·계약·핵심 RHI 재현, 운영 asset hash 불변, 실패·skip이 집계에서 숨겨지지 않음.

## 다음 착수 순서

1. **UX-01:** 구현된 배치 패널의 첫 사용자 수용·UI 직접 복제/삭제·남은 배율/입력 경계를 닫는다. Editor 영구 설치는 별도 요구로 확정한다.
2. **UI-03:** 기존 작업 흐름을 유지하며 해상도/DPI·모든 소유 패널의 주요 동작과 설명을 검증한다. 마지막 PCD 요약 화면 확인도 포함한다.
3. **RT-08 + RT-01/03 + DATA-01:** 구현된 PCD 사전 점검을 다양한 echo/backend 실측과 대조한다. 동적 작업량 admission·공정 배분은 확인된 병목을 기준으로 별도 설계한다.
4. **RT-05/06 + LIFE-01 + QA-01:** 통과한2+2 PCD10분/10회 조건을 기준으로 대표 장면·부하 환경·조작 응답을 확장한다. 4+4/60분은 별도 지원 gate다.
5. **PL-01/02/03 → PL-04:** 위 사용 흐름과 데이터 계약을 유지하면서 프로젝트 의존성·설정·장비 등록을 정리한 뒤 플러그인/간편 설치를 진행한다.

PCD 외 포맷의 신규 개선(IO-01 포함)은 보류다. 관련 정확성·정합성·종료 회귀는 각 단계에 포함하지만 전체 미완료 목록을 매번 재실행하거나 배치 작업의 착수 조건으로 삼지 않는다.

완료 이력은 각 항목에 검증 SHA·보고서 경로·남은 제한을 붙인다. 이 문서 작성은 해당 기능의 구현 승인이나 실행 완료가 아니다.
