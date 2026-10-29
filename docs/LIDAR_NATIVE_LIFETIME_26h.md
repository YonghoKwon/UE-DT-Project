# LIDAR-NATIVE-LIFETIME-26h — native 범위·수명 수정과 남은 메모리 문제

기준 `09c5e60`, branch `fix/lidar-native-lifetime-26h`. **GPU 결함 수정과 일부 RSS 개선은 확인했지만 원래 #26 전체 해결은 미완료다.** 최신 [RESULT](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/RESULT.md), result.json, merge-check.json에 수행·실패·skip·미수행을 구분한다. 로컬 자동화이며 회사/실장비 수용이나 직접 마우스 UI 수용은 포함하지 않는다.

## 확인한 결함과 안전 계약

native DXIL은 선택 전에 네 metadata root SRV를 모두 읽는다. [Root descriptor에는 크기 검사가 없다](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-descriptors-directly-in-the-root-signature). 해치 primitive21712/8840/0/84에서 작은65536-byte buffer가 큰 mesh index로 읽힐 수 있었다. 같은 프로세스의 fault0x3A3851000은 mesh3 metadata0x3A3810000+0x41000에 해당한다. page 단위 fault로 정확한 ray index를 확정하지 않으며 인접한 해제 TLAS 주소와 겹친 사실만으로 UAF라고 단정하지 않는다.

`55672be`는 모든 metadata binding을 장면 최대 primitive 도메인까지0padding한다. 작은 cache의 확장은 immutable 대체이며 이전 job은 원래 GPU 주소를 완료 fence까지 유지한다. `0f6bfd1`은 cache-only sharedref1·마지막 사용 fence 완료 조건에서만 BLAS/TLAS 입력·AS·scratch·instance buffer를 재사용한다. 현재 요청에 필요한 고정 revision은 재사용 슬롯에서 보호한다. 작은 buffer는 대체 확장하며 refit 대신 full rebuild를 사용한다. AS 상태를 유지한 읽기→쓰기 전후 [UAV barrier](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)를 둔다. live+cached+retired512MiB 상한·점 결과·품질·20Hz 정책은 유지한다. 진단 off는 `-SensorNativeNoGeometryPool`이다.

GBV는 command-list Reset 뒤 UNGUARDED 모드를 재적용·read-back 검증한다. 엔진 startup probe/warm-up과 정확히 확인된 ID527·엔진 command list·SceneDepthZ barrier 오류는 native 자원과 소유 범위를 분리한다. native list/buffer·유사명·다른ID·CORRUPTION은 제외하지 않는다. 원문과 엔진소스/log SHA를 보존하며 전체 프로세스 오류0을 주장하지 않는다. native 오류0 기준은 유지한다. 자체 드라이버 조회의 렌더 스레드 CVar ensure는 `2cb9d14`에서 초기화된 RHI 값 사용으로 수정했다.

## 기본 경로와 자동화 격리

자동 native는 실제 검증한 NVIDIA0x10de / 정확한 RTX4070Ti / driver621.01(621.1 표기 허용)에 한정한다. 더 높은 미검증 버전도 지원 RHI를 유지한다. `-SensorRtNative`는 명시적 진단, `-SensorRtRhi`는 지원 RHI 강제 선택이다.

세션 guard는 덮어쓰지 않고 건강한 GPU drain 뒤에만 삭제한다. 비정상 종료·device removal·validation/submission 실패는 이후 native 접수를 중단하거나 다음 실행을 RHI로 보호하고 로그를 남긴다. 제거된 장치를 같은 프로세스에서 복구했다고 주장하지 않는다. 실제 사용자 crash guard를 자동 해제하지 않는다.

Automation의 강제 정상 종료도 guard를 남기므로 Hatch/perf/smoke는 실행별 새 UserDir를 사용한다. 초기에 RHI로 전환된 자료를 native라고 해석한 것은 정정했다. task PID59576이 만든 메인 guard만 시각·PID·log·SHA 근거로 원래 없던 상태로 복원했다. fixture는 각 UserDir의 Saved/Reports로 정확히 복사·해시 검증하며 release는 기존1920×1080 조건을 명시하고 환경을 복원한다. 제품 parser·테스트 기대값·asset은 바꾸지 않는다.

## 검증 결과

- 최종 패키지3대1000Dig/scoped GBV/DRED: native19624제출, buffer생성27/재사용7413, native오류0·fallback0·RHI scene혼입0·device removal0.
- 실제 RHI wide metadata/cache-growth1000·실제 해치1000·다중 scene whole-byte oracle3개 PASS/skip0. 1대 oracle와3대 stress는 별도 증거다.
- 해치2시간 두 번, Rev7×3·PCD·preview기본: actual Hz 최저19.996810/19.997508, fallback0. 준비 후 새 native buffer/BLAS/TLAS/view/upload 생성과 live/peak 증가0. 직접 native GPU 할당은 별도 gauge로 기록하므로 RHIMisc만으로 설명하지 않는다.

|조건, MiB/h|RSS|Commit|RHIMisc|
|---|---:|---:|---:|
|Hatch2h-1 마지막1/3|1.818|1.287|-.0146|
|Hatch2h-1 마지막20분|**8.280 FAIL**|5.360|.0661|
|Hatch2h-2 마지막1/3|3.782|.516|-.0135|
|Hatch2h-2 마지막20분|1.054|-2.184|-.0252|
|LiDAR 미생성60|-1251.485|-2.172|.0706|
|Shipping Hatch60|76.603|38.929|미측정|
|사행 맵 기본 Depth60|-3.777|2.302|.0222|
|사행 native 진단60|7.175|4.593|-.0534|
|코일 맵 기본 native60|5.448|3.495|.0124|

목표8/8/2에 대해 마지막1/3은 두 번 모두 이내다. 기존 분석기는 추가 마지막20분도 요구하므로1회차 MemoryGoalsMet=false,2회차 true를 보존한다. 근소한 초과도 면제하지 않는다. RSS 급락은 Commit이 유지된 상주 집합 변화이며 할당 해제의 증거가 아니다.

Shipping OS 꼬리는 Working Set+74.912 / Private Working Set+75.198 / private Commit+37.134MiB/h이다. PrivateBytes와 Commit은 같은 private commit이다. LLM 비활성의0을 RHIMisc 측정으로 세지 않는다. watcher 지연으로 종료 정리 표본이 섞인 자체 분석 오류는 runtime UTC 정렬·불확실한 끝 경계 보호로 수정했고 raw/이전 분석을 보존했다. 관련10개 테스트가 통과했다.

사행 Rig는 원래 DepthProjection을 명시한다. 기본 맵60분과 공개 Actor API로 HardwareRayTracing을 선택한 진단60분을 분리했다. 맵·저장값·품질·출력은 유지한다. 코일은 기본 Auto에서 native를 사용했고 override하지 않았다.

원래 #26은 새 UserDir·동일 소스·17warm Dig+35초·90초 창·3Rev7·12000preview로5쌍을 완주했다. 절대 RSS는5/5통과했지만 Commit은50.973~148.297MiB로5/5실패했다. RSS 차이도1쌍10.676MiB로실패했고 Commit 차이 중앙값106.840MiB다. pair4 engine/GT/callback173.776/171.691/174.938ms로150ms도실패했다. 이 실패는 ABAB 중앙값 통과로 면제하지 않는다.

RT on ABAB2%는 A/B각6회, FPS59.986796→59.993312, p9516.666900→16.666900ms, GT6.581504→6.539184ms로 통과했다. 최종 release는 실행Success469/Skip44/실패0, WBP7개 메모리compile·어휘검사도 통과했다. Skip44는 native 통과 증거가 아니다. 분석단위33개·quick10개·layer0→0·DTCore guard가 통과했다. 필수 gate 실패가 남아 추가8시간은 수행하지 않았다.

## 남은 원인과 보호 범위

모든50Dig에서 native 새 buffer/live/peak 증가는0이고 재빌드50·재사용350회다. PlatformUntracked는66~145MiB 증가하지만 FMalloc은 대부분 줄거나 수MiB 이내다. geometry 변경의 engine procedural mesh/physics 업데이트·driver AS build/query metadata·할당자 retention이 후보이며 정확한 호출자는 미확인이다. Shipping3센서·PCD·preview·분석과1센서 맵의 차이도 추가 분리해야 한다. Untracked 전체를 driver나 실제 누수라고 부르지 않는다.

VMMap의 PrivateData 증가는 OS heap과 동의어가 아니다. ETW VirtualAllocation은 현재 non-admin token에서0x80070005로 거부됐고 세션0이다. 다음 선택지는 권한이 있는 Windows VirtualAlloc/heap 추적이다. 미확인 원인에 대한 추가 추정 수정과 엔진·드라이버·DTCore 변경은 하지 않았다.

main 검증은 기록된 초기 사용자 overlay를 포함한다. ABAB archive는 그 overlay 없이 기준/후보를 비교하므로 출처를 구분한다. native core1737199와 진단3e3e의15개 core/shader 파일은 해시가 같으며 유일한 runtime 차이는 명시적 generic 진단 플래그다. 각 빌드·측정 source/hash를 Saved manifest에 남겼다.

DTCore·Config/Game.ini·Samples/PixelStreaming·SensorTestMap·초기 사용자 변경은 수정/stage하지 않았다. 개인242파일·보호310파일과 사용자 변경 바이트를 검증·복원했다. task VMMap registry와 main guard는 원래 없던 상태로 복원했다. 소유 package/dump/trace 약19.4GB와 도구를 정리했고 엔진 PDB·Editor·공유 캐시·사용자 asset은 유지했다. 비밀 노출·push·PR·merge는 없다. 원래 Commit/150ms 실패와 Shipping 증가를 남기고 검수용 커밋 후 멈춘다.

## 샘플러 비용

5초 간격의 수집·JSON·write/flush·파괴를 포함한 GT 비용은 해치2시간1회차 평균2.009/최대14.488ms,2회차 평균1.948/최대19.828ms/표본이었다. native 사행 평균0.833/최대6.183ms,코일 평균0.839/최대8.733ms다. 이 비용은 관찰한 실측이며 sampler off/on 상대 게이트를 새로 통과했다고 주장하지 않는다.

## 2026-10-29 r17: Shipping·디깅 Commit 분리 조사 중단

재개 기준은 `6be56b7`이다. 기존 Shipping은 실제 native3대·약20Hz·fallback0이었고, 첫 Development60은 다른 revision 및 내부 샘플러였다. 같은 소스의 Development/Shipping 패키지를 빌드하고 `-NoLLM -SensorRtNative`·같은 외부5초 OS 샘플러 비교를 준비했다. Shipping의 Slate throttling 명령행 미적용은 전용 UserDir 진단 설정으로만 맞췄고 제품 기본값은 변경하지 않았다.

새 Development60분의 외부 마지막20분은 Working Set **+41.546**, Private Working Set **-1.517**, private Commit **-1.480MiB/h**다. 10~60분은 WS+25.391/Commit+18.002로 창에 따라 다르며, WS8기준 실패를 유지한다. VMMap10→40분 Commit은 Private Data+11.539/Heap+.258MiB, Mapped/Image0이다. 전체 WS와 private 할당을 동일시하지 않으며 LLM 비활성을 RHIMisc0으로 해석하지 않는다. GT 샘플러 비용은 평균1.339/최대6.892ms/5초다.

다른 프로젝트의 `DTCore.Soak` NullRHI Editor가40분부터 겹쳤다. 이를 WS 증가의 원인으로 확정하지 않지만 깨끗한 비교 조건이 아니며 새 통과 증거로 세지 않는다. Shipping60과 후속 B 실험은 Editor guard에서 중단됐다. 외부 작업은 조작·종료하지 않았다.

기존90초5쌍 재분석은 Dig Commit 평균+96.377MiB, PlatformUntracked+101.732/FMalloc-5.542MiB다. Physics/ChaosTrimesh·관측 프로젝트 태그·native 신규/live 버퍼의 지배적 순증가는 없다. 4쌍은 처음10회 이후 대체로 비슷한 수준이고1쌍은 변동이 크다. 엔진 ProceduralMesh의 같은 토폴로지 갱신도 엔진 BLAS를 Release/Init한다는 코드 근거와 함께 예약·보존을 우선 후보로 두지만, 실제 누수 부재나30분 수렴은 입증하지 못했다.

진단 snapshot request/ack 경계·명시 CPU 참조·Editor 전용 충돌 재쿠킹 생략 토글을 작성하고 Editor 빌드했다. 재쿠킹 생략은 의도적으로 stale collision인 할당 분리이며 제품/수용 검증에 사용할 수 없다. 8MiB/150ms assertion은 변경하지 않았다. 새 토글의 실제 RHI 회귀·Shipping 재측정·30분 디깅·원인 수정·수정 후5쌍/1000회/byte oracle/ABAB은 미수행이다. 기존 #26 실패는 그대로다.

자세한 표·그림·출처·환경 제한·복원/정리·재개 조건은 [r17 RESULT](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r17/RESULT.md), result.json, merge-check.json을 참조한다. 초기 사용자 변경12파일·보호 경로·기준을 보존하고 질문 없이 중단 지점의 결과만 커밋한다. 외부 검증 종료 후 새 실행 label과 깨끗한 환경에서 비교를 다시 시작해야 한다.

## 2026-10-29 r18: 깨끗한 패키지 비교·디깅 실패 재현

재개 기준 `e89c968`. 외부 Editor를 감시하고 중첩된 Shipping45분/3분 시도를 취소·보존한 뒤 재실행했다. 같은 runtime 소스·native3대·NoLLM·Slate0·60FPS·동일 외부5초 샘플러의 깨끗한60분 꼬리는 Shipping WS/Commit **4.782/3.040**, Development **-.527/-.805MiB/h**다. 실제 약19.996/19.997Hz,fallback0. 다만10~60분 전체는 Shipping34.929/36.918,Development7.915/8.776이므로 전체 증가나 #26 해결을 선언하지 않는다. WS만 기준 밖으로 증가한 새 실행이 없어 EmptyWorkingSet 진단은 미수행이다.

|원래5쌍,MiB|1|2|3|4|5|
|---|---:|---:|---:|---:|---:|
|Dig Commit|94.504|82.566|71.914|139.355|95.445|
|Idle Commit|-10.449|-5.523|-17.773|-12.316|-5.930|
|Dig RSS|1.910|-5.203|7.055|3.730|5.039|

원래 조건90초·50회/0회·snapshot gate 없음·같은SHA `003f23c`로 Commit 절대/차이5/5FAIL, RSS 절대5/5PASS·차이3/5FAIL, 예상 밖 기능/150ms 오류0이다. 기준8을 유지한다.

VMMap 경계 계측에서는 증가가 작았으나 원래 조건 실패를 가렸다. 무계측 재확인은 CPU-RTon Commit140.199/RT-off133.922/재쿠킹 생략76.242/cargo렌더제출 생략110.660/점미리보기off55.152MiB로 모두8실패다. BLAS·물리·cargo렌더·preview 중 하나를 원인으로 확정하지 않는다. 경계 대기만으로 Untracked 순증가가 작아졌지만 실제 뒤 경계도2~4초여서 앞/뒤 효과를 완전히 분리하지 못했다. 짧은 외부 counter burst에서도 ACK 전 큰 직접 회수는 보이지 않았으며 이후 재생성·프로세스 종료값은 제외했다.

30분1804초·882dig·17복원은 기존 분석기 **누수 의심**을 유지한다. 내부 마지막1/3 RSS/Commit14.969/60.856,외부OS8.762/-17.320MiB/h다. 큰 일시 변동·관측 위상 차이가 있고 상한은 단조 증가하지 않았지만 안정화/누수 부재로 바꾸지 않는다. UObject 마지막1/3기울기0,RHIMisc.269MiB/h이며 NoLLM 패키지의 미측정RHIMisc와 혼동하지 않는다.

새 코드는 분석기의 중첩/감시공백 거부와 Editor 전용 audit+`HatchMemorySkipCargoRender` 진단뿐이다. 후자는 CPU표면·물리·센서는 갱신하되 시각표면이 stale이며 제품/수용 경로가 아니다. 제품 기본값·품질·Hz·엔진/DTCore/드라이버·기존 assertion 변경0. source는 이미 같은 topology에서 UpdateMeshSection을 쓰고 privateproxy cast 때문에 CreateSceneProxy만 대체하는 것은 안전하지 않다. 원인이 확정되지 않아 추정에 기반한 렌더러 재작성이나 물리 생략을 제품에 적용하지 않았다.

다음 추적은 무계측 재현 상태의 Stop/GC/Drain/EndFrame 주위 할당·VirtualAlloc 수명/스택을 대조해 공통 분석/UI·RHI/드라이버·비동기 수명을 구분하는 것이다. 이번에 새 ETW/heap stack attribution은 수행하지 않았다. 자세한 수치·한계·보존·정리는 [r18 RESULT](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r18/RESULT.md), result.json, merge-check.json을 따른다. #26은 미해결이며 이전 통과나 진단 대기 결과로 원래 실패를 대체하지 않는다.
