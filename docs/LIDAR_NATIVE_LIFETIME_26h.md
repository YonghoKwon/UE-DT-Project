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

## 2026-10-30 r19: Windows 할당 스택 추적 권한·수집 신뢰성 차단

기준 `276ac77`. 세션1은 RDP이며 RTX4070Ti(621.01)·Remote Display Adapter를 기록했다. 실제 D3D12 선택은 NVIDIA adapter0이지만 엔진 startup의 `SwapChain4.GetReference()`/`EnsureColorSpace` 오류가 발생했다. 오류 원문은 결과 요약에 보존하며 콘솔 환경의 성능·메모리 수용 증거로 세지 않는다.

WPR VirtualAllocation은 `0x80070005`, xperf NT Kernel Logger 및 별도 SystemTraceProvider API는 Access denied(5)로 차단됐다. 기존 Performance Log Users 권한으로 대상 Heap 이벤트는 수집됐지만 검증용 할당에도 스택이 없어 호출 경로 분석에 부적격이었다. 권한·IFEO/GFlags·엔진·드라이버 설정은 변경하지 않았다.

CDB 대체 진단에서 native3대·50회/90.728초의 RSS **+2.531**, Commit **+144.902**, LLM Untracked **+143.898MiB**를 관찰했다. 편집 최대63.357ms이나 engine400/game506.145/callback491.657ms로150ms 실패이며 디버거/기본 debug heap 영향이 있는 별도 진단이다. 기존8MiB/150ms assertion을 유지했다. UObject/ChaosTrimesh 순증가0·RHIMisc-.039MiB는 관측치이며 누수 부재를 뜻하지 않는다.

단일·8스레드 검증 할당은 주소/크기를 대조했지만 실제 Unreal에서는 진입/복귀 누락이 발생했다(첫 완료 수집 missing31646,pending21618). 호출 프레임 대조·late install·debug heap 해제 재시도도 자격 실패했고 외부 ShaderCompileWorker56564 중첩으로 취소됐다. **이 자료의 크기·수명 합산과 상위 증가 스택 순위는 거부**한다. 관찰된 `MiMallocVirtualAlloc`·NVIDIA UMD RVA 스택만으로 증가분 소유자를 확정하지 않는다.

Shipping 기존10~60분 WS/Commit34.929/36.918 대 마지막20분4.782/3.040MiB/h, VMMap10→40분 Private Data+12.543/Heap+.645MiB를 유지한다. 이전 원시 시계열·로그가 정리되어 계단 시각과 GC/송신/streaming 이벤트의 정확한 대조는 불가능하다. 새 Shipping 스택 수집은 커널 ETW 권한 차단으로 미수행이며 새 통과 결과를 만들지 않는다.

추가한 [WPR 수집기](../Scripts/record_windows_memory_trace.ps1)는 기존 정상 실행의 owned-process를 확인하고 고유 instance만 start/stop한다. 자동 권한 상승·영구 설정 변경·타 세션 종료는 하지 않는다. 비관리자 거부·구문 검사만 검증했으며 관리자 실제 수집은 미검증이다. [분석기](../Scripts/analyze_windows_allocations.py)는 reserve/recommit/해제를 구별하고 누락·미완료 collector를 거부한다. ETL은 WPA에서 대상 PID·경계·lost events·정확한 PDB를 확인해야 하며 이 JSON 분석기에 직접 입력하지 않는다.

원인 확정·제품 수정은 없고 사용자 지시대로5번 최종 검증을 시작하지 않았다. 상세 실패/미완료·환경·재실행 명령·정리는 [r19 RESULT](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r19/RESULT.md), result.json, merge-check.json을 따른다. #26은 미해결이다.

## 2026-10-30 r20: ETW 실할당 경로와 해치 헤더 안정화

기준 `8504aee`, 구현 `c8f232f`(진단 전용 UTC/QPC 경계)·`f7a4c34`(헤더 예약과 선택형 풀 진단). 디깅 Commit의 큰 증가원을 텍스처 생성까지 좁혀 최소 UI 수정을 적용했다. 전체 #26 해결 판정은 보류한다.

정상 ETW 상위 경로는 `FD3D12Adapter::CreateCommittedResource → FD3D12TextureAllocatorPool::AllocateTexture → FRenderTargetPool::FindFreeElement → FRDGBuilder::Execute → RenderViewFamilies_RenderThread`다. dig-etw-03-quiet의 상위 backing은 90.938MiB, idle의 같은 경로는 0이다. 엔진·프로젝트 PDB의 GUID/age 및 함수를 포함하는 RVA 범위를 확인했고 nearest export는 사용하지 않았다. 미확인 드라이버 함수는 module+RVA로 남겼다. lost events가 발생한 dig-etw-02는 귀속 판정에서 제외했다. 렌더 스레드 스택에 게임 스레드 UI 호출자가 직접 나타나는 것은 아니며, 우리 코드 기원은 뷰·레이아웃 시각과 통제된 수정 전후 결과로 확인했다.

90초·50회 디깅에서 `TSR.Output`과 `TSR.History.Metadata`가 각각 200회 생성됐다. 폭 3776은 같고 높이는 1348↔1376, 포맷은 26/71, mip은 2/1, flags는 0x20008이었다. 헤더가 미리보기/신선한 결과에서 123↔136px, 장면 높이가 687↔674px로 바뀌었다. 양쪽 디스크립터가 준비 구간에 이미 있어 주기적 목록만으로는 교체를 식별하지 못했다. 비실시간 배경 Editor 뷰와 TextureTarget이 없는 세 LiDAR SceneCapture는 제외했다.

제품 수정은 ShipHatchMonitorWidget 두 파일의 private 레이아웃 캐시다. 같은 viewport/font에서는 필요한 최대 헤더 높이를 예약하고 실제 크기·글꼴 변경 및 Bind에서 재계산한다. 문구 자르기, 센서 Hz·품질·기본 정책 변경은 없다. 수정 후 진단 50회에서 장면 사각형 변화 0px, 새 TSR 텍스처 생성 0회, RSS −4.844 / Commit −11.508MiB였다. 풀 진단은 기본 off이며 추가 캡처·trim·flush·풀 참조 보존을 하지 않는다.

원래 조건: 준비 35초, 90초, 50dig vs 0dig, Rev7×3/native, PCD off/preview on, 1920×1080/font100/view1, LLM/RHI audit/fullRHI drain. 추적·VMMap·풀 진단 없이 5쌍을 수행했고, 경계 UTC/QPC 메타데이터와 외부 OS·환경 감시는 남겼다. 완료된 3쌍은 재사용하고 취소된 네 번째 대조 시도는 제외 후 재실행했다.

|쌍|50dig RSS MiB|50dig Commit MiB|RSS 차이 MiB|Commit 차이 MiB|절대 8MiB|별도 차이 8MiB|
|---|---:|---:|---:|---:|---|---|
|1|2.203|0.133|7.637|11.734|PASS|FAIL|
|2|-5.125|-12.355|-3.715|-7.719|PASS|PASS|
|3|-2.996|-9.391|1.902|0.934|PASS|PASS|
|4|2.734|-1.699|8.078|4.246|PASS|FAIL|
|5|-1.434|-3.438|-1.695|1.355|PASS|PASS|

5회 디깅의 RSS·Commit 절대 8MiB 및 edit/engine/game/callback 150ms는 모두 PASS, 장면 변화는 0px, 예상 외 오류는 0이다. 별도 차이 기준은 pair1 Commit 11.734MiB와 pair4 raw/retained RSS 8.078MiB가 FAIL이다. 중앙값 RSS/Commit 1.902/1.355MiB는 설명용이며 개별 실패를 덮지 않는다.

수정 전 30분·880dig에서는 풀 114~116개, 2,803,948~2,835,628KiB 범위를 오갔다. 마지막 1/3 RSS 0.908 / Commit −44.692MiB/h, UObject 0/h이며 Commit 진폭은 약 200MiB다. 슬롯·주소 재사용이 있으므로 GPU 해제 완료나 모든 누수 부재를 단정하지 않는다. UE5.3 풀은 free·디스크립터 호환 조건으로 재사용하고 unused>2frames와 최소 풀 400MiB 정책으로 트리밍한다. OnFrameStart의 10frame 반환 힌트를 이 tick이 직접 해제로 소비하지 않는 점도 확인했다. 엔진 CVar·기본값 수정은 없다.

새 Shipping 추적의 15~19분 OS Commit +11.664MiB는 PCD 수신·디코딩·LZF·파서가 상위였고, 19~20분 +23.105MiB에는 Hit 배열 backing 8.254MiB, 디코딩 3.352MiB, D3D12 ExecuteCommandLists 3MiB가 보였다. 해당 구간에서 확인된 CreateCommittedResource/AllocateTexture/RenderTargetPool 스택은 없어 같은 TSR 원인으로 판정하지 않는다. 옛 r19 계단에는 정확한 이벤트 추적이 없어 소급 귀속하지 않는다. HitPool은 최대 4개/32MiB로 반환·Reset되며 Ray 요청은 scan 수명에 소유된다. backing page 확보는 live 배열 누수의 증거가 아니므로 추가 수정하지 않았다.

Shipping 10~60분 WS 22.611 / Commit 21.163MiB/h와 마지막 20분 −6.013/−5.728을 함께 보고한다. VirtualAlloc 10→40분 +24.859MiB와 OS +8.125MiB 사이 −16.734MiB 잔차가 남아 전체 증가원 귀속은 미완료다. Shipping은 UI 수정 전 소스이며 이번 재개에서 새 60분이나 5단계 장시간 검증을 시작하지 않았다.

회귀 6개(1000회 디깅, 다중 장면 byte oracle, 150% 레이아웃, 해치 표시, runtime, 짧은 3대 성능) PASS. 1000회는 다른 2대를 멈춘 1대 활성 harness이고 GBV는 켜지 않았다. 실제 3대 획득률은 19.989/19.989/20.022Hz, 평균 60.00FPS·GT 6.357ms다. 직접 PIE 클릭·글꼴 100/150, 별도 일반 창 최대화/복원·1600×900 재배치를 DPI96에서 확인하고 캡처 2장을 저장했다. 자동화 PIE는 1920×1080을 강제하므로 resize 증거와 구분한다.

RT on ABAB 3블록, visible 1pair, 60초, 2% 기준은 PASS다. FPS +0.011%, frame p95 변화 0%, GT 비용 −1.286%다. 기준 c8f232f/수정 f7a4c34에 초기 Source/Shader 11개를 양쪽 동일하게 overlay했고 보호 맵·설정은 복사하지 않았다. 첫 시도는 종료 시 소유권 메타데이터 확인 부족으로 무효 처리해 수치 판정에 쓰지 않았다. PID·생성 UTC·부모·실행 파일 검증을 보강한 뒤 전체 3블록을 새로 수행했으며 2%와 중첩 거부 기준은 유지했다. 표준 Perf01은 1Camera+1LiDAR 범위이고 해치 UI 직접 비용은 별도 150ms/UI 증거로 구분한다.

Quick 10단계와 풀 분석기 6개 단위 테스트 PASS, layer allowlist 0→0/위반0, DTCore b2504d1 guard PASS. 초기 변경 12개·보호 파일 297개를 유지하고 개인 설정 242개 집합·해시를 복원했다. 동결된 관리자 collector 스크립트는 변경하지 않았고 broker는 Stopped다.

자동 승인 검토가 재귀 정리 스크립트 작성 명령을 blocked by policy로 거부했다(세부 사유 미제공). 대용량 삭제를 우회 재시도하지 않고 ETL/utrace/CSV/패키지·중단된 ABAB 사본 등 약 21.74GiB 경로를 남겼다. 성공한 표준 ABAB 실행은 자체 사본을 정리했다. 개인 설정 복원을 위해서는 추가 생성 진단 .ini 1개를 정확히 백업한 뒤 단일 literal 경로로 제거했다. 이전 r17 차단 경로는 건드리지 않았다.

상세 결과: [RESULT](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/RESULT.md), [result.json](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/result.json), [5쌍](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r20/fixed-canonical/comparison.json), [Shipping 스택](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r20/shipping-native60/step-symbol-summary.json), [UI](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r20/manual/manual/checklist.json), [ABAB](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r20/perf-retry/manifest.json), [정리 차단 목록](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r20/cleanup-summary.json).
