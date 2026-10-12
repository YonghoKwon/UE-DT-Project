# 검증 이력 보관

이 문서는 2026-10-11 문서 재구성 때 README·IMPROVEMENTS·ROADMAP에 날짜별로 섞여 있던 **과거 검증 기록을 원문 그대로 옮긴 보관본**이다. 현재 보장·규칙이 아니며, 현재 상태는 [README](../README.md)와 [IMPROVEMENTS](IMPROVEMENTS.md)를 따른다. 문장 안의 SHA·수치·보고서 경로는 당시 조건이다.

[작업 지침](../AGENTS.md) · [현재 기능](../README.md) · [백로그](IMPROVEMENTS.md) · [로드맵](ROADMAP.md)

새 검증 기록은 아래 "README에서 옮긴 이력" 절보다 앞에 날짜 절로 추가한다. 옮긴 원문 문장은 수정하지 않는다.

## 2026-10-12 UXR-02 CLI 마무리와 UXR-03

- 시작: `codex/architecture-skeleton`, HEAD `e9bb72f`, DTCore `b2504d1`. `C:\Unreal Projects\ma0t10_dt`에서만 작업했다. 모니터 부재/WinDisc 1612×816 환경이며 요청에 따라 Computer Use는 수행하지 않았다.
- UXR-02: 기존 상태 칩 변경과 신규 `MA0T10.UXR02.PlacementMarkerIsolationRhi`를 재빌드하고 D3D12 UXR 7개 Success/실패·skip 0을 확인했다. 코드 `bd857a3`, 문서 `75a9f41`. 최종 삭제는 보류(환경: 모니터 없음) — 확인/취소 직접 검수 + 실제 삭제 자동화 검증 완료, 사용자 복귀 후 재검수.
- 통합 중 마커 Camera 픽셀 비교가 실패했다. 셰이더 완료·초기 픽셀 안정 대기를 추가한 `e5e9041` 이후 픽셀 완전 일치가 통과했다. 비교 허용치를 완화하지 않았으며 실제 PCD/CPU/GPU/semantic/ISM/Niagara와 가시/충돌 음성 대조를 유지했다. 이 보완은 테스트 fixture만 변경한다.
- UXR-03 코드 `2c453d2`: 소유 데이터 5탭·출력 편집 단일화·재생 요약·사전 점검/해결 링크·결과/진단 분리 구현(수정 15/추가 2, 이동 0). 검증 시작 HEAD `e5e9041`+작업 소스 17개의 hash가 커밋 소스와 일치함을 확인했다. 모든 실행이 커밋 이후 시작됐다고 주장하지 않는다. 기존 Workspace/UDxWidget/DTCore 전면·입력 차단 경로를 유지한다. 같은 옵션을 새 Subsystem/SaveGame에 복제하지 않고 기존 Slab Actor·재생 패널 상태로 위임한다. 사전 점검의 판단과 상세 형식을 공유하며 접힌 UI는 상세 요약을 만들지 않는다. 명시적 Blueprint 상세 조회는 유지한다. 파일 완료는 표시 polling의 5Hz 제한을 우회하지 않는다.
- Editor Development·layer gate 18/18(신규·stale 0)·DTCore guard·V2 `-RequireAssets`·WBP 7개 메모리 컴파일 통과. 최초 V2 실행은 CLI PATH에 `rg`가 없어 실패했고 설치된 VS Code ripgrep 경로를 프로세스 PATH에 추가한 후 통과했다. 검증 스크립트/시스템 PATH는 변경하지 않았다.
- 집중 자동화 16 Success/실패·skip 0. D3D12 전체 `MA0T10`은 **221 실제 실행 Success / 20 조건부 skip / 실패 0**. 기준선 `Saved/Reports/UXR/UXR-02/automation/index.json`의 238개 이름별 상태가 유지됐다. 추가는 `UXR02.PlacementMarkerIsolationRhi`, `UXR03.SingleOutputEditorAndDefaults`, `UXR03.PreflightDetailBudget` 3개다. skip 목록은 같은 조건부 opt-in 환경을 사용했다.
- 기존 기대값/검사 갱신: `SensorOwnedUiLayoutTests`(파일 탭 표시 index 1→2, legacy enum ordinal 불변), `SensorWorkflowToolbarTests`(진단 독립 탭), `SensorPlacementComparisonTests`(기본 결과 A/B·금지 용어 검사 추가), `SlabRunHistoryTests`(새 결과 필드에서 UUID/출력 검사, 진단에 snapshot/receipt 유지 검사 추가). 최초 전체 실행의 이력 테스트 1건 실패는 옛 필드 위치 기대였으며 실제 저장 실패/재시도·중복 거절·파일 생성 검증은 그대로 유지했다.
- 자동 렌더: 5탭×2해상도×2글자 배율의 20장. 실제 client 1280×720/1920×1080, window DPI 1.0, 글자 100/150%. Slate 창 테두리를 포함한 PNG는 1286×726/1926×1086이다. 본문은 스크롤이며 마우스로 하단 버튼 접근/resize를 확인한 것은 아니다. 최초 전송 진입의 재생 열 활성화는 실제 PIE fixture에서 검사했다. 기본 결과 렌더는 빈 목록이고 A/B·저장 재시도는 별도 자동화 fixture에서 검증했다.
- 성능: 실제 D3D12/1920×1080 client, 검증맵 `SlabScenarioValidationMap`, 1 Camera(D455)+1 LiDAR(MLX80/GPU), FullSpec, 미리보기/배치 기본 UI, 전송·파일 저장 없음, 10초 준비+30초 표본. 기존 계측기 `UXR02.Performance`의 전후 JSON은 `Saved/Reports/UXR/UXR-03/performance/`에 보관한다. `average_fps`/`frame_p95_ms`는 자동화 callback wall pacing이고 game thread는 `GGameThreadTime` cycles다. engine frame p95나 열린 데이터 탭의 전후 수치로 해석하지 않는다.
- 공통 UI 성능 전후: FPS **60.001→60.001**, callback p95 **21.163→20.967ms(-0.926%)**, engine game thread 평균 **6.091→5.634ms(-7.504%)**, 각 1800 표본. 2% 범위 안이며 절대 p95 20ms 목표 통과를 뜻하지 않는다. 열린 데이터 탭의 전후 비교와 물리 화면 수용은 계속 보류다.
- 보류: UXR-03 직접 조작 전체와 데이터 탭을 열어 둔 동일 조건 전후 성능, 물리 화면/DPI 수용. 가상 화면 자동 렌더/공통 성능 측정을 직접 검수로 세지 않았다. 체크리스트는 `Saved/Reports/UXR/UXR-03/manual/CHECKLIST.md`에 있다. UXR-02는 최종 삭제 클릭 1건만 보류다.
- 원본: `Saved/Reports/UXR/UXR-02/cli-closure/`, `Saved/Reports/UXR/UXR-03/RESULT.md`, `validation-manifest.json`, `test-comparison.json`, `skipped-tests.json`, 빌드/자동화/렌더 로그. 보호 파일 **474/474 hash 불변**, 개인 SaveGame/UI 슬롯 **9/9 복원**, 추가 SaveGame 0, Editor/PIE 정상 종료를 확인했다. Broker 재설정·서비스 재시작·DTCore 변경·운영맵/자산 저장·push/PR/merge는 수행하지 않았다.

## README에서 옮긴 이력

### 2026-10-10 단일 폴더·배치 UI 검수 (README 원문)

- 최신 구현은 `ma0t10_dt`에서 직접 빌드·실행한다. 네 별도 checkout 경로와 Git worktree를 정리했고, 이전 자료와 Git 이력은 `Saved/WorkspaceConsolidation`에 보존했다. 현재 Artemis 로그도 이 프로젝트의 보고서 폴더를 사용한다.
- 배치 창 생성/진입·취소된 복원 요청·북마크 선택/이름·늦은 저장 완료를 보완했다. PCD 점검은 exact/wildcard, 다음 시나리오와 수동/현재 출력, backend 연결과 프레임/초 예산을 구분한다.
- Runtime `81b56ba`: UE5.3 Editor Development, 전체 MA0T10 217 Success/실패0 중 실제215·조건부skip2, WBP7개 메모리 컴파일·V2 자산 검사 통과. 사용자 맵 높이 assertion은 원본을 보존하고 커밋본 격리 fixture로 재검증했다.
- 실제1920×1080의2+2 PCD-only 10+60초는 평균59.89FPS·1% low51.13·frame p9516.67ms, 각PCD19.95/19.9667Hz·Camera acquisition29.938/29.954Hz다. 승인2,395건의 내부/UI/외부 누락·비선택Topic·invalid/gap/duplicate/overflow0, 내부/외부e2e p9579/93ms다. 기존 기준선 대비 평균/low 저하는 약0.05/4.13%, frame p95는 같다.
- 두 client 해상도와 네 배율의 렌더 캡처를 확인하고 배치 창 잘림을 보정했다. 실제 Slate/RHI 입력과 물리 마우스는 구분한다. 직접 마우스 검수는 재연결 후에도 Computer Use node kernel 초기화(os error3)가 실패해 미완료다. 최신 결과·스크린샷·제한은 `Saved/Reports/PlacementUiPreflightClosure/FINAL_STATUS.md`다. 이전600초/10회 PCD 증거는 보존했다.

### 2026-10-09 PCD 중심 기본 생성 장비 검증 (README 원문)

- 실제 표시된1920×1080 PIE, 같은 Slab 검증맵·pose·UI에서 JSON stride1/max0를 고정해 정책을 교차 비교했다. 기본 제출 예산2, Camera1280×720/30Hz·Native576×56/20Hz, PCD 전체 검출점·FrameStride1을 유지했다. 2+2 새 정책은10+60초3회 모두 통과: 평균59.99~60FPS·1% low59.17~59.80·p9516.67ms. 기존 전체 JSON 정책의 low29.74~36.02는 실패로 보존한다. 새 정책1+1도60FPS·low59.87로 통과했다.
- 새 정책의 같은 월드·연결30초×10회는11,995개 승인 request의 제출·receipt·내부/UI·외부 수신이 일치했다. 최소 Camera29.9667Hz·LiDAR/PCD19.9667Hz·FPS59.9523·low55.966, 최대frame p9516.669ms다. JSON 작업0, 복귀 추가 제출0, drain 후body/events/active0, PIE 후weak owner16개·Camera global job0을 확인했다.
- 10+600초2+2도 통과: 평균59.99FPS·low59.58·p9516.67ms, 각PCD19.9967/19.9983Hz. 고정 승인 집합23,997건의 외부/내부/UI 누락·clock 오류0, e2e p9580/92ms(내부/외부)다. 비선택 Topic·invalid/gap/duplicate/overflow0이다. JSON 생성이 필요 없을 때만 생략하며 기존 JSON 소비 경로는 유지한다.
- 최종 전체MA0T10은211 Success/실패0 중 실제208·조건부skip3이다. 배치 입력 RHI는 별도 통과(Camera/LiDAR input→GPU end-frame p9534.849/34.753ms, 원규격 preview31.250/62.104ms). Camera marker987개, WBP7개 메모리 compile·자산검사·기존 자산저장0을 확인했다.
- 직접 UI는 Windows 캡처/커서 오류0x80070057/0x80070005로 현재 미완료다. 활성화에 성공했던 조사용 화면의 작업 영역은1440×900이었다. 이전 작업의 직접 조작을 이번 새 정책 UI의 통과로 바꾸지 않는다. 숨긴/offscreen 시험의 낮은 FPS도 보존하며 이 수치를 표시된 PIE 지원으로 확대하지 않는다.
- 재현: `Scripts/run_pcd_demand_acceptance.ps1 -Phase Compare`, `-Phase Repeat`, `-Phase Endurance`. 센서별 기대 수·속도·실행 수·정합성·성능 중 필수 조건이 실패하면 실패 종료한다. 증거와 제한은 `Saved/Reports/PcdDemandPlacementAcceptance/FINAL_STATUS.md`에 있다. DTCore·운영맵·개인 UI 저장값을 보존하고 push는 수행하지 않는다.

### 2026-10-08 출력 신뢰성 검증 (README 원문)

2026-10-08 검증: UE5.3 Editor Development, 최종 D3D12 MA0T10 **222 Success/실패0(실제216·opt-in skip6)**, 별도 실제30초 A/B ProductionRuntime, WBP6개 메모리 compile/자산 검사·저장0. 1+1 전체 출력은30/20Hz·평균59.98FPS·1% low57.86·p9516.67ms. 2+2 PCD 전용은각PCD 약19.9Hz, Camera/LiDAR정보 Topic0건. 2+2 전체 출력은**명시적 제출예산4건/프레임**에서각Camera29.883Hz·각LiDAR/PCD19.917Hz·평균59.76FPS·1% low51.81·p9517.04ms와 승인 request의 receipt/내부/UI/외부 누락0이다. 기본예산2건은Camera큐과부하로실패이며 지원으로표시하지않는다. 실제1920×1080 Camera기즈모·정지유지·LiDAR정밀이동·첫Esc/20Hz복귀, 실제1280×720 PCD재생·실행선택·A/B상세·resize/scroll을 직접 확인했다. 60초2+2를장기/4+4인증으로확대하지않는다. baseline의1%low정의차이와Camera지연p95 증가도 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`에 남겼다.

### 2026-10-09 배치 작업공간 검증 — a78b443 이전 이력

- 최신 D3D12 전체 `MA0T10`은 **209 Success/실패 0, 실제 수행 206·opt-in skip 3**이다. 실제 `topic.scenario` 발행을 준비해 DTCore 수신도 통과했다. skip 중 배치 입력 RHI는 별도 실행으로 통과했고, Geometry 성능·세 출력 반복 opt-in은 이번 PCD-only 범위에서 미수행이다. WBP **7개** 메모리 컴파일·자산 검사 통과, 기존 자산 저장 0이다.
- 배치 API로 만든 1+1·2+2 장비의 실제1920×1080·10초 준비+60초 PCD-only 시험을 통과했다. 최종1+1은 평균60.00FPS·low59.92·p9516.67ms, 승인20Hz·외부수신19.962Hz다. 2+2는 센서별 승인20Hz·외부합산39.925Hz·Camera acquisition30Hz, 평균60.00FPS·1% low59.65·p9516.67ms다. 기본 제출 예산2건/프레임을 유지했고 비선택 Topic 제출은0이다. 이 성능 fixture는 미송신 호환JSON에 기존 stride32/max1024를 적용한다. PCD는 전체 검출점·FrameStride1을 유지하며, 이 결과를 신규 장비의 기본 호환JSON stride1/무제한 조건으로 확대하지 않는다.
- 같은 월드·연결의 **2+2 PCD 10분**은 각 PCD19.993Hz·평균59.98FPS·1% low58.27·p9516.67ms다. 두Camera를모두실행한 최종 **30초 시나리오10회**의 서로 다른 RunUUID에서 총11,938개 PCD request를 제출·receipt·내부/UI·독립 외부 수신과 대조했다. invalid/gap/duplicate/overflow0, 초기 위치 복귀의 추가 제출0, 정리 후 outstanding bytes/pending events/active runs0이다. Slab 행 수와 센서 프레임 수를 같다고 강제하지 않는다.
- **반복 성능은 미완료:** 최종10회는 송수신·수명 시험의 통과이며, 회차별1% low38.42~40.25FPS는45FPS 목표에 미달한다(평균59.26~59.35, p9516.69~16.77ms). 첫Camera가정지했던초기시험11,982건·low36.84~43.96도이력으로보존한다. 반복 fixture의 신규LiDAR는 기본 호환JSON stride1/무제한이며 연속 성능 fixture와 후처리 조건이 다르다. 이 차이와 Slab/preview 비용은 원인 후보이고 인과 분해·동일조건 재시험이 남는다. 실행 종료 후에 생성하는 검증 파일만을 원인으로 단정하지 않는다.
- 직접1280×720에서 장비 추가·즉시 preview·북마크 저장/이동·숫자 입력·Undo/Redo·첫Esc 원규격 복귀·전체 배치 저장을 확인했다. PIE 재시작 후1920×1080에서 전체 배치 변경 미리보기/적용·고유ID/북마크 복원·PCD/결과 진입·150% 배율·접기/drag/resize를 확인했다. 85/125%와 모든 패널의 전체 직접 체크리스트, UI 직접 삭제/복제·전체 DPI는 별도다. 마지막 간결한 PCD 요약의 추가 화면 확인은 Windows 캡처 오류0x80070057로 미완료다.
- 부하가 있던 최초 시험은 기존·신규 장비 모두 약47FPS, 2+2 PCD 약15.7Hz로 실패했다. 사용자가 다른 GPU 작업을 멈춘 후 위 시험은 통과했지만 특정 앱을 원인으로 확정하거나 모든 환경에서 무부하를 보장하지 않는다. 원본 실패와 조용한 환경 재시험을 모두 보존한다.
- 재현: `Scripts/run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -PointCloudOnly -LayoutApiFixture -SensorPairs 2 -ClientWidth 1920 -ClientHeight 1080 -MeasurementSeconds 600`, `Scripts/run_slab_repeat_stability.ps1 -Repeated -LayoutApiFixture -RunCount 10 -ResourceDiagnostics -ReportGroup SensorPlacementWorkspace`. 상세 범위는 `Saved/Reports/SensorPlacementWorkspace/FINAL_STATUS.md`다. DTCore·운영맵·Game.ini·PixelStreaming과 개인 UI 저장값은 보존한다. 4+4·60분 soak·Engine STOMP/WSS 운영 인증·Editor 영구 설치는 포함하지 않는다.

### 2026-10-08 배치 조작 이전 검증 (README 원문)

이전 `7c6ed29` 검증: 30/60/120FPS의 2초 이동·회전 차이 1% 이내, 실제 파일 재로딩·실패 시 보존·20/50개 상한·잠금·시작 snapshot 불변을 확인했다. 당시 전체 회귀는 206개 수행 통과·조건부 skip 6·실패 0이었다. 두 해상도의 북마크·정밀 이동·Undo/Redo·Esc 직접 검증 범위는 `Saved/Reports/SensorPlacementWorkflow/FINAL_STATUS.md`에 보존한다. 후속 새 자세 프레임·A/B 비교의 최신 근거는 아래를 따른다.

### 배치 비교 이전 검증 (README 원문)

이전 배치 비교 검증은 210개 수행 통과·조건부 skip 6·실패 0, WBP 6개 메모리 컴파일은 저장 없이 통과했다. 실제 1280×720·1920×1080 RHI에서 원규격 새 자세 프레임 준비는 Camera 약31ms·LiDAR 약67ms, 입력→viewport GPU end-frame p95는 약34~43ms였다. 정격 세 스트림은 평균59.99FPS·Camera29.983Hz·PCD20Hz, 비교 UI 표시 전후 frame p95는16.67ms였다. 당시 직접 마우스는0x80070005로 미완료였고 이후 SensorOutputReliability에서 추가 직접 검수를 수행했다. 원래 근거는 `Saved/Reports/SensorPlacementComparison/FINAL_STATUS.md`, 최신 범위는 이 문서 상단과 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`를 따른다.

### 2026-10-08 Slab 반복 실행과 결과 조회 검증

이번 자원 수명 보강은 `Saved/Reports/SensorResourceLifecycle`에서 별도로 검증한다. 아래 10회 반복 결과는 이전 소스의 증거다.

- 이번 최종 반복은 실제1920×1080에서 PCD 전용/세 출력 각각30초×30회다. 승인·제출·receipt·내부 실제 MESSAGE·외부 request ID집합 대조 통과: PCD17,989개, 세 출력합계62,979개. 종료 bytes/events/active/retiring0, 결과·종료진단20개, source별 worker/connection 동일. 반복 최소FPS59.82/59.94, 가장 느린1% 평균 low47.80/55.32, frame p95≤16.67ms.10초 준비+60초 변경 전후 각3회도5% 회귀 조건을 통과했다.
- `1d17016` 최종 전체207개 Success 상태(그중15개 경고 포함)에서 명시 opt-in skip6개를 분리해 실제201개 수행으로 기록한다. 이전206개 결과와 원본 실패는 보존한다. 반복시험·실제 topic.scenario Broker 수신은 별도 opt-in 실행에서 통과했다. WBP6개 메모리 컴파일과 자산 검사는 저장0이다. 입력 시험의 최초 실패는 접혀 저장된 Settings를 펼친 뒤 Slate 배치를 기다리는 fixture 보정 후 집중·전체 재시험으로 통과했으며 원본 로그를 보존한다.
- 직접1280×720의 Camera 이동/숫자 회전·정지/실행 첫 Esc·LiDAR 숫자 위치/첫 Esc·패널 조작에 이어, 세션 복구 후 실제1920×1080에서 Camera 기즈모 이동·회전·정지 첫 Esc·Settings 숨김, LiDAR 숫자Z1800→1850·4Hz→20Hz Esc 복귀, 데이터 패널 열기·네 글자 배율·resize/scroll/collapse를 추가 확인했다. 캡처1180×771에는 아래쪽 일부가 보이지 않아 전체1920 checklist·Main/외부 Widget 입력 전수·직접 준비 취소는 미완료다. 정상 반복의 private memory는 실행1→30에서 약15/44MiB 증가했고 전체 allocator/driver/engine 인과 분해는 남는다. **전체 최종 검수 완료로 표시하지 않는다.**
- 직접 정체 시험에서 획득이 멈췄는데 모니터가 마지막20Hz를 계속 표시하는 문제를 재현했다. `1d17016`은 등록된 소유 모니터만 Scheduler의 현재 유효Hz·준비/정지/조작/정체/pause 상태를 표시한다. 실제1280×720에서0Hz·정체 경고→20Hz 복귀, 일시정지→재개를 확인했다. 저장된 측정 이력·송신 계약·주기·비대상 Widget의 표시 방식은 변경하지 않는다.
- 이 표시 수정까지 포함한 최종 source의 실제1920×1080·10+60초 PCD/세 출력 각3회도 통과했다: 평균59.98~60FPS,1% low≥59.96,p9516.67ms,PCD외부수신≥19.912Hz, invalid/gap/duplicate0. 기존 baseline 대비FPS/low/p95의5% 회귀 없음. source별 원문과 구간별 카운터는 `Saved/Reports/SensorResourceLifecycle/final-performance-comparison.json` 및 `Saved/Reports/lifecycle_final_monitor_*`에 보존한다.

- Camera의 readback·encode·완료 순서 대기 전체를 8프레임으로 제한한다. 앞 JPEG만 2초 지연한 실제 D3D12 시험에서 순서 대기46→8, 완료 JPEG44→7을 확인했다. 한도 도달은 명시적 출력 실패이며 측정을 막거나 이전 이미지로 채우지 않는다. 실패한 앞 프레임 뒤의 완료 결과는 해제하고, Stop은 취소된 결과를 발행하지 않는다.
- 진단용 CPU/Niagara 점군과 World debug line batcher를 센서 SceneCapture에서 제외한다. 플레이어의 3D 표시는 유지한다. 실제 RHI의 표시 OFF/ON에서 Camera 픽셀 변화0, GPU 깊이 차이0을 확인했고, 격리를 해제한 대조군은 Camera2,536픽셀·중앙 깊이795.013→200cm의 변화를 검출했다. 사용자 Mesh·머티리얼은 수정하지 않는다.
- 자원 snapshot은 요청 시에만 생성하며 CPU 소유/공유 용량과 GPU 추정을 구분한다. 상세 파일은 `Scripts/run_slab_repeat_stability.ps1 -Repeated -RunCount 30 -ResourceDiagnostics -ReportGroup SensorResourceLifecycle [-AllOutputs]`로 수집하고 `Scripts/analyze_sensor_resource_lifecycle.ps1`로 상한을 대조한다. 공유 bytes를 서로 다른 소유자 사이에서 합산하지 않는다. Camera readback 재사용 최적화는 원인 확인 조건부로 유지한다.

- 같은 월드·Raw worker·연결로 30초×10회 PCD 전용과 세 출력 실행을 검사했다. PCD 전용6,000건, 세 출력 Camera8,999/LiDAR정보6,000/PCD6,000건의 승인 request ID·제출·receipt·실제 내부 검증·독립 외부 수신을 대조했다. invalid/gap/duplicate/overflow0, 복귀 추가 제출0, 종료 outstanding bytes/sequence0이다.
- 실제1920×1080에서 최종 반복은 PCD59.90~60.00FPS·1% low≥52.91, 세 출력59.91~59.99FPS·1% low≥54.27, p95≤16.672ms. 같은 조건 변경 전후 각3회의 중앙값 FPS는 PCD59.96→59.99, 세 출력59.89→59.96이며 p95 증가5% 이내다. 장비는 현재 RTX4070Ti/UE5.3 한 대 구성이다.
- 최초 반복 후반 p95약25ms와 ISM update 최대15.9ms를 trace로 재현했다. 진단 미리보기 인스턴스의 충돌/물리/navigation을 제외한 뒤 같은 후반30초 갱신 inclusive2.122초→0.044초, 최대0.291ms를 확인했다. 점 수·센서 측정 규격은 동일하다. 과거 다른 SHA/조건의24.9ms 기록과 모든 PC 지원은 별도다.
- 최종 D3D12 `MA0T10`203개 목록 중 최초202 Success/1 Fail(외부 publisher 준비 누락), 실제 `topic.scenario` 발행을 준비한 해당 시험 재검증 통과. 명시 opt-in skip5개는 제외해 실제198개 수행으로 구분한다. TCP 장애6종 각2회, 집중 종료/저장/preview isolation 및 WBP6개 메모리 컴파일/저장0을 확인했다.
- 직접 client1174×683에서 결과 선택·숨김 보존·저장 실패→mouse retry→실제 저장 완료·초기 복귀 결과 보존·9.37초 중단/187 receipt·UUID 복사를 확인했다. 목표1280×720·1920×1080 전체 직접 조작 및 직접 준비 취소는 미완료다. 자동화 exact1920과 구분한다.
- 세부 증거·원본 실패·제한: `Saved/Reports/SlabRepeatRunStability/FINAL_STATUS.md`. 재현: `Scripts/run_slab_repeat_stability.ps1 -Repeated [-AllOutputs]`, `Scripts/analyze_slab_repeat_stability.ps1`. DTCore/운영맵/개인 UI 저장값은 보존했다.

### 2026-10-06 main 기반 최소 DTCore 적용

DTCore는 main `b22af0b`에서 필요한 오류·종료 안전성만 적용한 `b2504d1`을 사용한다. 기존 센서·Slab 기능을 유지하는 별도 연동 브랜치에서 검증했다. `OnConnected`는 성공/실패를 포함한 구독 완료 callback 집계이며 모든 구독 성공을 보증하지 않는다. custom 수신 binding을 보존하고, 공통 5초 timeout·all-ready 상태기계·World cleanup의 GI 전역 큐 폐기는 포함하지 않는다. 이전 안정화 기록의 해당 API 설명은 현재 계약으로 사용하지 않는다.

로그 쓰기·parse 종료 안전성, Registry EndPlay/stream-out 정리와 Widget 제거 후 Blueprint hook을 검증했다. SingleRelease와 FPS 숨김은 프로젝트 Controller가 선택하며, 미등록 Widget 정책은 유지한다. 공통 변경 이유와 소비자 이관은 [최소 수정 계약](Plugins/DTCore/docs/MINIMAL_MAIN_MIGRATION.md)을 참조한다. 검증 결과는 `Saved/Reports/DTCoreMinimalAcceptance/FINAL_STATUS.md`에 있다. 최종 `443b48a` 전체 RHI 자동화184 Success(실제179·opt-in skip5), 신규 실패0이며 독립 host/프로젝트 Editor·Shipping과 Development 패키징을 통과했다. 직접1286×760 PIE에서 조작·숫자 입력창 첫 Esc·폰트·패널·Slab 실행/재생을 확인했다. PCD 전용과 세 stream의 별도1920×1080·10+60초 시험은20/30Hz·평균60FPS·p9516.67ms, 승인 request 집합의 내부/외부 누락0이다. **재생 성능20ms 조건은 기존24.908ms/최소수정24.925ms로 두 버전 모두 실패**하므로 성능을 포함한 최종 검수 완료로 표시하지 않는다. Geometry 성능·10회 stream 반복의 최종 버전 시험은 미수행이다. 이후 발견한 소유 숫자 편집창 Esc는 프로젝트에서 수정했고 전송 계약은 동일하다.

### 2026-10-07 Slab 초기 배치 복귀 검증

- 진행 패널에서 종료·정리 후 초기 위치/회전으로 복귀한다. 형상·보관 목록은 유지하고 진행·차트·3D 분석을 대기로 초기화한다. 사용법은 아래 Slab 절을 따른다.
- UE 5.3 Editor Development 빌드, 전체 `MA0T10` 194건 Success/실패 0(실제 185·조건부 skip 9), 별도 Slab 12건 Success(실제 11·PCD opt-in skip 1), 소유 WBP 6개 메모리 컴파일(저장 0)을 확인했다. 전체 시험의 skip은 실제 통과로 합산하지 않는다.
- 실제 Artemis의 `30초 PCD → drain → 복귀 → 같은 시나리오 30초 재실행`은 각 600건, 총 1,200건 제출/receipt/내부·외부 수신이 일치했고 invalid/gap/duplicate/overflow 0이다. 복귀 후 대기 1초의 추가 제출은 0건이다. 센서와 Slab 프레임 개수가 항상 같다는 보장은 아니다.
- Computer Use로 실제 client **1286×760, 글자 100%**에서 실행/pause 비활성 사유, 완료·중단 후 복귀, 동일 목록 재생, 숨긴 차트 재표시와 3D 라벨 제거를 확인했다. 임시 GUI 관찰 스크립트의 미노출 Python API 오류는 보정 후 재실행했으며 production 결함으로 집계하지 않는다. DTCore·운영맵은 수정하지 않았고 개인 UI 저장값은 복원했다.
- 증거: `Saved/Reports/SlabResetPlacement/FINAL_STATUS.md`. PCD 시험의 실제 viewport는 1068×360이다. 이번 시험을 목표 해상도 전체 성능 검수나 기존 재생 p95 문제 해결로 확대하지 않는다. push/PR은 별도 요청 전까지 하지 않는다.

### 2026-10-07 송신 신뢰성 검증 (README 원문)

2026-10-07 송신 신뢰성 검증: Editor Development 빌드, 최종 D3D12 `MA0T10` **198 Success/실패0(실제188·opt-in skip10)**, 소유 WBP6개 메모리 compile/save0. 별도 TCP fixture의 인증 거절·연결 지연·단절·receipt 누락·과부하·센서 삭제 6종을 통과했다. 실제 Artemis 30초 두 실행에서 PCD 전용1,200건 및 세 출력 Camera1,800/LiDAR정보1,200/PCD1,200건의 제출·receipt·내부 수신이 일치하고 외부 PCD1,200건도 검증했다. 실제 자동화 viewport1068×360, 직접 client1286×760/100%다. 세 출력 평균59.85~59.91FPS·p9516.67~16.69ms는 작은 viewport의 집중 결과이며 목표해상도·기존 replay p95 문제 해결 인증이 아니다. 직접 필수 선택·준비 대기·실패·결과 조회·복귀 후 기록 보존을 확인했다. 상세 증거는 `Saved/Reports/ScenarioDataReliability/FINAL_STATUS.md`. Engine STOMP/WSS 필수 모드의 별도 실사용 시험과 장시간 수집·외부 업무 ACK는 남아 있다.

### 2026-09-29 런처 연계 패키징 점검

- 현재 체크아웃(기준148cc336, DTCore a1b333e)의 Windows Development Build/Cook/Stage/Archive를 실제 완료했다. 최종 성공은 맵 제외 없이 수행했다.
- 게임 빌드를 막던 에디터 전용 테스트11파일의 가드를 보완했다(`760082c`). D3D12 에디터 관련29테스트 실패0/미실행0:12건 정상,17건 격리된 SaveGame 파일 부재 경고 동반. 센서 계산/전송 로직은 변경하지 않았다.
- DT_DxLevel이 참조하는 TestMap의 누락된 구형 모니터 클래스를 사용자 승인 후 현재 WBP_VirtualSensorMonitorPanel로 연결했다. 원본은 Saved/LauncherPackagingBackup에 보존했고 재실행 시 파일 불변을 확인했다. SensorTestMap과 Config/Game.ini의 원래 바이트는 보존했다.
- 패키지328파일/878,409,923바이트. ZIP+외부 release.json을 격리된 런처 배포 서버에 접수·승인하고 사용자 확인 후 실제 GUI로 설치·실행했다. Manifest328파일 해시, SensorTestMap/D3D12 창, 런처 종료/재실행 중 동일 UE bootstrap/자식 생존, 정상 종료 후 Quiescent를 확인했다. 실제 버전 업데이트/복구·Linux/회사 인수까지 완료한 것은 아니다.
- `Scripts/inspect_launcher_package_map.py`는 기본 맵의 출력 설정을 읽기만 한다. 현재 저장된 기본 맵의 센서 출력은 LogOnly이며, 런처 시험은 별도 UserDir와 DTCore 로컬 시험 주소를 사용한다.
- DTCore의 EnhancedInput 플러그인 의존 선언 경고는 남아 있다. DTCore 및 gitlink를 임의 수정하지 않았으며 Shipping/Linux·회사 서버·실장비 검증을 완료로 표시하지 않는다.
- 실제 실행에서 DTCore의 CustomLogs가 설치 폴더에 생성됐다. `-UserDir`만으로 모든 로그가 분리되는 것은 아니며 Program Files·서비스 계정의 로그 쓰기 경로는 후속 점검한다. 이번 시험은 시험 폴더 안에서만 이루어졌다.


### 이전 최종 검수 이력 — 2026-10-03

아래는 당시 production source `2eb30f0`, DTCore `067195b`의 이력이다. 최신 기준은 문서 상단의 `2ec67a2`/`b2504d1`을 따른다. 당시 검수에서는 DTCore 공통 소스·wire schema·센서 규격을 바꾸지 않고 숫자 Transform 편집의 경량 조작 유지, 실제 Slate 포인터 경로, 큰 글자에서 소유 패널 최소 폭을 보완했다.

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

## IMPROVEMENTS에서 옮긴 이력

### 2026-10-10 단일 작업 폴더·배치 접근성 보완

- 기존 `ma0t10_dt`와 별도 최신 작업본의 버전 차이로 추가/저장 기능을 찾지 못한 상황을 확인했다. 현재 소스와 의존성은 `ma0t10_dt`에서 직접 빌드한다. 별도 worktree를 새로 생성하지 않는다.
- 배치 미등록 상태의 무응답 경로에 소유 Host 생성/실패 안내를 연결했다. 선택 배치가 바뀌면 이전 복원 요청을 취소하고, 북마크 이동 후 이름 편집 대상도 동기화한다.
- 사전 점검은 활성 exact/wildcard 우선순위, 수동 추가와 시나리오 전환, Raw/Engine 연결, GPU fallback echo 상한, 단일 프레임 bucket 예산을 구분한다. 동적 admission 구현으로 확대하지 않으며 RT-08 전체 진행률은 유지한다.
- Runtime `81b56ba`에서 최종 전체217 Success/실패0(실제215·skip2), WBP7개와 V2 자산 검사가 통과했다. 실제1920×1080의2+2 PCD-only는 각19.95/19.9667Hz·평균59.89FPS·low51.13·p9516.67ms, 승인2,395건 내부/UI/외부누락0이다. 두 해상도·네 배율의 렌더 캡처와 Slate/RHI 입력 증거를 남겼다. 직접 조작 도구의 node kernel 초기화(os error3)는 재연결 후에도 실패했으므로 물리 마우스 수용과 첫 사용자 검수는 미완료다. UI/UX 전체 진행률을 완료로 올리지 않는다.
- 일반 실행은 요청대로 31000 접속 정책을 유지한다. 확인 당시 listener와 기본 WebSocket Topic이 없었다. 실제 사용할 업무 서버 실행·`topic.scenario` 구독 설정은 프로젝트 배포/운영 준비 항목이며 DTCore 소스 결함으로 분류하지 않는다. 로컬 Artemis로 TC/DataSync/Slab 수신 경로는 별도 검증했다.

### 2026-10-09 PCD 수요 정책 검증 — 이전 조건

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

### "읽는 방법" 절에 있던 날짜별 기록 (IMPROVEMENTS 원문)

**출력 신뢰성 보완 — 2026-10-08:** 느린 LiDAR JSON이 새 측정마다 폐기되던 경로를 수정해 정상 완료를 전달하고 측정 당시 metadata를 보존한다. 50ms 입력에 200/700ms 생성 지연, 전역 4개 작업 상한과 다섯 번째 센서의 진행, Stop/삭제 후 callback 폐기를 집중 검증했다. PCD·Raw telemetry 중복 발행을 막고 선택된 호환 JSON만 drain에 포함한다. 실행 상세와 A/B에 센서·출력별 전달 수를 추가했으며 wildcard의 센서 간 FrameId 간섭·카운터 덮어쓰기·round-robin 순회 오류를 보완했다. `MeasuredOutputRateHz=0`만으로 PCD 실패를 판단하지 않는다. 전송 계약·센서 규격·DTCore·외부 Widget은 그대로이며 UE 학습 데이터 묶음 저장은 추가하지 않았다. 실제 다중 센서 수용 범위와 남은 검증은 RT-05 및 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`를 따른다.

최종 검증: D3D12 전체222 Success/실패0 중6개 opt-in skip(실제216), 별도30초 A/B 실제송수신, WBP6개 메모리컴파일·저장0. 직접1920×1080 Camera기즈모·첫Esc/정지유지와LiDAR정밀이동·10→20Hz복귀, 직접1280×720 PCD재실행·센서별600건A/B·패널resize/scroll을확인했다. PCD전용에서비선택Camera0Hz를측정률실패로오판하던경고를수정했다. 입력fixture는개인의열린Data패널에의존하지않게우리보조패널만숨긴다. 광범위한배치/모든폰트/RMB직접체크리스트와장기지원은별도다. RT-05·RT-06·UI-01 전체완료로확대하지않는다.

**배치·실행 결과 비교 — 2026-10-08:** 맵 값 복원의 실패 안내·Undo, SensorId 변경의 북마크 캐시 및 이전 저장 결과 적용을 보완했다. 새 위치 미리보기는 측정 시작 시각·자세·규격·generation과 실제 완료를 확인하며 Camera 확인에는 JPEG/readback을 추가하지 않는다. 실제 1920×1080 RHI에서 원규격 새 자세 프레임 복귀는 Camera 약31ms·LiDAR 약67ms, 입력→viewport GPU 완료 p95는 약34ms였다. 현재 PIE의 확정 결과 A/B에 배치·설정·기록 시점 Hz와 실행 전체 전송 카운터를 표시한다. 원문 hash·조건 차이·확인 불가를 구분하고 배치 품질 점수로 확대하지 않는다. 실제 30초 A/B의 PCD600/599개 및 모든 acquisition 행렬을 독립 대조했고 비교 UI 표시 전후 frame p95는16.67ms로 같았다. 최종 근거와 직접 조작 범위는 `Saved/Reports/SensorPlacementComparison/FINAL_STATUS.md`를 따른다. UX-01·RT-06의 생성/복제/삭제·다중 구성 완료율은 이번 기능으로 올리지 않는다.

**Slab 초기 배치 복귀 — 2026-10-07: 구현·관련 검증 완료.** 종료·송신 정리 후 진행 패널의 `초기 위치로 복귀` 또는 Actor API로 PIE 시작 당시의 월드 위치·회전에 즉시 복귀한다. 현재 형상/재질/스케일과 보관 목록은 유지하고 진행률·현재 프레임·세 차트·이전 3D 분석 라벨을 초기화한다. 실행·pause·파싱·재생/센서 준비·drain 중 거절, 실패/중단 후 복귀, cm/mm/m와 회전된 Track, 반복 호출·지연 적용 차단을 검사했다. 실제 Artemis 두 실행의 PCD 1,200건 제출/receipt/내부·외부 수신 일치와 복귀 추가 제출 0을 확인했다. 전체 D3D12 자동화 194 Success(실제 185·skip 9), Slab 집중 12 Success(실제 11·skip 1), WBP 6개 메모리 컴파일과 직접 1286×760/100% 조작을 수행했다. 숨김 중 차트 초기화와 동일 시나리오 새 RunUUID 재생도 직접 확인했다. DTCore·운영맵·기존 UI 저장값은 보존했다. `Saved/Reports/SlabResetPlacement/FINAL_STATUS.md`를 따른다. 이는 개별 복귀 기능의 완료이며 RT-05/QA-01 또는 기존 재생 p95 목표를 완료로 올리지 않는다.

**최소 DTCore 적용 검증 — 2026-10-06:** 별도 체크아웃에서 main 기반 최소 수정본을 연동했다. 1차 D3D12 전체161개 Success에는 조건부skip5개가 포함되며 실제156개다. 추가 공통 수명 검증은 13개 Success(실제 수행 12개·Slab Broker opt-in skip 1개), 실제 topic.scenario 수신은 별도 전체 시험에서 통과했다. PCD 전용 및 세 스트림의 1920×1080·10초 준비+60초 시험에서 승인 집합의 내부/외부 누락 0, invalid/gap/duplicate 0을 확인했다. PCD 20Hz, Camera 30Hz, 평균 60FPS·프레임 p95 16.67ms. 단순 누적 카운터가 아니라 고정 측정 request 집합을 대조했다. 최종 소스 전체 자동화는184개 Success(실제179·조건부skip5), 신규 실패0이다. 직접1286×760에서 네 글자 배율·패널·Slab 실행/재생·입력창 Esc 복구를 확인하고 최종 Shipping/Development 패키징을 통과했다. 재생 데이터1200건은 일치하지만 p95는기존24.908ms/최소수정24.925ms로 두 버전 모두20ms 목표가 실패다. Geometry 성능·10회 stream 반복은 미수행으로 유지한다. 기능 회귀와 성능을 포함한 최종 검수 완료를 구분한다. `Saved/Reports/DTCoreMinimalAcceptance/FINAL_STATUS.md`를 따른다. 상세 원인은 이 문서의 부록으로 추가하지 않는다.

**이전 이력 — 2026-09-29 런처 실행:** 격리된 관리형 런처 GUI에서328파일 설치/해시·UE 창·런처 종료 후 동일 UE 프로세스 유지·정상 종료를 확인했다. DTCore CustomLogs가 설치 경로에 쓰이는 점을 발견했으므로 회사 Program Files 권한/로그 위치는 미검증으로 남긴다. 실제 버전 변경/복구·Linux·운영망 검증은 이 결과로 완료되지 않는다.

**이전 이력 — 2026-09-29 패키징:** Runtime의 에디터 전용 테스트11파일을 WITH_EDITOR로 제한하고, 승인된 TestMap의 누락 모니터 참조를 복구했다. 당시 Windows Development 전체 Build/Cook/Stage/Archive와 관련 D3D12 자동화29건을 확인했다. 이후 현재 기준에서는 EnhancedInput 의존 선언, 프로젝트·격리 host의 Editor/Shipping 빌드와 Development 패키징을 검증했다. Linux·회사 운영망 및 실제 소비 프로젝트의 검증은 별도다.

**이전 검수 이력 — 2026-10-03:** `Saved/Reports/DTCoreFinalAcceptance/FINAL_STATUS.md`와 README의 최신 검수 요약을 우선한다. 현재 전체 자동화는190건 Success/실패0이며 실제 수행185건과 opt-in skip5건을 구분한다. 현재 source의10분 연속·10회 반복 송수신은 통과했지만 최종 성능 비교·일부 opt-in·전체 직접 조작 검수는 남아 있다.

## ROADMAP에서 옮긴 이력

같은2026-09-29 후속으로 격리된 Windows 관리형 런처에서 실제 패키지 설치/실행·Manifest328파일 해시·런처 종료 후 UE 수명 유지·정상 종료를 확인했다. 회사 설치 경로 쓰기 권한, 실제 업데이트/복구·Linux/RHEL 인수는 별도 gate로 유지한다.

2026-09-29 배포 준비 증거: 현재 UE5.3 프로젝트의 Windows Development 패키징을 완료하고 외부 런처의 격리된 ZIP/sidecar 접수·승인을 준비했다. 에디터 테스트 가드와 승인된 TestMap의 누락 위젯 참조만 보완했다. 이는 R5 플러그인화·간편 설치 또는 회사 운영 인수 완료가 아니며, 실제 런처 실행·Shipping/Linux·회사 환경 수용은 각각의 증거로 판단한다. 자세한 범위는 [README의 패키징 점검](../README.md)을 따른다.


## 2026-10-12 — UXR-03 전송 탭 보완 CLI 검증

- 소스 커밋 `22d54b9`: 6파일 수정, 이동 0. 쉬운 사전 점검 문장/툴팁, `점검 대상:` 세그먼트, 직접 전송 접힘 헤더와 숨김 표시 비용 차단. 기존 Blueprint API·enum·정책·wire 계약 불변.
- Editor Development, layer 18/18(신규/stale 0), DTCore guard, V2 `-RequireAssets`, WBP 7개 메모리 컴파일 통과. 집중 13 Success/실패·skip 0. 테스트 이름은 `Saved/Reports/UXR/UXR-03/supplement/focused-classified.json`.
- 기존 테스트 기대값 변경 없음. `SensorDataWorkflowTests.cpp`에 기본 문구/접힌 문자열 비용 검증 추가. 초기 텍스트 탐색 실패는 `SNewSensorTool` 선언 이름과 독립 switcher 평가 문제로, 실제 `STextBlock` 클래스 타입과 전송 subtree를 검사하도록 수정했다. SCheckBox는 원래 선언 타입 검사 유지. 초기 실패 3회는 원본 로그에 보관했다.
- 최종 5탭×1280/1920×100/150% 자동 렌더 20장. 요청된 전송 4장은 직접 열어 확인했다. 실제 client 1280×720/1920×1080, window DPI 1.0; PNG에는 6px 테두리 포함. 150%의 아래 점검/상세는 스크롤 접근이며 마우스 접근 검수는 미수행.
- 성능: D3D12, SlabScenarioValidationMap, 실제 1920×1080, 글자 100%, 전송 탭 열림/다른 창 숨김, FullSpec D455 Camera 1+MLX80 Native GPU LiDAR 1 측정, Slab 대기, 송신·파일 저장 없음, 10초+30초. FPS 59.9968→59.9960(-0.0013%), engine frame p95 16.6669→16.6669ms(0%), callback wall p95 20.9410→20.9862ms(+0.2158%), game thread 평균 5.6673→5.7254ms(+1.0237%). 2% gate 통과. engine delta는 FApp::GetDeltaTime(엔진 pacing 포함), callback 지연과 구분. 가상 WinDisc이며 물리 디스플레이/절대 지연 목표 인증 아님.
- 기준선 production은 `3abccdf`, 동일 Editor 계측기만 먼저 빌드했다. 최종 검증은 그 HEAD+변경 소스로 수행 후 `22d54b9` 커밋; 소스/binary hash는 `validation-manifest.json`과 `baseline-manifest.json`. 전체 MA0T10 이름별 비교는 요청의 마지막 통합 단계에서 수행 예정이며 이 보완의 집중 결과로 대체하지 않는다.
- 보호 파일 475/475 불변, 개인 SaveGame 9/9 복원. DTCore `b2504d1` gitlink/worktree 불변. Editor/PIE 정상 종료. 마우스 체크리스트: `Saved/Reports/UXR/UXR-03/supplement/manual/CHECKLIST.md`.


## 2026-10-12 — UXR-04 시나리오 화면 CLI 검증

- 소스 `9477732`: 수정 7파일+추가 3파일, 이동 0. 기존 Progress/Replay 인스턴스와 DTCore 전면/입력 등록을 유지한 나란히 기본 배치, 진행 창 차트 토글, 통합 표시 설정 접힘. 사용자 저장 배치는 우선한다. 소유 native 재생 출력 편집 0개, 기본 정책·기존 API/enum/저장값·wire 계약 불변.
- Editor Development, layer 18/18(신규/stale 0), guard, V2 `-RequireAssets`, WBP 7개 메모리 컴파일 통과. 집중 28 Success/실패·skip 0, 기존 기대값 변경 없음. 신규 `MA0T10.UXR04.ScenarioScreenAndChartToggle`: 차트 토글/실행 불변/접힘 상세 비용/숨김 표시 정지/기본 재생 출력 보존.
- 최종 자동 렌더 8장: 시나리오 진행+보관 목록 및 표시 설정, 실제 client 1280×720/1920×1080×글자 100/150%, window DPI 1.0. 클릭 검수가 아니며 1280×720 150%에는 기존 커서 위치에 따른 일시적인 제목 툴팁이 함께 잡힌다. 주요 버튼·표시/하단 영역은 캡처에서 접근 위치가 보인다. 개인 저장 배치의 직접 복원/마우스 resize/scroll 검수는 보류.
- 성능 fixture: D3D12 SlabScenarioValidationMap, 실제1920×1080, 글자100%, 진행+목록 열림/다른 창 숨김, FullSpec D455 Camera1+MLX80 Native GPU LiDAR1 측정, 기존 synthetic600행을 시간값만3배로 늘린90초 관찰 실행, 10초+30초. 원본 fixture 이름의30s는 유지했으며 명시 duration과 행 elapsed는90초 기준이다.
- 처음 game thread 평균 5.9173→6.0410ms(+2.09%)로 gate 실패. 버튼 상태·텍스트 FText의 표시용 조회를5Hz 캐시로 바꿨고 최종 FPS59.9994→59.8091(-0.3172%), engine frame p95 16.6669→16.6669ms(0%), callback wall p95 20.8877→21.0707ms(+0.8761%), game thread 평균5.9173→5.9611ms(+0.7400%)로2% gate 통과. 0Hz 센서를 평균에서 제외하지 않으며 센서30/20Hz 로그를 별도로 남겼다. callback pacing과engine delta(FApp::GetDeltaTime, pacing포함)는 구분한다. 가상WinDisc이며 물리 디스플레이/절대 지연 인증은 아니다.
- 기준선 production은 `22d54b9`/HEAD`1670b4b`, Editor 계측기만 먼저 빌드(`build-instrument.log`,4actions). 최종 검증은 HEAD+변경으로 수행 후 `9477732` 커밋했다. 소스 hash·캡처/이름별 결과는 `Saved/Reports/UXR/UXR-04/validation-manifest.json`, `captures.json`, `focused-classified.json`. 전체MA0T10는 마지막 통합 단계에서 비교 예정.
- 보호475/475·SaveGame9/9 복원, DTCore `b2504d1` gitlink/worktree 불변, 운영맵·자산·Config/Game.ini·PixelStreaming·Claude전용문서 불변. Editor/PIE정상종료. 직접 체크리스트 `Saved/Reports/UXR/UXR-04/manual/CHECKLIST.md`. 초기 캐시 편집용 inline Python의 quoting 실패1회는 실제 변경 없이 발생했고 UTF-8 파일 스크립트로 수정 후 최종빌드 통과.


## 2026-10-12 — UXR-08 Slab 정보·표면 CLI 및 최종 통합 검증

- 소스`7a63971`: 수정19+추가6파일/이동0. 정보 카드5Hz, 원본 행·누락·단위·기하 계산·현재 Track위치/속도구분, DTCoreClick→weak기존Host→UDxWidget진행인스턴스선택, 기존MID프리셋/opt-in광학값/복원, optional실행결과추가. UObject/Blueprint함수·enum순서·SaveGame슬롯·기본정책·wirebytes불변. 센서플러그인화 없음.
- 읽기 전용Unreal재질조회에서Plain은Hotness/EmissiveStrength/Roughness/Metallic와ColdColor/HotColor만지원. Oxidation은없고legacy재질에는있지만기존경로해석을바꾸지않아산화스케일미지원으로남겼다. 지원4외형×4해상도/배율+정보/표시8장=24장. 150% 표시 설정은자동스크롤로 접근위치확인. 직접마우스결과가아니다.
- EditorDevelopment, layer18/18(신규/stale0,production221파일), DTCoreguard, V2 `-RequireAssets`, WBP7개메모리컴파일 통과. 집중33 Success/실패·skip0. 마지막D3D12전체MA0T10는227 실제Success/20조건부skip/실패0. UXR-03기준선241이름상태유지, 신규6이름(UXR04 1+UXR08 5)은`test-comparison.json`. 기존테스트기대값변경없음.
- 실제RHI: 별도검증용EditorWorld(운영맵로드/저장없음), 결정적BaseColor SceneCapture픽셀, 실제CPU/GPU LiDAR스냅샷, 기존GPU형상분류buffer, 동일FrameId/UTC의envelope로PCD bytes 대조. 기본/link-off 픽셀·intensity·PCD정확히동일, hot/link-off에서LiDAR/PCD동일, link-onCPU평균raw intensity61945.472→24778.194 / GPU62546.359→25018.539, JSONlatest reflectivity940Nm0.08/linktrue확인. 결과metadata는설정provenance이며실측평균을제품JSON에새로집계하지않는다. productionFinalColor/JPEG bitwise동일성은보장하지않는다.
- 초기RHI에서CPU캐시가반사율변경을무시하고GPU가고정기본반사율을쓴것을확인. 기존public캐시무효화를명시변경시1회호출하는L2adapter, GPU기존semantic identity에nativeopt-in광학값을복사하고완료시그snapshot만사용하도록보완. 새flag기본false라기존GPU출력은유지한다. 기존분류buffer가없으면link요청을원자적으로거절하고추가capture/readback을하지않는다. setting/geometry가이후바뀌는경우지원범위재확인필요.
- 초기실패보존: misplaced bounded-id검사/A미정의compile1회; 테스트Intensity/Backend필드명compile1회; PreviewWorld에Results없어accessviolation1회; ScrollBoxenum명compile1회; EditorWorldPrepare에0시각을줘scan미시작timeout1회; FString동적format형식compile1회. 모두수정후최종build/focused/full통과. 초기SceneColorHDR default/no-op카메라는6픽셀최대2채널값renderer변동이있어정확비교실패; 판정허용치를낮추지않고독립BaseColor경로와visiblepositivecontrol로정확성대조했다. GPU분류view를명시한시험조건은report에기록한다.
- 성능같은맵/1Camera+1GPU LiDARFullSpec측정/90초600행관찰시나리오진행/진행+목록열림/기본표면+linkoff/1920×1080/font100/10초+30초. FPS59.998534→59.999763(+0.00205%), engineframep95 16.666900→16.666900ms(0%), callbackwallp95 20.937402→20.841401ms(-0.45851%), gamethread평균5.936682→5.910328ms(-0.44391%). 2%gate통과. 가상WinDisc이고enginepacing와callback은분리; physicalDPI/절대지연인증아님.
- 검증은HEAD`f408338`+변경소스로수행후`7a63971`커밋; baseline은그HEAD의production`9477732` binary. sourcehash/원본로그/skip목록/이름별비교/PNG는`Saved/Reports/UXR/UXR-08/`. 보호475/475불변, 개인SaveGame9/9복원, DTCore`b2504d1` gitlink/worktreeclean, 운영맵/자산/Config/Game.ini/PixelStreaming/Claude전용문서불변. Editor/PIE정상종료(초기테스트crash는실패로분리). push/PR/merge/worktree없음. 직접 마우스 보류와 독립 검토 대기로 진행률 90%.

## 2026-10-12 UXR-08 보완 CLI

검증 기준선0881f0b, DTCore b2504d1. 소스10파일, 라벨/값 그리드·숫자 형식·원본 행 상세 분리·일시정지 속도 명시·query 크기 불변 hover marker. Editor Development/layer18/guard/V2 assets/WBP7 통과. 관련33 및 최종 영향7 이름별 Success(실패/skip0), 렌더24장, scenario+archive+monitor 1920×1080 1Camera+1GPU LiDAR 10초+30초 성능2%gate PASS(FPS+0.004%, enginep95 0%, callbackp95+0.672%, game thread+1.575%). 보호475/개인SaveGame9 불변·복원. 직접 마우스는 미수행. 상세 Saved/Reports/UXR/UXR-08/supplement/RESULT.md.

초기 V2 정적 검사는 rg PATH 부재로 실패 후 기존 rg 경로 지정으로 통과. 첫 성능 호출은 잘못된 환경변수명으로 client1280으로 실행되어 비교에서 제외하고 before-1280-invalid.json으로 보존; 올바른 MA0T10_ACCEPTANCE_RES_X/Y 지정 후 실제1920×1080 기준선을 확보했다. source-only marker 종료 cache 갱신 뒤 incremental build와 영향7을 다시 통과했다. 처음 apply_patch는 동일 파일 delete/add 중복 대상으로 검증 단계에서 거절되어 파일 변경 없이 Update로 재적용했다. inline Python -c의 Windows 인용 실패는 파일 변경 없이 종료하고 Saved의 명시 Python 파일로 대체했다.
