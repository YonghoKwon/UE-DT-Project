# MEMORY-TREND-26b — 장시간 RSS 진단과 직렬 실행 계획

생성 2026-10-23, DTCore `b2504d1`. 최신 soak2는 기준 `4419436`, 측정 코드 `ce456eb`이며 PCD-on idle/디깅 각30분과 원래 RSS5쌍, 샘플러 off/on 비교를 완료했다. **RSS 실패는 미해결**이다. 아래 병렬 재분석, `a897c43`의 영상 감지 중단 자료, 완결된 soak2를 각각 구분한다. 과거 부분 자료를 새 곡선에 이어 붙이지 않았다.

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

## 직렬 실행 계획 — 2026-10-24 승인, 영상 디코딩 없는 환경 필수

다른 Editor/빌드/측정이 끝난 뒤 통합한 동일 SHA와 DTCore에서 수행한다. 먼저 Editor Development를 정상 빌드하고 관련 회귀를 확인한다. 아래 두 프로세스는 **순서대로** 실행하며, 개인 SaveGame/Editor 설정 파일 집합과 해시를 실행 직전에 백업하고 완료/오류 시 복원한다. 개인 파일 복원은 호출자의 책임이다. 맵은 저장하지 않는다. [직렬 실행기](../Scripts/run_memory_trend_serial.ps1)는 세 번의 사전 GPU decoder 검사와 실행 중5초 감시, 브라우저/미디어 프로세스 이름·PID 스냅샷을 기록한다(제목/URL/명령줄은 기록하지 않는다). 감지 시 취소 파일로 정상 PIE 종료하고 실패/미완료 자료를 보존한다. 사용자 지시대로 그 후 측정은 자동 재시작하지 않는다.

실행기는 기존 Raw TCP transport/publisher를 사용하는 진단용 PCD loopback을 연결한다. 기본 Full 프로필·압축 On·80MiB/s·FIFO·33-byte 레코드 계약을 유지하며 자격증명은 비운다. Native LZF 검증 DLL을 사용하고 loopback의 개별 frame 목록은0개로 제한해 장시간에는 누적 카운터만 기록한다. DLL은 기존 `Tools/Artemis/lzf_validate.c`를 빌드한 결과이며 Saved 아래에만 둔다. 원래 #26 조건 비교는 `Dig`/`DigIdle -WithoutPcd`(35초+90초·50회/0회 교차5쌍)로 별도 실행하며 새 PCD-on 장시간 자료와 합치지 않는다.

고정 조건: `/Game/MA0T10/Maps/Tests/ShipHatchTestMap`, Rev7×3/1024×64/요청20Hz, Auto→실제 HWRT, D3D12/ProjectDefault RT on, 1920×1080 오프스크린, font100%, 12,000점/센서, view1, 공유17회+35초 준비, 5초 샘플, 기본 Mimalloc, 출력/PCD 정책 동일. 실제 backend·센서 수/프레임 전진·출력 상태는 원본/로그로 확인하며 fallback 실행을 HWRT 결과로 세지 않는다. source-manifest와 summary에 실제 클릭/복원 수 및 시간·비용을 남긴다.

```powershell
# 영상 없는 환경을 확인하고 개인 설정을 백업한 뒤 새 경로에서만 재실행한다.
$trendRoot = 'Saved/Reports/MEMORY-TREND-26b/serial-' + (Get-Date -Format yyyyMMddTHHmmss)
$validator = 'Saved/Reports/MEMORY-TREND-26b/serial-20261023T233614Z/native/lzf_validate.dll'
powershell -NoProfile -File Scripts/run_memory_trend_serial.ps1 -Mode MemoryTrendIdle -ReportRoot "$trendRoot/idle" -NativeValidator $validator -Seconds 1800
powershell -NoProfile -File Scripts/run_memory_trend_serial.ps1 -Mode MemoryTrendDig -ReportRoot "$trendRoot/dig" -NativeValidator $validator -Seconds 1800
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

## 2026-10-24 직렬 부분 결과와 중단

원본: `Saved/Reports/MEMORY-TREND-26b/serial-20261023T233614Z/`.

- Editor Development 빌드3회 exit0. C++ `HATCH26.MemorySamplerContract` 1 Success(NullRHI·행/태그 상한/중복 샘플 억제/미완료 footer 계약), D3D12 `MA0T10.PcdBandwidth` 8 Success, 실제 HWRT+PCD `HATCH03.IdleMemoryControlRhi` 1 Success다. Python 회귀23 Success도 유지한다. 테스트한 C++ 소스 bytes는 최종 측정 커밋과 동일하다.
- 90.7208초 PCD qualification은 센서별1800/1800/1799프레임의 input/serialize/submit/receipt/내부 수신 일치, 구간 gap/invalid/오류0이다. 초기 wrapper의 null ExitCode 보고 오류는 실제 자동화 Success 원문과 별도 audit로 구분했고 process handle 유지 수정 및 기대 exit7 fixture를 검증했다. 처음 기록을 덮어쓰지 않았다.
- PCD-on 대조에서 **2026-10-24 00:20:18.0819251 UTC(09:20:18 KST), NVIDIA video decoder 1%**를 감지해 취소했다. 전역 GPU counter 근거이며 재생 프로세스 귀속은 확정하지 않는다. 사용자 앱을 닫거나 조작하지 않았다. 최초3회 사전 검사 및 그 전 감시 표본은0이었다. 취소 후 Editor/PIE가 종료됐고 반복 디깅·원래 RSS5쌍은 시작하지 않았다.
- 관찰1634.9836초, 표본327개(마지막 표본1632.7974초), footer `complete=false`, 실제 자동화는 취소 사유로 Fail1이다. **분석 판정 데이터 부족(exit2)**: 30분 미달·꼬리 구간10분 미달·미완료다. 안정화/누수 의심 완료 판정이나 수용 PASS로 바꾸지 않는다.
- 모든 표본의 세 센서 backend는 HWRT다. RSS 최종−첫 표본 +78.210938MiB, Commit +145.613281MiB. 기술적 OLS는 전체 RSS74.1580/Commit100.6083MiB/시간, 부분 마지막1/3(약9분) RSS147.8141/Commit292.7480MiB/시간이다. 미완료·환경 중단 시료의 기술적 수치이며 장시간 누수율로 확정하지 않는다.
- UObject는46515→46363(꼬리 기울기0개/시간). 고정 해치 Cook/RayTree/Update 태그 증가0, UObject 태그−0.204820MiB다. Default `Untracked` +90.613137MiB, `FMallocUnused` +26.079243MiB, `RHIMisc` +6.287968MiB다. 계층 태그는 합산하지 않는다. 우선 후보는 공통 측정/송신/RHI의 native 미계측 증가와 allocator 보존이며, 디깅을 하지 않은 실행이므로 디깅 고유 증가를 귀속할 수 없다. 실제 누수와 보존의 분리는 미완료다. 근거가 불충분해 제품 메모리 수정은 하지 않았다.
- 샘플 GT 비용327개 평균 **1.900630ms**, 최대 **3.695302ms**, 합계621.506080ms. 실제 수집/JSON/write/flush 비용이며 off/on 교차2% 성능 gate는 아니다. 종료 당시 admission skip 및 내부 gap은센서별8/8/9로 함께 남긴다(encode/delivery/invalid/overload/duplicate0). in-flight 경계 때문에 한 센서의 receipt가 측정 submit보다1개 많다. 무손실/완료 수신 수용으로 표시하지 않는다.
- 시작 전 개인44파일 백업 후 원본 집합·SHA256 복원, 보호481파일 일치. 새 개인 파일 제거0. 사용자 SensorTestMap 변경·DTCore b2504d1·Config/Game.ini·PixelStreaming·Claude 메모는 보존했다. 초기 D3D12 시작의 SwapChain4 ensure도 원문에 남기며 무오류 실행으로 숨기지 않는다.

위 기록은 첫 직렬 시도의 부분 자료다. 후속 soak2는 별도 새 경로에서 아래와 같이 수행했으며 원래8MiB 기준은 유지했다.

## 2026-10-24 soak2 완결 결과

원본은 `Saved/Reports/MEMORY-TREND-26b/soak2/`이다. `long-idle`/`long-dig`의 JSONL·분석, `gpu-default-five-pairs`의10개 원본, `sampler-cost`의12개 원본과 비교표, 환경 스냅샷·SHA·복원 증거를 보존한다. [곡선](../Saved/Reports/MEMORY-TREND-26b/soak2/memory-curves.png)과 [전체 결과](../Saved/Reports/MEMORY-TREND-26b/soak2/RESULT.md)를 함께 본다.

### 조건과 완료 범위

- 기준4419436/코드ce456eb/DTCore b2504d1, 같은 Source/asset616개 해시. Rev7×3/1024×64/요청20Hz/실제HWRT·ShipHatchTestMap·1920×1080 offscreen·D3D12/RT ProjectDefault·font100%·12,000점/센서·17회+35초 준비·기본Mimalloc이다. PCD-on 장시간/비용 비교는 Full 압축/FIFO/80MiB/s 및 무자격 loopback receipt+echo를 사용한다. 원래 RSS5쌍은 PCD-off·audit/-llm 플래그 없음·90초·50회/0회다.
- 모든24실행과1473개 실행 중 환경 표본에서 video decode 최대0%. 사전3회 검사·브라우저/미디어 process 이름/PID 스냅샷도 각 실행에 남겼다. user 앱/설정/영상 재생을 조작하지 않았다.
- C++ 순수 계약1 Success(NullRHI), 장시간2 Success, 원래 RSS 대조5 Success/디깅5 Fail, 실제 off/on12 Success로 합계 **20 Success/5 Fail/0 Skip**다. 모든 Fail은 원래 메모리 assertion이며 다른 오류 assertion은0이다. 합성 판정 fixture7개와 Python23개는 실제 RHI 실행 수에 넣지 않는다.

### 장시간 추이

| 지표 | idle | 반복 디깅 |
|---|---:|---:|
| 실제 관찰초/표본 | 1803.3984/361 | 1802.1616/360 |
| 편집/복원 | 0/0 | 880/17 |
| 판정 | 누수 의심 | 누수 의심 |
| RSS 전체/끝1/3 MiB/시간 | 112.6106/55.3238 | 71.1334/27.2685 |
| Commit 전체/끝1/3 MiB/시간 | 119.0330/39.8067 | 36.1616/11.1929 |
| 최종−첫 RSS/Commit MiB | 61.1328/62.2852 | 68.9063/140.9883 |
| UObject | 46470→46318 | 46470→46318 |
| UObject 끝1/3 개/시간 | 0 | 0 |
| 샘플 GT 평균/최대 ms | 1.676281/6.827902 | 1.590465/4.502002 |

두 실행 모두 complete=true, 모든 표본에 실제HWRT3개를 확인했다. RSS/Commit의 완만한 증가와 큰 회수 변동이 공존한다. 양의 꼬리 기울기가 사전 진단8MiB/시간 한도를 넘어 보수적으로 누수 의심으로 분류한다. 확정 누수나 제품 수용 판정은 아니다.

### 원래 RSS5쌍 — 기준 유지

| 쌍 | 디깅 RSS MiB | 대조 RSS MiB | 차이 MiB | 디깅 Commit MiB | Commit 차이 MiB |
|---|---:|---:|---:|---:|---:|
| 1 | 16.554688 | 7.101563 | 9.453125 | 88.355469 | 89.199219 |
| 2 | 24.265625 | 12.632813 | 11.632813 | 94.402344 | 83.210938 |
| 3 | 29.304688 | 7.343750 | 21.960938 | 131.785156 | 128.652344 |
| 4 | 18.160156 | 8.503906 | 9.656250 | 95.015625 | 98.015625 |
| 5 | 0.554688 | 18.921875 | -18.367188 | 97.644531 | 89.687500 |

raw/retained RSS 절대·차이 모두1~4쌍 FAIL, Commit 절대·차이는5쌍 모두 FAIL이다. 디깅 raw RSS 중앙값18.160156MiB, 최대차이21.960938MiB다. 순서는 AB/BA/AB/BA/AB이며 threshold8MiB·35초 준비·90초·50회·0회·기본 allocator·trim/drain 방식을 바꾸지 않았다. 차이 통과가 절대 실패를 대체하지 않는다. 샘플러/audit/PCD 없는 조건에서도 실패해 새 샘플러나 PCD만의 원인으로 귀속할 수 없다.

### 샘플러 off/on 상대 비용

`MemoryCostOff`/`MemoryCostOn`은 별도 opt-in `HATCH26.MemorySamplerOverheadRhi`의60초 비용 probe다. **장시간 관찰의1800~3600초 제한은 그대로**다. 양쪽에서 LLM·3HWRT 센서·PCD·UI·준비 조건을 같게 두고 수집기 호출만 끈다. Off는 표본0개, On은5초 간격 표본12개이며 frame/GT 통계와 시작/끝 backend를 기록한다. C++에서 engine/GT/callback150ms 한도를 검사하고, 비교기는 모든 candidate 센서의 획득률≥요청의99%를 확인한다.

ABAB3블록/Off6개/On6개를 공통 [상대 판정 함수](../Scripts/PerfRelativeGate.ps1)로 비교했다.

| 지표 중앙값 | Off | On | 변화 |
|---|---:|---:|---:|
| 평균FPS | 58.653270 | 58.714336 | +0.104114% |
| engine frame p95 ms | 19.822351 | 19.567300 | -1.286687% |
| GT 평균 ms | 10.355657 | 10.281637 | -0.714777% |

세 항목 모두 증가2% 기준 PASS다. 실행 변동 안의 관측 결과이며 샘플러가 성능을 개선한다고 주장하지 않는다. 실제 획득률·backend·complete·소스/조건 일치까지 확인한 이3LiDAR PCD fixture의 수집 비용 결과다. project-wide PERF01/표준1Camera+1LiDAR/회사 수용이나 RSS 실패를 대신하지 않는다. [비교기](../Scripts/compare_memory_sampler_cost.ps1)와 [합성 검증](../Scripts/test_memory_sampler_cost.ps1)은 누락·회귀·조건 불일치·decoder·0Hz·잘못된 backend를 거부한다.

### 증가원 후보와 한계

| 근거 | idle 전후 MiB | 디깅 전후 MiB |
|---|---:|---:|
| Untracked | +37.359064 | +106.422850 |
| FMallocUnused | +14.797312 | +15.335067 |
| RHIMisc | +4.074730 | +4.403003 |
| UObject 태그 | -0.204820 | -0.204820 |
| Hatch/CargoCook | 0 | +0.664513 |
| Hatch/RayTree | -0.000374 | +0.000374 |

끝1/3에서 RHIMisc는18.6834/14.4294MiB/시간, CustomName은23.7840/28.4160MiB/시간의 증가를 보였다. 계층 태그를 합산하거나 process aggregate를 독립 원인으로 세지 않는다. UObject 수·해치 소유 태그의 큰 누적 증가보다 native 미계측/RHI·명령/할당자 보존을 우선 추적한다. CargoCook의 작은 증가를 전체 Process 증가의 설명으로 확대하지 않는다. PCD나 새 수집기가 없는 원래 조건도 실패하므로 그것들만의 문제는 아니다.

실제 누수와 allocator/driver 보존을 나눌 allocation stack·native heap 귀속은 확보하지 못했다. 비용 probe60초는 장시간 sampler-off 메모리 추이를 증명하지도 않는다. **근거가 충분하지 않아 제품 메모리 수정은 하지 않았다.** 다음 필요한 작업은 동일 조건의 native allocation 귀속이다. 기존 `user accepted unresolved RSS (2026-10-23)`와 원래/새 FAIL을 함께 유지한다.

개인44파일 원본 집합/SHA 복원·보호481파일 일치·측정 source/asset616개 일치를 확인했다. 사용자 SensorTestMap 변경과 비추적 보호 파일은 그대로이며 전체 worktree clean으로 표현하지 않는다. DTCore/gitlink·자산·Config/Game.ini·PixelStreaming·Claude 메모는 stage/변경하지 않았고 push/PR/merge도 하지 않았다.
