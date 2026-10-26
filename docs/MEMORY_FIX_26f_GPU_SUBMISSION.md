# MEMORY-FIX-26f — GPU 제출·수명 추적 결과

기준 `a2fd42d`, branch `chore/memory-fix-26f`. **#26은 미해결이며 머지 차단 대상이다.** 두 번의 메모리 최적화 시도 후 반복 디깅 검증에서 residency 크래시가 발견됐다. 최종 `3485774`는 RT scene 재사용을 같은 렌더 프레임으로 제한한 안전 보정이다. 그 뒤 추가 메모리 최적화 실험은 하지 않았으며, 이전 SHA의 장시간 통과를 최종 소스에 이월하지 않는다.

## 변경과 검증 경계

- `9d8a255`: 같은 프레임의 호환 mesh revision/pose 요청을 RHI 배치·raygen으로 합치고 결과를 원래 센서 순서로 분배한다. ray/result/readback/SRV/UAV는 terminal GPU fence·unlock 뒤에만 반환하는 상한 있는 pool에서 재사용한다.
- `e6d009f`: 패키지 qualification의 처리량 저하를 확인해 배치를 Scheduler의 센서 준비 직후, main-view render 전에 제출한다. 프레임 끝 제출은 수동 호출 fallback으로 남긴다. 기존 실패 qualification과 취소된 준비 구간을 보존했다.
- `f436073`: 정적 hit/miss record를 매 dispatch마다 다시 쓰지 않고 PSO 변경·idle gap에서 갱신하며, package에서 기존 고정 GPU 점 renderer의 cooked material을 사용하도록 한다. 요청20Hz·점 예산·색/크기·wire·FIFO/예약 상한은 낮추지 않았다.
- `d323e98`: 명시적인 7200초 검증 플래그만 추가했다. 획득/렌더 최적화 변경은 아니다.
- `3485774`: UE5.3 D3D12의 SingleFrame scene 계약에 맞춰 캐시 hit를 같은 render frame으로 제한했다. byte/point oracle은 유지했다. unsafe cross-frame scene 생성을0으로 요구하던 구현 테스트만 올바른 scene refresh 계약으로 수정했으며 8MiB/150ms/20Hz/2% 기준은 바꾸지 않았다.

우리 코드에 센서ID/프레임별 RDG_EVENT_NAME·SCOPED_DRAW_EVENT·GPU stat 생성은 발견되지 않았다. 직접 RHI 경로를 한 배치로 묶었으며, 별도 RDG event/stat을 추가하지 않았다. depth/semantic의 타깃·texture readback은 이미 재사용한다. semantic proxy의 render-state refresh는 UE5.3의 이전 프레임 uniform-buffer 오류를 막는 기존 안전장치여서 무조건 제거하지 않았다.

본 메모리 측정은 프로젝트7개 LLM/trace scope를 `-NoProjectMemoryTags`로 끈다. **동일 RHIMisc 카운터를 얻기 위한 엔진 LLM은 유지**했다. 모든 LLM을 꺼 RHIMisc가 관측되지 않는 상태를0으로 간주하지 않았다. trace 파일은 생성하지 않았다.

## 장시간 결과 — 이전 SHA 한정

Rev7×3/1024×64/요청20Hz, Full압축PCD·blank127.0.0.1 loopback·기본preview,1920×1080 offscreen/font100/UMG1,binned2,engine controls1/1/1/time-preference0/Trim0,5초sample,첫10분제외,실행간120초cooldown이다. 앞서17회 public Dig/분석/restore와35초 준비를 공통 사용했다. 관측 중은 idle이다.

| 실행/Runtime | RSS 꼬리 MiB/h | Commit | RHIMisc | 최소 실제Hz | 판정 범위 |
|---|---:|---:|---:|---:|---|
| attempt1-validated/effect60 / e6d009f | +3.274 | +0.505 | +6.363 | 17.229870 | 60분 목표 미달 |
| attempt2/effect60 / f436073 | -0.547 | -1.014 | -0.111 | 19.999792 | 이전 SHA 통과, 최종 소스 이월 금지 |
| final/run1 / d323e98 | +1.759 | +2.466 | +0.070 | 19.998307 | 이전 SHA 통과, 최종 소스 이월 금지; 마지막40분 |
| final/run2 / d323e98 | +4.771 | +4.801 | -0.007 | 19.998572 | 이전 SHA 통과, 최종 소스 이월 금지; 마지막40분 |

2시간 실행은 마지막1/3(40분)과 마지막20분 모두8/8/2 목표 안이었다. 그러나 이후 반복 디깅에서 크래시가 발생했고 source 수명 계약을 변경했으므로 현재348의 메모리 수용 증거가 아니다. 첫60분의 제외후50분 전체 기울기21.838/22.416/5.361도 보존하며, 유리한 꼬리만으로 원인·누수 해결을 확정하지 않는다.

## 원래 #26 5쌍 — 최종 안전 보정 소스

| pair | dig rawRSS MiB | control rawRSS | 차이 | dig Commit MiB | control Commit | 차이 |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 26.137 | 16.203 | 9.934 | 102.680 | 17.641 | 85.039 |
| 2 | 13.270 | 12.043 | 1.227 | 161.992 | 11.254 | 150.738 |
| 3 | 8.469 | 15.137 | -6.668 | 146.801 | 14.078 | 132.723 |
| 4 | 23.332 | -3.422 | 26.754 | 72.781 | -3.699 | 76.480 |
| 5 | 4.504 | 7.770 | -3.266 | 97.812 | 6.887 | 90.926 |

**원래 absolute8MiB FAIL, dig-control8MiB FAIL.** raw/retainedRSS는 별도로 판정했고 전체 수치는comparison.json에 있다. Commit 차이 중앙값90.926MiB이며 pair2의150ms GT/콜백 기준 실패도 보존했다. 35초 준비·90초 공통 창·50회dig/0회control·12000 preview 예산을 유지했다. 실패 실행을 다시 골라 측정하거나 제외하지 않았다.

이전 d323의 pair3 control은D3D12CommandList.cpp30 GPU-mask assertion으로 중단됐다. 해당 버전은5개완료dataset만 있으며 완성된5쌍으로 세지않는다. 최종348에서는5쌍10개 실행을 완료했고 동일 residency 크래시는 관찰되지 않았다. 정확히 어떤nativehandle이깨졌는지의전체원인귀속까지확정한것은아니다.

## 현재 소스 gate

- RTon ABAB3블록12valid구간 PASS: FPS감소0.000470%,p95변화0.000000%,GT변화-0.154541%,GPU변화-3.598122%. 기준a2→348,1Camera+1Rev7실제HWRT,offscreen1920×1080,10초+30초,2%다.
- Editor/Development/Shipping build PASS. 최종 Development/Shipping30초 smoke complete/exit0이며 PCD invalid/encode/delivery/duplicate/overload0이다. Shipping의LLM비활성/RHIMisc미관측을메모리PASS로세지않는다.
- 센서당65,536점×3,센서/점별거리와miss를포함한freshGPU참조memcmp·analytic oracle,취소20회/50회디깅·revision/pose/identity·Rev7 3대·depth/semantic·동시capture회귀를수행했다. 최종safety RHI4Success/0skip. 전체batch별결과는measurement-summary.json을따른다.
- 직접UI는Windows잠금화면확인으로0input/미수행이다. 잠금화면을조작하거나해제하지않고offscreen만진행했다. 실제client/DPI직접수용을주장하지않는다.
- 최종quick/release관련/layer/guard/schema·복원·정리는result.json/최종감사에기록한다. skip/다른SHA/NullRHI를현재RHI통과로세지않는다.

## 중단과 다음 설계

추가 제품 메모리 수정은 더 하지 않는다. [LiDAR GPU 획득 재작성 계획](LIDAR_GPU_ACQUISITION_REWRITE_PLAN_26f.md)에 범위·영향 파일·residency/비동기/품질 위험·착수후7~10영업일·동일검증gate를 정리했다. 엔진/드라이버누수 귀속이나 재작성으로 해결된다는 보장은 없다. 플러그인·엔진·DTCore 변경 승인을 확대하지 않는다.

[RESULT](../Saved/Reports/MEMORY-FIX-26f/RESULT.md)·[result.json](../Saved/Reports/MEMORY-FIX-26f/result.json)·[merge_check](../Saved/Reports/MEMORY-FIX-26f/merge-check.json)·[원시요약](../Saved/Reports/MEMORY-FIX-26f/gpu/measurement-summary.json)·[5쌍](../Saved/Reports/MEMORY-FIX-26f/gpu/final/pairs-safe/COMPARISON.md)·[2시간1](../Saved/Reports/MEMORY-FIX-26f/gpu/final/run1/analysis.json)·[2시간2](../Saved/Reports/MEMORY-FIX-26f/gpu/final/run2/analysis.json).


## Final audit / gates

Current348: related feature step PASS; aggregate release474Success/6Coil presentation captureFAIL/45Skip. WBP memory compile7PASS, assets not saved. Original5pairs absolute/difference8MiBFAIL and150msFAIL retained. CurrentRTonABAB3blocksPASS, Dev/Shipping30sPASS. Current348longmemory NOTRUN; earlierSHA2hpasses not carried. Personal253 exactset/hash restored, protected483 unchanged. Owned7077files21,830,473,083bytes removed literally after ownership/prefix/reparse/hash checks;emptydirs retained. Engine/PDB/mainEditor/DTCore/sharedCooked/Intermediate/older26efixtures untouched. No trace created. No push/merge. See result.json and merge-check.json for final verdict.
