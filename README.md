# UE-DT-Project

Unreal Engine 5.3 기반 Digital Twin 가상 센서 프로젝트입니다. Camera와 LiDAR를 맵에 배치해 실시간 미리보기, 설정 변경, 디버그 표시, Payload 전송, 녹화와 point cloud 내보내기를 시험할 수 있습니다.

## 빠른 시작

1. Unreal Editor와 Live Coding을 모두 종료한 상태에서 `ma0t10_dtEditor`를 빌드합니다.
2. `Scripts/setup_sensor_refactor_test_map.py`를 실행해 `/Game/MA0T10/Maps/Tests/SensorRefactorTestMap`을 생성합니다.
3. 회귀 검증 후 `Scripts/setup_sensor_test_map.py`로 `/Game/MA0T10/Maps/SensorTestMap`을 갱신합니다.
4. 맵을 열고 PIE를 실행합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" `
  ma0t10_dtEditor Win64 Development `
  "-Project=$PWD\ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE

& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\ma0t10_dt.uproject" `
  -ExecutePythonScript="$PWD\Scripts\setup_sensor_refactor_test_map.py" `
  -Unattended -NoSplash -NoSound
```

자세한 에디터 조작과 WBP 설정은 [센서 테스트 가이드](docs/sensor_test_map_setup.ko.md)를 참고하세요.

## Sensor V2 구조

- `AVirtualSensorActorBase`: 센서 공통 실행, 상태, 설정, Preview, 외부 프레임 API
- `AVirtualCameraSensorActor`: `UVirtualCameraCaptureComponent`, 상태 없는 `FVirtualCameraPayloadCodec`, 공통 출력 조립
- `AVirtualLidarSensorActor`: Scan, Analysis, Visualization, Export Component 조립
- `AVirtualSensorCoordinator`: 센서 검색, 선택, 시작·중지와 일괄 측정
- `UVirtualSensorSchedulerSubsystem`: Camera/LiDAR 자동 작업을 round-robin으로 배분
- `UVirtualSensorOutputComponent`: Transport와 Recorder로 출력하고 중복 frame을 차단
- `URealSensorSourceComponent`: Replay·실장비 adapter가 Actor의 `SubmitExternalFrame`으로 프레임 전달

Payload 계약은 `virtual-camera.v1`, `virtual-lidar.v1`을 유지합니다. 저장 기본 경로는 `Saved/SensorCaptures`, 녹화 경로는 `Saved/SensorRecordings`입니다.

## V2 UI

| WBP | Native parent | 역할 |
|---|---|---|
| `WBP_VirtualSensorMonitorPanel` | `UVirtualSensorMonitorPanelWidget` | Camera/LiDAR 보기, 센서 전환, LiDAR 표시 방식과 진단 |
| `WBP_VirtualSensorSettingsPanel` | `UVirtualSensorSettingsPanelWidget` | Transform, 설정, 기즈모, 투사 범위, 맵 저장 예약 |
| `WBP_VirtualSensorCaptureExportPanel` | `UVirtualSensorCaptureExportPanelWidget` | 1회 캡처, Payload·point cloud 내보내기, timed capture |

`AVirtualSensorUiHostActor`의 `UVirtualSensorPanelHostComponent`는 Main Widget의 `AddWidgetPanel`이 유효한 Canvas이면 패널을 그 아래에 배치합니다. Main이 없으면 같은 패널을 Viewport에 배치하고, Main 교체를 감지하면 중복 생성 없이 재호스팅합니다.

접기 버튼은 본문 visibility만 바꾸지 않고 실제 slot 높이를 약 48px로 줄입니다. 펼칠 때 이전 크기를 복원하며 DPI와 화면 크기에 맞춰 위치를 보정합니다. 위치·접힘·LiDAR 표시 상태는 `Saved/SaveGames/MA0T10_VirtualSensorUI_v2.sav`에 사용자별로 저장됩니다. 기존 v1 파일은 처음 로드할 때 자동 변환됩니다.

## FullSpec과 최신 프레임 정책

- Camera FullSpec: 1280×720, 30Hz
- LiDAR FullSpec: 360×60, 10Hz
- Camera 2대 + LiDAR 2대 이하: 센서 작업 60 FPS 예산
- 각 종류 4대 이하: 센서 작업 30 FPS 예산
- 4대를 초과하는 종류가 있으면 30 FPS 최선 실행과 경고

자동 경로는 LiDAR trace를 chunk로 나누고 Camera readback·encode와 파생 출력을 비동기로 처리합니다. 센서별 acquisition/derived 작업은 쌓지 않고 최신 프레임을 우선하며 생략 수를 런타임 진단에 표시합니다. 수동 `Capture Once`는 생략하지 않습니다.

## LiDAR 표시 방식

| 방식 | 의미 |
|---|---|
| 거리 색상 | 가까운 점은 자홍·주황, 중간은 노랑, 먼 점은 청록·파랑 |
| 검출 마스크 | 검출 성공은 흰색, 미검출은 검정 |
| 의미 분류 색상 | Actor 태그·클래스로 계산한 `SemanticLabel` 규칙 색상 |
| 거리 회색조 | 가까운 점은 밝고 먼 점과 미검출은 어둡게 표시 |

적응형 거리, 깊이 경계, 격자는 색상 모드와 독립적인 오버레이입니다.

### V2 투영·색상 조합과 3D 포인트

LiDAR 모니터는 투영과 색상을 독립적으로 선택합니다.

- 투영: 거리 영상, 센서 로컬 XY 조감도, 전방 거리-높이 단면, 거리 영상+조감도 분할
- 색상: Turbo 거리, 색각 친화 Viridis 거리, 센서 상대 높이, SemanticLabel, 수직 채널/ring, MultiHit return 번호, 검출 마스크, 거리 회색조
- 월드 3D 포인트: 선택 LiDAR는 Niagara GPU sprite로 최대 21,600점을 표시합니다. Niagara 자산·SM5·업로드 경로를 사용할 수 없으면 저폴리곤 CPU ISM으로 자동 전환하고 모니터 진단에 이유를 표시합니다.
- 자산 재생성: `Scripts/setup_lidar_niagara_assets.ps1`

조감도와 거리-높이 단면은 이미지 위에서 직접 탐색할 수 있습니다. 좌클릭 드래그는 pan, 우클릭 드래그는 회전, 마우스 휠은 확대·축소이며 `보기 초기화`로 해당 투영의 기본 시점으로 돌아갑니다. 분할 모드에서는 아래쪽 조감도 영역만 탐색됩니다.

RangeImage 전용 오버레이인 적응형 거리·깊이 경계·격자는 TopDown/Elevation의 축·거리 원·높이 기준선과 별개입니다. 포인트 크기와 3D 표시 여부도 v2 UI SaveGame에 저장됩니다.

FullSpec 스케줄러는 선택 센서 우선순위를 측정 순서에 사용하지 않고 표시 갱신에만 사용합니다. Camera admission은 전체 12Hz 상한을 strict round-robin으로 공유하며, LiDAR trace는 60 FPS 단계 3ms·30 FPS 단계 5ms에서 시작해 rolling p95와 1% low가 나빠지면 0.5ms까지 줄입니다. 완료 point frame은 shared immutable snapshot으로 Payload·Visualization·Output에 전달합니다.

센서 Actor 계층은 `AInteractableActor → AVirtualSensorActorBase → AVirtualCameraSensorActor/AVirtualLidarSensorActor`이며 Camera·LiDAR 공용 Scheduler Subsystem은 `MA0T10/Core`에 위치합니다.

## 테스트

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\ma0t10_dt.uproject" `
  -ExecCmds="Automation RunTests MA0T10.SensorV2;Quit" `
  -Unattended -NoSplash -NoSound -NullRHI
```

주요 그룹은 `MA0T10.SensorV2.Architecture`, `MA0T10.SensorV2.UI`, 기존 Payload·Replay·Export·Transport·Recorder 회귀 테스트입니다. 실제 RHI 성능은 1920×1080 PIE에서 별도로 확인합니다.

구 타입 참조와 보호 경로 stage 여부는 `Scripts/validate_sensor_v2_refactor.ps1`로 검사합니다. WBP와 맵 생성 후에는 `-RequireAssets`를 함께 사용합니다.

## 주의사항

- `Plugins/DTCore`는 git submodule입니다. 이 프로젝트 작업에서 임의로 gitlink나 내부 소스를 변경하지 않습니다.
- `Config/Game.ini`는 로컬 override이므로 커밋하지 않습니다.
- 맵 생성 스크립트는 `SensorTestManaged` 태그 Actor만 교체합니다. 사용자가 배치한 비관리 Actor와 Mesh는 보존합니다.
- 구 클래스 이름은 `Config/DefaultEngine.ini`의 `CoreRedirects`로 로드를 지원합니다. 변경된 Blueprint 함수 노드는 수동 재연결이 필요할 수 있습니다.

마이그레이션 세부사항은 [Sensor V2 마이그레이션](docs/sensor_v2_migration.ko.md)을 참고하세요.
