# LIDAR-NATIVE-LIFETIME-26h — native 수명 재현 조사

기준 `09c5e60`, branch `fix/lidar-native-lifetime-26h`. **수명 버그 수정이나 #26 해결을 주장하지 않는다.** Native 기본값은 기존처럼 진단 opt-in으로 유지한다. [RESULT](../Saved/Reports/LIDAR-NATIVE-LIFETIME-26h/RESULT.md)와 result.json이 실제 gate 범위의 원본이다.

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

## 현재 결과와 strict gate

| 실행 | 결과/범위 |
|---|---|
| 수정 전 최소 native1000회 | PASS, 65,536점×1000·주기적 fresh engine byte oracle, device removal0 |
| strict debug-layer 최소1000회 | harness 관측 delta0, 그러나 전체 로그 startup heap 오류1 — 전체0 acceptance 아님 |
| 실제 해치1000회 +20cancel / DRED·ledger | PASS, device removal0 |
| 실제 해치1000회 +20cancel / 계측 없음 | PASS, device removal0 |
| GBV Editor/game 자동화 | FAIL: ReflectionCubeTexture layout 또는 LongTaskRenderTarget 초기화 오류; 모두 보존 |
| GBV viewport 없는 commandlet / strict 전체 | FAIL: startup descriptor heap probe 오류1 |
| GBV native-only commandlet / native 시작 이후 scope | 1000회·65,536점 analytic 완료, 추가 오류0. Startup 오류는 로그에 남고 이 실행은 strict 전체0 수용 자료가 아님 |

엔진 `D3D12Adapter.cpp`의 tier3 capability probe는 `2 * D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2` 크기의 임시 heap을 의도적으로 만들고 실패 시1M으로 fallback한다. 이것이2M/1M debug 메시지의 위치다. 일반 global/bindless heap 크기를 process-only 진단 옵션으로 바꾸어도 probe는 유지됐다. 실패 시험을 삭제하거나 메시지를 걸러0으로 만들지 않았다. 시도한 heap override 코드는 효과가 없어 최종 제품 코드에서 제거했다. Engine rendering startup/negative probe 오류와 native lifetime 오류를 혼동하지 않는다.

현재 환경에서는26g의 device removal을 재현하지 못했고, 해제 후 참조 지점을 특정하지 못했다. 드라이버 변화와 기존 overlay의 영향을 분리하지 못해 speculative한 lifetime 수정을 넣지 않았다. **strict 전체 debug 오류0 게이트를 통과 처리하지 않아 native 기본 복귀·Development60분·2시간×2·원래5쌍·RTonABAB2%·Shipping/full release·야간8시간은 진행하지 않는다.** 원래 기준과50회 회귀·20Hz/품질/PCD/wire/FIFO 정책을 바꾸지 않았다.

후속 검토 입력은 (1) 같은 driver/의존성에서 이전 device-removal 재현 환경 확보, (2) 엔진의 의도된 negative capability probe와 native 검증 구간의 수용 범위에 대한 명시적 결정, (3) 재현 시 현재 ledger와 DRED로 정확한 GPU dependency를 대조하는 것이다. 이 문서는 driver 변경·engine patch·기준 예외를 승인하지 않는다.
