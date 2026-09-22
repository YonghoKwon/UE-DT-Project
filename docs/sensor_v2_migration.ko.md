# Sensor V2 마이그레이션

> **고정 참고자료 (2026-09-22 전환)**: 이 문서는 이전 상세 명세/검증 이력을 보존합니다. 현재 사용법·UI·지원 범위는 [README](../README.md), 작업 규칙은 [AGENTS](../AGENTS.md), 후속 구현은 [보완 사항](IMPROVEMENTS.md)과 [로드맵](ROADMAP.md)을 기준으로 합니다. 아래의 과거 "현재"·성능 수치·맵 생성 절차를 최신 보장이나 실행 승인으로 해석하지 마세요. 기존 링크와 스크립트 참조를 위해 본문/경로를 유지합니다.

## 변경 목적

V2는 Camera/LiDAR Actor가 공통 센서 API를 제공하고 Capture·Scan·Analysis·Visualization·Export·Output 책임을 Component로 분리합니다. UI와 외부 Source는 구현 Component의 public 필드 대신 Actor API를 사용합니다.

## 주요 이름 변경

| 구 타입 | V2 타입 |
|---|---|
| `AVirtualCameraAct` | `AVirtualCameraSensorActor` |
| `UVirtualCameraComp` | `UVirtualCameraCaptureComponent` |
| `AVirtualSensorAct` | `AVirtualLidarSensorActor` |
| `UVirtualLidarSensorComp` | `UVirtualLidarScanComponent` |
| `AVirtualSensorManager` | `AVirtualSensorCoordinator` |
| `UVirtualSensorPerformanceSubsystem` | `UVirtualSensorSchedulerSubsystem` |
| `UVirtualSensorDataTransportComp` | `UVirtualSensorTransportComponent` |
| `UVirtualSensorRecorderComp` | `UVirtualSensorRecorderComponent` |
| 기존 센서 Widget/Host | `*PanelWidget`, `UVirtualSensorPanelHostComponent`, `AVirtualSensorUiHostActor` |

구 이름은 `Config/DefaultEngine.ini`의 `CoreRedirects`로 직렬화 자산 로드를 지원합니다. wrapper UCLASS는 남기지 않습니다.

## Blueprint 확인 사항

CoreRedirect는 클래스와 속성 참조를 새 이름으로 불러오는 데 도움을 주지만 시그니처나 소유 클래스가 달라진 Blueprint 함수 노드를 자동으로 고치지는 못합니다.

1. 프로젝트 소유 Map과 WBP를 모두 로드합니다.
2. Compile 후 깨진 노드를 새 Actor API로 교체합니다.
3. 특히 설정은 `ReadEditableState`/`ApplyEditableState`, 외부 프레임은 `SubmitExternalFrame`을 사용합니다.
4. 세 패널은 새 WBP class로 교체하고 Level Blueprint의 중복 `Create Widget`을 제거합니다.
5. 모든 Blueprint를 저장한 뒤 Asset Registry와 `rg`에서 구 타입 참조를 확인합니다.

## 전환 순서

1. `SensorRefactorTestMap`에서 V2 Camera, LiDAR, Coordinator, UI Host만으로 기능·Payload·성능을 검증합니다.
2. `setup_sensor_test_map.py`로 `SensorTestMap`의 `SensorTestManaged` Actor만 V2로 교체합니다.
3. 사용자 비관리 Actor 보존과 WBP compile을 확인합니다.
4. 구 타입 참조가 redirect와 이 문서 외 0건인지 확인합니다.
5. PIE에서 설정·기즈모·디버그·캡처·내보내기·맵 저장을 수동 확인합니다.
