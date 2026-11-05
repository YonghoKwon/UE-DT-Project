> **완료된 계획, 현재 기준 아님.** 원문을 보존한 보관본입니다. 현재 사용·규칙·미해결은 [문서 색인](../README.md)을 따릅니다.

# NAMING-01 구조·이름 통일 규칙

상태: **확정(2026-11-04 사용자 결정 반영).** 적용 시점은 DEVICE-PLUGIN-01 PR 머지(6단계)와 에셋 이동(7단계) 이후입니다.
8-1(폴더·이름)을 먼저 하고, 코드 스타일(탭·중괄호)은 8-2로 따로 단계적으로 진행합니다.

기준은 DTCore(`Plugins/DTCore`)의 구조와 `CLAUDE.md` "코딩 컨벤션"입니다. 아래는 사용자 결정(2026-11-03)을 반영한 내용입니다.

## 1. 폴더 구조

### 과제 프로젝트

```text
Source/ma0t10_dt/MA0T10/<분류>/          .h와 .cpp를 같은 폴더에 둔다 (Public/Private 구분 없음)
Source/ma0t10_dt/MA0T10/<분류>/<하위>/   파일이 많은 분류만 하위 폴더로 나눈다
Source/ma0t10_dt/MA0T10/<분류>/Tests/    자동화 테스트는 대상과 같은 분류 아래에 둔다
```

- 분류 이름은 업무 영역 단위입니다. 예: `Ship`, `Coil`, `Slab`, `Crane`, `Manager`, `Common`.
- 기준: 한 폴더 바로 아래 파일이 약 30개를 넘으면 하위 폴더를 검토합니다.
- `Scenario` 폴더는 없애고 업무 영역 폴더로 나눕니다. 대상은 `Ship/`, `Coil/`, `Slab/Scenario/`, `Training/`, `Dataset/`, `Agent/`, `Manager/Automation/`, `Common/`입니다. 파일별 위치는 `NAMING_01_INVENTORY.md` 3절에 있습니다.
- UI가 아닌 측정 probe는 `Diagnostics/`로, 배치 도구 actor는 `Sensor/Tool/`로 옮깁니다. 이들은 지금 `UI/` 안에 섞여 있습니다.

### UI

```text
UI/Common/    여러 위젯이 공유하는 상태·문구·입력·호스트
UI/Freeview/  떠 있는(이동 가능한) 패널·위젯
UI/Topview/   닫을 수 있는 고정 패널 (맵 점검, 학습)
UI/Main/      해당 맵 또는 기능의 가장 메인이 되는 고정 기본 패널
              (하위 폴더: Main/Sensor 센서 도구 사이드바·툴바, Main/Ship, Main/Coil, Main/Slab)
```

### DTDevice 플러그인

- 플러그인은 다른 프로젝트가 헤더를 가져다 쓰는 라이브러리입니다. 그래서 `Public/Private` 구분을 유지합니다. 사용자는 "같아도, 달라도 좋다"고 했습니다.
- `Public/Device/<분류>`의 `Device/` 한 단계도 **유지를 권장합니다.** 앞서 "없앤다"고 말씀드렸는데, 확인해 보니 이 폴더는 include 이름공간 역할을 하고 있어 정정합니다.
  - 지금 include는 `#include "Device/Sensor/VirtualLidarScanComp.h"` 형태입니다.
  - `Device/`를 없애면 include가 `Core/…`, `Tests/…`가 됩니다. 그러면 DTCore의 `Core/DxWidgetSubsystem.h`, 엔진의 `Tests/AutomationCommon.h`와 경로 앞부분이 같아집니다. 같은 이름의 헤더가 생기면 엉뚱한 파일이 잡힐 위험이 있습니다.
  - DTCore도 모듈 하나라서 `DTCore/` 접두사 없이 쓰고 있지만, DTDevice는 모듈이 3개(DeviceNative/DeviceRuntime/DeviceUI)라 구분이 필요합니다.

## 2. 클래스 이름

| 대상 | 규칙 | 예 |
|---|---|---|
| ActorComponent 계열 (UActorComponent, USceneComponent 등을 상속) | 이름 끝은 `Comp`. 추상 기반 클래스는 `CompBase` | `UVirtualLidarScanComponent` → `UVirtualLidarScanComp` |
| DTCore `Dx` 클래스를 상속한 과제 클래스 | 부모 이름을 그대로 잇고 뒤에 기능 이름을 붙인다 | `ADxPlayerBase` → `ADxPlayerCrane`, `ADxLevelManagerBase` → `ADxLevelManagerShip` |
| Dx 클래스를 상속하지 않은 과제·DTDevice 클래스 | `Dx` 접두사를 쓰지 않는다 | `UShipHatchSimulation` |
| 구조체 / 열거형 | `F` / `E` (UE 기본) | 변경 없음 |
| 파일 이름 | 클래스 이름에서 `U/A/F/E`를 뺀 것과 같게 한다 | `VirtualLidarScanComp.h` |

### 결정 사항 (2026-11-04)

1. `UDxWidget`을 상속한 과제 위젯은 **(a) `UDxWidget<기능>`** 형식으로 바꿉니다.
   - 예: `UShipHatchMonitorWidget` → `UDxWidgetShipHatchMonitor`, `UVirtualSensorPanelWidgetBase` → `UDxWidgetVirtualSensorPanelBase`
   - 이름 끝의 `Widget`은 중복이므로 뺍니다.
   - DTDevice 패널을 거쳐 `UDxWidget`을 상속하는 과제 패널도 같은 규칙을 따릅니다.
   - 이미 `UDx…` 형태인 `UDxM7atMainBasic`, `ADxPlayerTest` 등은 바꾸지 않습니다.
2. DTDevice의 Dx 상속 클래스는 **`UDxDevice…`** 형식입니다.
   - 예: `UDeviceSensorPanelBase` → `UDxDeviceSensorPanelBase`, `UDeviceSensorMonitorPanel` → `UDxDeviceSensorMonitorPanel`
   - Dx를 상속하지 않는 DTDevice 클래스는 바꾸지 않습니다.
3. UI 분류: Freeview = 떠 있는 패널·위젯, Topview = 닫을 수 있는 고정 패널, Main = 맵 또는 기능의 메인 고정 기본 패널(1절).

## 3. 변수·함수 이름

DTCore와 UE 기본 규칙을 따릅니다. 대부분 이미 맞아 있으므로 일괄 변경 대상은 아닙니다. 8-1에서는 파일을 옮기거나 이름을 바꾸는 코드에서 보이는 위반만 고칩니다.

- 멤버·함수·지역 변수: PascalCase. bool은 `b` 접두사.
- 컴포넌트 멤버 변수 이름도 타입과 맞춥니다. 예: `ScanComponent` → `ScanComp`. 단, Blueprint에 저장된 이름이면 PropertyRedirects와 함께 바꿉니다.
- 로그: 게임 스레드에서는 `DX_LOG(GetWorld(), …)`, 백그라운드 스레드에서는 `UE_LOG`.
  - 로그 카테고리 `LogMA0T10`/`LogDTDevice`는 유지합니다. 측정 스크립트가 이 이름을 검색합니다.
- 주석은 한국어. 기존 파일의 주석 밀도를 따릅니다.

## 4. 바꾸지 않는 것 (호환 고정)

외부 서버·저장 데이터·측정 도구와 맞물린 이름입니다.

- 서버 payload 키와 값: `schemaVersion = virtual-lidar.v1`, `payloadPolicy`, `slabAnalysis` 등 JSON 키.
- SaveGame 슬롯 이름(`MA0T10_SensorToolWorkspace_v1` 등)과 저장 구조체 필드 이름.
- 자동화 테스트 이름(`MA0T10.*`), 로그 카테고리, 측정·검증 스크립트가 읽는 로그 문구.
- 모듈 이름(`ma0t10_dt`, `DeviceRuntime` 등)과 플러그인 이름.

## 5. 호환 처리 원칙

- 이름을 바꾼 UCLASS/USTRUCT/UENUM에는 `DefaultEngine.ini` CoreRedirects를 **최종 이름으로 직접** 넣습니다.
  - CoreRedirects는 한 번만 적용됩니다(39b7ed3d에서 확인). 그래서 기존 redirect의 NewName도 새 최종 이름으로 함께 갱신합니다.
  - 예: `VirtualSensorRecorderComp` → (기존) `VirtualSensorRecorderComponent` → (신규) `VirtualSensorRecorderComp`.
    - 이 경우 기존 항목의 NewName을 최종 이름으로 바꿉니다.
    - OldName과 NewName이 같아진 항목은 지웁니다.
- 이름을 바꾼 UPROPERTY에는 PropertyRedirects를 넣습니다. 같은 package 안에서만 가능하다는 점에 주의합니다.
- 파일 이동에 따른 include 경로는 스크립트로 일괄 갱신하고, 빌드로 확인합니다.
- 맵과 Blueprint는 다시 저장하지 않습니다(resave 금지). 전부 redirect로 처리합니다.

## 6. 검증

8-1을 끝낼 때 다음을 확인합니다.

- 프로젝트와 TestHost 빌드
- release 전체 테스트 (Fail 0)
- 각 테스트 맵 로드 시 `LogCoreRedirects`·`LogLinker` 경고 0건
- 성능 ABAB 2%
- 마지막으로 메모리 측정 (범위는 그때 사용자와 결정)

## 7. 8-1 반영 결과 (2026-11-04)

확정 명단28 Comp +18 Dx 타입, reflected 컴포넌트 참조45개와 직접 redirect를 반영했다. 실제 명단 소유 분포는 Comp 플러그인22/과제6이다. 명단의 `UVirtualCameraCaptureComponent` 누락은 범위에 임의 추가하지 않았다(그 Actor의 CaptureComponent 멤버는 CaptureComp로 통일). 공유 테스트 선언 헤더는 컨테이너 이름을 유지한다. inventory는 이전 이름→최종 이름 근거로 보존한다. 기존 serialized subobject 이름·SaveGame·wire·테스트 ID·로그 문구·enum67개 본문은 유지했다. 나머지 코드 스타일 일괄 정리는 8-2 범위로 남긴다.

## 8. r52b 누락 보완 (2026-11-04)

USceneCaptureComponent2D 기반 두 타입이 inventory의 endswith(Component) 판별 때문에 누락된 원인을 수정했다. VirtualCameraCaptureComp와 VirtualLidarGpuDepthProjectionComp까지 반영하여 전체 컴포넌트33개 중 추가 변경 후보0이다. 8-1 변경 타입은 Comp30+Dx18=48개, reflected 컴포넌트 참조 속성47개다. 위7절의 CameraCapture 명단 밖 유지 판단은 이 보완으로 대체한다. 기존 파일/클래스 alias의 NewName은 모두 최종 이름이며 같은 package PropertyRedirects를 유지한다. 이름만 변경하므로 r52b에서는 사용자 결정에 따라 ABAB/장시간 메모리를 재실행하지 않는다.
