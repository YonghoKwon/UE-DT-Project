# MEMORY-TREND-26c — 주기 로그 억제와 패키지 메모리 관찰

기준 `3ed0367`(PR #87), branch `fix/scheduler-log-26c`, DTCore `b2504d1`. Editor 전후15분과 Development 패키지60분 관찰을 완료했다. **기본 주기 로그는 제거됐지만 메모리 증가는 남아 있으며 #26 RSS 미해결 상태를 유지한다.** 원본·수치·최종 검증은 `Saved/Reports/MEMORY-TREND-26c/`의 [RESULT](../Saved/Reports/MEMORY-TREND-26c/RESULT.md), [result.json](../Saved/Reports/MEMORY-TREND-26c/result.json), [merge-check](../Saved/Reports/MEMORY-TREND-26c/merge-check.json)에 둔다.

## 기본 로그와 진단 켜기

[Scheduler](../Source/ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.cpp)의 global/camera/lidar 주기3개 callsite를 `LogVirtualSensorTelemetry`의 `Verbose`로 옮겼다. category 기본값은 `Log`이므로 평상시에는 출력하지 않는다. 기존 format string과 필드는 그대로다. `RefreshFrameStatistics`는 억제 검사 전에 실행하므로 UI/성능 통계는 계속 갱신한다. 로그가 꺼져 있으면 문자열 조립과 센서별 로그 순회도 건너뛴다. acquisition/admission/queue/저장·wire 정책은 바꾸지 않았다.

```text
# 시작할 때
-LogCmds="LogVirtualSensorTelemetry Verbose"
# 실행 중 콘솔에서 켜기 / 끄기
log LogVirtualSensorTelemetry Verbose
log LogVirtualSensorTelemetry Log
```

[해치 실행기](../Scripts/run_hatch_validation.ps1)와 [직렬 실행기](../Scripts/run_memory_trend_serial.ps1)의 `-SchedulerTelemetry`가 같은 옵션을 전달한다. 로그를 파싱하는 `run_fullspec_performance_evidence.ps1`은 해당 옵션을 명시해 기존 regex·숫자 기준을 유지했다. 상태 변경·연결·backpressure·resume·송신 조절 경고/오류 로그는 유지했다. 후자는 해당 이벤트가 있을 때만 출력하는 진단이며 무조건 매초 출력하는 Scheduler poll과 구분한다.

## 실제 로그 회귀와 기여량

| 실행 | direct global | direct sensor | 자동화 Info 보관 |
|---|---:|---:|---:|
| 수정 전 Editor15분 | 948 | 2,844 | 3,792 |
| 수정 후 기본 Editor15분 | 0 | 0 | 0 |
| 수정 후 opt-in60초+준비 | 105 | 315 | 0 |

행 수는 `LogTemp: Display`/`LogVirtualSensorTelemetry: Verbose`의 직접 출력만 세며, 같은 파일에 있는 AutomationController의 Info 재출력을 중복 계산하지 않는다. opt-in의1global+3sensor 관계·필드/센서 ID 집합을 기존과 대조했다. 모든5개 Scheduler 진단 format string도 동일하다. Verbose 메시지는 console log에는 남지만 자동화 Info에는 보관되지 않는다.

제거된 자동화 메시지3,792개의 UTF16 문자열 본문+NUL은 **2,827,646bytes(약2.70MiB)**다. 이는 문자열 payload만 계산한 수치다. FString/TArray capacity·allocator·OutputLog 이력 비용이나 전체 Process RSS 절감량으로 확대하지 않는다. [실제 로그 검증](../Saved/Reports/MEMORY-TREND-26c/log-regression.json)과 [정적 비교](../Saved/Reports/MEMORY-TREND-26c/log-policy-static.json)를 보존한다.

## 메모리 비교

공통 요청은 Rev7×3/1024×64/20Hz, 실제HWRT, ShipHatchTestMap, D3D12/RT ProjectDefault,1920×1080 offscreen/font100%,센서당12,000점, Full 압축PCD/FIFO/80MiB/s, 무자격 loopback receipt+echo,5초 샘플이다. 측정 구간은 모두 idle이다. Editor는 기존17회 완전 pointer/분석 준비+35초, package는 public Dig API의 near-surface ray17회/분석 대기/원복+35초다. package 사전 준비는 직접 마우스 검증이 아니다.

| 실행 / 코드 | 관찰 분 / 표본 | allocator | RSS 꼬리 MiB/h | Commit 꼬리 MiB/h | 실제 acquisition Hz 범위 |
|---|---:|---|---:|---:|---:|
| 과거 Editor soak2 / ce456eb | 30.06 / 361 | Mimalloc | +55.3238 | +39.8067 | 19.993~19.995 |
| Editor 수정 전 / 3ed0367 | 15.03 / 181 | Mimalloc | +328.1014 | +631.5479 | 19.989~19.994 |
| Editor 수정 후 / 893b5cb | 15.03 / 181 | Mimalloc | +119.5123 | +269.3481 | 20.000~20.001 |
| Development package / d958e00 | 60.05 / 720 | binned2 | +50.1741 | +18.6753 | 16.542~16.977 |

모두 **누수 의심**, UObject 꼬리 기울기는0개/h다. package는3,603.0836초 관찰과 정상 exit0·complete=true를 확인했다. UObject는25,232개로 일정했다. 첫/끝 RSS 차이는+36.6484MiB, Commit은+13.3711MiB다. 모든720표본에 실제HWRT3대·요청20Hz·12,000점과 LLM 활성 상태를 확인했다. package font100/UMG scale1·실제viewport1920×1080도 metadata에 남겼다. 요청률을 낮춰 실제 처리량 차이를 숨기지 않았다.

package 마지막1/3 Default LLM은 `RHIMisc` +11.6190, `FMallocUnused` +3.6440, `Untracked` +5.7894MiB/h다. `CustomName` 꼬리 coverage는79.58%여서 기울기를 계산하지 않았으며 누락 표본을0으로 채우지 않았다. 태그는 포함 관계가 있어 합산하지 않는다. UObject 일정과 낮은 특정 태그 기울기가 실제 native 누수를 배제하지 않는다. 로그 보관만으로 전체 증가가 설명된다고 볼 수 없다.

Editor15분 진단은 기존 분석기에 `--min-seconds 900 --min-tail-seconds 300`을 명시했다. package60분은 기본 최소30분/꼬리10분과 진단8MiB/h·UObject10개/h를 그대로 사용했다. **원래 #26의 절대·대조 차이8MiB/90초/5쌍은 변경·재통과시키지 않았고 이번 범위에서 재실행하지 않았다.**

단일 Editor 전후 pair의 차이 전체를 로그 효과로 귀속할 수 없다. 과거soak2는 다른 SHA/시간창이다. package는 Editor 이력이 없고 allocator·사전 준비·실제 획득률도 다르다. 주요 boot scalability 단계는 세 실행 모두3이지만 renderer footprint가 완전히 같다는 뜻은 아니다. package60분은 sampler/LLM on이며60분 off 대조를 따로 수행하지 않았다. allocator를 강제로 바꾸거나 품질/요청률을 낮추지 않았다. [비교 JSON](../Saved/Reports/MEMORY-TREND-26c/comparison.json)과 [boot 조건](../Saved/Reports/MEMORY-TREND-26c/render-config-comparison.json)을 함께 본다.

## 패키지 진단과 비용

[HatchMemoryRuntimeProbe](../Source/ma0t10_dt/MA0T10/UI/HatchMemoryRuntimeProbe.cpp)는 명시적 `-HatchMemoryRuntimeReport=<새 경로>`가 있을 때만 생성한다. 기본 실행에는 해당 object/tick이 없다. 기존 프로젝트 transport/publisher와 `memory_curve` JSONL 계약을 사용하며 고정10개 태그/상위16개·RSS/Commit/UObject·센서/PCD·이전 표본 GT 비용·footer를 기록한다. 매프레임 Actor 검색·문자열/행 이력 누적·강제 GC/trim은 없다. sample 수집/JSON/write/flush/임시 객체 해제 비용을 포함하며 header/footer·종료 drain은 별도다.

샘플 GT 평균/최대ms는 Editor 수정 전2.0313/6.6384, 수정 후2.1865/3.6988, package2.2061/6.3350이다. 이는 수집기의 실제 비용 기록이며 그 자체로 상대2% 성능 통과를 의미하지 않는다.

```powershell
# Source/asset/설정 백업과 영상 없는 환경 확인 후, 새 ReportRoot만 사용한다.
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/run_hatch_package_memory.ps1 `
  -Executable '<Development package>/Windows/ma0t10_dt/Binaries/Win64/ma0t10_dt.exe' `
  -NativeValidator '<owned>/lzf_validate.dll' -Seconds 3600 `
  -ReportRoot 'Saved/Reports/MEMORY-TREND-26c/repeat-new'
python Scripts/analyze_memory_trend.py Saved/Reports/MEMORY-TREND-26c/repeat-new/memory-trend.jsonl --output Saved/Reports/MEMORY-TREND-26c/repeat-new/analysis.json
```

직렬 실행기는 사전3회와 실행 중5초 간격으로 NVIDIA decoder를 확인한다. 양수/조회 실패 시 취소 파일로 정상 종료하고 이후 측정을 자동 재시작하지 않는다. 자격증명은 비우고127.0.0.1만 사용한다. userdir를 새 보고서 안으로 격리한다. 30초 mode는 진단 자격 검증이며 장시간 판정이 아니다.

Development/Shipping BuildCookRun을 완료했다. Shipping은 명령행 map override를 허용하지 않는 UE5.3 기본 정책을 유지했다. 별도 실험 패키지에만 임시 WindowsEngine.ini의 시작 맵을 지정해 Shipping30초/7표본/정상 exit0·HWRT3·PCD·12,000점을 확인했다. Shipping LLM은 비활성이고30초이므로 장시간 안정화 근거가 아니다. 원본 platform config는 남기지 않았다.

## 실패·수정·검증 한계

- 첫 실행은 정리된 native validator DLL이 없어 Editor 시작 전에 중단됐다. DLL을 다시 빌드해 새 경로에서 수행했다. 새 C++의 TObjectPtr 변환 오류2회는 수정 후 Editor 빌드를 통과했다.
- 첫 Game unity 빌드에서 기존 probe와 익명 namespace 변수명이 충돌해 고유 이름으로 수정했다. 다음 Development/Shipping 빌드가 통과했다.
- 첫 package 자격 실행은 desktop 자동 축소888×500으로1920×1080 검사를 통과하지 못했다. ForceRes로 실제1920×1080을 확인했으며 기준은 유지했다.
- 다음 자격 실행은30초 footer complete여도 종료 후0xC00000FD여서 수용하지 않았다. Landscape PreExit delegate race ensure를 보존했지만 terminal status의 직접 원인은 확정하지 않았다. 진단 종료 시 자기 센서/출력/렌더링을 footer 뒤에 정지·drain한 후 정상 종료하도록 했고, 새 package 자격과60분 관찰은 exit0다. 엔진/제품 전체 종료 문제의 해결로 확대하지 않는다.
- offscreen startup SwapChain4 ensure 및 누락된 Engine DaylightAmbientCubemap 경고는 원문에 남긴다. 실제 RHI를 NullRHI로 바꾸거나 무오류 실행으로 표시하지 않았다.
- Shipping 첫 실행은 기본 시작 맵에서 해치 Host를 찾지 못했다. 별도 실험 시작 맵을 지정했으며 Shipping map override 정책을 완화하지 않았다.
- UAT staging에서만 SecurityToken key를 INI denylist로 제외했다. 원본 DefaultEngine 설정은 출력·변경하지 않았고 DefaultGame 임시 directive는 매번 원본 bytes/hash로 복원했다. 최초 filtered-file mtime 검색0개는 SafeCopyFile이 원본 시간을 보존하는 특성을 반영해 보정했다.
- NullRHI 관련 회귀14 Success/1 명시 opt-in UI skip, 실제 Editor 로그 RHI3 Success다. synthetic Perf01CancelFile은 의도대로 Fail1/exit255/valid=false/phase=cancelled로 끝나 취소 계약 검증이 통과했다. 이 negative fixture를 성능 통과로 세지 않는다.

RT on1+1 ABAB 한 블록(A2/B2)은 사용자 요청의 간단 확인 범위이며 **2% PASS**다. 중앙값 FPS59.996850→59.999256(저하−0.004010%), engine p9516.666900→16.666900ms(+0%), GT4.780628→4.752973ms(−0.578490%)다. 기존 instrument·2%·150ms·provenance 판정을 사용하고 취소 flag만 동일 instrument에 추가했다. 독립 decoder monitor와 launch guard를 사용했다. 실험 archive는 Config를 원시 ZIP에서 제외한 뒤 양쪽 모두 SecurityToken key를 제거한 동일 config를 넣었다. Windows-inactive Android FileServer key 제외 외의 기준/품질/프로필 변경은 없다. 모든소유모니터1,385표본의 decoder 최대0%다. [성능 manifest](../Saved/Reports/MEMORY-TREND-26c/perf/manifest.json).

모든 측정 프로세스 종료 후 개인44파일 원본집합/SHA256·보호481파일 일치를 확인했다. 경로/소유권/해시를 기록한 package archive/stage 및 이번에 빌드한 game exe/PDB1,989파일·7,679,832,430bytes를 정리했다. 최초 정리 집계오류2건은 삭제 전에 발생했고 보정 후 삭제/부재검증을 완료했다. Editor/plugin 산출물·공유 Cooked/Intermediate·사용자 변경은 삭제하지 않았다. 최종 quick/schema/merge 판정은 RESULT/result.json의 manifest를 따른다. 회사/실장비 수용과 전체 release는 별도다.
