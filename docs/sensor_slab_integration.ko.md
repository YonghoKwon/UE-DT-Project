# 센서 패널과 Slab 시나리오 연동

PR #16(`94811a67`) 복원 후 센서 기능 확장입니다. 동료의 Topic 구독·벌크 파싱·Slab 이동·윤곽·차트는 유지하고 아래 함수만 호출합니다. Topic 이름은 센서 코드에서 알 필요가 없습니다.

## Blueprint 연결

먼저 Capture/Export의 서버 설정에서 STOMP를 선택하고 Broker/사용자명/세션 비밀번호를 적용합니다. 로그 전용 또는 HTTP 모드에서는 세 Topic 세션 시작을 거부합니다. 비밀번호는 재실행 후 다시 입력해야 하며 저장 파일에는 넣지 않습니다.

1. 재생 담당 Blueprint에서 `Get World Subsystem`으로 `VirtualSensorSlabContextSubsystem`을 얻습니다.
2. 실제 재생 시작 직전에 `Begin Slab Sensor Session`을 호출합니다. RunId를 비우면 새 UUID가 반환됩니다. 반환값이 비었으면 `Get Slab Sensor Session Status`의 Message로 오류를 확인합니다.
3. RunId를 멤버 변수로 보관합니다. TargetSensorIds가 비어 있으면 시작 당시 Coordinator의 Camera/LiDAR 전체를 대상으로 고정합니다. 특정 센서만 필요하면 정확한 SensorId 배열을 전달합니다.
4. 각 Slab 위치·회전 적용을 끝낸 **직후** `Notify Slab Frame Applied`에 RunId, mtl_no, frame_no, elapsed_sec를 전달합니다.
5. 첫 Notify부터 세 Topic의 세션 송신이 활성화됩니다. Notify는 스캔 명령이 아니며 기존 센서 주기를 바꾸지 않습니다.
6. Slab 움직임 종료 시 `End Slab Sensor Session`을 호출합니다. 정상 종료는 bAborted=false, 취소는 true입니다.
7. 일시정지·재개는 `Set Slab Sensor Session Paused`로 알립니다.

World에는 Coordinator가 하나 필요합니다. 빈 SensorId, 중복 SensorId, 역순 Slab 프레임, 감소하는 시나리오 시간과 잘못된 UUID는 거부합니다. 동일 프레임의 동일 정보는 멱등 처리합니다. 같은 시나리오를 다시 실행할 때는 새 실행 UUID를 사용합니다.

## C++ 호출 예제

```cpp
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"

auto* Sensors = GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
// 실제 재생 시작: RunId는 재생 클래스의 멤버 변수입니다.
RunId = Sensors->BeginSlabSensorSession(FString(),
    {TEXT("VCAM-TEST-001"), TEXT("LIDAR-TEST-001")});

// 기존 Slab Transform 처리가 끝난 뒤 호출합니다.
Sensors->NotifySlabFrameApplied(RunId, Row.MtlNo, Row.FrameNo, Row.ElapsedSec);

// Slab 움직임 종료.
Sensors->EndSlabSensorSession(RunId, false);
```

함수는 게임 스레드에서 호출합니다. 네트워크 callback에서는 기존 게임 스레드 재생기에 데이터를 넘기고, 재생기가 위치 적용을 완료한 뒤 호출하십시오. 이 API는 동기 스캔·socket 대기를 하지 않습니다.

## Slab 프레임과 센서 프레임

Slab frame_no는 입력 시나리오의 상태 번호, sensor FrameId는 각 센서가 실제로 획득한 측정 번호입니다. 둘은 독립적입니다. Slab 100번 상태에서 센서 9001번을 측정할 수 있으며, 30Hz Camera는 20Hz Slab 상태 하나에서 여러 번 측정할 수도 있습니다.

CPU/GPU 측정 요청이 시작될 때 context를 복사합니다. GPU readback이나 직렬화가 늦어져도 송신 직전의 최신 Slab 번호로 교체하지 않습니다. CPU 시간 분할 스캔의 context는 시작 상태를 뜻하며, 스캔 중 월드 전체가 고정된다는 뜻은 아닙니다.

| STOMP header | 의미 |
|---|---|
| `x-run-uuid` | 실행별 UUID |
| `x-mtl-no` | 측정 당시 소재 번호, 공백 포함 원문 보존 |
| `x-slab-frame-no` | 측정 시작 시 적용된 Slab 상태 번호 |
| `x-slab-elapsed-sec` | 해당 Slab 상태의 시나리오 시각 |
| `x-session-segment` | pause/resume 구간 번호 |

기존 `frame-id`/`x-frame-id`는 센서 번호로 유지됩니다. PCD schema와 binary 본문도 유지합니다. Camera와 LiDAR telemetry에도 동일한 연계 header를 제공합니다. 일반 비연동 프레임에는 Slab header가 없습니다.

`mtl_no`는 업무 연계 정보이며 전체 장면 PCD의 모든 점이 그 Slab에 속한다는 뜻은 아닙니다. 물체 전용 필터는 별도 기능입니다.

## 시작·종료 동작

Ready → Running → Draining → Completed/Incomplete 상태를 제공합니다. 벌크 네트워크 수신 완료와 실제 재생 시작을 구분하십시오. 첫 Notify 전에는 송신하지 않습니다.

`GetSlabSensorSessionStatus().EndReason`으로 정상 완료(`Completed`), 사용자 중단(`Aborted`), 측정 실패(`AcquisitionFailure`), 처리/송신 실패(`StreamFailure`), 종료 대기 초과(`DrainTimeout`)를 구분합니다. 사용자 중단 후 접수 데이터가 모두 마무리되면 State는 Completed이고 bAborted=true입니다. 실패나 timeout은 Incomplete이며, 일반 실패를 timeout으로 표시하지 않습니다.

PCD 크기 제한 거부는 `BodyLimitRejectedCount`, 재시도 없이 끝난 receipt 실패는 `DeliveryFailureCount`로 집계합니다. 용량 오류로 스트림이 중지되더라도 세션을 성공으로 판정하지 않습니다. 세션의 `StreamFailures`는 실행 시작 이후의 오류만 집계하며 이전 실행의 오류를 다음 실행에 가져오지 않습니다.

Raw TCP의 전송 실패·receipt 재시도 소진·연결 종료 시에는 더 이상 전달 경로가 없는 프레임의 pending 수를 감소시키고 실행 UUID별 실패를 동시에 기록합니다. 게임 스레드 진단 이벤트 처리가 늦더라도 거짓 성공이나 유령 pending에 의한 timeout으로 바뀌지 않습니다. 연결 종료 시 receipt 미확인 프레임은 소비자가 실제 받았을 가능성이 있어도 전달 미확인 실패로 취급합니다.

Begin은 센서 프로필·주기를 유지하며 정지된 대상의 측정을 시작합니다. End는 신규 세션 데이터 접수를 막고 종료 전에 시작한 유효 측정의 인코딩·직렬화·송신·receipt를 최대 10초 동안 마무리합니다. 네트워크 수신은 Slab 움직임 종료보다 늦을 수 있습니다. 실패 시 미완료 수와 사유를 표시합니다.

세션 종료 후 대상 자동 스트림은 자동 재개하지 않습니다. 원래 실행 중이던 Preview 측정은 유지하고, 세션이 시작했던 측정만 중지합니다. 다음 실행은 새 UUID로 Begin을 호출합니다. 이 버전의 세션 대상은 World 수명 동안 관리됩니다. 수동 내보내기는 별개입니다.

센서 주기를 유지하므로 Slab 600행마다 PCD가 한 건씩 생기거나 마지막 행이 반드시 별도 측정된다고 보장하지 않습니다. 실제 승인한 센서 결과의 제출·receipt·소비자 수를 대조합니다.

## 실시간 PCD와 용량 진단

기존에는 UI를 거치지 않고 StartStream을 호출하면 초기 CSV/LatestFrame 설정이 남을 수 있었습니다. 현재는 실시간 Point Cloud 진입점을 Binary PCD, ConnectedNoLoss, FrameStride=1, receipt=1로 정규화합니다. 수동 CSV/LAS/LAZ 계약은 유지됩니다.

Native 32,256점과 synthetic 64,512점은 각각 약 1.02MiB, 2.03MiB(헤더 제외)로 기본 8MiB에 들어갑니다. 작은 한도는 시작 전 예상 bytes로 안내하며 준비된 실제 body도 검사합니다. 한도·큐를 무제한으로 늘리지 않습니다. 시작 전 크기는 원본 배열 기준 보수적 상한이며 ROI/Tag 필터 적용 후 실제 크기는 줄어들 수 있습니다.

실제 backend·포맷·point count·body bytes·설정 한도를 함께 확인하십시오. 회사 프로젝트에서 같은 원인으로 오류가 발생하는지는 호출 경로 로그로 대조해야 합니다. wss://는 Raw TCP를 사용할 수 없으므로 Engine 호환 backend와 서버 설정을 별도로 확인합니다.

WSS는 Engine STOMP로 분기하고 Camera도 호환 JSON을 생성합니다. URL을 평문 TCP로 바꾸지 않습니다. 서버 방식 변경 전에 준비된 binary-only Camera/telemetry 파생 프레임은 stale로 기록해 폐기하고 다음 프레임을 사용합니다. 운영 서버를 바꿀 때는 세션/스트림을 먼저 종료한 뒤 설정을 적용하십시오. TLS 인증서·서버 설정은 별도 실제 환경 검증 대상입니다.

Engine STOMP 호환 경로는 login/passcode 헤더를 항상 쌍으로 구성합니다. 비밀번호가 비었다고 에디터 assertion으로 종료하지 않으며 Broker 인증 결과로 처리합니다. WebSocket upgrade Bearer와 STOMP login 인증의 혼용은 연결 전에 거부합니다. 연결 성공, Broker receipt, 소비자 검증은 서로 다른 상태입니다.

## 세 센서 패널 전용 글자 크기

Settings의 85/100/125/150% 버튼은 해당 Host의 Monitor·Settings·CaptureExport만 변경합니다. 공통 부모를 상속한 동료 위젯, 중첩 위젯과 전체 앱 DPI는 변경하지 않습니다.

native 컨트롤은 `SNewSensorTool`/`SAssignSensorTool` 생성 위치에서 등록합니다. 전체 Slate 트리 순회를 하지 않으며 원본 폰트 기준으로 배율을 적용합니다. 숫자 입력도 배율에 따라 변경됩니다.

사용자 WBP는 Event Construct에서 본인이 소유한 TextBlock에 `Register Sensor Text Control`, EditableTextBox에 `Register Sensor Input Control`을 호출합니다. 동료의 중첩 WBP 컨트롤은 거부됩니다. Host가 생성한 세 패널만 appearance owner를 받습니다.

설정은 `Saved/SaveGames/MA0T10_SensorToolAppearance_v1.sav`에 저장합니다. 100% 버튼은 글자 배율만 변경하고 기존 v6/v7·동료 설정을 삭제하지 않습니다. 기존 drag·resize·접기는 유지됩니다.

`전체 UI 초기화`는 해당 센서 Host의 live 배율과 별도 appearance 저장값도 100%로 초기화합니다. 글자만 초기화하려면 `ResetSensorToolAppearance`를 사용할 수 있습니다.

구 `Set/Get/ResetGlobalSensorUiFontScale` 이름도 호환 wrapper로 유지하지만 동작 범위는 동일한 센서 Host로 제한합니다. 미등록 동료 위젯에서는 setter/reset이 아무 작업도 하지 않습니다. `OnSensorUiFontScaleChanged` 이벤트는 구성 완료된 센서 소유 패널에서만 전달합니다.

## UE 5.3 캡처 ViewState

주기식 Camera와 GPU LiDAR 캡처는 `bCaptureEveryFrame=false`를 유지하고 `bAlwaysPersistRenderingState=true`로 ViewState를 재사용합니다. UE 5.3의 ViewState 없는 병렬 visibility 경로에서 relevance 해제와 occlusion 완료 이벤트의 순서 문제가 관찰되어, 센서 캡처가 해당 경로에 진입하지 않도록 한 프로젝트 수준 대응입니다.

LiDAR는 측정마다 camera-cut 플래그로 이전 영상 이력을 버리므로 현재 장면의 depth를 사용합니다. 전역 occlusion 설정, 렌더러 task schedule, 엔진 소스와 다른 SceneCapture는 변경하지 않습니다. 정상 occlusion 설정에서의 센서 경로 대응이며, `r.AllowOcclusionQueries=0` 등 별도 설정 또는 UE 엔진 전체의 모든 visibility 문제를 해결했다는 뜻은 아닙니다.

실제 RHI 시험은 Camera/LiDAR ViewState가 null이 아니고 측정 구간 동안 동일한 인스턴스로 유지되는지도 검증합니다.

## 재현 테스트

테스트 전용 transient `AVirtualSlabSensorTestDriver`가 벌크 JSON 600개 상태를 생성·파싱하고 임시 큐브 Slab를 움직입니다. 예제 단위는 mm/kg/degree, 소재는 SQ83521 047입니다. 운영맵에 저장하거나 동료 구현을 대체하지 않습니다.

- frame_no 0~599, elapsed_sec 0.00~29.95
- 상태별 0.05초 유지, 30.00초에 End 호출
- 센서 FrameId 독립 집계, hitch로 건너뛴 Slab 상태는 별도 기록
- 종료 후 서로 다른 UUID로 다시 실행

PIE 콘솔의 `ma0t10.SlabSensorTest`는 30초 fixture를 한 번 실행합니다. 센서 품질과 서버 설정을 먼저 적용하십시오.

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SlabSessions -TimeoutSeconds 130
```

runner는 D455 1대·ML-X Native 1대, 10초 워밍업, 서로 다른 UUID의 30초 세션 두 개를 검증합니다. 외부 구독자는 소재·Slab 시간·센서 연속성·checksum을 검사하고 세션별 count/30으로 Camera 29Hz, LiDAR/PCD 19Hz 이상을 판정합니다. 프로브 전체 63초 평균에는 종료 대기가 포함되므로 세션별 Hz와 구분합니다.

증거는 Saved/Reports의 runner JSON/Markdown/log와 `SlabSensorSession/synthetic_slab_30s.json`, `SlabSensorSession/session_runs.json`에 저장합니다. 결과는 커밋하지 않습니다. 실제 동료 프로젝트 검증은 동료 재생기의 호출 위치를 연결한 뒤 별도로 수행해야 합니다.
