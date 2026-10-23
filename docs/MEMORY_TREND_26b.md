# MEMORY-TREND-26b — 장시간 RSS 진단과 직렬 실행 계획

2026-10-23, 기준 `98e2131`, DTCore `b2504d1`. 이 문서는 #26b의 계측·분석 도구와 후속 실행 계획이다. **기존 RSS 실패는 미해결**이다. 이번 병렬 worktree에서는 Editor/UBT/실측/GPU를 실행하지 않았다. C++ 컴파일·실제 샘플러 비용·30~60분 추이는 직렬 단계에서 확인해야 한다.

## 저장 방식과 비용

- [ShipHatchDigTests.cpp](../Source/ma0t10_dtEditor/Private/ShipHatchDigTests.cpp)의 기존 `CurvePoint`/`memory_curve` 행 계약(`seconds`, `cycle`, `rss_bytes`, `commit_bytes`, `uobject_count`, `llm_default`)을 재사용한다. 기존 `Dig`/`DigIdle`의 50회·90초·8MiB assertion은 유지한다.
- 새 opt-in 자동화 `HATCH26.MemoryTrendDigRhi`/`MemoryTrendIdleRhi`는 `-HatchMemoryTrend`가 없으면 `[OPT-IN SKIP]`이다. 일반 회귀가 30분을 기다리지 않으며 skip을 실제 통과로 세지 않는다. [실행기](../Scripts/run_hatch_validation.ps1)의 `MemoryTrendDig`/`MemoryTrendIdle` 모드가 플래그와 `-llm`을 전달한다.
- 실행 중 `memory-trend.jsonl`에 metadata, `type=memory_curve` 행, 종료 summary를 순차 기록하고 매 샘플 flush한다. 중단 직전까지의 완전한 행은 남지만 완료 summary가 없으면 판정은 데이터 부족이다. 같은 파일이 있는 보고서 경로는 덮어쓰지 않는다.
- 기본 간격 5초, 허용 5~30초, 관찰 1,800~3,600초다. 밀린 샘플을 몰아서 쓰지 않는다. 종료는 요청 시간 이후 다음 정규 샘플에서 하므로 실제 시간은 최대 한 간격 정도 길 수 있다. 행 배열을 메모리에 누적하지 않으며 재사용 태그 map과 상위 16개 선택 배열을 사용한다.
- RSS=`FPlatformMemory::GetStats().UsedPhysical`, Windows process commit=`UsedVirtual`이다. UObject는 기존 O(1) `GUObjectArray.GetObjectArrayNumMinusAvailable()` 점유 슬롯 수로 pending-GC 객체를 포함한다. 살아 있는 객체를 순회하거나 강제 GC하지 않는다. 각 표본에는 센서별 frame_id·실제 acquisition backend enum 값·running·preview point budget도 저장한다. enum 해석은 동일 SHA의 [타입 정의](../Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorDeviceProfileTypes.h)를 따른다.
- LLM Default의 기존 고정 10태그와 `Untracked`/`TrackedTotal`/`Total`/`FMalloc`(존재 시), Default·Platform 각각 상위 16태그를 저장한다. LLM 미지원/비활성은 `llm_enabled=false`다. LLM이 매 프레임 갱신한 값을 읽고 별도의 `UpdateStatsPerFrame`, memreport, readback, render/GPU drain, `FMemory::Trim`을 장시간 경로에 추가하지 않는다.
- `previous_sample_total_gt_ms`는 직전 샘플의 수집·상위 선택·문자열 직렬화·파일 write/flush·임시 JSON 해제까지 포함한 GT wall 비용이다. 마지막 샘플 비용과 전체 합계/최댓값은 summary에 기록한다. metadata/summary 자체 쓰기와 기존 자동화·클릭 검사의 비용은 이 값에 포함되지 않는다. 파일 I/O도 GT에서 수행하므로 실제 디스크 지연은 직렬 실행에서 반드시 확인한다. 간격 제한은 비용 검증을 대신하지 않는다.
- 공유 초기화는 기존 17회 완전 클릭/측정/분석 및 35초 안정화다. 측정 중 Dig는 분석 완료를 기다려 최소 2초 간격으로 클릭하고, 성공한 50회마다 원래 원료로 복원한다. Idle은 같은 초기화 뒤 측정 구간 클릭 0회다. 이 반복 workload는 원래 50회 단발 gate와 구분한다. 화면·센서·PCD 기본 정책·할당자를 바꾸지 않는다.

## 분석과 진단 기준

[analyze_memory_trend.py](../Scripts/analyze_memory_trend.py)는 JSONL과 과거 `memory_curve` JSON을 읽고, **실제 경과시간**을 시간 단위로 변환해 전체/마지막 시간 1/3의 OLS 기울기를 계산한다. RSS/Commit은 MiB/시간, UObject는 개/시간이다. 끝점 단일 표본의 영향을 줄이기 위해 앞/뒤 각 20% 표본의 중앙값 차이도 기록한다. 연속 표본은 상관되어 있으므로 통계적 유의성이나 독립 표본의 신뢰구간을 주장하지 않는다.

기본 진단 한도는 메모리 및 연속 관측 Default LLM 태그 **8MiB/시간**, UObject **10개/시간**이다. 이는 새 경향 분류용 수치이며 **원래 절대·대조 차이 8MiB gate를 변경하거나 대체하지 않는다**. 명령줄 옵션으로 다른 진단 한도를 지정하면 출력 JSON에 함께 남는다. LLM 계층 태그는 중복되므로 합산하지 않는다. 상위 목록에서 빠진 태그를 0으로 채우지 않고 관측 비율을 표시하며, 꼬리 구간 전부에 존재하는 태그만 기울기를 계산한다. Platform 상위 목록은 원본에 보존하고 Default 태그와 별도로 검토한다.

| 판정 | 규칙 |
|---|---|
| 데이터 부족 (exit 2) | 완료 표시 없음, 전체 30분 미만, 마지막 시간 1/3이 10분 미만/12표본 미만, 간격 5초 미만 또는 60초 초과, 꼬리 구간 LLM 누락 등. 최대 공백 60초는 30초 설정의 프레임 지연을 허용하는 진단 옵션(`--max-gap-seconds`)이다. 마지막 1/3 양 끝의 이산 표본 정렬은 최대 이 공백 한도만큼 허용한다. 기존 90초 JSON은 기술적 기울기만 제공한다. |
| 누수 의심 (exit 1) | 충분한 데이터에서 RSS/Commit/UObject 또는 연속 관측 Default 태그의 꼬리 OLS 기울기나 중앙값 차이/관찰시간이 진단 한도를 넘는다. 할당자/렌더러/드라이버 보존도 이 분류에 들어갈 수 있으므로 확정 누수는 아니다. |
| 안정화 (exit 0) | 위 관측 범위에서 양의 증가가 모두 진단 한도 이내다. 해당 workload/시간창에 한정하며, 미관측 태그·아주 느린 누수·상쇄되는 할당 변화의 부재를 증명하지 않는다. |

누락 필드·NaN/무한대·중복/역행 시각·중간 JSONL 손상·종료 표본 수 불일치는 입력 오류(exit 2)다. 실행마다 따로 분석하고 다른 SHA/조건의 곡선을 이어 붙이지 않는다. `sampler_gt_ms`의 평균/최댓값과 실제 관측 비용 수를 출력한다.

## 보존된 #26 증거 재분석

읽은 원본은 메인 `Saved/Reports/FOLLOWUP-28-26/r2/`의 JSON 19개와 `RSS-26.md`, `memory-diagnosis.md`, 이전 `FOLLOWUP-26-28/RESULT.md`, [검증 이력](VALIDATION_HISTORY.md)의 #26 기록이다. 제거된 원시 CSV/log를 복원하거나 새 실측으로 표시하지 않았다. 재현 도구는 [reanalyze_hatch_memory.py](../Scripts/reanalyze_hatch_memory.py)이며 원본 상대 경로·SHA256·각 gate·태그 차이·곡선·자원 비교를 worktree `Saved/Reports/MEMORY-TREND-26b/historical-reanalysis.json`에 남겼다.

| 기본 GPU 5쌍 | 디깅 raw RSS MiB | 대조 raw RSS MiB | 차이 MiB |
|---|---:|---:|---:|
| 1 | -9.031250 | 15.343750 | -24.375000 |
| 2 | 18.781250 | 19.628906 | -0.847656 |
| 3 | 4.769531 | -5.812500 | 10.582031 |
| 4 | 15.484375 | -2.187500 | 17.671875 |
| 5 | 10.429688 | 12.207031 | -1.777344 |

원래 절대 RSS는 2/4/5쌍, 차이는 3/4쌍 FAIL이다. 중앙값 디깅 **+10.429688MiB**, 최대 차이 **+17.671875MiB**이며 retained 최대 차이 +18.257813MiB도 보존한다. Commit은 독립 검사로 재분석 JSON에 남긴다. 기본 5쌍은 곡선/LLM이 비어 있으므로 이 5쌍에서 UObject나 태그 추이를 추정할 수 없다.

별도 LLM 진단 디깅/대조 및 deep 디깅의 각 51표본·약 90초 곡선은 UObject **43,181→43,181, 범위 0**이다. 각 원본의 소유 자원(업로드 횟수 제외)과 RHI 종류별 수/bytes도 전후 동일하다. 짧은 마지막 1/3 RSS OLS는 디깅 +865.758/대조 +283.152/deep +383.388MiB/시간이지만 이는 약 30초의 잡음을 환산한 기술적 수치이며 장시간 누수율로 외삽하지 않는다. **세 곡선 모두 장시간 판정은 데이터 부족**이다.

| 원인 후보 순위 | 지지 근거 | 반증·남은 한계 |
|---|---|---|
| 1. 할당자/엔진 풀 보존과 RSS 상주 변동 (잠정) | 대조도 +15.34~19.63MiB 등 증가. deep `FMallocUnused` +3.412251MiB, LLM 대조 +5.658151MiB. 소유/RHI/UObject 수량 일정. | LLM 디깅 `FMallocUnused`는 **-5.287662MiB**이므로 보존만으로 설명되지 않는다. Mimalloc 상세 dump는 UE5.3 미구현. 정상 plateau는 아직 관찰하지 못했다. |
| 2. Untracked native/렌더러/드라이버 메모리 증가 (실제 누수 포함) | Default `Untracked`: LLM 디깅 +8.533692/대조 +6.901413/deep +7.749617MiB. deep PlatformUntracked +7.886719MiB. | 미계측 영역에는 정상 보존도 포함된다. 태그 차이를 합산하거나 모두 누수로 귀속할 수 없다. 장시간 native allocation stack 귀속이 필요하다. |
| 3. 프로젝트 소유 UObject/센서 버퍼/RHI 자원 누수 | 소유하지 않은 native 증가를 배제하지 못하므로 후보는 유지한다. | 관측한 90초에서는 UObject 범위 0, 소유 bytes/프록시·RHI 수량 동일, deep point-pool fallback 0. 따라서 현재 근거로는 우선순위가 낮다. |

별도 Binned2 진단은 raw RSS +13.457031MiB, allocator OS allocation +2.617187MiB여서 노출된 allocator 통계만으로 전체를 설명하지 못했다. 이는 보존된 Markdown 요약 근거이며 원시 allocator log 재검증은 불가능하다. 기본 Mimalloc은 변경하지 않는다. 태그 재사용/CPU scratch 후보의 원래 절대 RSS 실패와 원복 사실도 유지한다.

## 직렬 실행 계획 — 이번 병렬 작업에서는 실행 금지

다른 Editor/빌드/측정이 끝난 뒤 통합한 동일 SHA와 DTCore에서 수행한다. 먼저 Editor Development를 정상 빌드하고 기존 Dig 관련 회귀를 확인한다. 아래 두 프로세스는 **순서대로** 실행하며, 사용자의 개인 SaveGame/Editor 설정 파일 집합과 해시를 실행 직전에 백업하고 완료/오류 시 복원한다. 실행기는 환경변수만 복원하므로 개인 파일 백업·복원은 직렬 담당자의 책임이다. 맵은 저장하지 않는다.

고정 조건: `/Game/MA0T10/Maps/Tests/ShipHatchTestMap`, Rev7×3/1024×64/요청20Hz, Auto→실제 HWRT, D3D12/ProjectDefault RT on, 1920×1080 오프스크린, font100%, 12,000점/센서, view1, 공유17회+35초 준비, 5초 샘플, 기본 Mimalloc, 출력/PCD 정책 동일. 실제 backend·센서 수/프레임 전진·출력 상태는 원본/로그로 확인하며 fallback 실행을 HWRT 결과로 세지 않는다. source-manifest와 summary에 실제 클릭/복원 수 및 시간·비용을 남긴다.

```powershell
# 직렬 담당자가 통합/빌드한 checkout에서만 실행한다. 총 관찰 약 60분(각 30분).
$trendRoot = 'Saved/Reports/MEMORY-TREND-26b/serial-' + (Get-Date -Format yyyyMMddTHHmmss)
powershell -NoProfile -File Scripts/run_hatch_validation.ps1 -Mode MemoryTrendIdle -Sensors 3 -MemoryWarmupSeconds 35 -DigView 1 -MemoryTrendSeconds 1800 -MemorySampleSeconds 5 -ReportRoot "$trendRoot/idle"
powershell -NoProfile -File Scripts/run_hatch_validation.ps1 -Mode MemoryTrendDig -Sensors 3 -MemoryWarmupSeconds 35 -DigView 1 -MemoryTrendSeconds 1800 -MemorySampleSeconds 5 -ReportRoot "$trendRoot/dig"
python Scripts/analyze_memory_trend.py "$trendRoot/idle/memory-trend.jsonl" --output "$trendRoot/idle/analysis.json"
python Scripts/analyze_memory_trend.py "$trendRoot/dig/memory-trend.jsonl" --output "$trendRoot/dig/analysis.json"
```

중단/LLM 누락/간격 공백은 원본을 보존하고 데이터 부족으로 보고한다. 둘 다 안정화하면 보존 가설의 지지가 강화되지만 기존 gate 실패는 유지한다. 디깅만 증가하면 클릭/메시/분석 경로, 둘 다 증가하면 공통 센서·렌더러·할당자 쪽을 우선 조사한다. 필요하면 **별도 실행 승인 범위에서** 순서를 반대로 하거나 각 3,600초로 연장한다. 경향 분석만으로 기본값/allocator를 바꾸거나 누수 해결로 선언하지 않는다.

계측 비용은 `sampler_gt_ms` 평균/최대와 summary 합계/관찰시간으로 보고한다. 진단 실행의 FPS/Hz를 제품 수용 PASS로 사용하지 않는다. 실제 오버헤드 수용은 계측 off/on 동일 조건 교차 실행과 기존 2%/150ms 기준으로 따로 확인한다. 원래 35초+90초·50회/0회 교차 5쌍과 절대·차이 8MiB 검증은 별도 유지하며 장시간 결과가 이를 덮지 않는다. 회사/실장비 수용도 별도다.

## 빌드 없는 검증

```powershell
python -m unittest discover -s Scripts -p 'test_*memory*.py' -v
python Scripts/reanalyze_hatch_memory.py 'C:/Unreal Projects/ma0t10_dt/Saved/Reports/FOLLOWUP-28-26/r2' --output Saved/Reports/MEMORY-TREND-26b/historical-reanalysis.json
```

단위 테스트는 선형 증가·초기 증가 후 plateau·Commit/UObject/태그만 증가·경과시간/간격·불완전 파일·누락/비정상 수치·상위 태그 관측 누락·CLI 판정과 비용 집계를 검사한다. UE 컴파일, 수명/중단 동작과 실제 파일 I/O 비용은 이 테스트의 증명 범위 밖이다.
