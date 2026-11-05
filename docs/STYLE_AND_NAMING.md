# 현재 폴더·이름·스타일 규칙

NAMING-01/STYLE-02 적용 후 규칙입니다. 과거 변경 명단은 [보관본](archive/NAMING_01_INVENTORY.md)이며 실제 파일은 현재 소스가 기준입니다.

## 폴더

- 프로젝트 `.h/.cpp`는 `Source/ma0t10_dt/MA0T10/<업무>/`에 함께 둡니다. Ship/Coil/Crane/Slab, Slab/Scenario, Training/Dataset/Agent, Manager/Automation, Common을 사용합니다.
- UI/Common은 Workspace·appearance·공용 상태, UI/Freeview는 이동 패널, UI/Topview는 고정 상위 화면, UI/Main/{Sensor,Ship,Coil,Slab}은 장면별 메인 화면입니다. 배치 도구는 Sensor/Tool, 측정 probe는 Diagnostics입니다.
- 플러그인은 Source/DeviceRuntime, Source/DeviceNative, Source/DeviceUI의 Public/Private 구조입니다. 과제 코드와 공통 장치 코드를 섞지 않습니다.
- 하위 모듈은 host를 include하지 않습니다. Contracts → 장치 → 업무 → UI 의존 방향과 layer allowlist0을 유지합니다.

## 이름

| 대상 | 현재 규칙·예 |
|---|---|
| ActorComponent | `…Comp` 또는 기존 추상 `…CompBase`; `UVirtualLidarScanComp`, `UVirtualCameraCaptureComp`, `UVirtualLidarGpuDepthProjectionComp` |
| 컴포넌트 멤버 | `ScanComp`, `CaptureComp` 등 타입에 맞춤. 저장된 UPROPERTY 변경은 같은 package PropertyRedirect가 필요 |
| 과제 UDxWidget 파생 | `UDxWidget<기능>`; `UDxWidgetShipHatchMonitor`, `UDxWidgetTrainBatch` |
| 공통 DeviceUI | `UDxDeviceSensorPanelBase`, `UDxDeviceSensorMonitorPanel` |
| 파일 | 주 클래스와 같은 이름(UE prefix U/A/F는 일반 파일명에서 제외). helper/테스트 묶음 파일 예외는 현재 컨테이너 이름 유지 |

크레인3종은 이미 Comp 이름인 UCraneDataSyncComp/UCraneMechDriverComp/UCraneStatusVisualizerComp를 유지합니다. 엔진·DTCore 타입과 동결 DTO·wire·저장 계약을 이 규칙 때문에 임의로 바꾸지 않습니다. 모든 기존 class redirect는 최종 이름으로 직접1-hop, PropertyRedirect는 같은 package 원칙입니다. maps/BP/SaveGame을 재저장해서 호환 문제를 숨기지 않습니다.

## 포맷·커밋

- [.clang-format](../.clang-format): 탭 들여쓰기4, Allman 중괄호, include 순서 유지, 문자열·주석 재배치 금지, CRLF 기준.
- 순수 포맷과 기능 변경은 분리합니다. `Tools/Style/token_compare.py <before> <after>`로 공백 외 토큰 차이0을 확인합니다. 플러그인도 자체 Tools/Style과 설정을 사용합니다.
- blame 제외: `git config blame.ignoreRevsFile .git-blame-ignore-revs`. host/plugin 저장소 각각 설정합니다. [.git-blame-ignore-revs](../.git-blame-ignore-revs)의 포맷 커밋 목록을 사용합니다.
- 이동/이름/API export/Build.cs 조정과 로직 변경을 섞지 않습니다. 테스트 ID `MA0T10.*`, 로그 문구, server payload 키·schemaVersion, SaveGame 슬롯/필드/버전, enum 순서, 33-byte PCD는 이름/포맷 변경 대상이 아닙니다.
