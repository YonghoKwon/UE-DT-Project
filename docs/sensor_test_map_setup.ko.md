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

`투사 범위 표시`는 선택 센서에만 적용됩니다. Camera는 FOV 절두체와 중심선, LiDAR는 수평·수직 범위와 최대 64개 대표 광선을 표시합니다. 런타임 UI 상태이며 맵에 저장하지 않습니다.

## 맵에 영구 반영

PIE 변경은 자동 저장되지 않습니다. Settings의 `SensorTestMap에 저장 예약`을 명시적으로 눌러야 Editor 종료 파이프라인이 관리 태그 Actor의 Transform과 허용 설정만 원본 맵에 복사합니다. 다른 맵, 중복 태그, Actor 누락, 저장 실패 시 원본 맵을 변경하지 않습니다.

## 패널 역할

- Monitor: Camera/LiDAR 미리보기, 센서 전환, LiDAR 표시 방식·오버레이, 상세 진단
- Settings: Transform·기즈모, 기본/고급 설정, 도움말, 투사 범위, 맵 저장 예약
- CaptureExport: 1회 캡처, Payload, CSV/JSONL/PCD/LAS/LAZ, timed capture, 최근 결과와 경로

패널 위치·접힘·LiDAR 표시 상태는 `Saved/SaveGames/MA0T10_VirtualSensorUI_v2.sav`에 저장됩니다. 기존 v1 파일은 자동 변환되며 센서 Transform과 장비 설정은 이 SaveGame에 저장되지 않습니다.

## 고급 설정 해석

- Camera 해상도·주기: 픽셀 수와 초당 캡처 횟수에 비례해 렌더·readback 비용 증가
- FOV: 수평 시야각, D455 기준 권장 87°
- JPEG 품질: 높을수록 압축 CPU와 Payload 크기 증가
- 출력 모드: 미리보기만은 readback/JPEG 생략, Payload 계열은 비동기 encode와 출력 수행
- LiDAR 수평 샘플×수직 채널: 스캔당 광선 수
- FOV·수직 각도: 같은 광선 수에서는 범위와 밀도를 변경
- Payload 간격·최대 점·미검출 포함: 서버 직렬화 크기 제어
- Preview 간격·최대 점·검출점만: 화면 표시 비용만 제어
- MultiHit·최대 검출 수: 한 광선의 복수 충돌을 수집하는 고부하 옵션
- 스캔별 자동 내보내기: 디스크·직렬화 부하가 커서 FullSpec에서는 수동 내보내기 권장

도움말 카드에는 Camera MP/s, LiDAR 광선/scan·광선/s, Preview/Payload 최대 점 수와 부하 등급이 표시됩니다.

## LiDAR 표시

### 투영 방식

- 거리 영상: 수직 채널과 수평 샘플을 행·열로 펼쳐 센서 원본 스캔 구조를 봅니다.
- 조감도: 센서 로컬 XY를 위에서 보며 축, 전방 방향, 거리 원으로 공간 분포를 봅니다.
- 거리-높이 단면: 전방 거리와 센서 상대 높이를 표시해 바닥·천장·장애물 높이를 봅니다.
- 분할: 거리 영상과 조감도를 동시에 비교합니다.

### 색상 방식

- Turbo/Viridis 거리, 상대 높이, SemanticLabel, 수직 채널/ring, MultiHit return index, 검출 마스크, 거리 회색조를 투영 방식과 자유롭게 조합합니다.
- 범례는 현재 프레임의 최소·중간·최대 거리/높이 또는 채널·return 번호를 표시합니다.
- 적응형 거리·깊이 경계·격자는 거리 영상 전용입니다. 조감도/단면은 축·거리 원·높이 기준선을 사용합니다.
- `월드 3D 포인트 표시`는 선택 LiDAR를 Niagara GPU sprite로 최대 21,600점 렌더링합니다. 실패 시 CPU ISM 5,000점 fallback과 원인을 진단에 표시합니다.
- Niagara 자산이 손상되거나 누락되면 `Scripts/setup_lidar_niagara_assets.ps1`로 다시 생성합니다.

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

CaptureExport 패널에서 절대 경로와 프로젝트 상대 경로를 확인하고 최근 저장 경로를 복사할 수 있습니다.
