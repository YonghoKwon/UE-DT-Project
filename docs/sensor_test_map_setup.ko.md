# 가상 센서 테스트 맵과 Widget Blueprint 설정

이 문서는 Unreal Engine 5.3에서 가상 LiDAR, 가상 카메라, 센서 매니저와 모니터 UI를 한 번에 검증하는 절차를 설명합니다.

## 1. 저장소에 포함된 테스트 맵

기본 smoke-test 맵은 다음 경로입니다.

```text
/Game/M7AT10/Maps/SensorTestMap
Content/M7AT10/Maps/SensorTestMap.umap
```

`Config/DefaultEngine.ini`의 `EditorStartupMap`과 `GameDefaultMap`도 이 맵을 사용합니다.

맵에는 다음 항목이 들어 있습니다.

```text
SensorTest_Manager          AVirtualSensorManager
SensorTest_LiDAR            AVirtualSensorAct
SensorTest_Camera           AVirtualCameraAct
SensorTest_MonitorHost      AVirtualSensorMonitorHostActor
SensorTest_Slab             StaticMeshActor, Actor Tag = Slab
SensorTest_Floor            StaticMeshActor
DirectionalLight / SkyLight / PlayerStart
```

`SensorTestMap`은 저장소에 없는 Designer WBP에 의존하지 않습니다. `SensorTest_MonitorHost`가 `UVirtualSensorMonitorWidget` 네이티브 fallback을 생성하므로, 맵을 열고 PIE를 실행하면 기본 조작 패널이 바로 나타나야 합니다.

## 2. 테스트 맵 재생성

맵 구성은 `Scripts/setup_sensor_test_map.py`로 재생성할 수 있습니다. 이 스크립트는 `SensorTestManaged` 태그가 있는 액터만 교체합니다.

먼저 Editor target을 빌드합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" ma0t10_dtEditor Win64 Development ".\ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE
```

한글이 들어간 OneDrive 경로에서 MSVC가 파일을 찾지 못하면 ASCII 경로를 매핑합니다.

```powershell
subst U: "$PWD"
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" ma0t10_dtEditor Win64 Development "U:\ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE
```

맵 생성 스크립트를 실행합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "U:\ma0t10_dt.uproject" -ExecutePythonScript="U:\Scripts\setup_sensor_test_map.py" -Unattended -NoSplash -NoSound
```

작업 후 임시 드라이브가 필요 없으면 해제합니다.

```powershell
subst U: /D
```

## 3. 맵 액터의 권장 Details 값

### SensorTest_Manager

`AVirtualSensorManager`를 선택하고 다음 값을 확인합니다.

```text
DigitalTwin | SensorManager
  bDiscoverOnBeginPlay                 true
  bStartSensorsOnBeginPlay             true
  bUseSynchronizedCapture              false

DigitalTwin | SensorManager | PointCloudOnly
  bPointCloudOnlyHideWorld             true
  bPointCloudOnlyAutoSelectLidarView   true
```

`SharedSensorTransport` component의 값은 첫 테스트에서 다음처럼 둡니다.

```text
TransportMode = LogOnly
```

`HttpPost`는 로컬 기능 검증이 끝난 후 endpoint와 backpressure/retry 정책을 함께 확인할 때만 켭니다.

### SensorTest_LiDAR

`AVirtualSensorAct`의 `LidarSensorComp`를 선택합니다.

```text
SensorId                          LIDAR-TEST-001
DeviceProfile                     LivoxMid360S
SimulationQuality                 RealTimePreview
OutputMode                        LogOnly
ScanInterval                      0.25
bAutoStartScan                    true
bAutoRegisterToManager            true
bUseMultiHit                      false
bDrawDebugRays                    false
```

서버 payload와 화면 preview는 서로 다른 정책입니다.

```text
ServerPayloadStride               1
MaxServerPayloadPoints            0
bIncludeMissPointsInServerPayload false

PreviewPointStride                2
MaxPreviewPoints                  3000
bPointCloudPreviewHitOnly         true
bPointCloudPreviewEnabled         false
```

`bPointCloudPreviewEnabled`는 평상시 false여도 됩니다. `Point Cloud Only` 버튼을 누르면 Manager가 선택된 LiDAR preview를 켭니다.

자동 파일 export는 smoke test 중 꺼 둡니다.

```text
bExportCsvOnScan          false
bExportJsonLinesOnScan    false
bExportPcdOnScan          false
```

Slab 분석 값은 다음처럼 둡니다.

```text
SlabSemanticLabel                 Slab
ReferenceSlabYawDegrees           0
MinSlabPointsForAnalysis          12
bIncludeSlabAnalysisInPayload     true
```

### SensorTest_Camera

`AVirtualCameraAct`의 `VirtualCameraComp`를 선택합니다.

```text
SensorId                 VCAM-TEST-001
DeviceProfile            IntelRealSenseD455
SimulationQuality        RealTimePreview
CaptureResolution        640 x 360
CaptureInterval          0.1
CaptureMode              PayloadAndOutput
OutputMode               LogOnly
bAutoStartCapture         true
bAutoRegisterToManager    true
```

카메라 preview가 검게 보이면 다음을 확인합니다.

1. 카메라의 빨간 frustum이 `SensorTest_Slab` 방향을 향하는지 확인합니다.
2. `DirectionalLight`와 `SkyLight`가 활성화되어 있는지 확인합니다.
3. `CameraRenderTarget`이 비어 있어도 런타임에 생성되지만, 운영 WBP에서 고정 render target을 쓰려면 별도 `TextureRenderTarget2D`를 지정합니다.
4. 성능이 무거우면 `CaptureMode = PreviewOnly`로 먼저 확인합니다.

### SensorTest_Slab

Actor의 `Tags` 배열에 정확히 다음 값을 넣습니다.

```text
Slab
```

LiDAR는 이 tag를 semantic label `Slab`으로 분류합니다. `ReferenceSlabYawDegrees = 0`은 정상 진행 방향을 월드 X축으로 본다는 뜻입니다.

### SensorTest_MonitorHost

Designer WBP가 아직 없을 때:

```text
MonitorWidgetClass                None
bUseNativeMonitorWidgetFallback   true
SensorManager                     SensorTest_Manager
bAutoCreateOnBeginPlay            true
bAutoFindSensorManager            true
bAddToViewport                    true
bShowLidarViewOnStart             false
bEnablePointCloudOnlyOnStart      false
ViewportZOrder                    10
```

Designer WBP를 완성한 뒤에는 `MonitorWidgetClass`만 `WBP_VirtualSensorMonitor`로 바꾸면 됩니다.

## 4. WBP_VirtualSensorMonitor 만들기

### 4.1 Widget Blueprint 생성

1. Content Browser에서 `Content/M7AT10/UI` 폴더를 엽니다.
2. `Add > User Interface > Widget Blueprint`를 선택합니다.
3. 이름을 정확히 `WBP_VirtualSensorMonitor`로 지정합니다.
4. WBP를 열고 `File > Reparent Blueprint` 또는 `Class Settings > Parent Class`에서 `VirtualSensorMonitorWidget`을 선택합니다.
5. Compile 후 저장합니다.

부모 클래스가 일반 `UserWidget`이면 C++ 자동 바인딩, Manager 연결, payload/preview 상태 갱신 기능을 사용할 수 없습니다.

### 4.2 권장 Designer Hierarchy

다음 구조로 시작하면 기능 확인이 쉽습니다.

```text
CanvasPanel
└─ Border
   └─ VerticalBox
      ├─ TitleText                         TextBlock
      ├─ ViewImage                         Image
      ├─ StatusText                        TextBlock
      ├─ HorizontalBox_View
      │  ├─ ToggleButton                   Button
      │  │  └─ ToggleButtonText            TextBlock
      │  ├─ NextCameraButton               Button
      │  ├─ NextLidarButton                Button
      │  ├─ PointCloudOnlyButton           Button
      │  └─ LidarViewModeButton            Button
      │     └─ LidarViewModeButtonText     TextBlock
      ├─ HorizontalBox_Capture
      │  ├─ CaptureOnceButton              Button
      │  ├─ ExportServerPayloadButton      Button
      │  ├─ LogPointCloudButton            Button
      │  ├─ ExportPointCloudButton         Button
      │  └─ LocalSensorCaptureButton       Button
      │     └─ LocalSensorCaptureButtonText TextBlock
      ├─ HorizontalBox_Preview
      │  ├─ PreviewLessButton              Button
      │  ├─ PreviewMoreButton              Button
      │  └─ PreviewHitOnlyButton           Button
      │     └─ PreviewHitOnlyButtonText     TextBlock
      └─ HorizontalBox_RealSensor
         ├─ StartRealSensorSourcesButton   Button
         ├─ StopRealSensorSourcesButton    Button
         └─ PushRealSensorSourceButton     Button
```

권장 크기:

```text
ViewImage.MinDesiredWidth   640
ViewImage.MinDesiredHeight  360
StatusText.AutoWrapText     true
```

### 4.3 가장 중요한 이름과 Is Variable

아래 이름은 C++의 `BindWidgetOptional` 이름과 대소문자까지 같아야 합니다. 각 위젯에서 `Is Variable`을 체크합니다.

```text
ViewImage
TitleText
StatusText
ToggleButton
ToggleButtonText
NextCameraButton
NextLidarButton
PointCloudOnlyButton
LidarViewModeButton
LidarViewModeButtonText
LogPointCloudButton
ExportPointCloudButton
LocalSensorCaptureButton
LocalSensorCaptureButtonText
CaptureOnceButton
ExportServerPayloadButton
PreviewMoreButton
PreviewLessButton
PreviewHitOnlyButton
PreviewHitOnlyButtonText
StartRealSensorSourcesButton
StopRealSensorSourcesButton
PushRealSensorSourceButton
```

전부 optional이므로 일부가 없어도 크래시가 나면 안 됩니다. 하지만 이름이 틀리면 해당 버튼은 자동 연결되지 않습니다.

### 4.4 버튼 OnClicked Graph를 만들지 않는 이유

정확한 이름의 Button은 `UVirtualSensorMonitorWidget::NativeConstruct()`에서 C++ handler에 자동 연결됩니다.

예:

```text
CaptureOnceButton       -> CaptureSelectedSensorsOnce
PointCloudOnlyButton    -> SensorManager.TogglePointCloudOnlyView
PreviewMoreButton       -> IncreaseLidarPreviewBudget
PreviewLessButton       -> DecreaseLidarPreviewBudget
PreviewHitOnlyButton    -> ToggleLidarPreviewHitOnly
ExportServerPayloadButton -> ExportSelectedSensorServerPayload
```

Designer의 `OnClicked`에 같은 함수를 다시 연결하면 한 번 클릭할 때 두 번 실행될 수 있습니다. 특별한 추가 동작이 없으면 Button Event Graph는 비워 둡니다.

### 4.5 HostActor 방식 - 권장

1. `SensorTestMap`에 `VirtualSensorMonitorHostActor`를 배치합니다.
2. `MonitorWidgetClass = WBP_VirtualSensorMonitor`로 지정합니다.
3. `SensorManager = SensorTest_Manager`로 지정하거나 `bAutoFindSensorManager = true`를 사용합니다.
4. `bAutoCreateOnBeginPlay`, `bAddToViewport`를 true로 둡니다.
5. Level Blueprint에는 UI 생성 노드를 추가하지 않습니다.

이 방식은 맵마다 BeginPlay graph를 복사하지 않아도 되고, 생성 순서와 Manager binding을 C++에서 한 곳에서 관리합니다.

### 4.6 Level Blueprint 방식 - HostActor를 쓰지 않을 때

Level Blueprint에 다음 순서로 연결합니다.

```text
Event BeginPlay
  -> Get Actor Of Class
       Actor Class = VirtualSensorManager
  -> Create Widget
       Class = WBP_VirtualSensorMonitor
       Owning Player = Get Player Controller(0)
  -> BindSensorManager
       Target = Create Widget Return Value
       InSensorManager = Get Actor Of Class Return Value
  -> Add To Viewport
       Target = Create Widget Return Value
```

`Create Widget` return value는 `MonitorWidgetRef` 변수로 Promote해 두면 이후 테스트가 쉽습니다.

주의:

- Manager가 있는데도 `BindVirtualCamera`, `BindVirtualLidar`를 따로 호출하지 않습니다.
- `BindSensorManager`가 선택된 Camera/LiDAR와 real source를 WBP에 전달합니다.
- HostActor 방식과 Level Blueprint 방식을 동시에 사용하면 Widget이 두 개 생성됩니다.
- 기존 `TestMap`은 누락된 WBP를 `Create Widget`에서 참조하므로, 복구 전에는 기본 테스트 맵으로 사용하지 않습니다.

### 4.7 상태를 여러 TextBlock으로 나누고 싶을 때

기본 `StatusText` 하나만 사용하면 C++이 자동 갱신합니다. 운영 UI에서 줄을 나누려면 `GetMonitorDisplayData()`를 사용합니다.

Blueprint 예시:

```text
Event Construct
  -> Set Timer by Event (Time = 0.25, Looping = true)
       -> GetMonitorDisplayData
       -> Break VirtualSensorMonitorDisplayData
       -> SetText(SelectedSensorTextBlock)
       -> SetText(FrameTextBlock)
       -> SetText(ServerPayloadTextBlock)
       -> SetText(PreviewTextBlock)
       -> SetText(SlabTextBlock)
       -> SetText(WarningTextBlock)
```

매 프레임 복잡한 text binding을 평가하는 것보다 0.25초 timer로 갱신하는 편이 운영 UI에 적합합니다.

## 5. PIE 수동 테스트 순서

### 5.1 시작 확인

1. `SensorTestMap`을 엽니다.
2. Play 버튼 옆 옵션에서 `Selected Viewport`로 PIE를 시작합니다.
3. 모니터에 `VCAM-TEST-001` 또는 `LIDAR-TEST-001`이 표시되는지 확인합니다.
4. Output Log에서 센서 등록 수와 `LogOnly` payload 로그를 확인합니다.

### 5.2 가상 카메라

1. `Toggle View` 또는 `Next Camera`를 눌러 Camera view로 전환합니다.
2. `Capture Once`를 누릅니다.
3. `FrameId`, resolution, capture mode, cached payload byte 수가 갱신되는지 확인합니다.
4. `ViewImage`에 render target 영상이 표시되는지 확인합니다.

### 5.3 가상 LiDAR

1. `Toggle View` 또는 `Next LiDAR`를 눌러 LiDAR view로 전환합니다.
2. `Capture Once`를 누릅니다.
3. ray count, hit count, server payload point count, preview point count가 갱신되는지 확인합니다.
4. `Preview +`와 `Preview -`를 누른 뒤 preview count만 바뀌고 server payload 정책은 유지되는지 확인합니다.
5. `Hit Only`를 눌러 miss point preview 포함 여부가 바뀌는지 확인합니다.

### 5.4 Point Cloud Only

1. `PointCloudOnly` 버튼을 누릅니다.
2. Floor/Slab mesh가 화면에서 숨겨지는지 확인합니다.
3. LiDAR point preview는 계속 표시되는지 확인합니다.
4. hit count가 0으로 떨어지지 않는지 확인합니다. 이 모드는 visibility만 숨기고 collision은 유지해야 합니다.
5. 버튼을 다시 눌러 원래 visibility가 복원되는지 확인합니다.

### 5.5 Slab 분석

1. `SensorTest_Slab`의 Actor Tag가 `Slab`인지 확인합니다.
2. LiDAR capture 후 slab point count가 12 이상인지 확인합니다.
3. estimated yaw, reference yaw, angle deviation, confidence가 Status에 표시되는지 확인합니다.
4. Slab actor의 Yaw를 10도로 바꾸고 다시 capture하여 angle deviation이 변하는지 확인합니다.

### 5.6 Export

`Export Payload`는 현재 선택된 센서의 서버 JSON을 다음 위치에 저장합니다.

```text
Saved/SensorCaptures/<SensorId>/ServerPayload
```

`Export Point Cloud`는 LiDAR point cloud 파일을 저장합니다. LAZ는 외부 compressor가 연결되지 않으면 실제 압축 LAZ가 아니라 LAS-compatible source 경고 경로입니다.

### 5.7 Real Sensor 버튼

현재 Livox/RealSense/ROS2 class는 adapter 또는 placeholder 단계입니다. `Start Real Sources`에서 not-implemented 또는 deployment-evidence-pending 상태가 나오는 것은 현재 제한을 정확히 표시하는 동작입니다. 실제 SDK가 연결됐다는 증거로 해석하면 안 됩니다.

## 6. 자동화 테스트

Editor target 빌드 후:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "U:\ma0t10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests MA0T10.EditorSmoke; Quit" -TestExit="Automation Test Queue Empty"
```

기능별 테스트:

```text
MA0T10.EditorSmoke       맵 로드와 SensorTestMap 구성
MA0T10.SensorReplay      CSV/JSONL replay, payload, export
MA0T10.SensorManager     shared service와 point-cloud-only 정책
MA0T10.SensorMonitor     Widget status와 payload export
MA0T10.SensorTransport   HTTP loopback/backpressure 계약
MA0T10.RealSensorSource  placeholder/live JSON source 경로
```

전체 스크립트:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1" -SkipBuild
```

## 7. 현재 남은 수정

1. C++ module namespace는 `ma0t10_dt`, 기존 Content package namespace는 `/Game/M7AT10`입니다. Content 이름도 바꾸려면 파일시스템 폴더만 이동하지 말고 Unreal Editor에서 asset rename, fix redirectors, 전체 resave와 cook 검증을 별도 작업으로 수행해야 합니다.
2. 기존 `TestMap`의 Level Blueprint는 저장소에 없는 `WBP_VirtualSensorMonitor`를 참조합니다. `SensorTestMap`을 기본 smoke map으로 사용하고, 기존 맵은 WBP를 만든 뒤 graph를 다시 compile/save해야 합니다.
3. 운영용 `WBP_VirtualSensorMonitor.uasset`은 아직 저장소에 없습니다. 네이티브 fallback은 smoke test용이며, Designer layout은 위 절차로 Unreal Editor에서 만들고 검수해야 합니다.
4. DTCore의 일부 `USTRUCT` field는 기본 초기화 누락 오류를 출력합니다. DT-Project와 분리해 DTCore 저장소에서 enum/bool 기본값을 추가하고, plugin commit을 갱신한 뒤 submodule pin을 명시적으로 변경해야 합니다.
5. 실제 Livox, RealSense, ROS2 입력은 SDK/bridge 연결과 장비 기반 acceptance evidence가 별도로 필요합니다.
