> **과거 설계·검증 원문, 현재 기준 아님.** 원문을 보존한 보관본입니다. 현재 사용·규칙·미해결은 [문서 색인](../README.md)을 따릅니다.

# MEMORY-FIX-26f — 해결 계획 1·3단계

기준 `d8048bfaf2bc5107189fce666b3111ea9ff8e974`, branch `chore/memory-fix-26f`, Runtime `e389af6f31aeea0584bd1b84dec2abc047151de7`. **측정 흔들림과 engine control의 관측 기여를 확인했으며 #26 해결 완료는 아니다.** 60분 baseline3회와30분 분리5회, 짧은 qualification6회를 같은 Development exe로 완결했다. 제품 기본값·DTCore·LLM 태그를 바꾸지 않았고4단계를 시작하지 않았다.

[RESULT](../../Saved/Reports/MEMORY-FIX-26f/RESULT.md)·[result](../../Saved/Reports/MEMORY-FIX-26f/result.json)·[merge check](../../Saved/Reports/MEMORY-FIX-26f/merge-check.json), [고정 계획](../../Saved/Reports/MEMORY-FIX-26f/measurement-plan.json)·[실행 요약](../../Saved/Reports/MEMORY-FIX-26f/run-summary.json)을 함께 본다.

## 조건과 진단 지원

현재 master d8048bf의 기본 동작에 opt-in 진단 지원만 더한 단일 패키지를 모든 실행에 재사용했다. `-HatchMemoryExperiment`가 없으면 sidecar를 만들지 않으며 Trim도 하지 않는다. `-HatchMemoryTrimSeconds=60`에서만 sampler 방문당 최대1회 `GMalloc->Trim(true)`를 실행하고 전후 RSS/Commit·GT 비용을 별도 기록한다. GC나 강제 RHI drain·catch-up 호출은 없다. 기존 memory_curve 계약/LLM 태그와 제품 설정은 유지했다.

기본 binned2·Rev7×3/1024×64/요청20Hz·실제HWRT3·Full 압축PCD/FIFO80MiB/s·blank127.0.0.1 receipt+echo·기본미리보기·1920×1080 offscreen/font100/UMG1·LLM·5초 sampler다.12,000점은 preview 예산이며 PCD cap이 아니다. 기존17회 public Dig/분석/원복+35초 준비를 공통 사용하고 관측 중은 idle이다. 각 관측의 첫600초를 추가 준비 구간으로 제외했다.

baseline은 제외 후50분 전체와 공통 마지막1200초를 각각 분석한다. cut은 제외 후20분 전체가 공통 꼬리와 거의 같다. 60분/30분의 마지막1/3을 직접 비교하는 길이 차이를 피하려고 측정 전에20분 꼬리를 고정했다. 실제 시간 OLS이며 원시 표본을 선별하지 않는다. 실행 사이 고정120초 쿨다운,재부팅/캐시강제비우기 없음,환경 before/5초표본/쿨다운 및 제외 준비 구간의 GPU온도·clock·전력·RAM·주요CPU프로세스를 기록했다. 앱/서비스/전원정책은 조작하지 않았다.

## 1단계: 60분 idle3회

| Metric | run1 | run2 | run3 | median | min | max | range |
|---|---:|---:|---:|---:|---:|---:|---:|
| RSS | +13.866 | +20.572 | +17.429 | +17.429 | +13.866 | +20.572 | +6.706 |
| Commit | +9.551 | +19.146 | +19.353 | +19.146 | +9.551 | +19.353 | +9.803 |
| RHIMisc | +11.294 | +10.565 | +11.128 | +11.128 | +10.565 | +11.294 | +0.729 |

각 실행3602.992/3603.153/3602.938초,720표본,정상exit0·complete=true다. 세 baseline은 모두binned2,crash collection/GPUStats/RDG Events=1, GPU-time preference=0, Trim0회이며 control 값이 전 구간 일정했다. RSS·Commit 흔들림은 RHIMisc보다 컸다. 과거 단일60분 수치로 효과를 확정하지 않고 이 관측 범위를 기준으로 고정했다.

## 3단계: 한 조건씩30분

| 조건 | RSS MiB/h | Commit MiB/h | RHIMisc MiB/h | 최소 실제Hz | 후보 범위 / 제외 사유 |
|---|---:|---:|---:|---:|---|
| 01-crash-off | +11.392 | +8.766 | +6.976 | 15.1928 | 제외: 획득률15.193 < 비교 하한15.816 |
| 02-gpu-stats-off | +24.238 | +16.865 | +9.417 | 16.3116 | RHIMisc만 |
| 03-rdg-events-off | +20.277 | +16.920 | +9.684 | 16.3146 | RHIMisc만 |
| 04-mimalloc | +34.070 | +30.289 | +9.278 | 16.1065 | 제외: RSS/Commit 악화가 확장 범위를 넘음 |
| 05-trim-60s | +24.385 | +25.958 | +10.000 | 16.2538 | RHIMisc만; RSS/Commit 개선 없음 |

① `r.gpucrash.collectionenable 0` ② `r.GPUStatsEnabled 0` ③ `r.RDG.Events 0` ④ `-mimalloc` ⑤ binned2/기본CVar에서`-HatchMemoryTrimSeconds=60`이다. 모두 새 프로세스이고 실제 CVar 값·할당자·다른 controls 불변과 차이를 검증했다. installed UE5.3 source의 정의/조건과 package의 존재 여부를 대조했다. GPUStats off는 realtime GPU-stat timestamp 경로에 한정되며 모든D3D12 query off로 부르지 않는다. RDG off도 모든GPU event name off가 아니다.

후보 기준은 측정 전에 고정했다. 해당 기울기가 baseline최소보다 낮고,중앙값 대비 감소가`max(관측범위,1MiB/h)`보다 커야 한다. 최소센서Hz가 baseline최저의98%(15.815810Hz)이상이고,다른RAM지표가`baseline최대+max(범위,1)`를 넘게 악화되지 않아야 한다. 실제 control contrast와 다른control동일도 필요하다. N=3/조건별1회의 운영 기준이며 p-value나 통계적 유의성을 주장하지 않는다.

GPUStats/RDG/Trim은 RHIMisc만1.711/1.444/1.128MiB/h 감소로 기준을 넘었다. 세 경우 모두 RSS가 baseline중앙값보다 높으므로 RSS/Commit 개선 또는 해결책으로 해석하지 않는다. 특히 Trim의 RHIMisc 감소는 최소효과1MiB/h를 소폭 넘긴 단발 관측이다. crash-off는Commit/RHIMisc의 감소가 컸으나 실제최저15.1928Hz로 비교하한을 통과하지 못했다. mimalloc은RSS34.070/Commit30.289MiB/h로 확장 악화 범위를 넘었다. 범위/처리량 기준을 사후 완화하거나 유리한 실행만 골라 재측정하지 않았다.

Trim 실제30회·총58.875ms·평균1.963ms·최대2.567ms, 즉시 RSS 감소 중앙값26.988MiB다. 이는 회수 가능한 allocator 보존이 있음을 지지하지만 RSS/Commit 꼬리 증가를 제거하지 못했다. 주기적 Trim을 제품 기본값으로 적용하지 않았다.


## 데이터 품질과 제한

모든14개 프로세스가 같은exe SHA256으로 정상종료했고 본 실행8개 모두 decoder0·CVar값 불변이다. binned2 baseline최저Hz16.2471/16.1386/16.2848,각cut최저Hz는 표와 같다. 요청20Hz를 낮추거나0Hz센서를 평균에서 제외하지 않았으며20Hz cadence 수용은 아니다. 모든UObject 꼬리 기울기는0이다.

외부 loopback invalid/오류0이며 내부encode/delivery/validation/overload/duplicate도0이다. 송신 제어 생략과 수신frame gap은 baseline3회 각각13/14/16,crash-off6/6/7,나머지cut각6/7/8이다. 두 카운터는 일치하지만 무손실/업무ACK 수용을 주장하지 않는다. acquisition/serialize/submit/receipt/검증수신의 원문 카운터는 run-summary.json에 별도로 보존했다.

전체sampler GT평균은 baseline2.754~2.798ms,cuts2.645~2.855ms이며 최대8.357ms였다. sampler에는 sidecar/Trim 비용이 포함되고 순수Trim 비용도 별도로 보관했다. source changes는 진단 hook·runner·범위 분석/테스트에 한정되며2단계의 LLM태그는 메인에 추가하지 않았다. 분석9신규 포함 Python32 PASS, Editor/Development package build PASS, NullRHI순수sampler/Trim schedule2 Success다. NullRHI를 RHI검증으로 세지 않고 실제8회packageD3D12/HWRT조건을 별도 확인했다. 전체release/직접mouse/회사수용은 이번 범위가 아니다.

1단계 범위는3표본의 관측 범위이며 모집단분포나 장기간 재현성을 보장하지 않는다. 고정순서·단일cut으로 교란/조합효과·원인전체귀속을 확정하지 않는다. 같은CVar의 GameThread값과 actualallocator는 검증했으나 모든engine query의 개수/수명은 계측하지 않았다. 2단계 태그 병합 후4단계는 Claude 지시를 기다리며 메인에서 시작하지 않는다.

최종 성공 기준은 Development package2시간 idle의 RSS/Commit꼬리≤8·RHIMisc≤2MiB/h, 원래#26 5쌍PASS, ABAB2%다. **이번30/60분 진단을 그 성공으로 바꾸지 않는다.** 제품 기본값/기존8MiB·5쌍 기준을 완화하지 않았다.

## 보호와 정리

개인설정44개 원본집합/SHA와 보호파일483개·Runtime소스해시를 확인했다. DTCore/gitlink·Config/Game.ini·PixelStreaming·운영맵/asset·Claude메모의 기존 상태를 보존했다. 동일 패키지를3단계까지 보관하고 완료 후663파일2,558,500,424bytes를 목록/SHA 보존 후 삭제했으며 빈디렉터리만 남았다. 설치engine/PDB·다른과거사본은 건드리지 않았다. push/PR/merge는 하지 않는다.
