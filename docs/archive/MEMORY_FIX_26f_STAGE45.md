> **과거 설계·검증 원문, 현재 기준 아님.** 원문을 보존한 보관본입니다. 현재 사용·규칙·미해결은 [문서 색인](../README.md)을 따릅니다.

# MEMORY-FIX-26f — 2단계 통합·4/5단계

전체계획기준 `d8048bf`,이번단계시작 `6b682da`,Runtime `f8dc70d`,branch `chore/memory-fix-26f`. `4018dcd`·`719270f`를 `a0461e3`에서병합했고Probe충돌에서엔진진단hook과태그기록을모두보존했다. 후속 `5bdf5b1`·`f8dc70d`는기본꺼짐진단지원이다. **5개분리조건에서메모리개선후보없음, 제품메모리개선수정미적용, #26미해결.**

## 통합·진단 계약

7개LLM leaf와60개scope·분석기확장을통합했다. Rev7×3에는CameraGT작업이없어6개leaf가자연관측됐다. 별도30초registry검증에서만명시된4KiB CameraGT canary를할당해7개writer이름을실제기록했다. CameraGT기능수용이아니며장시간측정에는canary0/미사용이다. absent를0으로채우지않는다.

프로세스별 `-HatchMemoryExperiment -Memory26Case=baseline|preview-reuse|scene-reuse|readback-reuse|point-leases4|pcd-scratch-reuse`다. 모두동일패키지·단일조건분리다. scene은정확한meshrevision/pose키와LRU4,readback은terminalfence후GPUobject4·CPUhit4/32MiB,PCDscratch4/32MiB,preview배열4/8MiB,point는기본3/진단4lease다. consumer가보유중인payload를재사용하거나품질·요청Hz·출력bytes를바꾸지않는다.

첫qualification은package의고정preview경로가미실행되어중단됐다(30분본실험미시작). UE5.3 `UEngine::InitializeObjectReferences`는AllowDebugViewmodes에서만VertexColorMaterial엔진포인터를적재한다. 동일엔진재질의cookreference와진단resolver·fixed/legacy분기카운터를추가한뒤모든qualification을통과했다. 기본렌더선택은그대로이며재질참조의상수로드비용은공통패키지에포함된다. 엔진CVar·제품설정·FIFO/예산·wire·SensorTestMap은바꾸지않았다.

## 4단계 결과

Runtime `f8dc70d`, 단일 Development exe SHA256 `D2A4892DF5BA64DF7D70E14E64A49F563EE3BA0E09E4DA4620B74C10F1770B56`. 각30분, 첫600초 제외, 공통 마지막1200초 실제시간OLS. 요청20Hz·Rev7×3·Full압축PCD·기본미리보기·LLM·1920×1080 offscreen/font100/UMG1, binned2/enginecontrols1·1·1/GPU-time0/Trim0. 쿨다운120초·환경 before/5초표본·decoder0.

| 조건 | RSS MiB/h | Commit MiB/h | RHIMisc MiB/h | 최소 실제Hz | 후보 |
|---|---:|---:|---:|---:|---|
| 00-baseline | +18.350 | +15.269 | +7.766 | 13.845469 | reference; Hz 미달 |
| 01-preview-reuse | +29.333 | +31.493 | +10.343 | 19.998815 | 없음 |
| 02-scene-reuse | +13.470 | +11.449 | +8.371 | 15.398692 | 없음; Hz 미달 |
| 03-readback-reuse | +16.899 | +17.634 | +10.579 | 17.243583 | 없음 |
| 04-point-leases4 | +15.970 | +13.460 | +7.647 | 15.768930 | 없음; Hz 미달 |
| 05-pcd-scratch-reuse | +25.085 | +20.468 | +10.603 | 16.362098 | 없음 |

1단계 범위/효과/처리량/다른RAM악화 기준을 그대로 사용했다. RSS/Commit/RHI 중앙값17.429/19.146/11.128, 범위6.706/9.803/.729MiB/h, 최소효과 `max(range,1)`, Hz하한15.815810이다. taggedbaseline과의차이도 별도 저장했다. 과거범위보다 낮더라도 현재기본보다 RHI가 낮지 않은 장면 조건, 현재기본대비0.12차이인 point4를 개선으로 해석하지 않는다.

| 조건 | TLAS 생성/재사용 | readback 생성/재사용 | hit 배열 생성/재사용 | preview 배열 생성/재사용 | preview fixed/legacy 생성 | point fallback | PCD scratch 생성/재사용 |
|---|---:|---:|---:|---:|---:|---:|---:|
| 00-baseline | 49928/0 | 49928/0 | 49927/0 | 12938/0 | 0/12938 | 3 | 49914/0 |
| 01-preview-reuse | 72063/0 | 72062/0 | 72063/0 | 0/14079 | 0/0 | 15 | 72042/0 |
| 02-scene-reuse | 0/55591 | 55591/0 | 55592/0 | 13883/0 | 0/13883 | 31 | 53202/0 |
| 03-readback-reuse | 63012/0 | 0/63012 | 0/63011 | 14799/0 | 0/14799 | 148 | 62998/0 |
| 04-point-leases4 | 56938/0 | 56938/0 | 56938/0 | 14209/0 | 0/14209 | 0 | 56924/0 |
| 05-pcd-scratch-reuse | 59531/0 | 59531/0 | 59533/0 | 14808/0 | 0/14808 | 167 | 0/59517 |

카운터는 첫600초 뒤부터 끝까지의 차이다. 모든 실행에서 BLAS 추가생성0이며, idle의 BLAS 매프레임 재생성은 관찰되지 않았다. TLAS/readback/preview/PCD 할당 빈도는 해당 옵션으로 제거했지만 RSS·Commit 개선을 확정하지 못했다. point4는 fallback을0으로 만들었으나 기울기 개선을 확인하지 못했고 추가 상주capacity 비용도 있다.

| 조건 | RtScene | Readback | Points | Preview | PcdEncode | HatchAnalysis | Untracked | FMallocUnused |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 00-baseline | -0.019 | +0.413 | +0.018 | +0.109 | +1.843 | +0.001 | -2.460 | -1.766 |
| 01-preview-reuse | -0.205 | -0.124 | -1.543 | -0.000 | -0.412 | +0.000 | +23.152 | +6.748 |
| 02-scene-reuse | -0.235 | +0.039 | -0.387 | -0.060 | -1.419 | -0.002 | +0.643 | +0.989 |
| 03-readback-reuse | -0.040 | -0.011 | +2.187 | +0.336 | +1.319 | -0.001 | +1.315 | -0.465 |
| 04-point-leases4 | -0.016 | -0.848 | +0.555 | +0.444 | +1.410 | +0.001 | +6.767 | +1.917 |
| 05-pcd-scratch-reuse | +0.107 | -0.382 | -2.657 | +0.199 | +0.756 | +0.000 | +7.025 | +3.049 |

단위MiB/h. leaf는 서로/부모/RHI와 합산하지 않는다. 순간 live amount의OLS는 프레임 위상과 겹치므로 작은 양수 기울기를 누수 확정으로 부르지 않는다. CameraGT는 관측되지 않았다(0으로 채우지 않음). UObject 꼬리도 함께 원본분석에 보관했다.

다섯 경로 모두 제품수정 후보 없음. 엔진 query/profiler/cache·allocator/driver 보존과 계측 밖 native 경로는 여전히 가설이다. 프로젝트 leaf가 평탄하다는 사실만으로 이를 확정하거나 우리 코드를 완전히 배제하지 않는다. 우리 RDG/GPU 통계 스코프 검색에는 동적 이벤트 이름/통계 스코프가 없어 이름 정적화나 scope 제거를 적용하지 않았다.

원본8MiB/5쌍·최종2시간RSS/Commit≤8/RHI≤2·ABAB2% 기준은 완화하지 않았다. 현재 60분 추가기본 확인은 제품수정 효과검증이 아니라 원인미확정 상태의 재확인이다.

## 5단계 결과

5개 분리 경로에서 기존 기준을 통과한 메모리 개선 후보가 없어 제품 동작 수정은 적용하지 않았다. 같은 f8dc70d 패키지로 기본 조건을60분 추가 기록했다. **수정 효과 검증으로 부르지 않으며 #26은 미해결이다.**

| 지표 | 마지막20분 MiB/h | 첫10분 제외50분 MiB/h | 진단 목표 |
|---|---:|---:|---:|
| RSS | +18.236 | +16.944 | ≤8: 미달 |
| Commit | +15.014 | +15.008 | ≤8: 미달 |
| RHIMisc | +11.819 | +10.132 | ≤2: 미달 |

실제 센서Hz는 16.366501, 16.336519, 16.693645. 모든 센서를 포함했고 요청20Hz를 낮추지 않았다. UObject꼬리0.000000/h. 전체sampler GT평균2.804ms/표본,최대8.011ms,총2018.807ms이며수집·태그조회·sidecar·JSON·write/flush·임시객체소멸을포함한다.

이 실행의 leaf 꼬리: RtScene −0.092 / Readback +0.157 / Points −0.007 / Preview −0.144 / PcdEncode +0.443 / HatchAnalysis +0.0003MiB/h. CameraGT는미관측(0대체없음). RHIMisc +11.819, Untracked +1.816, FMallocUnused +1.618MiB/h다. 중첩/계층지표를합산하지않는다. 순간live표본의작은기울기로개별누수를확정하지않으며 CPU leaf가GPU/driver회계를완전히포함하지않는다.

BLAS추가생성0, TLAS148274/readback148275/hit148276/PCDscratch148239 생성, previewlegacy36879 생성, pointfallback266회(모두제외후50분차이)였다. 임시할당빈도는확인했으나,이를끄거나재사용한분리결과로RSS/Commit증가의원인을확정하지못했다.

우선 남는 후보는 RHI/query/profiler/cache와 allocator/driver 보존 또는 계측 밖 native 수명이다. 이전Trim에서즉시RSS회수는있었지만꼬리개선은없었다. 현재결과만으로엔진/드라이버누수라고확정하지않으며 ray/resultGPU업로드버퍼·deferredRHI수명등모든경로를배제한것도아니다.

최종2시간 idle·원래#26 5쌍·회사수용은이번단계에서미수행이다. 다음6단계는Claude검토후별도지시다.

## 검증과 한계

Editor/Development package build PASS, Python40PASS, 순수테스트7Success(초기2중복포함시9실행), 실제RHI검증13Success/1Fail/0skip이다. 실패는 `HATCH03.DigRuntimeRhi`의기존8MiB rawRSS/retainedRSS/Commit3assert다. 기대값을바꾸지않았다. previewstartup에비치명SwapChain4ensure가있었고보존했다. RTonABAB3블록/12구간(기준d8048bf→f8dc70d,1Camera+1Rev7실제HWRT,offscreen1920×1080,10초+30초,2%)은 **PASS**. 자동화·성능·직접mouse/회사수용을구분한다.

같은exe로qualification7회+30분6회+60분1회=14프로세스,전체관측7회complete/exit0/decoder0·내외부invalid0이다. 요청20Hz수용이나무손실/업무ACK를주장하지않으며 sender제어생략·receivergap은별도보존했다. sampler GT평균2.529~2.804ms/표본,전체최대10.098ms였다. 기본30분·scene·point4의처리량미달을비교가능으로바꾸지않았다. N3과거범위/조건별1회·고정순서로통계적유의성이나전체인과를확정하지않는다.

개인설정·보호경로·패키지정리·최종quick/layer/guard/schema/merge_check는 [RESULT](../../Saved/Reports/MEMORY-FIX-26f/RESULT.md)·[result.json](../../Saved/Reports/MEMORY-FIX-26f/result.json)·[merge-check](../../Saved/Reports/MEMORY-FIX-26f/merge-check.json)의최종감사를따른다. DTCore/gitlink·Config/Game.ini·PixelStreaming·운영맵/uasset·Claude메모를수정/stage하지않았다. push/PR/외부merge는하지않았다(명시승인LLM브랜치의로컬병합만수행).

[4단계표/원문](../../Saved/Reports/MEMORY-FIX-26f/stage45/stage4/RESULT.md)·[5단계표/원문](../../Saved/Reports/MEMORY-FIX-26f/stage45/stage5/RESULT.md)·[고정계획](../../Saved/Reports/MEMORY-FIX-26f/stage45/measurement-plan-v2.json)·[전체수치](../../Saved/Reports/MEMORY-FIX-26f/stage45/measurement-summary.json)·[분리비교](../../Saved/Reports/MEMORY-FIX-26f/stage45/comparison-stage4.json)·[태그qualification](../../Saved/Reports/MEMORY-FIX-26f/stage45/tag-recording-qualification.json)·[회귀](../../Saved/Reports/MEMORY-FIX-26f/stage45/preparation-summary.json). 이전1·3단계상태는 [별도기록](MEMORY_FIX_26f_STAGE13.md)에보존한다.

## 최종 보호·정리

개인설정253개와보호483개원본집합/SHA를검사해복원했다. GUI창배치는조작하지않았다. 현재작업소유패키지·gameexe/PDB·ABAB사본5729파일16,875,385,127bytes는소유권·prefix·reparse·SHA검사후파일별로삭제했고빈폴더만남았다. 패키지6,318,167,284bytes와성능사본10,557,217,843bytes다. 현재작업trace생성0이며Engine/PDB·mainEditor·DTCore·공유Cooked/Intermediate·이전26e성능사본을건드리지않았다.

RTonABAB는 FPS −0.000257% / enginep95 +0% / GT +0.480829% / GPU −5.722628%로2%통과했다. 절대중앙값은A→B FPS59.999897→59.999743, p9516.666900→16.666900ms, GT5.141758→5.166481ms, GPU6.783865→6.395649ms다. 관련HATCH03와60분진단목표는실패로보존하므로성능PASS를전체메모리성공으로해석하지않는다.
