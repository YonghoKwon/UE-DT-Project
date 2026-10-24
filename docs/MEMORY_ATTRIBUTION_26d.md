# MEMORY-ATTRIBUTION-26d — 패키지 메모리 콜스택 추적

기준 `1c19ebb`, branch `chore/memory-attribution-26d`, DTCore `b2504d1`. 실제 패키지/trace 코드는 `92308a4`, 이후 live 분석기 보정은 `dbb043d`다. **RHIMisc의 유력 증가 경로를 엔진 D3D12 query 배열·crash-event 문자열 캐시로 좁혔다. 기존 무추적 RSS+50/Commit+19MiB/h 전체 귀속과 확정 누수 판정은 미완료다.** 프로젝트의 무제한 보유가 입증되지 않아 제품 버퍼/정책 수정은 하지 않았다.

[RESULT](../Saved/Reports/MEMORY-ATTRIBUTION-26d/RESULT.md)·[result.json](../Saved/Reports/MEMORY-ATTRIBUTION-26d/result.json)·[merge-check](../Saved/Reports/MEMORY-ATTRIBUTION-26d/merge-check.json), [상위50개 CSV](../Saved/Reports/MEMORY-ATTRIBUTION-26d/top-allocations.csv), [집계](../Saved/Reports/MEMORY-ATTRIBUTION-26d/attribution-summary.json), [소스 근거·완화 제안](../Saved/Reports/MEMORY-ATTRIBUTION-26d/source-evidence.json)을 함께 본다. 기존 #26 RSS 미해결 수용과 원래8MiB/90초/5쌍 기준은 유지한다.

## 심볼 검증과 해석 범위

사용자가 설치한 UE5.3 Editor 기호를 로컬 DbgHelp로 먼저 검증했다. `UnrealEditor-Core.dll`과 일치한 PDB에서 `FMallocMimalloc::Malloc`은 `MallocMimalloc.cpp:64`, D3D12RHI DLL은 `D3D12Texture.cpp:2813` 등 실제 private 함수·소스 줄로 해석됐다. PDB mismatch 허용 옵션이나 nearest export를 사용하지 않았다. [엔진 심볼 증거](../Saved/Reports/MEMORY-ATTRIBUTION-26d/engine-symbol-proof.json).

Development game exe의 프로젝트 PDB도 이미지와 일치하며 `UVirtualLidarScanComponent::BeginSceneQueryBatch`는 프로젝트 `VirtualLidarScanSceneQuery.cpp:188`로 확인됐다. 그러나 monolithic game의 **엔진 부분은 PDB public-code 함수명만 있고 소스 줄은 없다**. 설치된 stock UnrealGame PDB도 같은 제한이다. Editor DLL의 주소/줄을 game 주소에 이식하지 않았다. [game 심볼 증거](../Saved/Reports/MEMORY-ATTRIBUTION-26d/game-symbol-proof.json).

game public 함수는 PE unwind/chained-parent의 시작 RVA와 심볼 시작 RVA가 일치할 때만 이름을 원인 해석에 사용했다. 경계 미입증 public 이름·exports는 `Unknown`/module+offset으로 두고 approximation 필드에만 보존했다. 주 보고서의 unique public frame405개 중361개가 경계 검증을 통과했다. 프로젝트/엔진 구분은 파일 경로를 사용한다. exe 이름에 ma0t10_dt가 있다는 이유로 엔진 코드를 프로젝트 코드로 분류하지 않는다. 원래 출력 `allocations-stream.json`과 보수적으로 보정한 `allocations.json`을 모두 남겼다.

## 분석기 calibration과 제한

별도30초 package trace에서 known canary1→4MiB와 기본 TraceServices/live-only 모델을 대조했다. Binned2에서 기존 분석기가 같은 주소의 겹친 heap을 덮어쓰는 문제가 발견됐다. UE5.3 `FHeapAllocs`와 같은 LIFO 목록으로 보정하고 implicit heap free 경로도 추가했다. 이는 **분석 도구 보정**이며 제품 메모리 누수 수정이 아니다.

| CPU user+backing 보정 검증용 합계 | 기본 provider | live-only |
|---|---:|---:|
| A bytes / count | 3,569,289,302 / 563,820 | 동일 |
| B bytes / count | 3,579,945,002 / 599,887 | 동일 |
| canary 순증가 | 별도 참조 | 4,718,608bytes |
| 비교 창 미대응 이벤트 | 로그 창0 | 0 |

합계는 두 reader의 회계 검증용이며 포함 관계인 heap을 합산해 physical memory로 해석하지 않는다. user/backing 분할은 UE provider의 과거 heap-mark flag 변경 때문에 다를 수 있다. 기본 분석기는 startup/종료 후 System-memory 이벤트 오류로 exit1이었고 live-only는 exit0이었다. 비교 창 안의 해당 기본 provider 로그 오류는0개다. raw 오류와 초기 실패 calibration도 보존했다. [CPU 검증](../Saved/Reports/MEMORY-ATTRIBUTION-26d/calibration/cross-validation.json).

**GPU 숫자는 검증되지 않았다.** 같은 normal GPU 주소의 중복/alias 할당이 있고, 기본 provider의64개 recent cache와 주소-map overwrite 의미가 live 모델과 재현되지 않았다. 모든 alias를 별도 보유하는 대안도 과대 집계돼 폐기했다. GPU unique-address 해석은 raw 별도 항목으로 남기며 실제 GPU 순증가/누수의 증거로 사용하지 않는다. 본 작업은 GPU 전체 귀속 완료가 아니다. 원시 이벤트의 allocation identity/엔진 provider 보존 방식 한계를 후속 조사 입력으로 둔다.

## 20분 package trace

Development package·기본binned2·Rev7×3/1024×64/요청20Hz·실제HWRT3·기본Full 압축PCD/FIFO/80MiB/s·무자격127.0.0.1 receipt+echo·12,000점/센서·1920×1080 offscreen/font100/UMG1·LLM on이다. 기존 package 준비17회 public Dig/분석/원복+35초를 사용했고 측정 구간은idle·canary off다. 일반30분~60분 모드와 별개인 명시 investigation900/1200초만 추가했다. BEGIN/END는 기존 query 계약 `MEMORY26B_BEGIN/END`를 재사용한다.

실제1,202.7206초/241표본/complete=true/정상exit0, trace44.68GiB를 수집했다. 실행 중247환경표본 decoder최대0%다. 요청률을 낮추지 않았고 실제획득15.728~15.826Hz를 기록했다. source query A는시작+300초(`348.2986975`), B는teardown전END직전(`1251.0183248`)이다. live-only는5,556,006,157events를 처리했고 분석RSS 약1.4GiB·exit0, 비교창미대응0/이전184,중복normal주소58,850개다.

| 5분→종료 | 변화 |
|---|---:|
| CPU user live bytes / count | +3.7972MiB / −26,370 |
| CPU backing bytes / count | −8.0586MiB / +42 |
| 가장 가까운 sample RSS / Commit | −2.8203 / −2.5820MiB |
| sample UObject | 변화0(25,232개) |
| Default RHIMisc | +2.9351MiB |

heap을 합산하거나 양의 top 행만 더해 전체 증가라고 부르지 않는다. 20분 trace의 기술적 꼬리 OLS는 RSS+8.8435/Commit−58.9111MiB/h·UObject0이며 min1200/tail400의 별도 진단이다. sampler GT 평균4.3257/최대15.4720ms로 무추적26c의2.2061/6.3350ms보다 비용이 크다. trace 실행을 기존 무추적60분의 재통과나 동일 처리량 성능 비교로 사용하지 않는다. **원래 +50/+19가 이 비교 창에서 재현되지 않았다.**

## 확인된 상위 코드 경로

| CPU user 증가 | 개수 차이 | 위치 / 판단 |
|---|---:|---|
| +3.6794MiB | +1 | `FObservation` 복사→`TFuture::Get`→프로젝트 `ShipHatchMonitorWidget.cpp:489`; 한 pending/current 결과의 차이로 무제한 보유 미입증 |
| +2.3195MiB | +2 | 엔진 `TArray<FD3D12QueryLocation>`→`FD3D12DynamicRHI::ForEachQueue/ProcessSubmissionQueue`; query-family 배열 증가 |
| +2MiB | +1 | 프로젝트 Bootstrap `SensorOwnedRayTracing.cpp:280`의 hit/readback 배열; finished readback·retired scan 제거 경로 존재 |
| +0.6333MiB | +1 | 프로젝트 `Sensor/VirtualPcdCodec.cpp:131`의 출력 배열; in-flight buffer 한 개 차이 |
| +0.5645MiB | +2,176 | 엔진 `FD3DGPUProfiler::GetOrAddEventStringHash`의 FString |
| +0.3052MiB | +1 | 같은 profiler의 event-string map allocation |

엔진 소스 위치는 game PDB 소스 줄 매핑과 구분한 **동일 함수의 로컬 소스 정의/호출 경로 근거**다. `D3D12Submission.cpp:354,428~445`는 RHIMisc/ProcessSubmissionQueue scope에서 query를 모으며,501~571은CPU-visible sync point일 때 payload로배열을Move한다. 정확히어느queue/array의backlog·capacity인지한쌍의snapshot으로확정하지못한다. pendingquery는timestamp/occlusion/pipeline가족을포함한다. unresolvedquery를임의삭제/상한절단하는수정은하지않았다.

`D3D12Stats.cpp:185~209`의 crash-event cache는 `bTrackingGPUCrashData`일 때 문자열을 추가하며10,000개 초과 시 `Empty(10000)`으로 비운다. 따라서 관측한 cache 기여는 실제이지만 무제한 누수로 부르지 않는다. 두 engine 경로가 RHIMisc 증가의 유력 기여자이며 전체 Process RSS 설명으로 확대하지 않는다.

CPU backing의양의상위는 `FCachedOSPageAllocator/ FMallocBinned2`에서 PCD codec·STOMP receive·stat/RDG·preview로이어진다. backing을유발한stack이그전체예약의현재소유자라는뜻은아니다. 선택창backing/RSS/Commit감소는보존·resident변화/작업위상가설을지지하지만느린native/driver누수를배제하지않는다. 프로젝트buffer의1~2개차이를원인확정으로삼지않았다.

## 완화 제안과 다음 판정 경계

적용한 제품 완화/기본값 변경은 없다. 다음은 근거가 있는 별도 실험/설계 후보이며 메모리 기준 우회가 아니다.

1. `r.gpucrash.collectionenable 0`을 **분리 진단 실행**에서 대조한다. D3D12Stats BeginFrame이 이 값을 latch한다. cache cardinality/bytes 기여를 확인할 수 있으나 GPU crash breadcrumbs/context를 잃으므로 기본 변경을 제안하지 않는다. cache 자체에는 이미10,000개 count 한도가 있다.
2. query-family Num/Max/fence-age를 queue별저빈도로계측한다. 적절한CPU-visiblefence뒤완료배열을재사용하고**idle pool**만예산화한다. active/unresolvedquery절단·매프레임GPUdrain은금지하며engine변경에는별도성능/수명검증이필요하다.
3. allocator 통계가보존을확인한뒤Binned2 OS-cache/idle-trim을분리평가한다. Win64 fallbacksource는64entry/64MiB정책과FreeAll/Trim경로를가지지만컴파일변형을확인해야한다. cache축소/trim의할당·지연비용을측정하기전적용하지않는다.
4. `TFuture::Get` 결과이전/관측scratch재사용은boundedcopy최적화후보다. source revision/generation검사를유지해야하며원래RSS추이의해결로가정하지않는다.

프로젝트 누수가 확정되지 않았으므로 조건부 제품 수정·수정 후package60분≤8MiB/h/평탄화·RTonABAB2%는 수행하지 않았다. engine/driver 단독원인 또는무누수도확정하지않았다. 전체native귀속과GPU alias 수치는미해결로남긴다. 회사/실장비수용·전체release·원래RSS5쌍은별도다.

## 검증과 보존

Editor build와 Development package를 통과했다. pure C++ sampler contract1(NullRHI), Python memory tests23, 실제 calibration30초/주trace20분·정상exit0를 확인했다. NullRHI분석은RHI성능통과가아니다. 최종quick/layer/guard/schema/merge 및 개인44/보호481 복원은RESULT의manifest를따른다.

원시trace/cache/debugCSV11파일50,845,512,983bytes와package archive/stage/gameexe/PDB663파일2,558,113,352bytes는해시·요약보존후삭제했다. 설치한엔진PDB/엔진binaries는수정·삭제하지않았다. source/DTCore gitlink·ConfigGame.ini·PixelStreaming·운영맵/자산·Claude메모의사용자상태를보존하며push/PR/merge는하지않는다.
