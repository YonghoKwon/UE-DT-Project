# LiDAR 높이, 의미 분류 및 PCD 좌표

## 화면에서 판과 물체 구분

- **높이 색상**은 기하학적 높이를 색으로 보여준다. 월드 XY 조감도에서는 기본적으로 월드 Z를 사용한다. 높이 기준 버튼으로 센서 로컬 Z/월드 Z를 명시적으로 선택할 수 있다.
- **의미 분류**는 Actor 태그·클래스·이름 규칙이다. 높이만으로 자동으로 Slab라고 분류하지 않는다. 동일 분류는 같은 색이다.
- 기본 규칙에서는 Actor의 `Slab`, `Floor`, `Roller`, `Conveyor` 태그 등을 사용한다. Editor의 화면 표시명(Actor Label)만 바꾸는 것은 Actor Tag 지정과 다르다. 테스트맵의 일반 표적처럼 규칙에 맞는 태그가 없는 물체는 모두 회색일 수 있다. 이 경우 높이 구분은 높이 색상을 사용하고, 의미 구분이 필요하면 해당 Actor 태그 또는 센서의 분류 규칙을 명시적으로 설정한다.
- 높이 자동 범위가 넓어 차이가 잘 보이지 않으면 자동을 끄고 최소/최대 높이를 **m 단위**로 입력한다. 예: 판 상단이 월드 1m, 물체 상단이 1.25m라면 1~1.25를 사용한다.
- 설정은 모니터 전용이다. 다른 개발자의 Widget 스타일·좌표나 실제 센서 측정값을 변경하지 않는다.

기존 저장값은 `ProjectionDefault`로 해석한다. WorldTopDown은 월드 Z, 나머지 투영은 센서 Z다. 명시적인 기준 선택은 투영 변경 후에도 유지한다.

## GPU 의미 분류의 범위

분류가 필요한 측정만 별도 렌더 장면에서 식별용 proxy를 캡처한다. 원본 Mesh의 material, stencil, visibility, collision을 바꾸지 않는다. 물체 ID와 깊이가 같은 acquisition에 묶이며, 원래 깊이와 1cm 넘게 다르면 미분류로 남긴다.

불투명 StaticMesh/ISM/HISM을 대상으로 한다. 투명·마스크·WPO 재질 및 지원되지 않는 geometry는 분류를 추정하지 않는다. 원래 측정 깊이는 그대로 유지된다. 새 컴포넌트 발견에는 최대 0.5초가 걸릴 수 있고, 등록된 물체 이동은 매 측정에 갱신된다. GPU 분류 실패를 CPU 전체 스캔으로 숨기지 않는다.

센서 전용 재질은 `/Game/MA0T10/Sensor/Materials/M_LidarSemanticId`다. 신규 이식 시 해당 자산도 함께 옮긴다. `Scripts/create_lidar_semantic_material.py`는 이 자산이 없을 때만 생성하며 사용자 맵을 저장하지 않는다.

## 수신 PCD의 Z는 월드 높이가 아니다

기존 계약을 유지한다: PCD XYZ는 측정 당시 센서 기준, X 전방/Y 좌측/Z 위쪽, 단위 m. 센서가 아래를 바라보면 현장 높이는 주로 로컬 X 방향 차이에 나타난다. 값을 왜곡해서 Z로 옮기지 않는다.

Binary 레코드는 `FIELDS/SIZE/TYPE/COUNT`를 읽어서 해석한다. 현재는 33바이트, little-endian이며 C++ 구조체 메모리 정렬과 다르다. 12바이트 XYZ만 있다고 가정하거나 36/40바이트 padding을 붙여 읽으면 두 번째 점부터 잘못된다.

PCD 주석 `# MA0T10_META {...}`에는 실제 acquisition snapshot이 있을 때 아래 선택 필드가 들어간다.

- `coordinate_frame`, `coordinate_units`, `coordinate_axes`
- `sensor_position_world_m`, `sensor_rotation_ue_xyzw`
- `sensor_to_world_m`: row-major 4×4, column vector 적용. UE 월드 meter로 변환하며 Y 반전을 포함한다.

`world = matrix × [pcd_x, pcd_y, pcd_z, 1]`로 변환한다. quaternion만 적용하면 Y축 handedness 변환을 놓칠 수 있다. 판 기준 높이는 변환한 world Z에서 판 상단 world Z를 뺀다. 기울어진 판이라면 평면 방정식까지 필요하며 이번 기능은 평면 추정을 제공하지 않는다.

예제:

```powershell
python Scripts/inspect_binary_pcd.py received.pcd --plate-z-m 1.0
```

독립 파서는 표준 라이브러리만 사용하며 필드 길이·본문 크기·유한 XYZ를 검사한다. acquisition pose가 없는 외부 주입 프레임에는 가짜 identity transform을 넣지 않는다. 이 경우 수신자는 센서 설치 정보를 별도로 확보해야 한다.

Slab 소재 번호·Slab 프레임·시나리오/실행 UUID는 기존 의미를 유지한다. PCD 센서 프레임과 Slab 프레임은 별개다. Binary point layout, Topic, 기존 schema는 바뀌지 않는다.

## 회귀 기준과 증거

비교 기준은 PR #16 직전 `f7ec4ab4`, PR #16 `94811a67`, 현재 소스다. GPU 의미 분류 미지원과 센서 로컬 XYZ는 이미 PR #16 이전에도 존재했으므로 해당 PR만을 원인으로 단정하지 않는다.

`MA0T10.LidarRegression`은 로컬 XYZ/Binary 복원, 25cm 높이, 좌표 행렬과 GPU 장면 검사를 포함한다. GPU 테스트의 NullRHI skip은 GPU 통과가 아니다. 실제 로그와 성능 보고서는 `Saved/Reports`에서 확인하며, 문서 자체는 실측 성능 통과를 의미하지 않는다.
