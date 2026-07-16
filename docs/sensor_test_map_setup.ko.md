# 가상 센서 테스트 맵과 Widget Blueprint 설정

이 문서는 Unreal Engine 5.3에서 가상 LiDAR, 가상 카메라, 센서 매니저와 모니터 UI를 한 번에 검증하는 절차를 설명합니다.

## 1. 저장소에 포함된 테스트 맵

기본 smoke-test 맵은 다음 경로입니다.

```text
/Game/MA0T10/Maps/SensorTestMap
Content/MA0T10/Maps/SensorTestMap.umap
```

`Config/DefaultEngine.ini`의 `EditorStartupMap`과 `GameDefaultMap`도 이 맵을 사용합니다.

맵에는 다음 항목이 들어 있습니다.

```text
SensorTest_Manager          AVirtualSensorManager
SensorTest_LiDAR            AVirtualSensorAct
SensorTest_Camera           AVirtualCameraAct
SensorTest_MonitorHost      AVirtualSensorMonitorHostActor
SensorTest_Floor            StaticMeshActor, Actor Tag = KeepInPointCloudOnly
SensorTest_HallBackWall     StaticMeshActor
SensorTest_HallLeftWall     StaticMeshActor
SensorTest_HallRightWall    StaticMeshActor
SensorTest_TargetCube       StaticMeshActor
SensorTest_TargetSphere     StaticMeshActor
SensorTest_TargetPillar     StaticMeshActor
DirectionalLight / SkyLight / RectLight x4 / SkyAtmosphere / PostProcess / PlayerStart
```

다음 세 WBP도 저장소에 포함됩니다. Designer hierarchy가 비어 있어도 각 C++ 부모의 native-backed UI가 표시됩니다.

```text
WBP_VirtualSensorMonitor       UVirtualSensorMonitorWidget          오른쪽 중앙
WBP_VirtualSensorSettings      UVirtualSensorSettingsWidget         왼쪽 중앙
WBP_VirtualSensorCaptureExport UVirtualSensorCaptureExportWidget    아래 중앙
```

세 패널은 밝은 맵 위에서도 구분되는 어두운 반투명 테마를 사용합니다. 제목 표시줄을 왼쪽 마우스로 끌어 이동할 수 있고 `접기`, `위치 초기화`를 제공합니다. 패널 위치와 접힘 상태는 사용자별 UI SaveGame에 저장됩니다. `SensorTest_MonitorHost`가 모두 생성·바인딩하므로 Level Blueprint는 필요하지 않습니다. 기본 공간은 약 16×12×4m의 밝은 개방형 실내 테스트 홀입니다.

테스트 맵에는 Slab mesh를 넣지 않습니다. 사용자가 원하는 mesh를 배치한 뒤 Actor Tag `Slab`을 추가하면 기존 Slab 분석 기능을 그대로 사용할 수 있습니다.

## 2. 테스트 맵 재생성

맵 구성은 `Scripts/setup_sensor_test_map.py`로 재생성할 수 있습니다. 이 스크립트는 WBP가 없으면 먼저 생성하고, `SensorTestManaged` 태그가 있는 액터만 교체한 뒤 WBP를 MonitorHost에 연결합니다.

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

1. Settings 패널에서 카메라를 선택하고 `선택 센서 투사 범위 표시`를 켜 청록색 frustum이 일반 타깃 방향을 향하는지 확인합니다.
2. `DirectionalLight`, `SkyLight`, 천장 `RectLight` 4개가 활성화되어 있는지 확인합니다.
3. `CameraRenderTarget`이 비어 있어도 런타임에 생성되지만, 운영 WBP에서 고정 render target을 쓰려면 별도 `TextureRenderTarget2D`를 지정합니다.
4. 성능이 무거우면 `CaptureMode = PreviewOnly`로 먼저 확인합니다.

### 사용자 Slab mesh (선택)

맵에는 Slab mesh가 기본 배치되지 않습니다. 분석할 mesh를 직접 배치하고 Actor의 `Tags` 배열에 정확히 다음 값을 넣습니다.

```text
Slab
```

LiDAR는 이 tag를 semantic label `Slab`으로 분류합니다. `ReferenceSlabYawDegrees = 0`은 정상 진행 방향을 월드 X축으로 본다는 뜻입니다.

### SensorTest_MonitorHost

```text
MonitorWidgetClass                WBP_VirtualSensorMonitor
SettingsWidgetClass               WBP_VirtualSensorSettings
CaptureExportWidgetClass          WBP_VirtualSensorCaptureExport
bUseNativeMonitorWidgetFallback   true
bUseNativeToolWidgetFallback      true
bAutoCreateToolPanels             true
SensorManager                     SensorTest_Manager
bAutoCreateOnBeginPlay            true
bAutoFindSensorManager            true
bAddToViewport                    true
bShowLidarViewOnStart             false
bEnablePointCloudOnlyOnStart      false
ViewportZOrder                    10
SettingsViewportZOrder            20
CaptureExportViewportZOrder       30
bConfigurePlayerInputOnCreate     true
bShowMouseCursorOnCreate          true
```

PIE 시작 시 `Game and UI` 입력 모드와 마우스 커서가 자동 설정됩니다. 별도의 Level Blueprint에서 `Create Widget`이나 입력 모드 노드를 만들 필요가 없습니다.

### 런타임 Sensor Settings

Settings 패널에서 Camera/LiDAR를 선택하고 위치, 회전, 프로필, Simulation Quality와 기본 성능값을 수정합니다. 기본 좌표계는 센서 로컬입니다.

숫자는 Enter/포커스 해제 또는 SpinBox 드래그가 끝날 때 한 번 적용됩니다. SensorId commit, profile/quality 버튼과 고급 checkbox는 유효성 검사 후 현재 PIE Actor에 즉시 적용됩니다. 범위 오류, 중복 SensorId, NaN Transform은 거부하고 마지막 runtime 값을 다시 표시합니다.

```text
센서 조작 시작                 기즈모와 키보드 입력 활성화
모드: 이동/회전                3축 화살표와 회전 링 전환
좌표: 로컬/월드                조작 기준 전환
PIE 시작값으로 되돌리기        PIE 시작 시 맵 값으로 복구
SensorTestMap에 저장 예약      현재 값을 저장 대기열에 추가
```

키보드는 `W/S` 전후, `A/D` 좌우, `Q/E` 아래/위, 방향키 Pitch/Yaw, `Z/C` Roll입니다. `Shift`는 5배, `Ctrl`은 0.2배이며 `Esc`는 조작 모드를 종료합니다. 텍스트 입력칸에 포커스가 있을 때는 조작 키를 무시합니다.

단축키 설명은 `단축키 도움말 펼치기`에서 확인합니다. `전체 UI 초기화`는 세 패널의 위치·접힘 상태와 Monitor 표시 옵션을 기본값으로 되돌리지만 센서 값과 맵 저장 대기열에는 영향을 주지 않습니다.

`고급 설정 펼치기`에는 Camera FOV/JPEG/CaptureMode, LiDAR sample/channel/FOV/수직 각도, server payload와 preview 정책, multi-hit/max hits, 자동 CSV/JSONL/PCD export, 외부 source 시작/중지를 노출합니다. HTTP endpoint, semantic rule 배열과 외부 LAZ 실행 파일은 런타임 패널에서 편집하지 않습니다.

각 설정 행의 `ⓘ`를 누르면 아래 도움말 카드에 단위, 성능 영향, 권장값과 주의사항이 표시됩니다.

| 항목 | 의미와 성능 영향 | 권장값 |
|---|---|---|
| 시뮬레이션 품질 | 해상도·광선 수·명목 주기를 함께 적용하는 preset | 조작은 실시간 미리보기, 최종 검증은 FullSpec |
| 카메라 해상도 | 가로×세로 픽셀 수에 비례해 렌더/readback/압축량 증가 | FullSpec 1280×720 |
| 카메라 캡처 주기 | 값이 작을수록 초당 캡처 횟수 증가 | FullSpec 0.033초(30Hz) |
| 카메라 FOV | 수평 시야각. 같은 해상도에서는 주로 화면 밀도 변화 | D455 87° |
| JPEG 품질 | 높을수록 압축 CPU와 Payload 크기 증가 | FullSpec 80 |
| 출력 모드 | 미리보기만은 readback 생략, Payload만은 JSON 생성, Payload 및 출력은 전송까지 수행 | 보기만 할 때 `미리보기만` |
| LiDAR 수평 샘플×수직 채널 | 스캔당 광선 수이며 두 값의 곱에 trace 비용이 비례 | FullSpec 360×60 |
| LiDAR 스캔 주기 | 값이 작을수록 광선/s 증가 | FullSpec 0.1초(10Hz) |
| 최대 거리 | 광선의 trace 거리. 복잡한 장면에서는 비용 증가 가능 | 일반 40m, 장비 cutoff 100m |
| 수평 FOV·수직 최소/최대 | 측정 각도 범위. 광선 수가 같으면 주로 각도 밀도 변화 | 360°, -7°~52° |
| 서버 Payload 간격 | N번째 점마다 서버 JSON에 포함 | 정밀 1, 실시간 2~8 |
| 서버 최대 점 수 | Payload 상한. 0은 제한 없음 | 수신 서버 한도에 맞춤 |
| 미검출점 포함 | miss 광선도 최대 거리 점으로 전송 | 일반적으로 끔 |
| 미리보기 간격·최대 점 수 | 화면 렌더링 전용이며 서버 Payload에는 영향 없음 | FullSpec stride 6, 최대 5000 |
| 미리보기 검출점만 | miss 점을 화면에서 제외해 ISM 부하 감소 | 켬 |
| MultiHit·광선당 최대 검출 수 | 한 광선의 복수 충돌을 모아 trace/메모리/Payload 증가 | 기본 끔, 최대 3 |
| 스캔별 자동 내보내기 | 매 스캔 직렬화·디스크 기록 | FullSpec에서는 끄고 수동 export |

`현재 예상 부하`는 Camera MP/s, LiDAR 광선/스캔과 광선/s, Payload/Preview 최대 점 수를 낮음·보통·높음으로 표시합니다. 자동 성능 단계, 활성 Camera/LiDAR 수, 평균 FPS·1% low·p95 frame time, 스케줄러 작업 시간, 작업 대기와 생략 수도 같은 카드와 Monitor `상세 진단`에 표시됩니다.

### FullSpec 실행 정책

자동 시작 경로는 공용 스케줄러가 센서를 round-robin으로 처리합니다. Camera 2대+LiDAR 2대 이하는 60 FPS 단계(프레임당 LiDAR trace 4ms), 각 종류 4대 이하는 30 FPS 단계(8ms), 그 이상은 30 FPS 최선 실행입니다. 엔진 FPS를 강제하지 않고 센서용 game-thread 예산만 제어합니다.

LiDAR 360×60 스캔은 128~1024 ray chunk로 나뉘며 시작 Transform과 설정을 완료 때까지 고정합니다. 카메라는 `FRHIGPUTextureReadback`으로 비동기 회수하고 JPEG/Base64/JSON을 worker thread에서 처리합니다. 센서별 acquisition/readback과 파생 작업은 각각 하나만 허용하므로 처리보다 입력이 빠르면 오래된 파생 프레임을 쌓지 않고 최신 프레임을 우선하며 생략 수를 올립니다.

카메라 FullSpec 설정은 30Hz를 유지하지만 여러 카메라의 실제 GPU 장면 캡처는 전체 15Hz 상한으로 공정 분배하고, Monitor에서 선택한 카메라는 격회 우선합니다. 따라서 실측 카메라 Hz가 30보다 낮은 것은 FPS 보호 동작이며 Payload 규격 변경이 아닙니다. 선택 LiDAR의 texture/ISM preview도 우선합니다. 비선택 LiDAR는 같은 순서로 측정하고 Payload를 만들되 화면 preview 파생 작업만 생략하며, 선택되는 즉시 다음 완료 스캔부터 다시 표시합니다. 수동 `Capture Once`는 이 backpressure로 생략되지 않습니다.

FullSpec 자체 규격은 Camera 1280×720·30Hz, LiDAR 21,600 rays·10Hz를 유지합니다. MultiHit와 스캔별 자동 내보내기는 추가 부하로 분류되므로 완료 주기를 보장하지 않습니다.

`선택 센서 투사 범위 표시`는 Camera frustum 또는 LiDAR FOV/최대 거리 대표 광선을 표시합니다. 선택 센서 하나만 표시하며 LiDAR는 최대 64개 광선으로 제한됩니다. 이 토글은 맵에 저장되지 않고 legacy `bDrawDebugRays`도 켜지 않습니다.

`SensorTestMap에 저장 예약` 후 PIE를 종료하면 Editor 모듈이 `SensorTestPersistent_PrimaryCamera` 또는 `SensorTestPersistent_PrimaryLidar` 태그로 원본 Actor를 찾아 `/Game/MA0T10/Maps/SensorTestMap`을 저장합니다. 다른 맵이 열려 있거나 태그가 중복/누락되면 저장하지 않고 Editor 알림을 표시합니다. 이 기능은 packaged build에서는 동작하지 않습니다.

### Monitor와 LiDAR 표시 방식

Monitor는 선택 센서, 프레임, 주기, 해상도 또는 측정점/검출점을 기본 요약으로 표시합니다. Payload·Preview·Slab·전송·배포 진단은 `상세 진단 펼치기`에 있습니다.

| 표시 방식 | 화면에서 보이는 의미 |
|---|---|
| 거리 색상 | 자홍/주황=가까움, 노랑=중간, 청록/파랑=멀음, 검정=미검출 |
| 검출 마스크 | 흰색=검출, 검정=미검출 |
| 의미 분류 색상 | `SemanticLabel`에 연결된 분류 색상, 미분류는 기본 회색 |
| 거리 회색조 | 가까울수록 밝고 멀수록 어두움 |

`적응형 거리`는 현재 프레임에서 검출된 최소·최대 거리로 색상 범위를 다시 잡습니다. `깊이 경계`는 인접 점의 거리 차이가 큰 곳을 흰색 윤곽으로 표시하고, `격자`는 스캔 행·열 기준선을 표시합니다. 이 세 오버레이는 드롭다운 표시 방식과 별도로 켜고 끌 수 있습니다.

다음 UI 상태는 `Saved/SaveGames/MA0T10_VirtualSensorUI_v1.sav`에 저장됩니다.

```text
Monitor / Settings / CaptureExport 패널 위치와 접힘 상태
LiDAR 표시 방식, 적응형 거리, 깊이 경계, 격자
Monitor 상세 진단과 Settings 단축키 도움말 펼침 상태
```

SaveGame이 없거나 손상되었거나 버전이 맞지 않으면 기본 배치와 기본 표시 옵션을 사용합니다. 이 파일은 사용자 로컬 UI 환경이며 저장소에 커밋하지 않습니다.

### Capture & Export 저장 위치

Capture & Export 패널은 작업 결과와 절대 경로, 파일 크기를 최근 8개까지 보여주며 저장 폴더를 Explorer로 열거나 `최근 저장 경로 복사`로 클립보드에 복사할 수 있습니다. 최근 파일이 삭제됐거나 경로가 없으면 복사하지 않고 한글 오류를 표시합니다.

```text
Server payload       Saved/SensorCaptures/<SensorId>/ServerPayload
Point cloud          Saved/SensorCaptures/<SensorId>/PointCloud
Timed camera         Saved/SensorCaptures/LocalTimedCapture/<UTC>/Camera
Timed LiDAR          Saved/SensorCaptures/LocalTimedCapture/<UTC>/Lidar
Recorder             Saved/SensorRecordings
Transport SaveToFile Saved/SensorCaptures/<SensorId>
```

Point cloud 형식 버튼은 CSV → JSONL → PCD → LAS → LAZ 순으로 전환됩니다. 외부 compressor를 설정하지 않은 LAZ는 기존 정책대로 LAS-compatible source만 생성하고 경고 상태를 표시합니다.

## 4. WBP 확인과 Designer 확장

### 4.1 자동 생성된 Widget Blueprint 확인

1. Content Browser에서 `Content/MA0T10/UI` 폴더를 엽니다.
2. 세 WBP의 `Class Settings > Parent Class`를 확인합니다.

```text
WBP_VirtualSensorMonitor       VirtualSensorMonitorWidget
WBP_VirtualSensorSettings      VirtualSensorSettingsWidget
WBP_VirtualSensorCaptureExport VirtualSensorCaptureExportWidget
```

3. 아무 Designer widget도 추가하지 않은 상태에서는 각 부모의 C++ native fallback UI가 자동으로 표시됩니다.
4. 자체 모니터 디자인을 만들려면 아래 hierarchy와 정확한 변수 이름을 추가한 뒤 Compile/Save합니다. Settings/Capture 패널은 현재 native UI를 기준으로 사용하고, Designer 확장 시에도 parent class와 HostActor binding을 유지합니다.

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
```

Monitor에는 보기 기능만 둡니다. 캡처·저장은 `WBP_VirtualSensorCaptureExport`, 센서 값·preview 정책·외부 source 제어는 `WBP_VirtualSensorSettings`가 담당합니다.

권장 크기:

```text
ViewImage.MinDesiredWidth   640
ViewImage.MinDesiredHeight  360
StatusText.AutoWrapText     true
```

### 4.3 Optional Bindings와 Is Variable

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
```

전부 optional이므로 일부가 없어도 크래시가 나면 안 됩니다. 과거 Capture/Preview/RealSource optional binding과 Blueprint 함수는 기존 사용자 WBP 호환성을 위해 C++에 남아 있지만 새 Monitor Designer에는 추가하지 않습니다.

운영용 binary WBP를 Designer에서 수정하기 전후에는 로컬 backup/hash evidence 도구를 사용할 수 있습니다. 두 명령은 asset을 자동 stage하지 않습니다.

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\prepare_monitor_wbp_editor_review.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_post_edit_hash_report.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "."
```

### 4.4 버튼 OnClicked Graph를 만들지 않는 이유

정확한 이름의 Button은 `UVirtualSensorMonitorWidget::NativeConstruct()`에서 C++ handler에 자동 연결됩니다.

보기 기능 예:

```text
PointCloudOnlyButton    -> SensorManager.TogglePointCloudOnlyView
ToggleButton            -> ToggleView
NextCameraButton        -> SelectNextCamera
NextLidarButton         -> SelectNextLidar
LidarViewModeButton     -> CycleLidarViewMode
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

반환 타입은 `FVirtualSensorMonitorDisplayData`입니다. 개별 Blueprint pure getter가 필요하면 `GetSelectedSensorIdText`, `GetFrameSummaryText`, `GetServerPayloadSummaryText`, `GetPreviewPolicySummaryText`, `GetSlabAnalysisSummaryText`, `GetLazExportSummaryText`, `GetTransportWarningText`를 사용할 수 있습니다.

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

1. `카메라/LiDAR 전환` 또는 `다음 카메라`를 눌러 Camera view로 전환합니다.
2. Capture & Export 패널에서 `선택 센서 1회 캡처`를 누릅니다.
3. `FrameId`, resolution, capture mode, cached payload byte 수가 갱신되는지 확인합니다.
4. `ViewImage`에 render target 영상이 표시되는지 확인합니다.

### 5.3 가상 LiDAR

1. `카메라/LiDAR 전환` 또는 `다음 LiDAR`를 눌러 LiDAR view로 전환합니다.
2. Capture & Export 패널에서 `선택 센서 1회 캡처`를 누릅니다.
3. ray count, hit count, server payload point count, preview point count가 갱신되는지 확인합니다.
4. Settings의 고급 설정에서 미리보기 점 간격/최대 점 수를 바꾸고 server payload 정책은 유지되는지 확인합니다.
5. `미리보기에서 검출점만 표시`를 전환해 miss point 포함 여부가 바뀌는지 확인합니다.

### 5.4 위치 조작과 투사 범위

1. Settings에서 Camera 또는 LiDAR를 선택하고 `센서 조작 시작`을 누릅니다.
2. 이동 기즈모 축을 드래그하고 `W/A/S/D/Q/E`로 세밀하게 이동합니다.
3. `모드: 회전`으로 전환해 회전 링과 방향키, `Z/C`를 확인합니다.
4. `좌표: 로컬/월드`를 전환해 축 기준이 바뀌는지 확인합니다.
5. `선택 센서 투사 범위 표시`를 켜고 Camera frustum 또는 LiDAR 범위가 위치·FOV·거리에 즉시 맞춰지는지 확인합니다.
6. 잘못된 값을 입력했을 때 Actor가 변하지 않고 `적용 실패` 상태가 한글로 표시되는지 확인합니다.

### 5.5 Point Cloud Only

1. `포인트 클라우드 전용` 버튼을 누릅니다.
2. 일반 타깃 mesh가 숨겨지고 `KeepInPointCloudOnly` 태그가 있는 Floor는 남는지 확인합니다.
3. LiDAR point preview는 계속 표시되는지 확인합니다.
4. hit count가 0으로 떨어지지 않는지 확인합니다. 이 모드는 visibility만 숨기고 collision은 유지해야 합니다.
5. 버튼을 다시 눌러 원래 visibility가 복원되는지 확인합니다.

### 5.6 Slab 분석

1. 직접 배치한 mesh의 Actor Tag가 `Slab`인지 확인합니다.
2. LiDAR capture 후 slab point count가 12 이상인지 확인합니다.
3. estimated yaw, reference yaw, angle deviation, confidence가 Status에 표시되는지 확인합니다.
4. 해당 mesh의 Yaw를 10도로 바꾸고 다시 capture하여 angle deviation이 변하는지 확인합니다.

### 5.7 Export

`서버 Payload 내보내기`는 현재 선택된 센서의 서버 JSON을 다음 위치에 저장합니다.

```text
Saved/SensorCaptures/<SensorId>/ServerPayload
```

`포인트 클라우드 내보내기`는 LiDAR point cloud 파일을 저장합니다. LAZ는 외부 compressor가 연결되지 않으면 실제 압축 LAZ가 아니라 LAS-compatible source 경고 경로입니다.

### 5.8 외부 Sensor Source

현재 Livox/RealSense/ROS2 class는 adapter 또는 placeholder 단계입니다. Settings의 고급 영역에서 외부 source를 시작했을 때 not-implemented 또는 deployment-evidence-pending 상태가 나오는 것은 현재 제한을 정확히 표시하는 동작입니다. 실제 SDK가 연결됐다는 증거로 해석하면 안 됩니다.

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
MA0T10.SensorControl     Transform/설정 검증, panel clamp, map apply queue
MA0T10.SensorPerformance 자동 단계, FullSpec 규격, 부하 계산, 설정 도움말
MA0T10.SensorDebug       투사 디버그 광선 예산과 기본값
MA0T10.SensorExport      저장 경로와 export 결과 계약
MA0T10.SensorTransport   HTTP loopback/backpressure 계약
MA0T10.RealSensorSource  placeholder/live JSON source 경로
```

전체 스크립트:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1" -SkipBuild
```

실제 GPU/RHI 성능 보고서:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_fullspec_performance_evidence.ps1" -CameraCount 2 -LidarCount 2
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_fullspec_performance_evidence.ps1" -CameraCount 4 -LidarCount 4
```

이 스크립트는 `-NullRHI`를 사용하지 않습니다. command-line benchmark sensor는 runtime에서만 만들어지고 맵에 저장되지 않습니다. 결과는 `Saved/Reports/fullspec_<N>c_<N>l.json`과 `.md`에 기록되며 평균 FPS, 보수적인 1% low, 최악 rolling p95, 센서별 완료 Hz·처리 시간·생략 수와 대기 작업 상한을 포함합니다.

HTTP transport 계약의 대표 automation은 `MA0T10.SensorTransport.HttpPostLoopbackAcceptance`입니다.

## 7. 현재 제한과 후속 작업

1. 자동 생성 WBP는 즉시 테스트 가능한 native-backed UI입니다. 운영용 디자인이 필요하면 4장의 hierarchy/변수 이름 규칙에 맞춰 Designer를 꾸미고 1920×1080과 1280×720 PIE로 검수합니다.
2. 실제 D455 depth calibration/stream이 아니라 SceneCapture preview를 사용합니다.
3. DTCore의 일부 `USTRUCT` field는 기본 초기화 누락 오류를 출력할 수 있습니다. DTCore 수정은 별도 저장소에서 처리한 후 submodule pin을 명시적으로 갱신해야 합니다.
4. 실제 Livox, RealSense, ROS2 입력은 SDK/bridge 연결과 장비 기반 acceptance evidence가 별도로 필요합니다.
5. 실제 LAZ 압축과 고밀도 GPU/Niagara point renderer는 외부 도구 또는 후속 backend 검증이 필요합니다.
