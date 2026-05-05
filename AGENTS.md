# AGENTS.md - LiDAR Point Cloud Only View 작업 가이드

> 브랜치: `feature/lidar-point-cloud-only-view`  
> 기준 브랜치: `feature/real-sensor-performance-optimizations`  
> 목적: 실제 환경 메시를 숨긴 상태에서 LiDAR가 측정한 point cloud만 볼 수 있는 모드 제공

이 문서는 `feature/lidar-point-cloud-only-view` 브랜치의 구현 의도, 위젯 블루프린트 설정 방법, 에디터 실행 방법, 향후 수정 시 주의점을 정리한다.

## 1. 핵심 기능 요약

이 브랜치는 기존 가상 센서 플랫폼에 `PointCloudOnly` 모드를 추가한다.

동작 개념은 다음과 같다.

```text
일반 모드:
- 실제 환경 메시가 보임
- Camera View / LiDAR Heatmap View를 위젯에서 토글

PointCloudOnly 모드:
- 실제 환경 PrimitiveComponent를 화면에서 숨김
- collision은 유지
- LiDAR는 숨겨진 실제 환경을 계속 LineTrace로 측정
- 측정된 hit point만 InstancedStaticMesh 기반 point cloud로 월드에 표시
```

중요한 점은 실제 환경을 `SetVisibility(false)`와 `SetHiddenInGame(true)`로 숨길 뿐, collision을 끄지 않는다는 것이다. 그래서 LiDAR 측정은 계속 가능하다.

## 2. 수정된 주요 파일

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h
Source/m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.cpp
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorManager.cpp
Source/m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.h
Source/m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.cpp
```

## 3. 추가된 Manager ViewMode

`EVirtualSensorViewMode`에 아래 값이 추가되었다.

```cpp
PointCloudOnly
```

Manager에서 사용할 수 있는 주요 함수는 다음과 같다.

```cpp
SetPointCloudOnlyMode(bool bEnabled)
TogglePointCloudOnlyView()
IsPointCloudOnlyModeEnabled()
```

에디터/블루프린트에서는 보통 다음 방식으로 켜고 끈다.

```text
Get Actor of Class: AVirtualSensorManager
→ TogglePointCloudOnlyView
```

또는 명시적으로 다음을 호출한다.

```text
SetPointCloudOnlyMode(true)
SetPointCloudOnlyMode(false)
```

## 4. LiDAR Point Cloud Preview 설정

`UVirtualLidarSensorComp`에 point cloud preview 옵션이 추가되었다.

```cpp
bPointCloudPreviewEnabled
bPointCloudPreviewHitOnly
PointCloudPreviewStride
MaxPointCloudPreviewInstances
PointCloudPreviewPointScale
PointCloudPreviewMesh
```

권장 기본값은 다음과 같다.

```text
bPointCloudPreviewEnabled = false
bPointCloudPreviewHitOnly = true
PointCloudPreviewStride = 1
MaxPointCloudPreviewInstances = 5000
PointCloudPreviewPointScale = 0.035
PointCloudPreviewMesh = None
```

`PointCloudPreviewMesh`를 지정하지 않으면 Engine 기본 Sphere mesh를 먼저 시도하고, 실패하면 Cube mesh를 fallback으로 사용한다.

성능이 무거우면 아래 순서로 낮춘다.

```text
1. PointCloudPreviewStride = 2 또는 4
2. MaxPointCloudPreviewInstances = 1000 ~ 3000
3. SimulationQuality = Debug
4. bUseMultiHit = false
5. bDrawDebugRays = false
```

점이 너무 크면 다음 값을 낮춘다.

```text
PointCloudPreviewPointScale = 0.01 ~ 0.02
```

점이 너무 작으면 다음 값을 높인다.

```text
PointCloudPreviewPointScale = 0.05 이상
```

## 5. Manager PointCloudOnly 설정

`AVirtualSensorManager`에 아래 옵션이 추가되었다.

```cpp
bPointCloudOnlyHideWorld
bPointCloudOnlyAutoSelectLidarView
PointCloudOnlyKeepActorTags
```

권장 설정은 다음과 같다.

```text
bPointCloudOnlyHideWorld = true
bPointCloudOnlyAutoSelectLidarView = true
```

특정 Actor를 PointCloudOnly 모드에서도 계속 보이게 하고 싶으면 해당 Actor에 tag를 추가하고, Manager의 `PointCloudOnlyKeepActorTags`에 같은 tag를 넣는다.

예시:

```text
Actor Tag = KeepInPointCloudOnly
PointCloudOnlyKeepActorTags = [KeepInPointCloudOnly]
```

주로 유지할 만한 Actor:

```text
- 사용자 안내용 UI anchor
- sensor rig debug actor
- 기준 좌표계 actor
- 조명 대신 시각적으로 꼭 보여야 하는 helper actor
```

## 6. Widget Blueprint 설정 방법

권장 위젯 이름:

```text
WBP_VirtualSensorMonitor
```

부모 클래스:

```text
UVirtualSensorMonitorWidget
```

만약 이미 만든 Blueprint Widget이 있다면 `Class Settings`에서 Parent Class가 `VirtualSensorMonitorWidget`인지 확인한다.

## 7. Widget Designer에 배치할 요소

아래 위젯들은 C++에서 `BindWidgetOptional`로 잡기 때문에 반드시 전부 있을 필요는 없다. 하지만 기능을 모두 쓰려면 아래 이름으로 배치하고 `Is Variable`을 체크한다.

### 필수에 가까운 기본 요소

```text
ViewImage           : Image
TitleText           : TextBlock
StatusText          : TextBlock
ToggleButton        : Button
ToggleButtonText    : TextBlock
```

### 선택 기능 버튼

```text
NextCameraButton      : Button
NextLidarButton       : Button
PointCloudOnlyButton  : Button
```

각 Button 안에는 TextBlock을 자식으로 넣어도 된다. 단, C++에서 직접 텍스트를 바꾸는 것은 `ToggleButtonText`만 현재 연결되어 있다. `PointCloudOnlyButton`의 표시 텍스트는 Designer에서 직접 설정하면 된다.

권장 버튼 텍스트:

```text
ToggleButton          = Camera / LiDAR 전환
NextCameraButton      = Next Camera
NextLidarButton       = Next LiDAR
PointCloudOnlyButton  = Point Cloud Only
```

## 8. Widget Hierarchy 예시

간단한 구성 예시는 다음과 같다.

```text
CanvasPanel
└─ Border 또는 VerticalBox
   ├─ TitleText                TextBlock
   ├─ ViewImage                Image
   ├─ StatusText               TextBlock
   └─ HorizontalBox
      ├─ ToggleButton          Button
      │  └─ ToggleButtonText   TextBlock
      ├─ NextCameraButton      Button
      │  └─ TextBlock          "Next Camera"
      ├─ NextLidarButton       Button
      │  └─ TextBlock          "Next LiDAR"
      └─ PointCloudOnlyButton  Button
         └─ TextBlock          "Point Cloud Only"
```

`ViewImage`는 권장 크기를 다음처럼 잡는다.

```text
Width = 640
Height = 360
```

또는 화면에 맞게 Stretch해도 된다.

## 9. Widget 변수 체크리스트

Designer에서 아래 항목을 선택한 뒤 Details 패널에서 `Is Variable`을 반드시 체크한다.

```text
ViewImage
TitleText
StatusText
ToggleButton
ToggleButtonText
NextCameraButton
NextLidarButton
PointCloudOnlyButton
```

이름이 정확히 일치해야 C++에서 자동 바인딩된다.

나쁜 예:

```text
PointCloudButton
PointCloudOnlyBtn
BtnPointCloudOnly
```

좋은 예:

```text
PointCloudOnlyButton
```

## 10. Level Blueprint 연결

Level Blueprint의 BeginPlay에서 다음 순서로 연결한다.

```text
Event BeginPlay
→ Create Widget
   Class = WBP_VirtualSensorMonitor
→ Add to Viewport
→ Get Actor of Class
   Actor Class = AVirtualSensorManager
→ BindSensorManager
   Target = 생성한 WBP_VirtualSensorMonitor
   InSensorManager = Get Actor of Class 결과
```

중요한 점:

```text
Camera/LiDAR를 위젯에 직접 BindVirtualCamera / BindVirtualLidar로 연결하지 말고,
가능하면 BindSensorManager만 호출한다.
```

Manager가 선택된 Camera/LiDAR를 위젯에 주입하고, Next Camera / Next LiDAR / PointCloudOnly 모드를 관리한다.

## 11. PointCloudOnlyButton 동작

`PointCloudOnlyButton`은 C++에서 자동으로 다음 함수에 연결된다.

```cpp
HandlePointCloudOnlyButtonClicked()
```

내부 동작:

```text
SensorManager가 있으면
→ SensorManager->TogglePointCloudOnlyView()
```

따라서 Widget Blueprint에서 별도 OnClicked 그래프를 만들 필요는 없다. 이름만 정확히 맞고 `Is Variable`만 체크되어 있으면 된다.

수동으로 Blueprint에서 직접 연결하고 싶다면 다음처럼 연결할 수 있다.

```text
PointCloudOnlyButton.OnClicked
→ Get Actor of Class: AVirtualSensorManager
→ TogglePointCloudOnlyView
```

하지만 C++ 자동 바인딩과 중복 호출될 수 있으므로, 일반적으로는 수동 연결하지 않는 것을 권장한다.

## 12. 실행 중 기대 동작

### 일반 Camera / LiDAR 토글

```text
ToggleButton 클릭
→ Camera View / LiDAR Heatmap View 전환
```

### Point Cloud Only 모드 켜기

```text
PointCloudOnlyButton 클릭
→ 실제 환경 메시 숨김
→ 선택된 LiDAR의 bPointCloudPreviewEnabled = true
→ LiDAR hit point가 월드에 작은 점으로 표시
→ 위젯은 LiDAR View 상태로 전환
```

### Point Cloud Only 모드 끄기

```text
PointCloudOnlyButton 다시 클릭
→ 숨겨진 환경 메시 visibility 복원
→ LiDAR point cloud preview 제거
→ 이전 Camera/LiDAR ViewMode로 복귀
```

## 13. 에디터 배치 체크리스트

레벨에 다음 Actor가 있어야 한다.

```text
AVirtualSensorManager
AVirtualSensorAct
AVirtualCameraAct
```

`AVirtualSensorAct`의 `LidarSensorComp` 권장 설정:

```text
DeviceProfile = LivoxMid360S
SimulationQuality = RealTimePreview
bAutoStartScan = true
bAutoRegisterToManager = true
bPointCloudPreviewHitOnly = true
PointCloudPreviewStride = 1
MaxPointCloudPreviewInstances = 5000
PointCloudPreviewPointScale = 0.035
bUseMultiHit = false
bDrawDebugRays = false
```

`AVirtualSensorManager` 권장 설정:

```text
bDiscoverOnBeginPlay = true
bStartSensorsOnBeginPlay = true
bPointCloudOnlyHideWorld = true
bPointCloudOnlyAutoSelectLidarView = true
SharedSensorTransport.TransportMode = LogOnly
```

## 14. 빌드/패키징 주의

PointCloudOnly 모드는 런타임에 `UInstancedStaticMeshComponent`를 생성해서 point cloud를 표시한다.

주의할 점:

```text
- Point 개수가 많으면 렌더링 부하가 증가한다.
- FullSpec + PointCloudOnly + MultiHit 조합은 무거울 수 있다.
- 패키징 smoke test는 RealTimePreview / LogOnly / MultiHit false로 시작한다.
```

패키징 첫 테스트 권장값:

```text
LiDAR SimulationQuality = RealTimePreview
PointCloudPreviewStride = 2
MaxPointCloudPreviewInstances = 3000
SharedSensorTransport.TransportMode = LogOnly
bUseMultiHit = false
bExportCsvOnScan = false
bExportJsonLinesOnScan = false
bExportPcdOnScan = false
```

## 15. 알려진 한계

현재 PointCloudOnly 모드는 “측정된 hit point를 작은 mesh instance로 표시”하는 방식이다.

아직 구현되지 않은 부분:

```text
- point color를 거리/intensity에 따라 다르게 표시
- point cloud 전용 material 자동 생성
- GPU point rendering
- Niagara 기반 point cloud rendering
- replay payload에서 point cloud 복원 표시
- 실제 Livox packet 기반 point cloud 입력 표시
```

## 16. 향후 개선 권장

다음 단계로는 아래 작업을 추천한다.

```text
1. PointCloudPreview 전용 material 추가
2. 거리 기반 색상 또는 intensity 기반 색상 적용
3. PointCloudOnly 모드용 전용 카메라/관찰자 pawn 추가
4. Replay 데이터에서 point cloud를 복원해 동일 모드로 표시
5. 실제 Livox SDK / ROS2 point cloud source와 동일 표시 경로 공유
6. UI에서 point size / stride / max instances를 조절하는 슬라이더 추가
7. FullSpec 사용 시 UI warning 표시
```

## 17. 수정 시 원칙

- PointCloudOnly는 visibility만 숨기고 collision은 유지해야 한다.
- LiDAR 측정 대상 actor를 삭제하거나 collision off하지 않는다.
- 선택된 LiDAR만 point cloud preview를 켜는 구조를 유지한다.
- 여러 LiDAR가 있을 때 NextLidarButton과 PointCloudOnly 표시 대상이 함께 바뀌어야 한다.
- Widget 요소는 `BindWidgetOptional` 구조를 유지한다.
- 무거운 기능은 기본값으로 켜지 않게 한다.
- 에디터가 느려질 수 있는 설정은 UI/문서에서 명확히 경고한다.
