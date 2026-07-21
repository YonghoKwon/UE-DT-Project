# UE-DT-Project

> 센서 캡처/내보내기 패널, 세 가지 실시간 Topic, 부하 제한 정책과 로컬 Artemis 검증 방법은 [docs/sensor_streaming.ko.md](docs/sensor_streaming.ko.md)를 참고하십시오.

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
- `AVirtualSensorExternalSourceHostActor`: 외부 입력 Source와 DTCore 기반 Camera/LiDAR/Point Cloud Topic 자체 수신 진단을 관리
- `UVirtualCameraStreamReceiverTC`, `UVirtualLidarStreamReceiverTC`, `UVirtualPointCloudStreamReceiverTC`: Topic별 Payload를 백그라운드 파싱하고 형식을 검증

Payload 계약은 `virtual-camera.v1`, `virtual-lidar.v1`을 유지합니다. 저장 기본 경로는 `Saved/SensorCaptures`, 녹화 경로는 `Saved/SensorRecordings`입니다.

## V2 UI

| WBP | Native parent | 역할 |
|---|---|---|
| `WBP_VirtualSensorMonitorPanel` | `UVirtualSensorMonitorPanelWidget` | Camera/LiDAR 보기, 센서 전환, LiDAR 표시 방식과 진단 |
| `WBP_VirtualSensorSettingsPanel` | `UVirtualSensorSettingsPanelWidget` | Transform, 설정, 기즈모, 투사 범위, 맵 저장 예약 |
| `WBP_VirtualSensorCaptureExportPanel` | `UVirtualSensorCaptureExportPanelWidget` | 1회 캡처, Payload·point cloud 내보내기, timed capture, 송신·자체 수신 진단 |

`AVirtualSensorUiHostActor`의 `UVirtualSensorPanelHostComponent`는 Main Widget의 `AddWidgetPanel`이 유효한 Canvas이면 패널을 그 아래에 배치합니다. Main이 없으면 같은 패널을 Viewport에 배치하고, Main 교체를 감지하면 중복 생성 없이 재호스팅합니다.

접기 버튼은 본문 visibility만 바꾸지 않고 실제 slot 높이를 약 48px로 줄입니다. 펼칠 때 이전 크기를 복원하며 DPI와 화면 크기에 맞춰 위치를 보정합니다. Monitor 패널은 오른쪽 아래 `↘` 영역을 드래그해 가로·세로를 자유롭게 조절할 수 있고 `크기 초기화`로 현재 해상도의 기본 크기로 돌아갑니다. 위치·접힘·Monitor 확장 크기·LiDAR 표시 상태는 `Saved/SaveGames/MA0T10_VirtualSensorUI_v3.sav`에 사용자별로 저장됩니다. 기존 v1/v2 파일은 처음 로드할 때 자동 변환됩니다.

## FullSpec과 최신 프레임 정책

- Camera FullSpec: 1280×720, 30Hz
- LiDAR FullSpec: 360×60, 10Hz
- Camera 2대 + LiDAR 2대 이하: 센서 작업 60 FPS 예산
- 각 종류 4대 이하: 센서 작업 30 FPS 예산
- 4대를 초과하는 종류가 있으면 30 FPS 최선 실행과 경고

자동 경로는 LiDAR trace를 센서별 독립 chunk로 나누고 Camera readback·encode와 파생 출력을 비동기로 처리합니다. 센서별 acquisition/derived 작업은 쌓지 않고 최신 프레임을 우선하며 생략 수를 런타임 진단에 표시합니다. 수동 `Capture Once`는 생략하지 않습니다. FullSpec 센서를 기즈모로 조작하는 동안에는 설정값을 덮어쓰지 않고 Camera 640×360·최대 5Hz, LiDAR 120×24·최대 4Hz의 출력 없는 경량 요청만 사용합니다. 조작 종료 뒤 스케줄러가 coherent FullSpec 프레임을 다시 생성합니다.

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

- 투영: 거리 영상, 센서 로컬 XY 조감도, 월드 XY 조감도, 방사 거리-높이 프로파일, 전방 수직 슬라이스, 거리 영상+조감도 분할
- 색상: Turbo 거리, 색각 친화 Viridis 거리, 센서 상대 높이, SemanticLabel, 수직 채널/ring, MultiHit return 번호, 검출 마스크, 거리 회색조
- 월드 3D 포인트: 선택 LiDAR는 Niagara GPU sprite로 최대 21,600점을 표시합니다. Niagara 자산·SM5·업로드 경로를 사용할 수 없으면 저폴리곤 CPU ISM으로 자동 전환하고 모니터 진단에 이유를 표시합니다.
- 자산 재생성: `Scripts/setup_lidar_niagara_assets.ps1`

`방사 거리-높이 프로파일`의 X축은 `sqrt(local X² + local Y²)`, Y축은 센서 기준 Z입니다. 좌우 점을 같은 방사거리로 합치므로 바닥·천장·경사와 물체 높이 분석용이며 방향 판별용은 아닙니다. 방향이 필요한 경우 `전방 수직 슬라이스`를 사용합니다. 이 모드는 회전한 센서 로컬 X–Z 기준면 주변의 설정 두께 안에 있는 점만 보여 줍니다. 조감도와 슬라이스는 좌클릭 pan, 우클릭 회전, 휠 zoom을 지원합니다.

수직으로 설치한 LiDAR의 검출점을 공장 평면도처럼 보려면 `월드 XY 조감도`를 선택합니다. 이 모드는 센서의 Pitch/Roll/Yaw와 무관하게 가로축을 월드 Y, 세로축을 월드 X로 고정하고 검출점의 월드 XY 경계를 자동으로 맞춥니다. 반면 `센서 로컬 조감도`는 센서 로컬 XY를 사용하므로 센서를 아래로 90° 기울이면 바닥 평면이 아니라 센서 기준 단면처럼 보이는 것이 정상입니다. 자동 맞춤을 끄면 이미지 위 좌클릭 이동, 우클릭 회전, 휠 확대·축소로 원하는 구도를 유지할 수 있고 `보기 초기화`로 자동 맞춤 상태에 돌아갑니다.

첨부 예시와 같은 결과를 만드는 권장 순서는 다음과 같습니다.

1. LiDAR를 대상 위에 배치하고 광선이 대상과 교차하도록 Pitch와 수직 FOV를 설정합니다.
2. Monitor의 `LiDAR 표시 방식`에서 `월드 XY 조감도`를 선택합니다.
3. `검출점 자동 맞춤`을 켜고, 빈 최대거리 endpoint를 숨기려면 Settings의 `미리보기에서 검출점만 표시`를 켭니다.
4. 윤곽을 단색으로 보려면 `검출 마스크`, 거리나 높이를 구분하려면 Turbo/Viridis 또는 `센서 상대 높이`를 선택합니다. 월드 조감도에서 이 높이 모드는 월드 Z를 사용합니다.
5. 모니터 오른쪽 아래 resize grip으로 패널을 키운 뒤 필요하면 pan/rotate/zoom을 조정합니다.

투영 좌표계를 바꿔도 실제 측정 광선이나 FOV는 바뀌지 않습니다. 따라서 화면이 비어 있으면 먼저 상세 진단의 `검출점` 수와 디버그 투사 범위가 대상에 닿는지 확인해야 합니다.

RangeImage 전용 오버레이인 적응형 거리·깊이 경계·격자는 TopDown/Elevation의 축·거리 원·높이 기준선과 별개입니다. 포인트 크기, 3D 표시 여부와 월드 조감도 자동 맞춤은 v4 UI SaveGame에 저장됩니다.

FullSpec 스케줄러는 선택 센서 우선순위를 측정 순서에 사용하지 않고 표시 갱신에만 사용합니다. Camera admission은 전체 12Hz 상한을 strict round-robin으로 공유하며, LiDAR trace는 60 FPS 단계 5ms·30 FPS 단계 7ms 상한을 사용하고 rolling p95와 1% low가 나빠지면 2.5ms까지 줄입니다. 완료 point frame은 shared immutable snapshot으로 Payload·Visualization·Output에 전달합니다.

## 외부 Source와 서버 전송

`SensorRefactorTestMap`의 `SensorTest_ExternalSources`는 CSV/JSONL replay, buffered Camera/LiDAR JSON, LiDAR HTTP(`127.0.0.1:8082/ma0t10/lidar/live`)와 UDP 입력 Component를 기본 정지 상태로 제공합니다. Settings의 `선택 Source 1회 주입`은 프레임을 `SubmitExternalFrame`으로 전달할 뿐 외부 서버로 다시 보내지 않습니다. ROS2, Livox SDK, RealSense SDK adapter는 아직 구현되지 않은 확장 지점입니다.

외부 전송은 CaptureExport 패널에서만 실행합니다. CSV/JSONL/PCD/LAS/LAZ는 드롭다운으로 고르고, 완료 Payload 또는 최근 내보내기 파일을 HTTP POST나 Artemis STOMP/WS로 전송할 수 있습니다. HTTP 파일 전송은 raw `application/octet-stream`, STOMP 파일 전송은 Base64 envelope입니다. 비밀번호와 Bearer token은 세션 메모리에만 유지됩니다. STOMP receipt는 Broker 수락만 뜻하며 ACK Topic을 실제 소비자가 처리하기 전에는 소비자 처리 완료로 간주하지 않습니다. 개발용 Broker는 `Tools/Artemis/docker-compose.yml`을 사용합니다.

`SensorRefactorTestMap`에서는 `SensorTest_ExternalSources`가 DTCore WebSocket 연결을 재사용해 Camera, LiDAR, Point Cloud Topic을 자동 구독합니다. 기존 송신 Body에는 `MESSAGE_ID`가 없으므로 Topic이 세 전용 TC Handler를 직접 선택합니다. 수신기는 schema와 메타데이터를 매 프레임 검사하고 매 10번째 Camera/Point Cloud 프레임의 실제 Base64 바이트와 형식 서명까지 검증하며, 수신 데이터를 Sensor Actor에 재주입하거나 다시 송신하지 않습니다. CaptureExport의 `서버/로그` 탭에서 구독 해제·재연결, Topic별 성공·실패·교체 횟수와 최근 검증 결과를 확인할 수 있습니다. 자세한 사용법은 [센서 스트리밍 가이드](docs/sensor_streaming.ko.md)를 참고하세요.

### 포인트 클라우드 렌더러 상태

- 렌더 정책은 `Auto / Niagara 강제 / CPU 강제`입니다. 기본 `Auto`는 Niagara 컴파일·시스템 실행·점 업로드가 모두 성공한 경우에만 GPU 렌더러를 사용하고, 그 외에는 CPU ISM으로 자동 전환합니다.
- `포인트 클라우드 전용 켜기/끄기`는 토글입니다. 새 스캔이 빈 프레임이어도 직전의 정상 클라우드를 유지하며, 선택 LiDAR가 바뀌면 표시 상태도 새 LiDAR로 옮겨갑니다.
- 거리 영상·조감도·각 단면·3D 포인트는 JSON 직렬화나 서버 전송 완료를 기다리지 않고 LiDAR 측정 snapshot이 완성되는 즉시 갱신됩니다. 움직이는 물체가 다음 스캔에 검출되면 포인트 클라우드 전용 토글을 다시 누르지 않아도 화면에 반영됩니다.
- 상세 진단은 측정점·검출점·업로드점·표시점 수, 현재 렌더러, fallback/실패 사유, `검출점 없음`을 별도로 표시합니다.
- 실제 RHI 회귀 검증은 `Scripts/run_point_cloud_rhi_smoke.ps1`을 사용합니다. D3D12에서 `SensorRefactorTestMap` PIE를 실행하고, 실제 hit과 CPU fallback ISM 인스턴스·월드 좌표·뷰포트 투영 영역을 검증한 후 PNG·JSON·Markdown·log를 `Saved/Reports/point_cloud_rhi_smoke.*`에 생성합니다.

성능 보고서는 요청 규격(1280×720·30Hz, 360×60·10Hz)과 실제 완료 Hz를 분리합니다. FPS만 통과해도 Camera/LiDAR 최소 완료 Hz, 공정성, queue overflow, acquisition 실패 기준을 만족하지 못하면 실패입니다. `budget skip`은 성능 예산을 지키기 위한 정상적인 최신 프레임 정책으로, 실제 처리 실패와 다르게 집계됩니다.

센서 Actor 계층은 `AInteractableActor → AVirtualSensorActorBase → AVirtualCameraSensorActor/AVirtualLidarSensorActor`이며 Camera·LiDAR 공용 Scheduler Subsystem은 `MA0T10/Core`에 위치합니다.

## 다른 Unreal 프로젝트로 이전

현재 Sensor V2는 독립 플러그인이 아니라 `ma0t10_dt` 프로젝트 모듈 안에 있으므로 C++ 파일만 복사하고 `ma0t10` 문자열을 일괄 치환하는 방식으로는 완전히 이전되지 않습니다. 다음 결합 지점을 함께 처리해야 합니다.

- `AVirtualSensorActorBase`는 DTCore의 `AInteractableActor`를 상속합니다.
- `UVirtualLidarVisualizationComponent`는 DTCore의 `UStatusVisualizerCompBase`를 사용합니다.
- 센서 패널은 DTCore의 `UDxWidget`과 `UDxWidgetSubsystem`을 사용합니다.
- Runtime 모듈은 DTCore 외에도 Niagara, Stomp, HTTP, RHI, UMG, EnhancedInput 등의 모듈에 의존합니다.
- WBP와 Niagara Material/System은 `/Game/MA0T10` 아래의 별도 Content 자산입니다.
- Niagara 기본 경로, UI SaveGame 슬롯, 맵 저장 대상과 Editor 생성 스크립트에는 `/Game/MA0T10`, `/Script/ma0t10_dt` 또는 `MA0T10` 이름이 남아 있습니다.

### 같은 DTCore 기반 프로젝트로 빠르게 이전

1. `Camera`, `Sensor`, `Core/VirtualSensorSchedulerSubsystem`, 센서 관련 `UI` C++ 코드를 대상 Runtime 모듈로 옮깁니다.
2. Unreal Editor의 **Migrate** 기능으로 다음 자산과 의존성을 옮깁니다. 탐색기에서 `.uasset`만 복사하지 않습니다.
   - `WBP_VirtualSensorMonitorPanel`
   - `WBP_VirtualSensorSettingsPanel`
   - `WBP_VirtualSensorCaptureExportPanel`
   - `NS_VirtualLidarPointCloud`
   - `M_VirtualLidarPointSprite`
3. 대상 `Build.cs`와 `.uproject`에 DTCore, Niagara, UMG/Slate, EnhancedInput, RenderCore/RHI, ImageWrapper, Json, HTTP/HTTPServer, WebSockets/Stomp, Sockets/Networking 등의 실제 사용 의존성을 추가합니다.
4. 대상 Runtime 모듈 이름이 예를 들어 `FactoryDT`라면 다음 항목을 변경합니다.
   - include prefix: `ma0t10_dt/...` → `FactoryDT/...`
   - export macro: `MA0T10_DT_API` → `FACTORYDT_API`
   - script class path: `/Script/ma0t10_dt.*` → `/Script/FactoryDT.*`
   - 옮긴 Content 위치에 맞춰 `/Game/MA0T10/...` soft object path와 생성 스크립트 경로 변경
5. 새 레벨에 `AVirtualSensorCoordinator` 1개, Camera/LiDAR Actor, `AVirtualSensorUiHostActor` 1개를 배치합니다. Replay·HTTP·UDP 같은 외부 입력이 필요하면 `AVirtualSensorExternalSourceHostActor`도 배치합니다.
6. UI Host에 세 WBP class를 연결합니다. WBP를 연결하지 않아도 native fallback UI는 표시되지만 Niagara 자산이 없으면 LiDAR 3D 표시는 CPU fallback을 사용합니다.

`Source/.../MA0T10`이라는 단순 폴더 이름은 반드시 바꿀 필요가 없습니다. 빌드와 Reflection에 직접 영향을 주는 것은 모듈명, include 경로, export macro, `/Script/<Module>` 경로입니다. 새 프로젝트에서 기존 자산을 이어 쓰지 않는다면 구 클래스용 CoreRedirect도 복사할 필요가 없습니다.

현재 `SensorTestMap에 저장 예약`은 `/Game/MA0T10/Maps/SensorTestMap` 전용 Editor 흐름입니다. 다른 레벨에서 사용하려면 저장 대상 Map을 설정값으로 바꾸거나 이 기능을 비활성화해야 하며, 런타임 측정·미리보기·캡처·내보내기 기능은 이 Editor 저장 기능과 독립적입니다.

### 권장: 재사용 플러그인으로 분리

여러 프로젝트에서 사용할 계획이라면 프로젝트마다 이름을 치환하기보다 다음처럼 `DTVirtualSensor` 플러그인으로 한 번 분리하는 방식을 권장합니다.

```text
Plugins/DTVirtualSensor/
├─ DTVirtualSensor.uplugin
├─ Source/
│  ├─ DTVirtualSensorRuntime/
│  │  ├─ Camera/
│  │  ├─ Sensor/
│  │  ├─ Core/
│  │  └─ UI/
│  └─ DTVirtualSensorEditor/
└─ Content/
   ├─ UI/
   └─ VFX/
```

Runtime 모듈에는 측정·시각화·UI·Source·Transport를, Editor 모듈에는 Map 영구 반영과 Niagara/WBP 생성 도구를 둡니다. WBP, Niagara System/Material, 저장 대상 Map, SaveGame 슬롯, 저장 루트와 기본 전송 Topic은 플러그인 설정 또는 soft object property로 노출합니다. 그러면 다른 프로젝트에서는 DTCore와 `DTVirtualSensor` 플러그인을 활성화하고 Actor만 원하는 레벨에 배치하면 되며, 프로젝트 모듈 이름을 다시 치환할 필요가 없습니다.

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
