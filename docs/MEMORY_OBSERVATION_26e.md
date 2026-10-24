# MEMORY-26e — 해치 결과 복사 제거와 패키지 추적 반복

기준 `15326e6`, branch `fix/hatch-observation-copy-26e`, 실측 Runtime `eb4d515`다. **해치 결과의 deep copy를 제거했지만 메모리 안정화와 전체 RSS 원인 귀속은 미해결이다.** Development package20분 trace2회·비추적60분1회를 완결했다. 모든 실행은 정상exit0·complete=true·영상 디코딩0이다. 제품/엔진 기본값·PCD 계약·예약 상한은 바꾸지 않았다.

[RESULT](../Saved/Reports/MEMORY-26e/RESULT.md)·[result.json](../Saved/Reports/MEMORY-26e/result.json)·[merge-check](../Saved/Reports/MEMORY-26e/merge-check.json), [비교](../Saved/Reports/MEMORY-26e/comparison.json)·[상위 스택 CSV](../Saved/Reports/MEMORY-26e/top-allocations.csv)·[소스 근거](../Saved/Reports/MEMORY-26e/source-evidence.json)를 함께 본다. 기존2026-10-23 RSS 미해결 수용과 원래8MiB/90초/5쌍 기준을 유지한다.

## 결과를 한 번 이동하는 경로

설치된 UE5.3 `Async/Future.h`의 `TFuture::Get()`은 값을 복사하며 `Consume()`은 없다. L0 [ConsumeReadyFuture](../Source/ma0t10_dt/MA0T10/Contracts/ConsumeReadyFuture.h)는 완료된 unshared future를 먼저 이동해 원래 future를 무효화하고 결과를 한 번 이동한다. UE5.3이 const accessor만 제공하는 mutable state에 한해 `const_cast`를 사용한다. caller는 state를 공유하거나 continuation을 설치하지 않아야 한다. 이 세 caller는 `Async`의 단일 consumer이고 IsReady 확인 후 호출한다. 엔진 수정·추가 heap allocation·게임 스레드 대기는 없다.

- `ShipHatchMonitorWidget::RefreshAnalysis`: Observations·Analysis·ComparisonScratch 등을 이동한다. 실제 FResult를 move-only로 만들어 `Get()` 복사 재도입을 컴파일 단계에서 차단했다. revision/cab/cell 검사와 frame-ID 분기를 유지하며 새 frame이 없어도 소비한 Pending은 invalid다.
- `ShipHatchSimulation::PollGeneration`: heightfield·shape 배열을 이동하고 pending 재생성·scratch 재사용을 유지한다.
- `AgentTestSetPcdPreview::Pump`: 저장 PCD의 큰 point 배열을 Ready로 이동하며 Read의 invalid/재생/종료 흐름을 유지한다.

다른 tracked Source의 future 선언/소비도 검색했다. shared data pointer·작은 오류/파일명/상태 결과는 변경하지 않았다. [검색·판단](../Saved/Reports/MEMORY-26e/future-audit.json).

신규 `MA0T10.Hatch.AnalysisFutureConsumesBuffers`는 실제 move-only FResult로 결과 값·배열 주소·capacity·pending/ready/소비 후 invalid를3회 검사한다. NullRHI1 Success, D3D12 관련22 Success/1 Fail/0 Skip, Python 분석 회귀23 PASS다. HATCH06의 탱크톱 시선·회색 픽셀4assertion 실패는 기준15326e6에서도 동일하며 test source도 일치한다. 이를 PASS로 바꾸지 않는다. [기준 비교](../Saved/Reports/MEMORY-26e/baseline-comparison.json).

## 같은 패키지의 조건과 기울기

기본binned2·Rev7×3/1024×64/요청20Hz·실제HWRT3·기본Full 압축PCD/FIFO/80MiB/s·blank127.0.0.1 receipt+echo·기본 미리보기·1920×1080 offscreen/font100/UMG1·LLM·5초 이상 sampler다. 각 실행은17회 public Dig/분석/원복+35초 준비 후 idle이며 측정 중 디깅/복원은0회다. 동일 exe/PDB 해시와 소스 fingerprint를 보관했다.

**12,000점은 센서별 미리보기 예산이다. PCD의12,000점 제한이 아니다.** 실제 Full PCD는 hit 약3.08만/5.07만/3.91만점을 보냈고26d·26c도 같은 범위다. 이전26d 문서의 `12,000점/센서` 표현은 이 preview 조건으로 읽어야 한다. 실제 point 수·센서별 Hz·PCD 단계별 카운터는 comparison.json에 보관했다.

| 실행 | 초/표본 | RSS 꼬리 MiB/h | Commit 꼬리 MiB/h | RHIMisc 꼬리 MiB/h | UObject/h | 판정 |
|---|---:|---:|---:|---:|---:|---|
| 26d trace, 과거 참고 | 1202.72/241 | +8.84 | −58.91 | +14.98 | 0 | 누수 의심 |
| 26e trace1 | 1202.70/241 | +102.74 | +112.39 | +8.77 | 0 | 누수 의심 |
| 26e trace2 | 1202.95/241 | +34.07 | −43.16 | +6.95 | 0 | 누수 의심 |
| 26c 비추적, 과거 참고 | 3603.08/720 | +50.17 | +18.68 | +11.62 | 0 | 누수 의심 |
| 26e 비추적 | 3602.87/720 | +17.50 | +13.84 | +11.81 | 0 | 누수 의심 |

20분 trace는26d와 같은 min1200/tail400 별도 진단이며8MiB/h 기준을 유지한다. 비추적60분은 기존 min1800/tail600을 사용하고 실제 꼬리는 마지막20분이다. RSS17.50MiB/h도8MiB/h를 넘는다. 과거60분보다 낮은 수치를 복사 제거의 인과 효과로 확정하지 않는다. 동일 요청 조건이어도 traced/비추적 및 과거 실행의 실제 처리량·작업 위상·환경 차이가 있다. 원래 RSS5쌍 gate의 재통과 자료가 아니다.

실제 획득률은 trace1 15.331~15.332Hz, trace2 15.401~15.462Hz, 비추적16.278~16.684Hz다. 요청20Hz를 낮추거나0Hz 센서를 평균에서 제외하지 않았다.20Hz cadence 수용 통과로 표시하지 않는다. sampler GT 평균/최대는 각각2.7980/7.2615ms,4.6415/11.9792ms,2.1691/8.4493ms다. 측정 실행의 환경 표본은247/247/718개이며 decoder 최대0%다.

인코딩·전달·내부 검증 오류/overload/duplicate는0이다. 송신 제어 생략은 trace1 센서별4/4/4, trace2 4/4/5, 비추적13/14/16프레임이고 내부 수신 frame gap도 각각 같다. acquisition·serialized·submitted·receipt·검증 수신의 카운터를 분리했다. 외부 probe에는 사전준비가 포함되고 내부 publisher 카운터는 측정 시작 기준으로 차감한다. receipt는 업무 ACK가 아니며 무손실 수용 통과를 주장하지 않는다.

## 반복된 CPU 경로와 한계

두 trace는 시작5분 뒤→END 직전을 같은 live-only reader로 비교했다.5,411,113,168/5,444,627,290events, 비교창미대응0·분석exit0다. 새 game PDB와 이미지가 일치하고 프로젝트 private 소스 줄이 해석됐다. Editor engine private PDB도 별도로 검증했다. game engine public 이름은 해당 game PE unwind/chained 함수 시작 경계와 일치할 때만 사용했으며 nearest export·경계 미입증 이름을 원인 해석에서 제외했다. 엔진 로컬 소스 줄은 같은 함수의 정의/호출 경로 근거이며 game 명령 주소의 줄로 이식하지 않았다.

| CPU user 상위 양의 경로 | 26d 참고 MiB | 26e trace1 MiB / count | 26e trace2 MiB / count | 두 새 실행의 후보 |
|---|---:|---:|---:|---|
| 해치 observation future 복사 | +3.6794 | 상위50 없음 | 상위50 없음 | copy 금지 타입으로 제거 확인 |
| D3D12 query 배열 | +2.3195 | +2.3200 / +3 | +2.3200 / +3 | 예 |
| profiler event-string cache | +0.5645 문자열, +0.3052 map | +0.2901 / +1118 | +0.3069 / +1183, +0.3052 / +1 | 예, 이미10,000개 한도 |
| PCD encode 버퍼 | +0.6333 | +0.6331 / +1 | +0.1901 / 0 | 예, 일시 보유/capacity 차이 미분리 |
| RT hit/readback 버퍼 | +2.0000 | +2.0000 / +1 | 상위50 없음 | 제외 |
| scene point lease | 상위50 없음 | 상위50 없음 | +25.0000 / +1 | 제외 |
| preview batched points | 상위50 없음 | 상위50 없음 | +1.4992 / +1 | 제외 |

후보는 두 새 trace의 **반환된 상위CPU user 양의 스택**에 반복된 경로라는 뜻이다. 전체 family의 net·physical RSS·무제한 누수를 뜻하지 않는다. 누락 행은0할당의 증거가 아니다. 관측 복사 제거는 상위 목록 부재뿐 아니라 move-only 컴파일·주소/capacity 회귀로 확인했다.

전체 CPU user live 차이는 trace1−0.1473MiB, trace2+30.6039MiB이며 backing은−1.6444/+30.7305MiB다. 포함 관계인 user/backing을 합산하지 않는다. trace2의25MiB는 `AcquireScenePointStorage` game PDB451행, 로컬450행 fallback Reserve에 해당하는 point 배열1개다. 세 lease를 소비자가 보유하거나 cleanup 중이면 일반 저장소를 예약하는 경로다. retained pool과 외부 소비자 보유를 구분해야 하며 fallback 빈도/무제한 수명을 이 한쌍으로 확정할 수 없다. 한 실행의 큰 차이를 반복 누수 후보로 올리지 않았다.

**GPU alias 집계는 기존 calibration 한계로 미검증이다.** GPU root 값은 raw로 분리하며 실제 GPU 순증가/누수의 증거로 쓰지 않는다. CPU combined 회계 검증과 GPU 검증을 혼동하지 않는다. RSS 증가가 tracked CPU live 증가와 매번 일치하지 않아 allocator 보존/작업 위상/engine·driver의 미귀속 native 경로를 계속 구분해야 한다. UObject 꼬리0만으로 native 누수를 배제하지 않는다.

## 완화 제안과 gate

추가 제품 수정은 없다. D3D12 query-family Num/Max/fence-age를 queue별 저빈도로 확인하고 CPU-visible fence 뒤 완료 배열 재사용·idle pool 예산을 검토할 수 있다. unresolved query 절단이나 매프레임 GPU drain은 제안하지 않는다. `r.gpucrash.collectionenable 0`은 crash breadcrumb/context를 잃는 별도 진단 비교만 제안하며 기본값은 유지한다. cache는 이미10,000개 한도다. allocator trim/cache 변경은 실제 컴파일 변형과 보존 통계를 확인한 뒤 지연·처리량 비용을 검증해야 한다. point lease는 필요 시 available/pending-reset/fallback/소비자 보유 계측을 먼저 추가한다.

표시형1920×1080·1Camera+1LiDAR·준비10초+60초·RT on ABAB1블록(A2/B2)은 모든 실행 valid·decoder0이지만 **GPU mean+2.4487%로 FAIL**이다. FPS+0.0074%, engine p95+0%, GT−0.2811%는 각2% 기준 안이다. A GPU6.6274→6.3627ms/B6.9218→6.3865ms 변동과 원인 미확정을 보존했다. 이 표준 workload는 해치 결과 소비 자체를 분리 측정한 실험이 아니다. 기준 완화·선별 재측정·추정 GPU 수정은 하지 않았다.

Editor/Development package build·순수 C++/Python 회귀·quick·layer/allowlist0→0·DTCore guard는 통과했다. 관련 HATCH06 기존 실패와 GPU gate 실패를 그대로 result/merge_check에 반영한다. 전체 release·직접 마우스 수용·회사/실장비 수용·원래 RSS5쌍은 별도이며 이번에 수행하지 않았다.

개인44개 원본 집합/해시·보호483개·Runtime 소스 해시 일치를 확인했다. 기존 사용자 Map/Config/PixelStreaming/Claude 메모 상태를 보존하고 DTCore/gitlink를 수정하지 않았다. trace2파일93,851,147,494bytes와 패키지663파일2,558,051,912bytes를 SHA·요약 보존 후 삭제했다. 패키지 빈 디렉터리만 남았다. 엔진 PDB/엔진 binaries는 건드리지 않았다.

자동 승인 검토가 작업 소유 성능 사본 `Saved/PerfAbab-20261024T143717Z-owned`의 재귀 삭제를 `blocked by policy`로 차단했다. 사본3693파일10,550,737,393bytes와 정리 목록을 보존했으며 우회 삭제하지 않았다. 다른 기존 사본은 건드리지 않았다. push/PR/merge는 하지 않는다.
