> **완료된 계획, 현재 기준 아님.** 원문을 보존한 보관본입니다. 현재 사용·규칙·미해결은 [문서 색인](../README.md)을 따릅니다.

# 7단계: 에셋 3종 이동 + TestHost 데모 맵 + 기본 모니터 패널·적용 가이드 (계획 초안)

상태: **사용자 승인(2026-11-03), r51 실행. DEVICE-PLUGIN-01 PR #99 머지 이후.**
실행 기준: host `e8ef7221`, DTDevice `5e9b870`. 아래는 승인된 이동·검증 범위입니다.

## 1. 옮길 에셋

| 현재 | 새 위치 | 용도 |
|---|---|---|
| `/Game/MA0T10/Sensor/Materials/M_LidarSemanticId` | `/DTDevice/Sensor/Materials/M_LidarSemanticId` | LiDAR semantic(물체 구분) 캡처 재질 |
| `/Game/MA0T10/Sensor/VFX/NS_VirtualLidarPointCloud` | `/DTDevice/Sensor/VFX/NS_VirtualLidarPointCloud` | point cloud 미리보기 Niagara |
| `/Game/MA0T10/Sensor/VFX/M_VirtualLidarPointSprite` | `/DTDevice/Sensor/VFX/M_VirtualLidarPointSprite` | 위 Niagara의 점 sprite 재질 |

`DTDevice.uplugin`에는 이미 `"CanContainContent": true`가 있어서 플러그인 Content 폴더를 바로 쓸 수 있습니다.

## 2. 참조 조사 결과

| 참조 위치 | 내용 | 처리 |
|---|---|---|
| `DeviceRuntime/Private/Core/DeviceSensorAssetSettings.cpp` | 기본 soft path 2개(semantic 재질, Niagara) | 새 경로로 변경 |
| `ma0t10_dtEditor/Private/DeviceAssetBoundaryTests.cpp` | "경로 변경 없음"을 확인하는 테스트 | 새 경로 기대값으로 교체. 테스트 의도("설정이 실제 에셋을 가리킨다")는 유지 |
| `LidarFidelityRhiTests.cpp`, `SlabAnalysisRhiTests.cpp`, `UxrSlabSurfaceRhiTests.cpp`, DTDevice `CameraGroundTruthTests.cpp` | `LoadObject`에 하드코딩한 경로 | 설정(`UDeviceSensorAssetSettings`)을 읽도록 바꾸거나 새 경로로 변경 |
| `SetupVirtualLidarNiagaraAssetsCommandlet.cpp` | `/Game/MA0T10/Sensor/VFX`에 에셋을 생성·갱신 | 생성 위치를 `/DTDevice/Sensor/VFX`로 변경 |
| `NS_VirtualLidarPointCloud` 내부 | `M_VirtualLidarPointSprite` import | 함께 옮기므로 PackageRedirects로 해결 |
| 맵·Blueprint | 문자열 검색 결과 3종을 직접 참조하는 맵·BP는 없음(자기 자신과 NS만) | 해당 없음 |
| `Config/*.ini` | 3종 경로 설정 없음 | 해당 없음 |

## 3. 진행 방법

1. **파일 이동**
   - `git mv`로 `.uasset` 3개를 `Plugins/DTDevice/Content/Sensor/...`로 옮깁니다.
   - 이 이동은 DTDevice 저장소의 커밋입니다. 과제 저장소에서는 삭제와 pin 갱신으로 나타납니다.
2. **경로 연결**
   - 과제 `DefaultEngine.ini`에 다음 3줄을 추가합니다. 에셋을 다시 저장하지 않고도 옛 경로 참조(NS 내부 import 포함)가 새 위치로 연결됩니다.

     ```text
     +PackageRedirects=(OldName="/Game/MA0T10/Sensor/Materials/M_LidarSemanticId",NewName="/DTDevice/Sensor/Materials/M_LidarSemanticId")
     +PackageRedirects=(OldName="/Game/MA0T10/Sensor/VFX/NS_VirtualLidarPointCloud",NewName="/DTDevice/Sensor/VFX/NS_VirtualLidarPointCloud")
     +PackageRedirects=(OldName="/Game/MA0T10/Sensor/VFX/M_VirtualLidarPointSprite",NewName="/DTDevice/Sensor/VFX/M_VirtualLidarPointSprite")
     ```

   - TestHost와 다른 소비 프로젝트는 처음부터 새 경로만 씁니다. 그래서 redirect는 플러그인 Config가 아니라 과제 Config에 둡니다.
3. **코드 경로 갱신**: 2절 표의 순서대로 갱신합니다.
4. **패키징 확인**
   - Development 패키지에 3종이 실제로 cook되는지 확인합니다. 근거로 cook 결과 목록이나 pak 목록을 남깁니다.
   - 패키지 실행 시 semantic 재질과 Niagara 로드 실패 로그가 0건인지 확인합니다.

## 4. TestHost 데모 맵

- 위치: `TestHost/Content/Maps/DTDeviceDemo.umap`. TestHost `DefaultEngine.ini`의 `GameDefaultMap`과 `EditorStartupMap`으로 지정합니다.
- 구성:

  | 요소 | 내용 |
  |---|---|
  | 바닥 | 엔진 기본 Plane, Actor Tag `Floor` |
  | Slab | 엔진 기본 Cube를 Slab 크기로 늘림, Actor Tag `Slab` |
  | LiDAR | `AVirtualLidarSensorActor` 1대. SimulationQuality = RealTimePreview, MultiHit off, ExportOnScan off |
  | 카메라 | `AVirtualCameraSensorActor` 1대 |
  | 조명·하늘 | 기본 Directional Light, Sky Light |

- 프로젝트 에셋은 쓰지 않습니다. 엔진 기본 도형과 플러그인 에셋 3종만 씁니다.
- **필요한 코드 1가지**
  - 과제는 `ProjectDeviceSemanticPolicy`에서 `DeviceSemanticPolicy::Configure`로 Slab, Roller, Floor 같은 semantic 규칙을 등록합니다.
  - TestHost에는 이 등록이 없어서, 지금 상태로는 Slab 라벨이 분류되지 않습니다.
  - TestHost 게임 모듈 `StartupModule`에 Slab과 Floor 2종만 등록하는 최소 정책을 추가합니다. 과제 정책은 복사하지 않습니다.
- 맵은 손으로 배치하지 않고 TestHost 전용 에디터 스크립트(commandlet)로 생성합니다. 그래야 다시 만들 수 있습니다.
- `TestHost/Scripts/run_measurements.ps1`에 "데모 맵 로드, LiDAR hit > 0, Slab hit > 0" 스모크 테스트 1개를 추가합니다.

## 5. 바로 쓰는 기본 센서 모니터 패널 (사용자 요청으로 추가, 2026-11-03)

목적: 별도 과제가 DTCore + DTDevice만으로 "센서 배치 → Play → 센서 화면 보기"를 할 수 있게 한다. 지금 ma0t10의 패널·사이드바·상태바는 과제 전용이라 플러그인에 없다.

- DeviceUI에 다음 2개를 추가한다. 둘 다 기존 DeviceUI 본문 조립 함수(`DeviceSensorMonitorBody` 등)와 `UDeviceSensorPanelBase`(UDxWidget 기반)를 재사용하고, 과제 코드는 복사하지 않는다.
  - `UDeviceSensorMonitorPanel`: 맵의 센서 목록 선택, 상태(센서 ID, 주기, hit 수, 전송 점 수, 미리보기 점 수, 경고), 카메라 영상, 1회 캡처 버튼, 미리보기 증감 버튼.
  - `ADeviceSensorMonitorHostActor`: 맵에 놓으면 PIE·게임 시작 시 패널을 띄운다. Blueprint 위젯 지정은 선택 사항이며, 없으면 C++ 기본 패널을 쓴다.
- 이름은 8단계 규칙(`NAMING_01_RULES.md`)을 따른다. 컴포넌트를 추가하면 `…Comp`, Dx 상속 위젯은 8-1 결정에 맞춘다.
- 서버 전송 정책과 미리보기 정책의 분리(AGENTS.md LiDAR 데이터 정책)를 그대로 지킨다. 패널의 미리보기 조절은 서버 payload를 바꾸지 않는다.
- TestHost 데모 맵에 host actor를 배치한다. 자동화 테스트 1개를 추가한다: 패널 생성, 센서 선택, 상태 갱신, 캡처 1회.

## 6. 새 과제 적용 가이드 (플러그인 저장소 `Docs/GettingStarted.md`)

- 내부망 저장소 미러 방법(`git clone --mirror` → `push --mirror`)과 검증된 DTCore·DTDevice 커밋 조합 확인 방법.
- 별도 과제 연결 방법.
  - C++ 프로젝트 필요.
  - submodule 2개 추가.
  - `.uproject` 플러그인 켜기.
  - DefaultEngine.ini 렌더링 설정: DX12, SM6, RayTracing, CustomDepth=3, DxGameMode.
  - Build.cs 의존성 추가.
  - `DeviceSemanticPolicy::Configure` 최소 예제.
- 맵에 센서 배치하는 방법.
  - LiDAR·카메라 actor 배치.
  - 스모크 권장값 적용.
  - Actor Tag 지정.
  - 모니터 host actor 배치 → Play.
- TestHost 데모 맵을 그대로 따라 할 수 있는 예제로 연결한다.

## 7. 검증

- 과제: release Fail 0, 테스트 맵 로드 시 `LogLinker`·`LogCoreRedirects` 경고 0건, Development 패키지 smoke.
- TestHost: setup → 빌드 → 측정 17개 + 데모 맵 스모크 통과. `Open_TestHost.bat`으로 열었을 때 데모 맵 화면을 캡처해 보고합니다.
- 에셋 이동과 새 UI 추가만 하고 기존 센서 경로는 바뀌지 않으므로 성능과 메모리 재측정은 하지 않습니다. 패키지 smoke에서 point cloud 표시를 확인합니다.
- 가이드 검증: 빈 C++ 프로젝트(TestHost와 별개)에서 가이드 순서대로 연결 → 빌드 → 센서 배치 → Play → 패널 표시를 1회 확인하고, 화면 캡처를 보고합니다.
- 예상 소요: 약 1일(기존 반나절 + 패널·가이드 반나절).

## r51 실행 보완

Niagara의 기존 sprite import는 파일 이동 후에도 옛 `/Game` 경로입니다. 빈 소비 프로젝트에서도 resave 없이 해석되도록 플러그인 `DefaultDTDevice.ini`에 같은 세 PackageRedirects를 제공하고 DeviceRuntime 시작 시 읽습니다. 호스트의 세 줄도 유지합니다. 기존 바이너리 3개는 SHA256 동일성을 확인합니다.
