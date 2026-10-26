# LiDAR GPU 획득 재작성 계획 — MEMORY-26f

이 문서는 후속 설계안이며 구현·엔진 변경·플러그인 제작 승인이 아니다. 이번 버전의 모듈과 DTCore 경계를 유지한다. 기준은 자율 추적 시작 `a2fd42d`와 `gpu/initial-state.json`이다.

## 전환 근거와 현재 한계

첫 수정은 호환 요청을 한 RHI 배치·raygen으로 합치고 ray/result/readback/SRV/UAV를 fence 뒤에 재사용했다. 첫60분 꼬리 RSS/Commit은3.274/.505MiB/h였지만 RHIMisc6.363MiB/h와 실제17Hz가 남았다. 추가 수정은 정적 hit/miss shader record의 반복 쓰기와 package PDI preview proxy 재생성을 줄였다. 해당 중간 소스의60분 및2시간2회 idle 지표는8/8/2MiB/h와실제20Hz를통과했다.

그러나 원래 HATCH03은8MiB·150ms 기준에실패했고,3번째control실행에서D3D12 `UpdateResidency` GPU-mask assertion이발생했다. UE5.3D3D12엔진소스는RTscene을SingleFrame만지원하며build시BLASresource의residencyhandle주소들을저장한다. 프레임간scene재사용의수명안전성은보장되지않는다. 크래시가발생한정확한handle의전체원인은추가확인이필요하지만,지원계약을넘는scene재사용을같은렌더프레임으로제한하는안전보정을적용한다.

**이 안전보정 뒤의 최종 소스에 이전 SHA의 장시간 통과를 이월하지 않는다.** 원시 실패·크래시·중간 소스 결과를 보존하고, 추가 메모리 최적화 실험은 더 하지 않는다. 최종 결과의 실제 수행/실패/미수행은 [RESULT](../Saved/Reports/MEMORY-FIX-26f/RESULT.md)와 result.json을 따른다. 누수의 엔진 귀속은 확정하지 않았다.

## 범위와 유지 계약

- `ma0t10_dtBootstrap`는 shader등록·중립GPU요청/결과·RHI수명만담당한다. Runtime(L1)은센서설정·snapshot·worker정규화·완료publication을소유한다. L0계약→L1센서→L2업무→L3UI방향을유지한다.
- 기존Scheduler·DTCore transport/publisher·UI관리경로를재사용한다. 새센서플러그인/UE모듈·별도Scheduler·위젯관리자를추가하지않는다. DTCore소스/gitlink·PixelStreaming·Config/Game.ini·운영맵은변경하지않는다.
- Rev7×3/1024×64/요청20Hz·기본preview·Full압축PCD·FIFO/128MiB전역·64MiB스트림/기존프레임상한을유지한다. 같은acquisition의FrameId/UTC/pose/revision/context를끝까지고정한다.
- 측정 생략·송신 전 제어 생략·reservation 접수·receipt·내부검증·외부수신·업무ACK를분리한다. 품질·점수·Hz를낮춰메모리문제를숨기지않는다.
- engine전역 GPU stat/RDG/crash CVar를제품기본값에서끄지않는다. RHI/driver/allocator보존가능성을별도측정한다. 전역Trim·GC·RHI drain을상시경로에넣지않는다.

## 설계와 영향 파일

| 작업 | 영향 파일/영역 | 설계 경계 |
|---|---|---|
| 요청·완료 수명 계약 | Bootstrap `Public/SensorOwnedRayTracing.h`, Runtime `Contracts/VirtualSensorRayQuery.h` | 불변요청·stream affinity·scene epoch·generation·terminal fence를명시. Blueprint/wire시그니처는유지 |
| world/frame별 GPU 제출 | Bootstrap `Private/SensorOwnedRayTracing.cpp`, `Core/VirtualSensorSchedulerSubsystem.cpp` | SchedulerTick의모든입장요청을mainrender전에1회제출. 호환scene만batch. 수동호출fallback·일시정지·world종료도같은종료계약 |
| 가속구조 수명 | Bootstrap RT backend | BLAS revision/pose와실제RHIallocation수명을구분. SingleFrame TLAS metadata를프레임간보존하지않고,문서화된API로안전한storage재사용만검토. rawresidency주소·PSOopaque포인터를장기캐시키로삼지않음 |
| 버퍼·입력 캐시 | Bootstrap RT packetpool | per-stream불변raycache와fencedring을분리. sharedslot이센서별입력을회전upload하는상태를계측. live/retired/free/bytes/peak에각각상한. 소비자소유payload는마지막release전재사용금지 |
| 정규화·lease·송신 | `Sensor/VirtualLidarScanSceneQuery.cpp`, acquisition/export, existingpublisher/codec | GPUhit→점→immutableframe과PCD소유권대조. batchoffset/다른raycount/miss/취소된member를시험. Outbytes는전송중재사용하지않음 |
| depth/semantic | `VirtualLidarGpuDepthProjectionComponent.*`, `VirtualLidarSemanticScene.*`, texture-readback helper | 타깃/형식/extent/세대별재사용. 두semantic scene의프레임UB수명문제를재현한뒤proxy재생성우회책을대체. 전체depth와격리semanticworld를합쳐품질을잃지않음 |
| preview | `LidarPlayerDisplayComponent.*`, visualization | 기존player-only고정GPUbuffer경로를유지. 포인트예산·색/크기·가림·SceneCapture/HWRT제외를검증. source측정payload는변경하지않음 |
| 계측·회귀 | `HatchMemoryRuntimeProbe.cpp`, `SensorRtBatchReuseTests.cpp`, existingGPU/Hatchtests, Scripts | 자원create/reuse/upload/bind/retire/bytes와실제Hz를5초이상간격으로기록. 본측정은trace/customLLM태그off,동일RHIMisc를위한engineLLM별도표기 |

shader수정이필요하면 `Shaders/Private/SensorOwnedRayTracing.usf`에한정한다. 샘플링·재질·법선·componentidentity·CPUfallback·지터모델을바꾸지않는다. inline-rayquery등backend교체는별도byte/point증거없이기본경로로승격하지않는다.

## 위험과 대응

1. **RHI수명/재배치**: logicalrevision이같아도nativeallocation이이동할수있다. 캐시수명·RHIthread순서·GPU완료를별도로검증하고supportedSingleFrame계약을우선한다. residency/validation오류는한번이라도실패다.
2. **비동기취소/수신자수명**: batch중하나만취소해도다른member를잃지않아야한다. weakownership·generation검사·예약바이트를기존종료경로와연결한다.
3. **품질/속도**: 추가GPUframe대기나CPU복사로20Hz를잃을수있다. 실제모든센서Hz·GT·GPU·p95를검사하고낮은설정으로통과시키지않는다.
4. **리소스상한**: dedicatedstreamcache는상주메모리를늘릴수있다. cold/steady/peak/retiredbytes를함께보고하며free수만으로상한을주장하지않는다.
5. **엔진/드라이버 귀속**: 재작성만으로잔여증가가사라진다고보장하지않는다. 엔진PDB정확심볼·CPUlive/backing·GPUlogical/alias회계를분리하고nearestexport를콜스택증거로쓰지않는다. 엔진패치가필수면근거·최소안을별도승인대상으로제시한다.

## 일정 제안 — 착수 후 영업일 7~10일

| 기간 | 산출물/종료 조건 |
|---|---|
| 1일 | 현행allocation/residencyledger·크래시최소재현·RHI지원계약·성능/메모리baseline고정 |
| 2~3일 | frame-localTLAS/안전storageprototype·per-streamraycache·fencedpool·batch/mixedcancelbyteoracle |
| 4~5일 | acquisition/정규화/PCD/취소통합·depth/semantic수명회귀·현재동작동일성검증 |
| 6일 | 반복생성/삭제/PIE·설정변경·profile/extent변경·cacheeviction·device-residency회귀 |
| 7~8일 | Development60분→2시간2회·원래5쌍·RTonABAB·Shipping·release관련gate |
| 9~10일(여유) | 실패재현과원인범위정리·직접UI수용·문서/독립검수. 엔진patch/실장비검증은별도추정 |

## 변경하지 않는 검증 gate

- 신선한GPU참조와batch결과: 센서당65,536점×3,센서/점별거리차이·miss·서로다른길이·mesh/pose/revision변경·취소를포함한byte/point비교. 기존정확도/identity/semantic기준유지.
- 정상연결PCD의wire33-byte/메타데이터/CRC/압축golden을검사하고acquisition→serialize→submit→receipt→검증수신카운터를대조한다.
- Development기본조건2시간idle2회,첫10분제외,마지막1/3및비교용20분RSS/Commit≤8·RHIMisc≤2MiB/h. trace/custom태그여부·GT계측비용·실제모든센서Hz를보고한다.
- 원래#26 5쌍: `HATCH03.DigRuntimeRhi`의rawRSS/retainedRSS/Commit8MiB와dig-control8MiB를따로판정. 150ms·2%·20Hz·점예산기준을완화하지않는다.
- RTonABAB2%·Shipping짧은실행·quick/release관련/layer/DTCoreguard. NullRHI/skip/이전SHA증거를현재RHI통과로세지않는다.
- SensorTestMap저장금지,개인설정백업/복원,decoder감지시중단,패키지/trace정리,result/schema/merge_check,의미단위커밋후독립검수. push/merge는별도권한이다.
