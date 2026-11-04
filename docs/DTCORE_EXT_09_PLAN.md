# 9단계: DTCore 공통 기능(660740f)으로 리팩토링 (계획 초안)

상태: **초안. DTCore `feat/dtcore-functional-ext`(660740f)가 사용자 검증·머지된 뒤 시작.**
기준: host master `8d6adfbe`, DTDevice `97dd8b4`. 8-2 포맷 이후 경로는 그대로이고 줄 모양만 바뀝니다.
분석은 읽기 전용입니다.

## 1. DTCore 새 기능 요약 (660740f)

| 기능 | API | 비고 |
|---|---|---|
| STOMP 송신 | `UDxWebSocketSubsystem::PublishMessage / PublishBinaryMessage`, `GetPublishDroppedCount` | DTCore의 **공용 WebSocket 연결**로 보냄. 연결이 Subscribing/Ready가 아니면 보내지 않고 false를 돌려줌. receipt 콜백은 GameThread에서 받으며, 재연결 시 버림 |
| 위젯 화면 이동 | `UDxWidgetSubsystem::FindOpenWidget / FocusOrOpenWidget`, `UDxWidget::NavigateTo / NavigateBack / HandleNavigateTo` | 위젯 **안의 탭·하위 화면 이동**과 이전 화면 기록(32개). DTCore `OpenWidget` 목록의 위젯만 대상 |
| 재연결 간격 | `DTCoreSettings::ReconnectInitialDelaySeconds / ReconnectMaxDelaySeconds / ReconnectBackoffMultiplier` | DTCore 연결 전용. 기본값은 기존과 같음(1초, 60초, ×2) |

## 2. 현재 코드와의 대응

### 2.1 STOMP 송신

- 센서 전송(`UVirtualSensorTransportComp`)은 DTCore 연결을 쓰지 않습니다.
  - UE `Stomp` 모듈의 `IStompClient`로 **별도 broker 연결**을 엽니다. 설정은 `SensorTransport.ini`의 `BrokerUrl`이고 기본값은 `ws://127.0.0.1:61616`(Artemis)입니다.
  - 이 연결로 JSON frame·상태와 binary PCD를 함께 보냅니다.
- 대용량 PCD는 `UVirtualSensorHighThroughputTransportSubsystem`의 전용 worker socket을 씁니다. 20Hz, backpressure, receipt, 재연결을 자체 관리합니다.
- DTCore 연결 URL(`DTCoreSettings::WebSocketUrl`)과 센서 broker(`BrokerUrl`)는 **서로 다른 설정**입니다. 현장에서 같은 서버인지는 정해져 있지 않습니다.

**제안**
1. 전송 모드에 `DTCoreWebSocket`을 **opt-in으로 추가**합니다.
   - 저빈도 JSON(상태, 수동 1회 전송, 슬랩 결과 등)을 `PublishMessage`로 보냅니다.
   - 기본 모드(`StompWebSocket`)와 PCD 전송 정책은 바꾸지 않습니다. "PCD 전송이 기본, 나머지는 선택 시" 원칙을 지킵니다.
   - 보내지 못한 경우 기존 결과 구조체(`FVirtualSensorTransportResult`)의 실패 메시지로 표시합니다. DTCore의 dropped count는 상태 표시에 연결합니다.
2. **20Hz PCD binary는 DTCore 공용 연결로 옮기지 않습니다.** 대용량 프레임이 DTCore의 다른 메시지 송수신을 막을 수 있고, 지금의 backpressure·receipt·메모리 검증(r50)이 전용 경로 기준이기 때문입니다.
3. 결정이 필요합니다: 현장에서 센서 broker와 DTCore WebSocket이 같은 서버라면, `DTCoreWebSocket` 모드를 해당 과제의 기본값으로 할지.

### 2.2 위젯 화면 이동

| 대상 | 현재 | 적용 방법 |
|---|---|---|
| 캡처·내보내기 패널 탭 (`UDxWidgetVirtualSensorCaptureExportPanel`, 실시간 전송 / 캡처 / 내보내기 / 연결 및 로그) | `SetActiveTab(EVirtualSensorCaptureExportTab)` 자체 구현, 선택 탭은 `VirtualSensorUiPreferences.CaptureExportActiveTab`에 저장 | `HandleNavigateTo`를 구현해 `NavigateTo("Capture")` 등으로 이동하고 `NavigateBack`을 지원. 저장 형식(uint8)과 기존 `SetActiveTab` API는 호환을 위해 유지 |
| 센서 도구 사이드바 (배치 / 보기 / 시나리오 / 전송 / 결과) | 툴바 위젯 자체 전환 | 사이드바 섹션을 SubTarget 이름으로 매핑. "장비 상세 설정 →" 같은 링크를 `NavigateTo`로 바꿔 뒤로 가기를 지원 |
| `ASlabActor::Click` | `SlabUiRegistry::Activate` | DTCore `OpenWidget` 목록의 Slab 위젯이면 `FocusOrOpenWidget`으로 이미 열린 창을 앞으로 가져옴 |

- 주의: 센서 패널 다수는 workspace subsystem이 직접 관리하고 DTCore `OpenWidgets` 목록에 들어가지 않습니다(`DxWidgetVirtualSensorPanelBase.cpp` 주석).
  - 따라서 `FindOpenWidget/FocusOrOpenWidget`은 DTCore 목록의 위젯에만 적용합니다.
  - workspace 패널을 DTCore 목록으로 옮기는 것은 이번 범위에서 제외합니다. 변경 폭이 커서 별도로 다룹니다.
- DTDevice `UDxDeviceSensorPanelBase` / `UDxDeviceSensorMonitorPanel`에도 `HandleNavigateTo`를 구현합니다(센서 선택을 SubTarget으로).

### 2.3 재연결 간격

| 위치 | 현재 | 제안 |
|---|---|---|
| DTCore 연결 | 1초 / 60초 / ×2 고정 → 660740f에서 설정값 | 과제 `DefaultGame.ini`에 현장 값만 기록(기본값 유지) |
| DTDevice 대용량 전송 (`VirtualSensorHighThroughputTransportSubsystem.cpp`) | `min(10초, 2^min(4,n))` 하드코딩 | DTDevice 설정(`UDeviceSensorAssetSettings` 또는 전송 profile)에 `ReconnectInitial/Max/Multiplier`를 추가. 기본값은 현재와 같은 1초 / 10초 / ×2. DTCore 설정을 그대로 공유하지 않는 이유: 센서 broker는 별도 연결이고 회복 목표(수 초 이내)가 다르기 때문 |
| `VirtualSensorStreamPublisherComp` 과부하 backoff | 자체 정책 | 변경 없음(재연결이 아니라 과부하 제어) |

## 3. 진행 순서

1. DTCore 660740f 사용자 검증 → DTCore PR 머지(사용자)
2. host DTCore pin을 머지 커밋으로 갱신. guard 기준(`Scripts/validate_dtcore_submodule_guard.ps1`의 고정 SHA)도 명시적으로 갱신. TestHost `setup_testhost.ps1`의 검증 커밋도 함께 갱신
3. 2.3 → 2.2 → 2.1 순서로 적용(위험이 작은 것부터)
4. 검증
   - 빌드, release Fail 0, 맵 로드 경고 0
   - 성능 ABAB 2%
   - **native 메모리 3시간 × 3장면**(사용자 결정)
   - 원료 하역은 r50에서 r37보다 높았던 6.66 MiB/h와 비교해 추세를 확인
5. PR·머지

## 4. 예상 소요

약 1~2일. 측정이 약 10~11시간이고, 구현·리뷰가 반나절~1일입니다.
