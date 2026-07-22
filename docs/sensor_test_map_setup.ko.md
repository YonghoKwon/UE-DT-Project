# SensorTestMap과 V2 Widget 설정

## 자산 생성

Unreal Editor를 종료하고 Editor target을 빌드한 뒤 아래 순서로 실행합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\ma0t10_dt.uproject" `
  -ExecutePythonScript="$PWD\Scripts\setup_sensor_refactor_test_map.py" `
  -Unattended -NoSplash -NoSound
```

이 스크립트는 다음 V2 WBP를 올바른 native parent로 생성하고 회귀맵에 배치합니다.

- `WBP_VirtualSensorMonitorPanel` → `UVirtualSensorMonitorPanelWidget`
- `WBP_VirtualSensorSettingsPanel` → `UVirtualSensorSettingsPanelWidget`
- `WBP_VirtualSensorCaptureExportPanel` → `UVirtualSensorCaptureExportPanelWidget`

먼저 `/Game/MA0T10/Maps/Tests/SensorRefactorTestMap`을 PIE로 검증합니다. 정상 확인 후 `Scripts/setup_sensor_test_map.py`를 실행해 운영 `SensorTestMap`을 V2로 갱신합니다.

스크립트는 `SensorTestManaged` Actor만 재구성합니다. 태그가 없는 사용자 Actor·Mesh는 보존하고 Slab mesh는 생성하지 않습니다.

## WBP를 직접 만들 때

1. Content Browser의 `/Game/MA0T10/UI`에서 Widget Blueprint를 만듭니다.
2. 위 표의 native parent를 정확히 선택합니다.
3. Designer가 비어 있어도 native fallback UI가 표시되므로 먼저 PIE 동작을 확인합니다.
4. 커스텀 Designer를 추가할 때 `BindWidgetOptional` 이름과 대소문자를 맞추고 `Is Variable`을 켭니다.
5. native에서 이미 연결한 버튼을 Event Graph에 다시 연결하지 않습니다.
6. `AVirtualSensorUiHostActor`의 Monitor/Settings/CaptureExport class에 WBP를 지정합니다.

## Main UI와 Viewport fallback

`AVirtualSensorUiHostActor`는 세 패널을 한 번만 생성합니다. `UVirtualSensorPanelHostComponent`가 Main Widget의 `AddWidgetPanel`을 찾으면 일반 Canvas 자식으로 배치하고, 찾지 못하면 Viewport에 배치합니다. 0.25초마다 Main 교체를 감지해 같은 인스턴스를 재호스팅하므로 Level Blueprint에서 별도로 `Create Widget`을 호출하면 안 됩니다.

접기 버튼은 본문을 숨기는 동시에 실제 높이를 약 48px로 줄입니다. Main에서는 `UCanvasPanelSlot::SetSize`, Viewport fallback에서는 desired size를 사용합니다. 펼치면 직전 크기를 복원하고 화면 경계 안으로 보정합니다.

## PIE 조작

Settings 패널에서 센서를 선택하고 `센서 조작 시작`을 누릅니다.

- `W/S`: 전진/후진
- `A/D`: 좌/우
- `Q/E`: 아래/위
- 방향키 좌/우: Yaw
- 방향키 위/아래: Pitch
- `Z/C`: Roll
- `Shift`: 5배
- `Ctrl`: 0.2배
- `Esc`: 조작 종료

이동 기본 단위는 10cm, 회전 기본 단위는 5°이며 로컬/월드 좌표계를 전환할 수 있습니다. 숫자는 Enter 또는 포커스 해제 시 한 번 적용되고 체크박스·프로필·품질은 선택 즉시 적용됩니다. 오류가 있으면 Actor를 변경하지 않고 이전 값으로 복원합니다.

장비 프로필 `아이요봇 ML-X(80)`을 선택하면 FullSpec 200×56, 0.05초, 15,000cm, 수평 80°, 수직 -11.65°~11.65°가 즉시 표시됩니다. 이후 품질 드롭다운에서 Balanced/RealTimePreview/Debug를 선택하면 거리와 FOV는 유지하면서 샘플 수와 주기만 낮아집니다. 수동 편집 시 품질은 Custom으로 바뀌며 최대 거리 입력 상한은 20,000cm입니다.

조작 시작 시 Monitor는 조작 대상 센서를 자동으로 따라가고 종료하면 이전 Camera/LiDAR 화면으로 돌아갑니다. 조작 도중 Monitor에서 사용자가 센서를 직접 바꾸면 자동 추적과 자동 복원은 해제됩니다. FullSpec 설정 자체는 유지되지만 조작 중에는 출력·JPEG·전송을 생략한 경량 미리보기 요청만 실행하므로 Transform 반응성을 우선합니다. 완료 프레임은 측정 당시 Transform을 포함한 immutable snapshot이므로 센서 이동 도중에도 이전 좌표와 현재 좌표가 한 화면에서 섞이지 않습니다.

`투사 범위 표시`는 선택 센서에만 적용됩니다. Camera는 FOV 절두체와 중심선, LiDAR는 수평·수직 범위와 최대 64개 대표 광선을 표시합니다. 런타임 UI 상태이며 맵에 저장하지 않습니다.

## 맵에 영구 반영

PIE 변경은 자동 저장되지 않습니다. Settings의 `SensorTestMap에 저장 예약`을 명시적으로 눌러야 Editor 종료 파이프라인이 관리 태그 Actor의 Transform과 허용 설정만 원본 맵에 복사합니다. 다른 맵, 중복 태그, Actor 누락, 저장 실패 시 원본 맵을 변경하지 않습니다.

## 패널 역할

- Monitor: Camera/LiDAR 미리보기, 센서 전환, LiDAR 표시 방식·오버레이, 상세 진단
- Settings: Transform·기즈모, 기본/고급 설정, 도움말, 투사 범위, 맵 저장 예약
- CaptureExport: 1회 캡처, Payload, CSV/JSONL/PCD/LAS/LAZ, timed capture, 최근 결과와 경로

패널 위치·접힘·LiDAR 표시 상태, 확장 크기, 듀얼 카메라와 캡처 선택은 `Saved/SaveGames/MA0T10_VirtualSensorUI_v6.sav`에 저장됩니다. 기존 v1~v5 파일은 자동 변환되며 센서 Transform과 장비 설정은 이 SaveGame에 저장되지 않습니다.

Camera 보기에서 `카메라 2대 동시 보기`를 켜고 주/보조 드롭다운에서 서로 다른 SensorId를 선택합니다. 주 카메라는 Settings 선택과 함께 바뀌고 보조 카메라는 보기 전용입니다. Refactor Map의 `VCAM-TEST-001`은 대각선, `VCAM-TEST-002`는 높이 10m·Pitch -90° 수직 하향 카메라입니다. 화면은 기존 RenderTarget만 공유하므로 듀얼 모드가 센서 캡처 횟수를 늘리지 않습니다.

Monitor 오른쪽 아래 `↘` 영역을 드래그하면 가로·세로 크기를 자유롭게 바꿀 수 있습니다. 최소 크기는 480×300이며 화면 밖으로 커지지 않도록 제한됩니다. 접힌 동안에는 크기를 바꾸지 않고, 다시 펼치면 마지막 확장 크기를 복원합니다. 헤더의 `크기 초기화`는 현재 해상도에 맞는 기본 크기로 되돌립니다.

## 고급 설정 해석

- Camera 해상도·주기: 픽셀 수와 초당 캡처 횟수에 비례해 렌더·readback 비용 증가
- FOV: 수평 시야각, D455 기준 권장 87°
- JPEG 품질: 높을수록 압축 CPU와 Payload 크기 증가
- 출력 모드: 미리보기만은 readback/JPEG 생략, Payload 계열은 비동기 encode와 출력 수행
- LiDAR 수평 샘플×수직 채널: 스캔당 광선 수
- FOV·수직 각도: 같은 광선 수에서는 범위와 밀도를 변경
- Payload 간격·최대 점·미검출 포함: 서버 직렬화 크기 제어
- Preview 간격·최대 점·검출점만: 화면 표시 비용만 제어. `검출점만=true`는 hit만, `false`는 miss를 최대 거리 endpoint에 낮은 투명도로 표시하며 True→False→True를 새 스캔 없이 마지막 snapshot으로 다시 그립니다.
- MultiHit·최대 검출 수: 한 광선의 복수 충돌을 수집하는 고부하 옵션
- 스캔별 자동 내보내기: 디스크·직렬화 부하가 커서 FullSpec에서는 수동 내보내기 권장

도움말 카드에는 Camera MP/s, LiDAR 광선/scan·광선/s, Preview/Payload 최대 점 수와 부하 등급이 표시됩니다.

## LiDAR 표시

### 투영 방식

- 거리 영상: 수직 채널과 수평 샘플을 행·열로 펼쳐 센서 원본 스캔 구조를 봅니다.
- 센서 로컬 조감도: 센서 로컬 XY를 위에서 보며 축, 전방 방향, 거리 원으로 공간 분포를 봅니다. 수평 설치 센서에 적합하며 센서를 아래로 기울이면 화면도 함께 기울어진 좌표계를 사용합니다.
- 월드 XY 조감도: 가로=월드 Y, 세로=월드 X로 고정한 공장 평면도입니다. 센서 회전과 무관하므로 천장이나 상부에 수직 설치한 LiDAR를 첫 번째 예시 같은 바닥 평면도로 볼 때 사용합니다. `검출점 자동 맞춤`은 현재 hit의 XY 경계에 여백을 더해 화면을 맞춥니다.
- 방사 거리-높이 프로파일: X축은 센서로부터의 수평 방사거리 `sqrt(local X²+local Y²)`, Y축은 상대 높이 Z입니다. 좌우 방향은 합쳐지므로 바닥·천장·경사·물체 높이를 분석할 때 사용합니다.
- 전방 수직 슬라이스: 회전 가능한 센서 로컬 X–Z 기준면과 설정 두께 안의 점만 표시합니다. 특정 방향의 벽·기둥·설비 높이를 분리해서 볼 때 사용합니다.
- 분할: 거리 영상과 조감도를 동시에 비교합니다.
- 조감도/거리-높이 단면 조작: 이미지 위 좌클릭 드래그로 이동, 우클릭 드래그로 회전, 휠로 확대·축소합니다. `보기 초기화`는 현재 투영의 pan/회전/zoom을 기본값으로 되돌리며 월드 XY 조감도에서는 자동 맞춤도 다시 켭니다.
- 분할 모드에서는 거리 영상은 센서 고정 시점으로 유지되고 아래쪽 조감도만 자유 시점 조작 대상입니다.

수직 설치 LiDAR를 평면도로 확인하는 순서:

1. 센서 디버그 투사 범위가 바닥과 대상 Mesh에 닿는지 확인합니다. 투영 방식은 이미 측정된 점의 표시 좌표만 바꾸며 광선 방향이나 FOV를 바꾸지 않습니다.
2. Monitor에서 `월드 XY 조감도`와 `검출점 자동 맞춤`을 선택합니다.
3. Settings에서 `미리보기에서 검출점만 표시`를 켜면 미검출 최대거리 endpoint가 사라져 설비 윤곽이 선명해집니다.
4. 단색 윤곽은 `검출 마스크`, 거리 구분은 Turbo/Viridis, 높이 구분은 월드 Z 기반 `센서 상대 높이`를 사용합니다.
5. Monitor 오른쪽 아래 grip으로 패널을 확대한 뒤 필요한 경우 pan/rotate/zoom을 조정합니다.

### 색상 방식

- Turbo/Viridis 거리, 상대 높이, SemanticLabel, 수직 채널/ring, MultiHit return index, 검출 마스크, 거리 회색조를 투영 방식과 자유롭게 조합합니다.
- 범례는 현재 프레임의 최소·중간·최대 거리/높이 또는 채널·return 번호를 표시합니다.
- 적응형 거리·깊이 경계·격자는 거리 영상 전용입니다. 조감도/단면은 축·거리 원·높이 기준선을 사용합니다.
- `월드 3D 포인트 표시`는 선택 LiDAR를 Niagara GPU sprite로 최대 21,600점 렌더링합니다. 실패 시 CPU ISM 5,000점 fallback과 원인을 진단에 표시합니다.
- 거리 영상·조감도·단면·3D 포인트는 Payload JSON 생성과 독립적으로 새 측정 snapshot마다 갱신됩니다. 움직이는 표적을 반영하기 위해 포인트 클라우드 전용 토글을 반복할 필요가 없습니다.
- Niagara 자산이 손상되거나 누락되면 `Scripts/setup_lidar_niagara_assets.ps1`로 다시 생성합니다.

### 3D 포인트 클라우드 문제 확인

1. 선택 대상을 LiDAR로 바꾸고 `포인트 클라우드 전용 켜기`를 누릅니다. Camera가 선택된 경우 UI가 LiDAR 선택을 안내합니다.
2. 상세 진단의 `검출점`이 0이면 렌더 문제가 아니라 스캔 경로에 충돌 대상이 없는 상태입니다.
3. `업로드점`은 0보다 큰데 `표시점`이 0이면 렌더러 상태와 fallback 사유를 확인합니다. `Auto`에서는 Niagara가 실제로 준비된 경우에만 GPU를 사용하고 나머지는 CPU ISM으로 전환됩니다.
4. 신규 프레임의 hit가 없어도 마지막 정상 클라우드는 유지됩니다. 버튼을 다시 눌러 전용 모드를 끜 경우에만 이전 2D 투영·오버레이 상태로 돌아갑니다.

RHI 자동 검증:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_point_cloud_rhi_smoke.ps1"
```

이 스크립트는 D3D12로 `SensorRefactorTestMap` PIE를 실행하고 검출 hit, CPU fallback 실제 ISM 인스턴스, 측정 월드 좌표 일치, 포인트 영역의 게임 뷰포트 중첩을 검증합니다. 결과는 `Saved/Reports/point_cloud_rhi_smoke.png`, `.md`, `.json`, `.log`에 저장됩니다.

- 거리 색상: 가까운 점 자홍·주황, 중간 노랑, 먼 점 청록·파랑
- 검출 마스크: hit 흰색, miss 검정
- 의미 분류 색상: `SemanticLabel` 규칙 색상, 미분류 회색
- 거리 회색조: 가까운 점 밝게, 먼 점과 miss 어둡게

적응형 거리, 깊이 경계, 격자는 색상 모드와 별개인 오버레이입니다.

## 저장 위치

- Payload: `Saved/SensorCaptures/<SensorId>/ServerPayload`
- Point cloud: `Saved/SensorCaptures/<SensorId>/PointCloud`
- Timed Camera: `Saved/SensorCaptures/LocalTimedCapture/<UTC>/Camera`
- Timed LiDAR: `Saved/SensorCaptures/LocalTimedCapture/<UTC>/Lidar`
- Recorder: `Saved/SensorRecordings`

## 최신 실시간 Topic 안내

캡처/내보내기 패널의 크기 조절, 네 탭 구성과 LiDAR·Camera·Point Cloud 자동 스트림은 [sensor_streaming.ko.md](sensor_streaming.ko.md)를 기준으로 사용하십시오. 스트림은 기본적으로 중지 상태이며 센서마다 최신 대기 프레임 하나만 유지합니다.

CaptureExport 패널에서 절대 경로와 프로젝트 상대 경로를 확인하고 최근 저장 경로를 복사할 수 있습니다.

Capture 탭에서는 `캡처 간격(초)`과 저장 출력을 선택합니다. `센서 주기 사용`은 현재 선택 센서의 주기를 복사하며 ML-X(80) FullSpec에서는 0.05초입니다. 이 값은 로컬 저장 주기이고 실시간 Topic은 별도의 `전송 간격(프레임)`을 사용합니다.

## 외부 센서 Source 사용

`SensorRefactorTestMap`에는 `SensorTest_ExternalSources`가 관리 Actor로 배치됩니다. 모두 기본 정지 상태입니다.

- `LidarCsvReplay`: `Samples/slab_replay_sample.csv`
- `LidarJsonLinesReplay`: `Samples/slab_replay_sample.jsonl`
- `LidarBufferedJson`: Blueprint/C++에서 buffered JSON 주입
- `LidarHttpJson`: `POST http://127.0.0.1:8082/ma0t10/lidar/live`
- `LidarUdpJson`: 로컬 UDP JSON. 기본 포트 0이므로 사용할 포트를 먼저 지정
- `CameraBufferedJson`: Camera JSON buffered 주입

Settings의 Source 시작/중지는 입력 lifecycle만 관리하고 `선택 Source 1회 주입`은 선택 입력의 다음 프레임을 센서 Actor의 `SubmitExternalFrame`으로 넘깁니다. 입력 프레임을 다른 서버로 보낼지는 CaptureExport 패널에서 별도로 선택합니다. ROS2, Livox SDK, RealSense SDK는 현재 비활성 확장 항목이며 SDK 연동 구현이 필요합니다.

## Artemis STOMP와 HTTP 전송

CaptureExport 패널에서 Broker/endpoint, Topic, 사용자명, 최대 메시지 크기, 선택 ACK Topic을 입력하고 `연결 시험`을 누릅니다. 비밀번호와 Bearer token은 SaveGame에 저장되지 않고 현재 PIE 세션에만 존재합니다.

- STOMP 기본 Topic: `topic.virtual.sensor.camera.0`, `topic.virtual.sensor.lidar.0`, `topic.virtual.sensor.export.0`
- Artemis routing: `destination-type=MULTICAST`
- HTTP Payload: `application/json`
- HTTP 최근 내보내기 파일: `application/octet-stream`
- STOMP 최근 내보내기 파일: Base64 JSON envelope
- `Broker 수락, 소비자 처리 미확인`: receipt는 성공했지만 실제 consumer ACK는 확인하지 않은 상태

개발용 Broker는 `Tools/Artemis`에서 `docker compose up -d`로 실행합니다. 운영 URL·계정·token은 저장소나 화면 캡처에 포함하지 마세요.
