# Slab 시나리오 메모리 보관 및 재실행

이 기능은 영상을 재생하지 않습니다. 저장한 원본 벌크 JSON을 **동료의 기존 Slab 재생기에 다시 전달**하므로, 시뮬레이션 중 자유롭게 시점을 바꿀 수 있습니다. 당시 맵/센서 설정/PCD 결과를 복원하는 기능은 아니므로 환경을 바꾸면 재측정 결과도 달라집니다.

## 보관 정책

- 게임 인스턴스별 최근 10개. PIE/프로그램 종료 시 모두 삭제하고 디스크에는 저장하지 않습니다.
- `_meta.UUID`가 필수입니다. UUID 누락 시 임의 생성하지 않습니다. `MESSAGE_ID`는 `IFactory-agent`, `DATA_MAP`은 비어 있지 않은 배열이어야 합니다.
- `_meta.scenario`, `CREATE_TIMESTAMP`, 각 행의 소재/프레임/시각/숫자/boolean을 검사합니다. 원본 좌표·단위·문자열은 변환하지 않습니다.
- 같은 UUID는 추가·덮어쓰기·순서 변경 없이 무시합니다. UUID 표기의 대소문자는 정규화하지만 저장된 원본 JSON은 수정하지 않습니다.
- 11번째 시나리오는 가장 오래 등록된 비재생 항목을 제거합니다. 현재 재생 항목은 보호합니다.
- UTF-8 JSON 한 건 8MiB, 검증 중 1건 + 대기 2건으로 제한합니다. `RegisterScenarioJson`의 true는 접수 성공이며 최종 저장 성공은 `OnRegistrationFinished`로 확인합니다. `GetRegistrationMessage`와 `GetPendingRegistrationCount`도 사용할 수 있습니다.
- 프레임 번호는 증가, 시각은 감소하지 않아야 합니다. 누락 행을 자동 생성하거나 마지막 시각만으로 종료를 추정하지 않습니다.

```json
"_meta": {
  "UUID": "d8ddf0b1-b00b-4724-a529-90b8348726e6",
  "scenario": "Continuous Casting Right Meandering - 30s"
}
```

## 동료 클래스의 C++ 연결

실제 클래스가 이 저장소에는 없으므로 아래 `StartExistingPlayback`, `ApplyExistingSlabState` 등은 **동료의 기존 함수로 교체할 예시 이름**입니다. 새 운영용 Slab 이동/파싱 로직을 만들 필요는 없습니다.

### 1. 초기화와 실시간 수신

```cpp
#include "ma0t10_dt/MA0T10/Core/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"

// 클래스 선언에 인터페이스를 추가합니다.
// class AYourPlayer : public AActor, public ISlabScenarioPlaybackAdapter
// virtual bool StartScenarioPlayback_Implementation(
//     const FString& Json, const FString& ScenarioUUID, const FString& RunUUID) override;

// BeginPlay에서 한 번 등록합니다. adapter는 강한 참조로 보관하지 않습니다.
auto* Replay = GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
Replay->RegisterPlaybackAdapter(this);

// 원본 JSON 수신 후, 게임 스레드에서 호출합니다.
Replay->RegisterScenarioJson(OriginalJson);
// 등록 실패/중복은 기존 실시간 데이터 처리 실패를 뜻하지 않습니다.

// 기존 라이브 재생 시작 직전에는 충돌 여부를 반드시 확인합니다.
if (!Replay->SetLivePlaybackActive(true)) {
    // 리플레이 진행 중이므로 기존 재생을 시작하지 않습니다.
    // 데이터는 위 보관 목록에 남을 수 있으며 조용히 동시에 움직이지 않습니다.
    return;
}
// 기존 라이브 파싱/시작 로직 유지. 시작 실패나 종료 시 SetLivePlaybackActive(false).
```

네트워크 수신 callback이 worker 스레드라면 기존 게임 스레드 전달 지점에서 호출하십시오. Subsystem의 Blueprint/C++ 상태 변경 API는 게임 스레드 전용입니다.

### 2. 저장 항목 재생 요청 수락

```cpp
bool AYourPlayer::StartScenarioPlayback_Implementation(
    const FString& Json, const FString& ScenarioUUID, const FString& RunUUID)
{
    // 실제 클래스의 실행 상태 검사. false면 움직임을 시작하지 않아야 합니다.
    if (IsExistingPlayerBusy()) return false;

    bReplayInput = true;       // 예시 멤버: 라이브 입력 경로와 구분
    CurrentRunId = RunUUID;   // 이번 실행 ID. 원본 _meta.UUID와 다릅니다.
    return StartExistingPlayback(Json); // 기존 JSON 파싱/재생 경로
}
```

**리플레이 경로에서는 `BeginSlabSensorSession`을 다시 호출하지 마십시오.** `RequestScenarioReplay`가 이미 세션을 준비합니다. 또 `SetLivePlaybackActive(true)`를 호출하면 본인의 리플레이를 충돌로 판단하므로 라이브 입력에서만 사용합니다. 원본 JSON의 UUID·생성 시각을 새 실행 ID로 바꾸지 않습니다.

### 3. 실제 시작·각 행 적용·종료

```cpp
// 실제 움직임을 시작할 때 (요청 수락 후 5초 안에 시작 알림 필요)
Replay->NotifyPlaybackStarted(CurrentRunId);

// 기존 Slab 위치/회전/윤곽 적용 직후
ApplyExistingSlabState(Row);
GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()
    ->NotifySlabFrameApplied(CurrentRunId, Row.MtlNo, Row.FrameNo, Row.ElapsedSec);

// 기존 재생기가 판단한 실제 종료 시점. 마지막 JSON 행 시각을 임의로 사용하지 않습니다.
Replay->NotifyPlaybackFinished(CurrentRunId, false); // 중단이면 true

// EndPlay에서 등록 해제. 현재 재생이면 중단 처리됩니다.
Replay->UnregisterPlaybackAdapter(this);
```

라이브 재생의 기존 센서 Begin/Notify/End 호출은 유지하며 종료 시 `SetLivePlaybackActive(false)`도 호출합니다. 기존 시작 함수가 라이브 전용 Begin/SetLive를 내장했다면 `bReplayInput` 분기에서 그 두 호출만 생략해야 합니다. 이 adapter 연결 확인은 실제 동료 코드를 받은 후 진행합니다.

## Blueprint/WBP 배치

1. `ASlabScenarioReplayUiHostActor`를 원하는 레벨에 배치하거나 Blueprint에서 Spawn합니다. 기존 세 센서 Host와 독립적입니다.
2. `ReplayWidgetClass`에는 `/Game/MA0T10/UI/WBP_SlabScenarioReplayPanel`을 지정합니다. 비어 있으면 이 자산, 자산이 없으면 native fallback을 사용합니다.
3. WBP 부모는 `SlabScenarioReplayPanelWidget`입니다. 제공된 자산의 Designer와 Event Graph는 비워 두어 native UI를 사용합니다. 별도 Designer를 만들면 공개 `SelectScenario`, `ReplaySelected` API와 목록/상태 조회 API에 바인딩하십시오.
4. 재생 담당 Blueprint의 Class Settings에 `SlabScenarioPlaybackAdapter`를 추가하고 `StartScenarioPlayback`에서 기존 JSON 처리 함수를 호출한 뒤 수락 여부를 반환합니다.
5. BeginPlay에서 `Get Game Instance Subsystem → SlabScenarioReplaySubsystem → RegisterPlaybackAdapter(Self)`를 호출합니다. 실제 시작/종료에도 해당 알림을 연결합니다.
6. 원본 수신 지점에서 `RegisterScenarioJson`을 호출합니다. 새로운 Topic 구독은 필요 없습니다.
7. PlayerController는 Game and UI 입력 모드와 마우스 커서를 허용해야 합니다. 기존 센서 Host의 입력 구성을 그대로 이용할 수 있습니다.

패널은 Main의 AddWidgetPanel Canvas를 우선 사용하고 Viewport로 fallback합니다. 별도 패널 key `SlabScenarioReplay`를 사용하며 세 센서 패널의 폰트 소유자에 등록하지 않습니다. 제목 드래그, 우하단 resize, 실제 높이 접기, 목록 스크롤을 제공합니다. 기본 600×540, 최소 420×300입니다. 시나리오 데이터는 저장하지 않지만 새 패널의 배치는 기존 UI 배치 저장 구조의 독립 key를 사용합니다.

재생 버튼이 비활성이면 adapter 연결, 항목 선택, 기존 실시간/센서 세션 종료 여부를 확인하십시오. 배속·seek·영상 녹화는 없습니다.

## 관찰 전용과 PCD

- 기본 관찰 전용은 Broker 설정 없이 시작할 수 있습니다. 현재 구현은 센서 Coordinator 하나와 대상 센서가 필요합니다.
- `TargetSensorIds`가 비어 있으면 시작 시 Coordinator의 센서 전체를 대상으로 고정합니다. 특정 LiDAR만 제어하려면 해당 ID를 지정합니다.
- 관찰 전용은 대상 자동 스트림을 중지하고 새 송신을 차단합니다. 측정/미리보기는 계속할 수 있습니다. 비대상 센서는 건드리지 않으며 시작 전 이미 Broker에 제출된 프레임을 취소하거나 되돌리지는 않습니다.
- PCD 송신을 켜면 STOMP 서버 설정과 LiDAR 대상이 필요합니다. Camera/LiDAR JSON 스트림은 자동으로 켜지지 않습니다.
- 실제 움직임 종료 후 승인된 프레임을 최대 10초 동안 drain합니다. 완료 또는 실패로 정리될 때까지 다음 재생을 막습니다. 이전 자동 스트림을 임의로 재개하지 않습니다.
- PCD 내부 `# MA0T10_META`의 `scenario_uuid`는 원본 시나리오 ID, `run_uuid`는 새 실행 ID입니다. 같은 항목을 재생해도 새 실행 UUID를 생성합니다. `frame_no`와 `sensor_frame_id`도 별개입니다.

## 테스트 방법과 증거

운영맵을 재생성하지 않습니다. WBP 자산만 재생성해야 할 때 `Scripts/setup_slab_replay_widget.py`를 Unreal Python으로 실행합니다.

수동 확인:

1. `SensorRefactorTestMap`을 PIE로 실행합니다.
2. Unreal 콘솔에서 `ma0t10.ScenarioReplayDemo`를 실행합니다. 테스트용 임시 Slab/adapter/패널과 600행 데이터만 생성하며 맵에 저장하지 않습니다.
3. 목록을 선택하고 PCD 체크를 끈 채 재생합니다. 30초 동안 Slab 이동과 자유 시점을 확인합니다.
4. 완료 후 같은 항목을 다시 재생하고 실행 UUID가 달라지는지 확인합니다. 송신을 시험하려면 먼저 Capture/Export에서 로컬 Artemis 설정을 적용하고 PCD 체크를 켭니다.
5. 패널 드래그/resize/접기를 확인하고 PIE를 종료합니다. 다시 실행하면 목록은 비어 있어야 합니다.

자동 확인:

- `MA0T10.ScenarioReplay.Catalog`, `.ObservationGate`: UUID/원본 보존/10개 제한/대기 상한/관찰 출력 차단.
- `Scripts/run_slab_scenario_replay_smoke.ps1`: 실제 D3D12에서 관찰 30초 + 같은 원본 PCD 재생 30초 두 번, 실제 UI API/자산/adapter/Artemis 외부 구독자 검사.
- 외부 수신 PCD는 `Saved/Reports/slab_replay_received.pcd`, 보고서는 `slab_replay_external.json`, `slab_replay_runtime.log/json`에 남습니다. `python Scripts/read_pcd_context.py Saved/Reports/slab_replay_received.pcd`로 파일 내부 원본/실행 UUID를 확인합니다.
- 2026-09-07 첫 RHI 결과: 관찰 송신 0건, PCD 1,196건 제출/receipt/소비자 일치, context invalid 0, 평균 엔진 FPS 53.89. 이는 60 FPS 성능 통과 증거가 아닙니다.
- 단독 재검증: 관찰 1회 + PCD 2회, PCD 1,197건 제출/receipt/소비자 일치, context invalid 0, 평균 엔진 FPS 59.83, 엔진 frame p95 16.67ms. 이 수치는 해당 테스트 환경의 엔진 delta 기준이며 모든 PC나 화면 크기의 성능을 보장하지 않습니다. 자동 렌더 캡처 `slab_replay_runtime.png`는 887×500 에디터 화면이며, 1920×1080/1280×720 직접 조작 완료 증거가 아닙니다.
- 2026-09-07 Computer Use 재시도에서는 New Editor Window PIE의 목록 선택·재생, 제목 드래그, 우하단 resize, 접기/펼치기, 접힌 영역의 클릭 통과, 재생 중 시점 변경을 직접 확인했습니다. 30초 관찰 재생 두 번 모두 600행을 적용했고 실행 UUID가 바뀌었습니다. 새 PIE에서 UI Host만 생성했을 때 목록이 비어 있는 것도 확인했습니다. 실제 창 캡처는 테두리 포함 816×726이므로 1920×1080/1280×720 검증으로 표시하지 않습니다.
- 알려진 작은 문제: 확대 후 `배치 초기화`는 크기를 복원하지만, 위치 계산을 크기 복원보다 먼저 수행해 최초 위치와 오차가 생깁니다. 아직 수정하지 않았습니다. 상세 증거는 `Saved/Reports/slab_replay_mouse_validation.md`와 `.log`에 있습니다. 실제 동료 클래스 연결, 여러 항목의 수동 스크롤 및 목표 해상도별 검증은 별도 확인이 필요합니다.
