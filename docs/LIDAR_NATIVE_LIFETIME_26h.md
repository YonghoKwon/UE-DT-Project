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

## 2026-10-30 r21: Shipping 자체 관찰·CPU scratch 추적 — 최신 실측 중단

요청 없는 자체 PCD 수신을 끄고 Ray·Hit 및 PCD 인코딩 버퍼를 상한 안에서 재사용하도록 수정했습니다.
첫 Shipping 60분은 전체 WS·Commit +12.92/+12.79MiB/h로 FAIL, 마지막 20분 −0.10/+4.20MiB/h로 PASS였습니다.
추가 수정 f16ee56 재측정은 영상 디코딩 4% 감지로 약 94초에 중단됐습니다. 최신 수정의 60분 효과와 ABAB은 미검증이며 5단계는 시작하지 않았습니다.

코드 커밋 f307b4a는 요청 없는 자체 구독만 off로 시작하고 제어 응답·명시적 수신 요청은 유지한다. 수신 청크1MiB/pump4MiB, CPU Ray free pool4개/32MiB, Hit no-shrink/best-fit로 큰 반복 할당을 줄였다. f16ee56은 진단에서만 켜지던 기존 PCD SoA·압축·hash pool4개/32MiB를 제품 None에서도 사용한다. wire/품질/Hz/PCD 기본 정책·GPU 자원 계약은 바꾸지 않았다.

f307의 유효60분은 전체 WS12.915/Commit12.789MiB/h로FAIL, 꼬리−0.102/4.203으로PASS다. 218457개외부PCD전점검증·native3약20Hz/fallback0/구독0이다. 추가codec수정의30초검증은PASS였으나 새60분은14:32:48UTC video decode4%에서94.403초로취소했다. 최신60분효과와RTonABAB은미검증이다. 평탄한꼬리로전체실패를덮지않는다.

외부 fixture의 구조검사와 내부ReadPoints의점별검사차이를확인해 --semantic과빠른native점검증(TLS8MiB,서버프로세스)을추가했다. 중간점NaN/Inf/confidence/validity오류를Python/native·AoS/SoA로검사한다. 메모리runner는모든수신PCD의semantic증거를필수로요구한다. 초기17분시험은범위보강을위해취소해판정에서제외했다.

최신codec회귀11개Success(PCDBandwidth9/nativebyteoracle1/3대성능1),이전수신정책·CPU상한·취소50dig회귀PASS,Pythonfixture5/strict분석기9/quick10/layer0→0/guardPASS. 영상감지규칙에따라추가GPU작업을중지했다. 5단계최종검증은시작하지않았고과거5쌍차이실패도유지한다. 개인242개·초기12개·보호297개정확히복원/보존했다. 대량정리는Claude에맡기며경로목록을보존했다. push/merge없음.

상세: [RESULT](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/RESULT.md), [result.json](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/result.json), [첫60분](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r21/shipping-native60-final/os-strict-analysis.json), [최신중단](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r21/shipping-codec60/run-manifest.json), [정리목록](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r21/cleanup-inventory.json).


## 2026-10-31 r22: 인코더 재사용 Shipping 재측정 — 전체 Commit 기준 미달

재개 기준 HEAD `3dfff67593013e43f12eb58e1c0b27a016953789` / branch `fix/lidar-native-lifetime-26h`. 제품 소스는 `f16ee563cd99587fef69c9b014112b5c1ec39a07`이며 이후에는 문서만 바뀌었습니다. 전체 Source/Shader fingerprint가 저장된 빌드 기록과 일치하고 stage/archive exe SHA256도 동일해 `r21/shipping22-stage`를 재사용했습니다. 초기 사용자 Source/Shader 변경 11개도 같은 상태로 포함됐으며 순수 clean checkout 결과로 표시하지 않습니다. `f7a4c34` 해치 헤더 수정이 포함됐습니다.

| 소스·실행 | 10~60분 WS / Commit (MiB/h) | 마지막 20분 WS / Commit (MiB/h) | 판정 |
|---|---:|---:|---|
| f307, r21/shipping-native60-final | +12.915 / +12.789 | −0.101 / +4.203 | 전체 FAIL |
| f16, r21/shipping-codec60 | 미완료 94.4초 | 미완료 | 이전 디코딩 4% 중단, 제외 |
| f16, r21/shipping-codec60-resume1 | -1368.647 / +8.456 | -41.519 / +6.890 | 전체 Commit FAIL |

기준은 각 구간 WS·Commit 각각 ≤8 MiB/h이며 OLS·원시 표본·시간 경계를 바꾸지 않았습니다. 꼬리 통과로 전체 실패를 덮지 않았습니다. 한 번씩의 실행 비교에서 전체 Commit 기울기는 감소했지만 아직 기준을 만족하지 않았고 통계적 인과 확정으로 해석하지 않습니다.

실제 측정 3602.957초, OS 5초 / VAD 60초. 콘솔 세션, RTX4070Ti 드라이버621.01, 실제1920×1080 offscreen, D3D12 native 명시, Rev7×3, 기본 압축PCD+미리보기, Slate throttling0, LLMoff, binned2, 내부 자체 수신 관찰off, ETWoff, Trim0회. `Memory26Case`는 `none`으로 기본 인코더 scratch 재사용이 실제 선택되는 조건입니다. 패키지·소스 해시와 fresh UserDir은 evidence에 보존했습니다. 한 실행만 돌았고 외부 Unreal 중첩·영상 디코딩이 모두 0이어서 재시도는 필요 없었습니다. 재시도 제어기는 디코딩 발생 시 연속600초0% 확인 후 최대3회까지 실행하도록 준비됐으나 사용되지 않았습니다.

## 계단과 남은 후보

- 15분 Commit 분별 중앙값 +7.254MiB, 22분 +3.590MiB. 15분 근처 VAD는 Private Commit +7,655,424바이트 / reserve −8,192,000바이트였고 Mapped·Image·스택·스레드는 증가하지 않았습니다. VAD60초와 OS5초의 근접 표본 비교이므로 정확한 할당 수지로 보지 않습니다.
- 10~60분 UObject 24,641→24,641, native BLAS/TLAS build·buffer create·native bytes 변화0, fallback0. 지속 native 자원의 관측 가능한 증가와 CPU 스레드/스택 증가는 이 실행의 설명력이 낮습니다. 엔진/드라이버 내부 private backing까지 배제한 것은 아닙니다.
- Raw 송신은 global128MiB / stream64MiB / stream20프레임 reservation을 접수~receipt까지 보유하고 receipt에서 해제합니다(`VirtualSensorHighThroughputTransportSubsystem.cpp`: admission252–253, receipt789–790, release405–408). 한 연결의 소켓 버퍼 크기는 연결 때 고정됩니다. 송신 body·header와 receipt/parser의 임시 할당은 남아 있어 allocator high-water 후보지만, 코드상 새 할당이 있다는 사실만으로 이번 증가분의 소유자로 확정할 수 없습니다.
- CPU Ray/Hit와 인코더 scratch는 기존 4개/32MiB free-pool 상한을 지킵니다. 인코더는 크기/필드 조합별 압축 bytes 동일 테스트를 통과했고 이번 실행도 기본 재사용 경로였습니다. free-pool 상한은 모든 in-flight 메모리를 뜻하지 않으므로 이것만으로 live size 평탄을 주장하지 않습니다.
- 잔여 후보는 **binned2/native heap high-water 및 송신 임시 backing**, **엔진/D3D12 UMD private backing**, 그 다음 **관측되지 않은 in-flight 풀 고수위**입니다. 무제한 큐 누적을 보이는 오류·overload·gap은 없지만 독립적으로 읽은 누적 카운터 차이를 실제 queue occupancy로 해석하지 않았습니다.
- 약35~36분 WS가 약1,240→480MiB로 크게 줄었고 약50분에도 추가 회수가 있었습니다. Commit은 유지됐습니다. 우리 EmptyWorkingSet/Trim 호출은0, 당시 물리 메모리 가용약35GiB였습니다. 외부 회수 원인은 확정하지 않았고 이 표본을 제외하지 않았습니다. 따라서 큰 음수 WS 기울기를 인코더 수정의 효과로 귀속하지 않습니다.

이 자료는 모듈/함수별 할당 소유자를 가르지 못합니다. 현재 같은 소스의 계단 구간 VirtualAllocation+Heap live/backing 스택을 얻어야 추가 최소 수정의 근거가 생깁니다. broker.json은 `Stopped`이며 동결된 수집 스크립트를 변경하지 않았습니다. **관리자 브로커 필요**를 기록하고, 추정으로 추가 제품 코드 수정이나 재측정을 하지 않았습니다.

## 회귀·보호·제한

외부218,599프레임 전부 LZF 해제와 모든 점의 XYZ·confidence·validity를 검증했습니다. invalid0 / subscription0 / echo0이며 receipt는 소비자 검증과 구분했습니다. native 실측Hz는 os_lidar_boom=19.999702, os_lidar_cabin_01=19.998869, os_lidar_cabin_02=19.999147. sampler의 이전 GT 비용 평균 1.452ms, 최대 11.686ms/5초이며 이를 ABAB 성능 판정으로 대체하지 않았습니다.

제품 소스가 동일해 r21의 실제 RHI byte/point oracle·Performance3 및 PcdBandwidth 9개(합11개 Success) 증거를 연결했습니다. 새 C++ 변경/빌드는 없습니다. quick/layer/DTCore guard와 보호·개인 설정 해시를 최종 확인합니다. 메모리 실패로 조건부 RT on ABAB는 실행하지 않았습니다. 원래 #26 5쌍 및 디깅 Commit 문제는 이번 Shipping 실험으로 해결/통과 처리하지 않았고, 5번 최종2시간/야간 검증도 금지대로 시작하지 않았습니다.

대용량 패키지·원시 자료는 삭제하지 않고 `r22/cleanup-inventory.json`에 Claude용 목록을 남겼습니다. push/merge/PR 없음. RESULT/result.json/merge-check는 root와 r22에 남기며 실패를 숨기지 않습니다.

주요 증거: `r22/package-reuse.json`, `r22/runtime-condition.json`, `r22/controller-status.json`, `r21/shipping-codec60-resume1/os-strict-analysis.json`, `backend-quality.json`, `step-correlation.json`, `r22/final-audit.json`.


## 2026-10-31 r23: 관리자 브로커 실제 종료로 추가 ETW 미수행

재개 기준 `72390df`. Shipping22/f16ee56 전체 Source/Shader와 stage/archive exe SHA256 일치, 동결 수집기 SHA `EBBDC15C…` 일치. 하지만 실제 broker.json은 `Stopped`였고 PID63872도 없었다. 시작 `2026-10-30T16:53:06.9813144Z` → 종료 `16:53:17.1210075Z`, `Handled=[]`. 현재 도구 토큰은 비관리자였다.

이전 `stop.request`는 `09:30:32.015652Z`부터 남아 있었다. 이 파일이 새 브로커의 즉시 종료를 유발했을 가능성은 있으나 내부 종료 로그 없이 확정하지 않았다. 이전 파일 사본·해시를 보존하고 최종 stop 요청을 남긴다. 다음 기동은 이전 stop 요청이 정리된 상태에서 `Running`과 PID 생존을 확인해야 한다.

새 게임·ETW 요청·수정·재측정·ABAB·5번 최종 검증은 모두 미수행. r22의 전체 Commit +8.456 MiB/h 실패는 그대로 미해결이며 새 측정으로 재표시하지 않았다. 관리자 브로커가 필요하다. 기존12개 변경·개인 설정·보호 경로·수집기를 보존하고, 새 대용량 파일 없이 결과/merge_check에 차단을 기록했다. 증거: `Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/r23/RESULT.md`, `broker-blocker.json`, `package-reuse.json`.

## 2026-10-31 r24: Shipping 계단의 upload heap·driver reset 호출자 확인

기준 HEAD `a950f9e7f8fa79c87f7ca36bf93932cde868c252`, branch `fix/lidar-native-lifetime-26h`. 제품 소스는 `f16ee56`이며 Source/Shader 전체 fingerprint·동결 수집기SHA·Shipping22 stage/archive exe SHA가 일치해 기존 패키지를 재사용했습니다. `f7a4c34` UI 수정과 이전부터 보존된 사용자 Source/Shader overlay가 포함됐습니다. 순수 clean checkout 결과로 표시하지 않습니다. 이번 재개에서 제품 소스·엔진·DTCore·드라이버·맵·설정은 수정하지 않았습니다.

콘솔 세션, RTX4070Ti621.01, D3D12 native 명시, 1920×1080 offscreen, Rev7×3·기본 압축PCD+미리보기·20Hz·LLMoff·binned2·Slate throttling0·내부 자체 수신관찰off. 외부 OS5초/VAD60초/점별 PCD 검증, PID creation identity, 다른 Unreal·디코딩 감시를 유지했습니다. 정상 종료 3604.498초, 환경 중첩·디코딩·native fallback·PCD invalid 모두0, 재시도0회입니다.

| 실행 | 10~60분 WS / Commit (MiB/h) | 마지막20분 WS / Commit (MiB/h) | 범위 |
|---|---:|---:|---|
| r22, f16 무추적 | −1368.646 / +8.456 | −41.519 / +6.890 | 전체 Commit FAIL 유지; 큰 WS 회수 원인 미확정 |
| r24, 같은 f16 ETW 진단 | +15.788 / +12.946 | -8.349 / -8.663 | 진단 INFO; 무추적 acceptance 대체 불가 |

기준(각 구간 WS·Commit ≤8 MiB/h), 준비10분, 전체/꼬리 구간, 표본, OLS는 바꾸지 않았습니다. 이번 진단 수치로 이전 실패를 덮거나 조건을 완화하지 않았습니다.

## ETW 신뢰성과 수지

관리자 브로커 PID24380의 Running·실제 생존을 시작 전에 확인했습니다. collector SHA `EBBDC15C440B33EF7E81A0F8F690D845A33858150AAC86DAE431AA9AF71E90A1` 고정, 수집기 수정 없음. 정상 게임 PID59612, 두 요청 ExitCode0, collector Captured. 디버거·IFEO/GFlags·시스템 설정 변경 없음. 두 ETL 합계 2.251GiB로 각각과 합계 모두16GiB 미만입니다.

| 구간(실제 runtime 분) | ETL GiB | user stack / 실제 alloc | OS Commit 순증가 MiB | 관측 VirtualAlloc 순증가 MiB | OS−VA 잔차 MiB |
|---|---:|---:|---:|---:|---:|
| 10.222~35.266 | 1.288 | 1736/1742 | +6.734 | +20.727 | -13.992 |
| 41.444~58.974 | 0.963 | 1197/1209 | -15.109 | -1.758 | -13.352 |

두 trace의 lost buffers/events0, target unique process key1개·살아 있는 종료 rundown·OS creation FILETIME와 collector creation UTC 일치·OS sample gap≤10초·stack sequence 문제0·unknown allocation flags0을 검사했습니다. trace origin+10초 이후와 종료 rundown−10초 이전의 실제 OS5초 표본을 경계로 썼으며 보간하지 않았습니다. 명목10~35/40~58분과 실제 경계를 구분했습니다.

전체 수지는 닫히지 않습니다. VirtualAlloc은 private backing의 최초 제출자 원장이고 Heap live 객체 원장이 아닙니다. mapped/kernel/driver/누락 메모리 종류나 경계 차이가 잔차로 남을 수 있습니다. 약−14/−13MiB 잔차를 특정 함수에 강제 배분하지 않았습니다. 반면 다음 국소 구간은 OS 변화와 가까워 계단 경로를 확인할 수 있습니다.

| 구간 | OS / VA MiB | 확인 경로 |
|---|---:|---|
| 14~17분 | +8.082 / +8.441 | upload heap 경로 +8MiB |
| 20~24분 | −7.766 / −6.984 | 같은 upload heap 경로 −8MiB |
| 30~33분 | +6.320 / +6.313 | upload +3.125MiB, NVIDIA command-list reset +2.188MiB 등 |
| 48~51분 | −6.613 / −6.102 | 후반 반환 구간 |
| 53~56분 | +7.965 / +8.008 | 같은 upload heap 경로 +8MiB, preview 양·음 변화 상쇄 |

## 특정된 호출 경로와 수명

1. **RHI upload backing**: `NDXGI::CDevice::AllocateCB_0022 → CResource/CHeap::FinalConstruct → CDevice::CreateCommittedResource → FRHIPoolAllocator::TryAllocateInternal → FD3D12DynamicRHI::RHILockBuffer`. texture/TSR history·query heap·PSO 생성으로 추정하지 않았습니다. RHI thread에서 지연 실행돼 개별 원래 vertex-buffer 소유자까지는 이번 provider로 식별하지 못했습니다. 첫 trace에서 이 stack의8MiB COMMIT43건, 그중40건은 이후 단일 covering free/decommit과 대응합니다. 나머지는 부분 해제 가능성을 포함하며 원장 경계는8.0→11.125MiB로 순증가가+3.125MiB입니다. 43×8MiB를 누적 누수로 더하지 않았습니다.
2. **NVIDIA command-list backing**: `nvwgf2umx.dll → CCommandList::VersionedResetCommandList / CGraphicsCommandList::Reset → FD3D12CommandList::Reset → FD3D12Device::ObtainCommandList`. 첫 구간 순증가 +2.188MiB, 후반 해당 분류의 새 순증가는0입니다. NVIDIA 내부 함수 이름은 private PDB가 없어 RVA로 남겼습니다. 관측상 고수위/예약 후보이지만 드라이버 내부 전체 수명·누수 부재를 확정하지 않았습니다.
3. **CPU allocator backing**: 최초 제출자 기준 `FVirtualPcdCodec::Encode` +3.129→후반−20.598MiB, `ULidarPlayerDisplayComponent::SendRenderDynamicData_Concurrent` +2.578/+2.578MiB, 기타 cached allocator +7.496/+7.176MiB. 이들은 `FCachedOSPageAllocator::AllocateImpl → FMallocBinned2::MallocExternalLarge/ReallocExternal` 경유입니다. 로컬 UE5.3 `MallocBinned2.h`의64bit cache 상한64블록/64MiB와 `CachedOSPageAllocator.cpp::FreeImpl`의 한도 초과 시 OS 반환을 읽기 전용 확인했습니다. 한도는 그 공용 캐시 하나에 대한 것이며 전체 malloc/driver 보유량 상한으로 일반화하지 않았습니다. PCD/preview 최초 제출자가 실제 live 버퍼 누적 소유자라는 뜻이 아닙니다.

PDB의 GUID/age와 실제 함수 범위를 모두 확인했습니다. 원래 SymTagFunction 범위가 없는 최적화 엔진 구간은 PE RSDS GUID/age·image size 일치, exception-directory의 [BeginAddress,EndAddress), PDB public **function** RVA=BeginAddress, 전체 public-function enumeration에서 unique name1개일 때만 추가 해석했습니다. export/nearest-name 추정은 사용하지 않았고 ICF alias·범위 없음은 미해석으로 남겼습니다. 실제 근거는 각 trace의 `PdbProof`, `unwind-public-proof.json`, `*-symbolized-bounded.json`입니다. 이 방식은 이름의 물리 함수 범위를 확인하며 NVIDIA private 함수 구현이나 원래 게임-thread 자원 이름을 만들어내지 않습니다.

## 수정 판단과 회귀

확인된 큰 변동은 엔진 upload heap 생성·반환이며, 작은 장기 후보는 engine/driver backing과 공용 allocator cache입니다. 우리 live 버퍼가 무한 누적된다는 근거는 확보하지 못했습니다. native BLAS/TLAS·buffer create·native bytes는10~60분 변화0, fallback0이고, 코드의 Raw reservation(global128/stream64MiB/20frames), CPU Ray/Hit 및 codec free-pool(4/32MiB) 상한·receipt 해제를 유지했습니다. free-pool 상한을 in-flight 전체 상한으로 오해하지 않았습니다. 근거 없는 payload/preview 재작성이나 엔진/드라이버/제품 기본값 변경은 하지 않았습니다.

따라서 추가 최소 제품 수정·수정 후 무추적60분·새 byte oracle/RTonABAB는 실행하지 않았습니다. r21의 같은 f16 소스 RHI/PCD 회귀 증거는 남아 있지만 이번 단계 새 실행으로 세지 않았습니다. 이번에는 host 할당 원장 단위10개를 새로 통과했고, fresh quick/layer/guard·보호/개인 설정 해시를 검사합니다. 전체5번 검증·2시간/야간은 금지대로 시작하지 않았습니다.

실측 native3대Hz: os_lidar_boom=19.999471, os_lidar_cabin_01=19.999471, os_lidar_cabin_02=19.999748. 외부218,693프레임 모두 LZF 및 모든 점의XYZ/confidence/validity 검사 통과, invalid0/echo0/subscription0. receipt를 소비자 검증과 혼동하지 않았습니다. sampler GT 비용 평균 1.334ms, 최대 6.872ms/5초이며 ABAB 성능 판정으로 대체하지 않았습니다.

## 남은 한계와 종료

Shipping ≤8MiB/h 해결 승인은 불가합니다. 다음 조사에서는 개별 RHI upload resource 식별과 heap live/free 계측이 있어야 미리보기/송신 임시 버퍼의 재사용을 추가 수정할지 판별할 수 있습니다. 현재 고정 VirtualAllocation 수집기로는 전체 잔차와 driver private 구현까지 닫지 못했습니다. 이전 엔진 토글 실험을 새로운 효과 검증 없이 제품 기본값으로 바꾸지 않았습니다.

두 요청 종료 후 `stop.request`를 기록해 broker Stopped·PID 종료를 확인했습니다. 새 패키지/Insights trace/dump/PerfAbab 없음. ETL과 생성된 원시/심볼 캐시는 요약 후 `r24/cleanup-inventory.json`에 Claude용 목록만 남겼습니다. 이전 대량 정리 차단 경로를 우회·재시도하지 않았습니다. RESULT/result.json/요약JSON·PDB증거는 보존 대상입니다. push/merge/PR 없음.

재현: `r24/run-traced.ps1`, `analyze-on-ready.ps1`, `extract_etw.py`, `analyze_trace.py`, `resolve_stacks.py`, `resolve_unwind.py`와 `dia_public.cpp`/빌드 명령을 reports 아래 보존했습니다. 동결 collector는 별도 사용자 관리자 브로커에서만 실행됐습니다. 현재 head/커밋·gate·보호 경로 상태는 result.json/merge-check.json이 최종입니다.

## 2026-10-31 r25: 업로드 계측·점군 저장 재사용 후 Shipping60 통과

branch `fix/lidar-native-lifetime-26h`, 시작 HEAD `0c2f60223a477bd50d414d567512bc9e69e4ba5b`. 마지막 제품 소스 `cab9d6e5f937ba286c991478860fe4c94af70a8d`이며 이후 결과 문서만 바뀝니다. 기존 사용자 Source/Shader overlay와 SensorTestMap·README·VALIDATION_HISTORY 등의 초기12개 변경은 그대로입니다. DTCore/엔진/드라이버·저장 정책·Blueprint/wire·센서품질·기본20Hz·PCD payload·미리보기 점 수와 갱신 상한은 바꾸지 않았습니다.

- `52bc4ba`: Shipping도 지원하는 `-SensorUploadStats` opt-in 진단. 고정256 processing-frame ring/domain, Record 경로에 동적 할당·문자열·로그 없음. RHI 요청, native mapped CPU쓰기, 엔진 관리 입력, readback을 별도 합산합니다. 장기 검증에서는 off이며 JSON `upload_stats`가 전 구간 없습니다. `record_cpu_ms`라는 진단 필드의 실제 의미는 lock/스케줄링 대기를 포함한 elapsed wall timer 합입니다. hardware CPU 회계로 해석하지 않았습니다.
- `17cc12a`: 동적 WriteOnly rename 대신 persistent GPU 목적지와 전체 스트림 staging 갱신. 위치는 bitwise 비교(부호0 포함), 색상은 원래 quantization 바이트가 달라질 때만 업로드합니다. 색 변환은 점당1회로 hoist했지만 결과는 같습니다. 임의 fence·NoOverwrite·동기 readback은 production에 추가하지 않았습니다. 이 첫 수정의 무추적60분은 실패했습니다.
- `cab9d6e`: 이미 `InitFromDynamicVertex`가 만든 원본 위치·색상 버퍼/SRV를 직접 갱신해 중복2세트를 제거했습니다. vertex/index buffer7→5, SRV6→4. 같은 vertex format/index/VF·정확한 좌표/색 바이트를 유지합니다. 원본과 별도 목적지의 두 벌 보유를 없앴고, CPU snapshot은 기존 PreviewPool(4개/8MiB)에서 독점 lease로 재사용합니다. free-pool exhaustion은 기존 임시 할당, live lease 강제 eviction/alias 없음. explicit diagnostic baseline은 기존 동작을 유지합니다.

## 업로드 전수 조사와 실제 측정

`upload-inventory-final.json`:25개 파일58 call sites. 직접 RHI, native mapped쓰기, ISM/절차메쉬/texture 같은 엔진 관리 입력, GPU capture/target생성, CPU-only 송신 staging을 구분했습니다. 초기 사용자 파일(예: Coil presentation/DepthProjection/PixelStreaming bridge)은 수정하지 않았고 현재 Hatch3 workload의 비활성 경로로 한계를 기록했습니다. engine/DTCore 내부 전역 upload를 모두 가로챘다고 주장하지 않습니다. managed mesh 입력은 위치/index 하한값이며 engine packing·추가 attribute·driver 페이지를 뜻하지 않습니다.

동일 조건의 opt-in Shipping30초 측정(35초 준비 후 reset)에서:

| 경로 | 수정 전 관측 | 해석 |
|---|---:|---|
| point position | 576,000B/호출,30초348회 | 직접 RHI |
| point color | 192,000B/호출,30초348회 | 직접 RHI |
| 직접 RHI frame peak | 768,000B/2회 | 센서3대 합을 임의로 한 프레임에 더하지 않음 |
| scan texture | 472,645,632B/1,803회 | engine-managed CPU 입력, 실제 driver heap bytes 아님 |
| native rays | 약3.78GB/30초 | 기존 persistent mapped 쓰기, RHI upload pool과 분리 |
| buffer recreation | 측정 구간0 | 기존 용량 유지 |

수정1/2의 직접 요청 peak도768,000B/2회였습니다. 실제 입력이 매 갱신 변해 byte-identical skip은0이었습니다. 따라서 **요청 바이트 peak가 감소했다고 주장하지 않습니다**. 입증한 개선은 목적지 중복 제거와 CPU/GPU 보유·할당 경로 재사용입니다. GPU byte oracle의2점 fixture는400→272B(32% 감소), native12,000점×3 구조에 같은 layout을 적용하면 vertex/index 보유량7,200,000→4,896,000B입니다. 후자는 fixture 측정 비율/정해진 layout의 산출치이며 driver heap 실측치가 아닙니다. CPU reuse와 GPU 중복 제거의 개별 기여를 분리 실험하지 않았습니다.

## Shipping 무추적60분

콘솔 세션, RTX4070Ti621.01/D3D12, Rev7×3(1024×64·요청20Hz), 실제1920×1080 offscreen·font100/UMGscale1, 기본 압축PCD+미리보기, 내부 자체 관찰off, binned2, LLMoff·projecttags off·ETW/Insights off·Trim0회·upload counter off·Slate throttling0. 동일 외부 OS5초/VAD60초·full point semantic receiver와 foreign Unreal/video guards를 사용했습니다. 실행당 fresh UserDir·실제 exe/source fingerprint·PID creation identity를 보존했습니다.

| 소스·실행 | 전체10~60분 WS / Commit MiB/h | 마지막20분 WS / Commit MiB/h | 판정 |
|---|---:|---:|---|
| 이전f16/r22 | −1368.646 / +8.456 | −41.519 / +6.890 | 전체 Commit FAIL; 큰 WS 회수 원인 미확정 |
| 첫 수정17cc/r25 | +13.775 / +12.316 | +0.863 / -1.846 | 전체 FAIL |
| 마지막cab9/r25 | +1.207 / -0.551 | +0.407 / +0.600 | 네 기준 PASS |

기준·10분 준비 제외·raw sample·OLS·꼬리20분을 바꾸지 않았으며 실패한 첫 실행도 보존했습니다. 마지막 3604.558초 정상 완료, native3대 각각19.999971Hz, 외부218,718프레임 모두 LZF·XYZ finite/confidence/validity 점별 검사 통과, invalid/encode/delivery/overload/native fallback/video/환경 중첩0, 환경 재시도0회입니다. 0Hz 센서 제외 없음, receipt와 검증 수신을 분리했습니다. 단일60분 통과이며 누수 부재의 무한시간 증명·원래 #26 전체 게이트 통과는 아닙니다.

## 회귀와 성능

Editor Development와 Shipping 빌드 성공. 각 수정의15개 실제 관련 테스트(ledger1, GPU preview stream byte1, native multi-scene point byte1, PCD9, HATCH performance3센서1, preview identity1, capture isolation1) 모두Success·Error0. RHI는 실제 D3D12이며 NullRHI skip 없음. GPU readback oracle은 원래 quad 수식·색 바이트, 반복12회, 카메라/픽셀 크기 변경·0면적 슬롯·resource/proxy 유지와 default scratch 반환/4개·8MiB 상한을 검사합니다. 경고는 fresh test UserDir의 Workspace/Appearance save 미존재이며 원문 보존했습니다.

| RT on ABAB | A→B FPS | p95 ms | GT ms | 변화율 FPS 하락 / p95 / GT | 판정 |
|---|---:|---:|---:|---:|---|
| 표준1Camera+1LiDAR,3블록·A/B각6회 | 46.653→46.651 | 29.549→29.547 | 7.085→6.953 | +0.0024% / -0.0071% / -1.8618% | ≤2% PASS |
| 부가Hatch3+PCD,1블록·A/B각2회 | 46.563→46.489 | 29.553→29.548 | 9.224→9.175 | +0.1586% / -0.0189% / -0.5310% | ≤2% PASS |

기준0c2f602 vs최종cab9d6e, 같은 instrument·맵SHA·초기11 Source/Shader overlay·1920×1080 visible·RT ProjectDefault·10초warmup/60초·매회 fresh UserDir, ABAB 순서와 raw frameCSV 보존. 표준 GPU timer는 원래 instrument에서 수집하지 않아 null이며 GPU2% 통과로 꾸미지 않았습니다. 부가Hatch GPU평균 4.333→4.318ms(-0.3639%).

부가 시험의 최초 default-off A는 내부 consumerHz를 기대하는 구형 test 전제와 충돌해 무효였습니다. 외부4,190프레임 점별 검증은 정상이었지만 내부0을 통과 처리하지 않았습니다. 같은 **진단 수신 opt-in 공통 test instrument**를 A/B 소유 사본에 적용해 재빌드·재측정했고 기존10/19Hz 내부 검증과 외부 점별/프레임 gap 검증을 유지했습니다. 제품 소스/기본값·무추적60분 조건은 그대로입니다. 이 부가 시험은 명시적 관찰 부하가 추가된 별도 조건이며 default Shipping 성능과 혼동하지 않습니다. 최초무효 evidence와 두 instrument SHA 모두 보존합니다.

## 직접 화면·보존·한계

모든 측정 종료 후 같은 Shipping 사본을 별도 fresh UserDir로 띄워 Computer Use로 점군 표시, drag회전, 센서 강조, 카메라 비스듬히 preset/접기를 직접 확인했습니다. 점군 갱신과3대20Hz 표시 유지. 실제 screenshot/viewport1920×1080, UMGscale1/font100, capture logical/physical1:1. 별도 OS GetDpiForWindow 값은 Sky API에 없어 추정96DPI로 기록하지 않았습니다. `manual/01~03*.jpg`와 `manual-checks.json` 보존. AltF4로 정상GameExit0·창/프로세스 종료 확인. 이 수동 run은 취소된 진단이므로 장시간/성능 PASS로 세지 않았습니다. 보호 맵 저장 없음.

초기12개·보호297개·DTCore b2504d1·동결 수집기 해시 유지. 개인 설정242개 원본집합/hash exact: Editor 설정1개 원복, 이번3개 cook 시간대에 생성된160B CrashReportClient 설정은 원본에 없음을 확인해 정확한 파일3개만 report로 이동 보존(재귀 삭제/복사 없음). 처음 재귀 personal copy/자동 source 정규화 복합 명령은 자동 검토에서blocked by policy(상세없음)로 거부돼 실행되지 않았습니다. 이를 재시도하지 않고 기존242개 backup의 read-only hash를 검증해 재사용했으며, 새로 만든 소스만 명시경로 CRLF 처리했습니다.

새 패키지/ABAB 사본 약23.21GiB는 `cleanup-inventory.json` 목록을 Claude용으로 남겼습니다. 대량 삭제/이전 차단 우회 없음. RESULT/result.json/summary/manifest/frameCSV/수동캡처/도구 source 보존. final quick·layer(allowlist0→0)·guard·schema·merge_check 결과는 result.json에 기록합니다. Release전체·2시간×2·야간8시간·원래 #26 5쌍/디깅Commit 최종 검증은 사용자 금지 범위라 시작하지 않았습니다. push/merge/PR 없음.


## r38b: native 안전 상태 표시와 초기화

센서 설정의 LiDAR 백엔드 진단에 native 복구 실행(RHI 사용) 또는
비정상 종료 2회에 따른 native 비활성 상태를 표시한다. 2회 차단 정책은 유지한다.
정상 장치의 native 제출 실패는 이번 세션만 RHI로 전환하며, 정상 종료를
비정상 종료로 누적하지 않는다. 검증 캐시는 무효화하여 다음 기본 실행에서
startup oracle을 다시 수행한다. 실제 장치 손실/Faulted는 기존 복구 정책을 유지한다.

수동 초기화가 필요하면 모든 관련 게임/Editor를 먼저 종료하고 해당 Saved 경로의
`NativeLidarSafety/active-<adapter·driver SHA1>.guard.state` 파일만 삭제한다.
패키지에서 별도 `-UserDir`를 사용했다면 그 UserDir의 Saved 아래에서 찾는다.
파일의 `failures`/`reason`과 실행 로그의 adapter/driver를 대조하여 해당 조합을
선택한다. 다른 조합의 기록이나 개인 SaveGame은 지우지 않는다. 드라이버 변경은
독립 조합으로 판정하므로 기존 조합의 차단이 새 드라이버에 적용되지는 않는다.

실행 중인 인스턴스가 소유한 `.guard`를 강제로 삭제하지 않는다. 비정상 종료의
stale `.guard`가 남아 있으면 상태 파일을 초기화해도 다음 1회는 안전 RHI 복구를
거친다. 초기화 후 native 선택은 기존 검증된 조합/제한된 startup oracle 정책을
따르며 강제 native 진단 플래그를 복구 수단으로 안내하지 않는다.
