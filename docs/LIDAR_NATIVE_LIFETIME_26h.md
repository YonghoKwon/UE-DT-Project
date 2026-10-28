# LIDAR-NATIVE-LIFETIME-26h — native 수명 재현 조사

기준 `09c5e60`, branch `fix/lidar-native-lifetime-26h`. **수명 버그 수정이나 #26 해결을 주장하지 않는다.** Native 기본값은 기존처럼 진단 opt-in으로 유지한다. 2026-10-28 후속 패키지 측정에서 device removal이 재현되어 최종 검증은 중단했다. [RESULT](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/RESULT.md)와 result.json이 실제 gate 범위의 원본이다.

## 환경과 보존 범위

26g 실패 환경의 NVIDIA610.88과 달리 현재4070Ti 드라이버는621.01이다. 사용자가 이미 체크아웃한 branch에는 PixelStreaming CPU bridge·UI/성능 시험·README·검증 이력·shader Platform include와 생성 hash의 미커밋 변경이 있었다. 시작 파일 해시와 백업을 기록했고 이번 커밋에 포함하지 않았다. 따라서 local proof는 base SHA에 해당 사용자 overlay를 더한 명시적 의존성 집합에 한정한다. 다른 SHA/driver 결과를 현재 통과로 이월하지 않는다.

DTCore/엔진/config/운영맵/PixelStreaming 자산은 수정하지 않았다. `SensorTestMap`은 저장하지 않았다. baseline/source fingerprint는 Saved의 initial-state/tested-source-manifest에 기록한다.

## 재현 도구

- `HATCH26.NativeLifetimeHarnessRhi`: logical sensor1대, 고정 pose의 배경과 모양/크기가 다른 적재 격자. 매회 구멍·깊이·mesh revision을 바꾸고65,536개 ray의 거리/miss/mesh/surface/normal을 해석적으로 검사한다. 기본1000회, 주기적으로 fresh engine raygen과 결과 전체 바이트를 비교한다. Metadata/AS cache eviction이 반복된다.
- `HATCH26.NativeProductionThousandDigsRhi`: 기존 identity/cancel/50-dig 회귀의 기본50회 기준은 보존하고, 별도1000회 시험을 추가했다. opening의 공개 치수와 `PickCargo`로 먼저 확인한 유효 광선에서 실제 `Dig`를 실행한다. 모든 회차의 다음 acquisition·immutable frame을 확인한다. 첫 고정 좌표가 coaming에 막혀 실패한 fixture 기록도 보존했다.
- `NativeLidarLifetime` commandlet: 같은 최소 harness를 viewport/AutomationController warm-up 없이 실제 D3D12로 구동한다. Renderer/engine startup 오류를 native 자원 검증과 분리하는 진단용이며 제품 Scheduler를 바꾸지 않는다. `-AllowCommandletRendering -SensorRtNative`가 필수다.

Runtime의 editor 전용 진단은 `WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR`로 보호했다. 새 UE 모듈·플러그인·제품 backend·설정은 추가하지 않았다.

## 주소·수명 추적

`-SensorNativeLedger`는 BLAS build 입력 vertex/surface/result/scratch VA·크기·revision·CPU descriptor, TLAS instance ID→BLAS/입력 주소, native global root 인자, 생성/마지막 사용 fence/완료 값/해제/device 상태를 기록한다. 이 구현은 refit/compaction이 없고 새 revision은 완전 build다. 입력은 BLAS owner, BLAS는 TLAS, 기록된 GPU 주소는 job strong ref로 terminal fence까지 유지된다. native inline query에는 SBT/로컬 루트 인자가 없으며 그 사실도 로그에 명시한다.

전용 committed allocation은 node mask1로 생성하고 app MakeResident/Evict는 호출하지 않는다. 생성 resident 상태를 유지하며 전체 live/cached/retired/free GPU buffer ledger는512MiB 상한이다. raw 엔진 residency 주소를 보관하지 않는다. [Microsoft DXR data rules](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)는 완료된 AS가 build 입력 정점을 참조하지 않고 TLAS→BLAS만 남는다고 규정한다. 현재 코드는 이보다 보수적으로 입력도 BLAS와 함께 유지한다. 이 계약과 CPU ledger만으로 드라이버 또는 미기록 참조의 정확성을 확정하지 않는다.

Saved `audit-ledger.py`는 생성/해제 VA generation, build/root 기록 시 live 여부, 기록 fence 이전 해제를 검사한다. 현재 trace에는 해당 위반이 없었다. 이것은 GPU의 모든 숨은 의존성·드라이버 정확성 증명이 아니다.

## 2026-10-28 수용 범위와 패키지 재현

명시적 사용자 결정에 따라 engine startup2M descriptor heap capability probe 오류1과 engine warm-up 오류를 native 자원 검증과 범위 분리했다. 원문·line·source/log SHA는 Saved native-validation-scope.json에 보존한다. 기준 완화가 아니며 native 자원 오류0은 유지한다. 전체프로세스오류0으로보고하지않는다.

최소harness·실제해치·계측없는실제해치는각1000회crash0이지만, 동일Development패키지/Rev7×3/PCD/preview기본은 준비 중DEVICE_REMOVED(HUNG)/PageFault로실패했다. ledger+DRED실행에서도재현되어native기본복귀와60분이후gate를중단했다. 첫faultVA0x3A42E1000,65536bytes. 두번째실행은74native제출까지기록주소/fence위반0이지만PageFault가있어overallFAIL이다. GPUcrash수집5분정체후소유진단프로세스만종료했다. 작은fixture성공을패키지성공으로확장하지않는다.

| 항목 | native60 MiB/h | fallback60 꼬리20분 MiB/h | 목표 | fallback 판정 |
|---|---|---:|---:|---|
| RSS | 미산출: 준비 구간 device removal | +17.409 | ≤8 | FAIL |
| Commit | 미산출: 준비 구간 device removal | +19.235 | ≤8 | FAIL |
| RHIMisc | 미산출: 준비 구간 device removal | +8.040 | ≤2 | FAIL |

Native60분곡선이없어지속장면이메모리증가와무관하다고입증할수없다. 추가후보는3sensor연속배치/geometryrevision교체에서engine/nativequeue·AS/residency의상호작용,미기록GPU의존성,driver경로다. Native는inlinequery/SBT없음/refit0이라nativeSBT를확정원인으로두지않는다. fallbackRayTraceDispatchSBT/querymetadata·driver보존도추가콜스택근거가필요하다. speculative수명변경·engine/DTCore/driver수정없음.

지원RHI fallback60분만대조완료. native2시간×2·원래#26 5쌍·RTonABAB2%·Shipping/release·야간8시간은package crash로NOT_RUN이다. 설정·원래사용자overlay보존과소유패키지/dump정리증거는RESULT/result.json에남긴다. push/PR/merge없음.
