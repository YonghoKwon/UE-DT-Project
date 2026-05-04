# AGETNS.md - 실제 센서 성능 최적화 브랜치 분석 문서

> 브랜치: `feature/real-sensor-performance-optimizations`  
> 기준 브랜치: `feature/real-sensor-device-profiles`  
> 프로젝트: UE-DT-Project / Unreal Engine 디지털 트윈 가상 센서 플랫폼

이 문서는 현재 브랜치의 구현 상태를 정리하고, 이후 개발자 또는 자동화 에이전트가 가상 카메라 / 가상 LiDAR 센서 시스템을 안전하게 수정할 수 있도록 기준을 제공한다.

## 1. 브랜치 목적

이 브랜치는 실제 센서 프로파일 작업을 이어받아, 실제 하드웨어 스펙을 Unreal 시뮬레이션 값에 그대로 적용했을 때 에디터/런타임이 지나치게 느려지는 문제를 해결하기 위해 만들어졌다.

핵심 설계 원칙은 다음과 같다.

```text
DeviceProfile = 실제 장비의 메타데이터와 물리적 기준
SimulationQuality = Unreal 런타임에서 실제로 계산할 시뮬레이션 비용과 미리보기/캡처 품질
```

실제 센서의 제조사, 모델명, FOV, 거리 범위, frame metadata, payload 구조는 실제 장비와 최대한 맞춰야 한다. 하지만 인터랙티브 미리보기 중에 Unreal이 실제 장비의 point rate나 camera frame rate를 매번 그대로 계산할 필요는 없다.

## 2. 대상 실제 장비

### Camera

- 제조사: Intel
- 모델: RealSense D455
- 구현 enum: `EVirtualCameraDeviceProfile::IntelRealSenseD455`
- 컴포넌트: `UVirtualCameraComp`

### LiDAR

- 제조사: Livox
- 모델: Mid-360S / Mid-360S 스타일 프로파일
- 구현 enum: `EVirtualLidarDeviceProfile::LivoxMid360S`
- 컴포넌트: `UVirtualLidarSensorComp`

현재 프로젝트는 이 장비들을 “시뮬레이션”한다. 아직 Intel RealSense SDK, Livox SDK, ROS2, UDP packet, vendor bridge middleware를 통해 실제 하드웨어 데이터를 직접 읽지는 않는다.

## 3. 주요 아키텍처 구성요소

### `AVirtualSensorManager`

경로:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorManager.cpp
```

역할:

- 레벨 안의 가상 카메라 / LiDAR 컴포넌트 검색
- Camera / LiDAR 센서 등록
- 선택된 Camera / LiDAR 관리
- 선택된 센서를 `UVirtualSensorMonitorWidget`에 바인딩
- 전체 센서 시작/정지
- 동기화 캡처 옵션 처리
- 센서 요약 정보와 Health 요약 정보 제공
- 공통 런타임 서비스 소유
  - `SharedSensorTransport`
  - `SharedSensorRecorder`

중요 속성:

```cpp
bDiscoverOnBeginPlay
bStartSensorsOnBeginPlay
bUseSynchronizedCapture
SynchronizedInterval
StaleSensorSeconds
SharedTransportComponent
SharedRecorderComponent
```

현재 구조:

```text
AVirtualSensorManager
├─ SharedSensorTransport
├─ SharedSensorRecorder
├─ 등록된 UVirtualCameraComp 목록
└─ 등록된 UVirtualLidarSensorComp 목록
```

Camera 또는 LiDAR가 등록되면 Manager가 해당 센서 컴포넌트에 공통 Transport와 Recorder를 주입한다.

### `UVirtualCameraComp`

경로:

```text
Source/m7at10_dt/M7AT10/Camera/VirtualCameraComp.h
Source/m7at10_dt/M7AT10/Camera/VirtualCameraComp.cpp
```

역할:

- `USceneCaptureComponent2D` 기반 Scene Capture
- RenderTarget 생성 및 관리
- JPEG 압축 및 Base64 인코딩
- JSON payload 생성
- `UVirtualSensorDataTransportComp`를 통한 전송
- `UVirtualSensorRecorderComp`를 통한 녹화
- UI / Health 모니터링용 RuntimeStatus 갱신

중요 설정:

```cpp
DeviceProfile = EVirtualCameraDeviceProfile::IntelRealSenseD455
SimulationQuality = EVirtualSensorSimulationQuality::RealTimePreview
CaptureMode = EVirtualCameraCaptureMode::PayloadAndOutput
CaptureResolution
CaptureInterval
JpegQuality
bApplyDeviceProfileOnBeginPlay
bAutoRegisterToManager
TransportComponent
RecorderComponent
```

Camera CaptureMode 의미:

```text
PreviewOnly      = RenderTarget 미리보기만 수행. JSON/JPEG payload 생성 없음.
Payload          = JSON payload는 만들지만 transport/output 경로로 보내지 않음.
PayloadAndOutput = payload를 만들고 transport/output 경로로 전송/저장.
```

### `UVirtualLidarSensorComp`

경로:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h
Source/m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.cpp
```

역할:

- Unreal collision trace 기반 LiDAR 스타일 ray sampling
- 선택적 multi-hit tracing
- 위젯 미리보기용 heatmap/texture 생성
- JSON payload 생성
- CSV / JSONL / PCD point cloud export
- `UVirtualSensorDataTransportComp`를 통한 전송
- `UVirtualSensorRecorderComp`를 통한 녹화
- UI / Health 모니터링용 RuntimeStatus 갱신

중요 설정:

```cpp
DeviceProfile = EVirtualLidarDeviceProfile::LivoxMid360S
SimulationQuality = EVirtualSensorSimulationQuality::RealTimePreview
PayloadPointStride
MaxPayloadPoints
bIncludeMissPointsInPayload
HorizontalSamples
VerticalChannels
ScanInterval
HorizontalFov
MinVerticalAngle
MaxVerticalAngle
MaxDistance
bUseMultiHit
MaxHitsPerRay
bFlipLidarViewHorizontal
bFlipLidarViewVertical
bExportCsvOnScan
bExportJsonLinesOnScan
bExportPcdOnScan
IgnoreActorTags
TransportComponent
RecorderComponent
```

### `UVirtualSensorDataTransportComp`

경로:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.cpp
```

역할:

- JSON / binary 데이터 출력 경로 통합
- LogOnly 출력
- SaveToFile 출력
- HTTP POST 출력
- 마지막 전송 결과를 UI / RuntimeStatus 용도로 저장

TransportMode:

```text
None
LogOnly
SaveToFile
HttpPost
```

중요 API:

```cpp
FVirtualSensorTransportResult SendJson(...)
FVirtualSensorTransportResult SendBinary(...)
```

### `UVirtualSensorRecorderComp`

경로:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorRecorderComp.h
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorRecorderComp.cpp
```

역할:

- Camera / LiDAR JSON payload frame 녹화
- 녹화 session을 JSON 파일로 저장
- session 파일 로드
- 녹화 frame을 `OnReplayFrame` 이벤트로 순차 재생

중요 API:

```cpp
StartRecording()
StopRecording()
ClearRecording()
RecordJsonFrame(...)
SaveSession(...)
LoadSessionFromFile(...)
StartReplay(...)
StopReplay()
```

현재 Replay 한계:

```text
Replay는 현재 녹화된 payload frame을 이벤트로 다시 발생시키는 단계이다.
Camera image 또는 LiDAR heatmap을 자동으로 복원해서 기존 monitor widget에 영상처럼 표시하는 기능은 아직 없다.
```

## 4. 성능 모델

### 이 브랜치가 필요한 이유

Livox 스타일 설정을 그대로 적용하면 예를 들어 다음과 같은 값이 된다.

```text
HorizontalSamples = 360
VerticalChannels = 60
ScanInterval = 0.1 sec
```

이 경우 계산량은 다음과 같다.

```text
360 * 60 = 21,600 traces / scan
21,600 * 10Hz = 216,000 traces / sec
```

Unreal에서는 각 LiDAR point를 만들기 위해 collision/ray 계산, hit 결과 처리, payload 생성, texture update, UI refresh, export, transport, recording 등이 함께 발생한다. 그래서 실제 장비 스펙을 그대로 실시간 시뮬레이션 값에 넣으면 에디터/패키징 런타임 모두에서 매우 무거워질 수 있다.

카메라도 동일하다. D455 스타일의 `1280x720 @ 30fps` 캡처에 RenderTarget readback, JPEG 압축, Base64 인코딩을 붙이면 단순 카메라 preview보다 훨씬 무겁다.

### SimulationQuality 프리셋

정의 위치:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorDeviceProfileTypes.h
```

Enum:

```cpp
EVirtualSensorSimulationQuality
```

값:

```text
Debug
RealTimePreview
Balanced
FullSpec
Custom
```

### 권장 런타임 기본값

Editor 및 packaged smoke test에서는 아래 값을 권장한다.

```text
Camera SimulationQuality = RealTimePreview
LiDAR SimulationQuality  = RealTimePreview
TransportMode            = LogOnly
LiDAR MultiHit           = false
LiDAR ExportOnScan       = false
Camera CaptureMode       = PayloadAndOutput 또는 화면만 볼 경우 PreviewOnly
```

### FullSpec 주의사항

`FullSpec`은 짧은 검증용으로만 사용한다. 해당 PC가 충분히 감당할 수 있다는 것이 확인되지 않았다면 editor/runtime 기본값으로 쓰지 않는다.

## 5. 에디터 권장 설정

레벨에 다음 actor를 배치한다.

```text
AVirtualSensorManager
AVirtualCameraAct
AVirtualSensorAct
```

### Camera 설정

`AVirtualCameraAct` 선택 후 `VirtualCameraComp`를 설정한다.

```text
DeviceProfile = IntelRealSenseD455
SimulationQuality = RealTimePreview
bApplyDeviceProfileOnBeginPlay = true
CaptureMode = PayloadAndOutput
bAutoRegisterToManager = true
```

성능 문제를 진단할 때는 다음처럼 낮춘다.

```text
CaptureMode = PreviewOnly
SimulationQuality = Debug
```

### LiDAR 설정

`AVirtualSensorAct` 선택 후 `LidarSensorComp`를 설정한다.

```text
DeviceProfile = LivoxMid360S
SimulationQuality = RealTimePreview
bApplyDeviceProfileOnBeginPlay = true
bAutoRegisterToManager = true
bUseMultiHit = false
bDrawDebugRays = false
bExportCsvOnScan = false
bExportJsonLinesOnScan = false
bExportPcdOnScan = false
```

LiDAR monitor view가 위아래/좌우로 반전되어 보이면 다음 옵션을 확인한다.

```text
bFlipLidarViewVertical
bFlipLidarViewHorizontal
```

현재 기본값은 vertical flip enabled이다.

### Manager 설정

`AVirtualSensorManager` 선택:

```text
bDiscoverOnBeginPlay = true
bStartSensorsOnBeginPlay = true
SharedSensorTransport.TransportMode = LogOnly
```

파일 저장 테스트:

```text
SharedSensorTransport.TransportMode = SaveToFile
```

HTTP 전송 테스트:

```text
SharedSensorTransport.TransportMode = HttpPost
SharedSensorTransport.HttpEndpoint = http://host:port/path
```

## 6. Widget 설정

주요 위젯 클래스:

```text
UVirtualSensorMonitorWidget
```

Designer에서 사용할 수 있는 optional widget 이름:

```text
TitleText
ViewImage
StatusText
ToggleButton
ToggleButtonText
NextCameraButton
NextLidarButton
```

Blueprint에서 사용할 위젯은 모두 `Is Variable`을 체크한다.

권장 Level Blueprint 흐름:

```text
Event BeginPlay
→ Create Widget: WBP_VirtualSensorMonitor
→ Add to Viewport
→ Get Actor of Class: AVirtualSensorManager
→ BindSensorManager
```

Manager가 있는 경우 Level Blueprint에서 Camera/LiDAR를 직접 위젯에 바인딩하지 않는 것을 권장한다. 선택 센서 관리와 위젯 바인딩은 `AVirtualSensorManager`에 맡긴다.

## 7. 파일 출력 위치

런타임 저장 데이터는 일반적으로 프로젝트 또는 패키징 앱의 `Saved` 경로 아래에 저장된다.

예시:

```text
Saved/SensorCaptures/{SensorId}/
Saved/SensorCaptures/{SensorId}/PointCloud/
Saved/SensorRecordings/
```

생성될 수 있는 파일:

```text
*.json   센서 payload / 녹화 session
*.jpg    Camera frame
*.csv    LiDAR point cloud export
*.jsonl  LiDAR point cloud export
*.pcd    LiDAR point cloud export
```

## 8. 패키징 가이드

가상 센서 경로는 패키징 친화적으로 동작하도록 설계되어 있다. 다만 merge/release 전에는 반드시 로컬 패키징 빌드를 수행해야 한다.

첫 패키징 smoke test 권장 설정:

```text
Camera SimulationQuality = RealTimePreview
LiDAR SimulationQuality = RealTimePreview
TransportMode = LogOnly
MultiHit = false
ExportOnScan = false
```

그 다음 무거운 기능을 하나씩 켜면서 확인한다.

```text
SaveToFile
HttpPost
Balanced
FullSpec
MultiHit
ExportOnScan
```

성능 문제나 패키징 문제를 진단할 때는 무거운 옵션을 한 번에 모두 켜지 않는다.

## 9. 알려진 한계

### 실제 하드웨어 연동은 아직 없음

현재 구현은 시뮬레이션 기반이다.

```text
Camera = Unreal SceneCapture 기반 가상 카메라
LiDAR  = Unreal trace 기반 가상 LiDAR
```

아직 구현되지 않은 부분:

```text
Intel RealSense SDK 입력
Livox SDK 입력
ROS2 bridge 입력
UDP point cloud 입력
실제 하드웨어 timestamp synchronization
실제 하드웨어 calibration import
```

### Replay는 데이터 이벤트 기반

`UVirtualSensorRecorderComp`는 payload frame을 녹화/저장/로드/재생할 수 있다. 하지만 현재 replay 경로는 payload 이벤트를 broadcast하는 단계이다. 녹화된 camera image 또는 LiDAR heatmap texture를 자동으로 복원해서 live monitor widget에 표시하지는 않는다.

### FullSpec은 실시간 보장 아님

고성능 PC에서도 full hardware-equivalent simulation은 비쌀 수 있다. Unreal은 실제 센서 데이터를 읽는 것이 아니라 scene intersection, payload 생성, texture update 등을 직접 계산하기 때문이다.

## 10. 향후 수정 시 개발 규칙

### DeviceProfile과 SimulationQuality를 분리 유지

실제 하드웨어 profile 적용이 무거운 runtime sampling을 기본으로 강제하면 안 된다.

좋은 예:

```text
DeviceProfile = IntelRealSenseD455
SimulationQuality = RealTimePreview
```

나쁜 예:

```text
DeviceProfile = IntelRealSenseD455 적용 시 항상 1280x720 @ 30fps 강제
```

좋은 예:

```text
DeviceProfile = LivoxMid360S
SimulationQuality = RealTimePreview
```

나쁜 예:

```text
DeviceProfile = LivoxMid360S 적용 시 editor에서 항상 216,000 traces/sec 강제
```

### Manager 소유의 공통 서비스를 우선 사용

공통 서비스는 `AVirtualSensorManager`가 제공한다.

```text
SharedSensorTransport
SharedSensorRecorder
```

특별한 이유가 없다면 각 센서가 별도의 transport/recorder instance를 제각각 생성하지 않도록 한다.

### 무거운 기본값 금지

기본값은 에디터가 쾌적하게 동작해야 한다.

권장 기본값:

```text
RealTimePreview
LogOnly
MultiHit false
ExportOnScan false
DrawDebugRays false
```

### 비싼 기능은 명시적 opt-in으로 유지

아래 기능은 명시적으로 켰을 때만 동작하도록 유지한다.

```text
FullSpec
MultiHit
CSV/JSONL/PCD ExportOnScan
HTTP high-rate streaming
High-resolution JPEG payloads
```

### UI는 optional binding 구조 유지

`UVirtualSensorMonitorWidget`은 optional widget binding을 유지해야 한다. 모든 optional button/text field가 Blueprint widget에 반드시 존재해야만 동작하는 구조로 만들지 않는다.

## 11. 빠른 문제 해결

### 에디터가 느려질 때

아래 순서로 낮춘다.

```text
1. LiDAR SimulationQuality = Debug
2. Camera CaptureMode = PreviewOnly
3. SharedSensorTransport.TransportMode = LogOnly
4. LiDAR bUseMultiHit = false
5. 모든 ExportOnScan 옵션 끄기
6. bDrawDebugRays 끄기
```

### LiDAR view가 위아래로 반전되어 보일 때

다음 옵션을 토글한다.

```text
bFlipLidarViewVertical
```

### LiDAR view가 좌우로 반전되어 보일 때

다음 옵션을 토글한다.

```text
bFlipLidarViewHorizontal
```

### Payload가 너무 클 때

다음 값을 조정한다.

```text
PayloadPointStride
MaxPayloadPoints
bIncludeMissPointsInPayload
```

### Recording 파일이 비어 있을 때

다음을 확인한다.

```text
SharedRecorderComponent.StartRecording() 호출 여부
Camera/LiDAR가 Manager에 등록되어 있는지
Camera/LiDAR RecorderComponent가 할당되어 있는지
Capture/scan이 실제로 실행 중인지
```

### HTTP 전송이 안 될 때

다음을 확인한다.

```text
SharedSensorTransport.TransportMode = HttpPost
SharedSensorTransport.HttpEndpoint가 유효한지
네트워크/방화벽이 outbound request를 허용하는지
서버가 application/json을 받는지
```

## 12. 다음 구현 권장 사항

1. 녹화 payload에서 camera image와 LiDAR heatmap을 복원하는 replay visualization mode 추가.
2. 고속 LiDAR용 JSON-only streaming 대신 binary point cloud transport 추가.
3. Manager 아래에 `VirtualSimulation`, `RealHardware`, `Replay` source mode 추가.
4. RealSense SDK / Livox SDK / ROS2 bridge adapter를 별도 source component로 추가.
5. 실제 장비 extrinsics/intrinsics calibration import 추가.
6. 자동 smoke test map과 패키징 테스트 절차 문서화.
7. UI에서 `SimulationQuality`, recording, replay, export, transport mode를 조절할 수 있는 controls 추가.
8. FullSpec 또는 ExportOnScan이 켜져 있을 때 UI warning 표시.

## 13. 요약

이 브랜치는 실제 장비 identity와 payload metadata를 목표 하드웨어에 맞추면서도, 기본 런타임에서는 full hardware-equivalent simulation 비용을 피하도록 가상 센서 플랫폼을 실사용에 가깝게 정리한다.

의도한 운영 모델은 다음과 같다.

```text
RealTimePreview = 에디터/런타임 인터랙티브 운영
Balanced        = 조금 더 높은 품질의 preview 또는 짧은 테스트
FullSpec        = 짧은 검증 캡처 전용
Replay          = 데이터 이벤트 기반 재생 토대
RealHardware    = 향후 SDK/bridge 연동 대상
```

이 브랜치를 수정할 때는 하드웨어 프로파일의 정확도와 런타임 시뮬레이션 비용을 분리하는 원칙을 반드시 유지한다.
