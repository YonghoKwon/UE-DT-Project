# AGENTS.md

이 문서는 저장소를 수정하는 개발자와 자동화 agent가 지켜야 할 작업 기준입니다.

## 프로젝트 기준

- Engine: Unreal Engine 5.3
- Runtime module: `ma0t10_dt`
- Editor module: `ma0t10_dtEditor`
- Content root: `/Game/MA0T10`
- 운영 테스트맵: `/Game/MA0T10/Maps/SensorTestMap`
- V2 회귀맵: `/Game/MA0T10/Maps/Tests/SensorRefactorTestMap`
- 맵 생성 source of truth: `Scripts/setup_sensor_test_map.py`

## 변경 금지 범위

- 사용자 요청 없이는 `Plugins/DTCore` 내부와 parent gitlink를 수정하거나 stage하지 않습니다.
- 로컬 override인 `Config/Game.ini`를 수정하거나 stage하지 않습니다.
- `Binaries`, `Intermediate`, `Saved`, `.vs`, packaged 결과를 커밋하지 않습니다.
- 사용자 worktree의 기존 변경을 되돌리거나 덮어쓰지 않습니다.
- `SensorTestManaged` 태그가 없는 맵 Actor와 Mesh를 삭제·이동하지 않습니다.

## Sensor V2 경계

- UI, Source, Gizmo는 Capture/Scan Component의 public 필드를 직접 변경하지 않습니다.
- 설정은 Actor의 `ReadEditableState`, `ValidateEditableState`, `ApplyEditableState` 경로를 사용합니다.
- 외부 프레임은 Actor의 `SubmitExternalFrame`을 우선 사용합니다. component 직접 주입은 호환성 테스트 fallback에만 둡니다.
- 출력은 `UVirtualSensorOutputComponent`에서 Transport·Recorder로 라우팅하며 같은 SensorId/frame ID를 중복 출력하지 않습니다.
- Camera와 LiDAR가 함께 사용하는 Subsystem은 `MA0T10/Core`에 둡니다. 자동 측정은 `UVirtualSensorSchedulerSubsystem`을 사용하며 수동 1회 측정 API의 동기 호환성은 유지합니다.
- `virtual-camera.v1`, `virtual-lidar.v1`, FullSpec 규격, 저장 경로와 export 포맷을 변경할 때는 계약 테스트와 문서를 함께 갱신합니다.

## UI 기준

- V2 패널 native parent는 각각 `UVirtualSensorMonitorPanelWidget`, `UVirtualSensorSettingsPanelWidget`, `UVirtualSensorCaptureExportPanelWidget`입니다.
- 공통 base `UVirtualSensorPanelWidgetBase : UDxWidget`가 drag, DPI clamp, 접기, 위치 복원을 담당합니다.
- Monitor는 공통 base의 자유 resize를 활성화합니다. 오른쪽 아래 grip, DPI 보정, 화면 clamp, 접힘 시 확장 크기 보존과 UI SaveGame v3 복원을 함께 유지합니다.
- `UVirtualSensorPanelHostComponent`는 Main `AddWidgetPanel`을 우선 사용하고 Viewport fallback을 제공합니다.
- 접기는 실제 Canvas slot 또는 viewport desired height를 약 48px로 줄여 빈 hit-test 영역을 남기지 않아야 합니다.
- native fallback UI와 사용자 WBP의 optional binding/API를 함께 유지합니다. 동일 버튼을 Blueprint Event Graph에 중복 연결하지 않습니다.
- 사용자 노출 문구는 한글을 기본으로 하되 SensorId, FOV, JPEG, JSONL 같은 기술 용어는 유지합니다.

## 맵과 자산

- 먼저 `setup_sensor_refactor_test_map.py`로 V2 전용 회귀맵을 검증합니다.
- 검증 후 `setup_sensor_test_map.py`로 운영 테스트맵의 관리 Actor만 교체합니다.
- 새 WBP를 생성하고 맵을 저장한 뒤에만 구 WBP를 삭제합니다.
- `.umap`, `.uasset`을 raw binary patch하지 말고 Unreal Editor Python이나 Editor API로 생성·저장합니다.
- Slab mesh는 자동 생성하지 않습니다.

## 다른 프로젝트 이식 기준

- Sensor V2를 다른 프로젝트로 옮길 때 C++ 파일 복사와 `ma0t10` 일괄 치환만으로 완료됐다고 판단하지 않습니다. C++ 모듈, DTCore 의존성, Content 자산, soft object path, Editor 도구를 각각 검사합니다.
- 현재 DTCore 결합 지점은 `AInteractableActor`, `UStatusVisualizerCompBase`, `UDxWidget`, `UDxWidgetSubsystem`입니다. 대상 프로젝트가 DTCore를 사용하지 않으면 동일 역할의 base/interface와 Viewport host를 먼저 제공해야 합니다.
- 모듈명을 변경하면 include prefix, `*_API` export macro, `/Script/<Module>` class path, Build.cs 의존성을 함께 변경합니다. `MA0T10`이라는 단순 source 하위 폴더명은 기능상 필수 변경 대상이 아닙니다.
- `/Game/MA0T10` 자산은 Unreal Editor의 Migrate 또는 Editor API로 이전합니다. `.uasset`을 파일 탐색기로 개별 복사하거나 binary patch하지 않습니다.
- 최소 이전 자산은 세 V2 WBP, `NS_VirtualLidarPointCloud`, `M_VirtualLidarPointSprite`이며 Asset Registry에서 누락된 dependency가 없는지 확인합니다.
- 임의 레벨의 기본 구성은 `AVirtualSensorCoordinator` 1개, 필요한 Camera/LiDAR Actor, `AVirtualSensorUiHostActor` 1개입니다. 외부 입력이 필요할 때만 `AVirtualSensorExternalSourceHostActor`를 추가합니다.
- `/Game/MA0T10/Maps/SensorTestMap`에 고정된 PIE→Editor 영구 저장 기능을 다른 Map에 그대로 적용하지 않습니다. 저장 대상은 설정 가능한 soft path로 바꾸거나 대상 프로젝트에서 명시적으로 비활성화합니다.
- 새 프로젝트가 기존 Blueprint/Map 자산을 가져오지 않는다면 MA0T10 구 클래스용 CoreRedirect를 불필요하게 복사하지 않습니다. 기존 자산을 마이그레이션할 때만 실제 old/new module 경로로 redirect를 작성합니다.
- 여러 프로젝트에서 재사용할 구현은 `DTVirtualSensorRuntime`, `DTVirtualSensorEditor`, plugin Content로 분리하는 것을 기본 방향으로 삼습니다. 프로젝트별 이름 치환보다 안정적인 plugin mount path와 설정 객체를 우선합니다.
- 플러그인화할 때 WBP/Niagara 경로, 저장 대상 Map, UI SaveGame 슬롯, 캡처 저장 루트, 전송 Topic을 hard-coded `/Game/MA0T10` 값으로 두지 않고 설정 또는 soft object property로 노출합니다.
- 이식 검증에는 대상 프로젝트 Editor Development 빌드, WBP compile, Niagara 로드와 CPU fallback, Coordinator의 센서 발견, UI Host의 Main/Viewport fallback, Capture/Export, PIE 종료 정리를 포함합니다.

## 검증 순서

1. Unreal Editor와 Live Coding을 종료합니다.
2. `ma0t10_dtEditor Win64 Development`를 빌드합니다.
3. V2 회귀맵 생성 후 `MA0T10.SensorV2` 자동화 테스트를 실행합니다.
4. 운영 테스트맵 갱신 후 WBP compile과 Asset Registry 참조를 확인합니다.
5. `rg`에서 구 타입 참조가 CoreRedirect와 마이그레이션 문서 외 0건인지 확인합니다.
   `Scripts/validate_sensor_v2_refactor.ps1 -RequireAssets`를 사용합니다.
6. 1920×1080과 1280×720 PIE에서 패널 재호스팅·실제 크기 접기·설정·디버그·캡처·내보내기·맵 저장을 확인합니다.
7. 커밋 전 `git diff --check`와 `git status --short`로 금지 범위가 stage되지 않았는지 확인합니다.

IDE 빌드에서 `Unable to build while Live Coding is active`가 나오면 Editor를 종료하거나 Editor 안에서 `Ctrl+Alt+F11`로 Live Coding을 종료한 다음 다시 빌드합니다.
