# 센서 도구 UI Workspace

모니터·센서 설정·데이터·Slab 재생 목록·Slab 차트·Slab 진행의 **6개 명시적 등록 패널**을 관리합니다. 외부 Chart 플러그인은 필요하지 않습니다. 동료가 공통 부모로 만든 Widget, 사용자 WBP의 Designer, DTCore의 전역 스타일은 일괄 변경하지 않습니다.

## 상단 메뉴와 창 조작

| 상단 항목 | 용도 |
|---|---|
| 선택 센서 | Camera/LiDAR 선택. 모니터와 설정의 SensorId·종류를 함께 변경 |
| 센서 도구 | 모니터 / 설정 / 데이터 패널 열기·숨기기 |
| Slab 도구 | 진행 / 차트 / 재생 목록 열기·숨기기 |
| UI 표시 | 글자 85/100/125/150%, 센서 도구 UI 초기화 |

- 첫 실행은 모니터 중심이며 여러 패널을 동시에 열 수 있습니다. 각 도구 메뉴의 ●/○가 열림 상태를 나타냅니다.
- 제목 드래그와 우하단 resize grip을 사용합니다. 접기는 실제 영역을 제목 높이까지 축소합니다.
- 숨김은 Widget을 파괴하지 않습니다. 다시 도구 메뉴에서 엽니다. 파일 저장·Topic 송신·Slab 재생은 계속됩니다.
- **더보기 → 이 창 위치·크기 초기화**는 그 패널의 크기를 먼저 복원한 후 위치를 계산합니다. 폰트 배율·센서 설정은 그대로입니다.
- **UI 표시 → 센서 도구 UI 초기화**는 등록된 6개 패널의 배치·열림·글자 배율만 초기화합니다. 동료 Widget이나 센서 장비 설정을 초기화하지 않습니다.
- 설정 패널을 숨기면 센서 조작 모드만 종료합니다. 센서 측정은 중지하지 않습니다.
- 조작 중 선택 센서를 바꾸면 이전 센서의 조작을 마무리하고 새 대상에 연결합니다. 미등록 설정 Widget의 독립적인 선택 방식은 유지합니다.

## 기본 화면과 고급 항목

| 패널 | 기본 화면 | 필요할 때 펼치는 항목 |
|---|---|---|
| 모니터 | 큰 미리보기, 센서·프레임·경고 요약, LiDAR 투영·색상·표현 | 고급 보기: 높이 범위, 적용 가능한 overlay, 3D 크기/renderer, 듀얼 카메라, 범례·상세 진단 |
| 설정 | 장비/품질/주요 측정값, 위치·회전 탭 | 고급 탭의 센서 식별, 외부 측정 프레임 입력, 화면 로그, 초기화·원본 맵 반영 |
| 데이터 | 실시간 전송 / 파일 저장 / 연결·진단 **3개 탭** | 추가 송신, 필터·상세 정책, 서버 편집, 수신 검증, 저장 결과·파일 전송 |
| Slab 재생 목록 | 항목 선택·처음부터 재생·PCD 선택 | 추가 Camera/LiDAR 송신, UUID·수신 정보·보호된 개별 삭제 |
| Slab 차트 | 진행된 시간까지의 기울기·중심 이탈·Margin 3개 차트 | 카드별 다른 지표, 시간/frame 축, 보기 확대·이동 |
| Slab 진행 | 소재·진행률·현재 원본 frame, 재생 조작 | 단위·외형·3D 분석 표시 및 실행·센서 세션 상세 |

장비 프로필과 시뮬레이션 품질은 다른 설정입니다. 투영과 색상도 다른 기능이며 제거하지 않습니다. 비활성 고급 항목을 숨겨도 기존 설정값은 유지합니다.

### 모니터의 세 가지 표현

- **2D**: Widget의 영상/투영만 표시합니다.
- **2D + 월드 포인트**: 월드 장면 위에 3D 점군을 함께 표시합니다.
- **포인트 전용**: 기존 Coordinator의 포인트 전용 보기로 전환합니다. 월드 숨김·복원은 기존 동작을 사용합니다.

포인트 전용에서 나오면 기존 상태를 복원한 뒤 선택한 2D/overlay 표현을 적용합니다. 투영·색상·높이 범위 값을 초기화하지 않습니다.

격자와 깊이 경계는 거리 영상 및 거리 영상이 포함된 Split에서만 보입니다. 적응형 거리 색상은 거리 계열 색상에서만 보입니다. 높이 최소·최대 입력은 높이 색상과 수동 범위 설정에서만 사용합니다. 3D 포인트 크기와 renderer는 3D를 표시할 때만 노출합니다.

주 카메라는 상단 선택 센서와 동기화됩니다. 듀얼 보기의 보조 카메라는 고급 보기에서 선택합니다. 두 화면은 기존 RenderTarget을 사용하며 UI를 위해 추가 측정하지 않습니다.

## 데이터 패널 사용

### 실시간 전송

직접 전송과 시나리오 연동은 목적이 다릅니다.

- **직접 전송**: PCD / Camera 이미지 / LiDAR 정보 카드에서 각 대상 SensorId를 확인한 뒤 시작·중지합니다. 기존 전체 종류를 한꺼번에 시작하는 버튼은 기본 화면에 노출하지 않습니다.
- **시나리오 연동**: 다음 신규 Slab 실행의 출력 정책입니다. 기본 PCD 켜짐, Camera/LiDAR 추가 출력은 접힌 옵션에서 선택합니다. 이미 시작된 실행의 정책을 소급 변경하지 않습니다.
- 현재 Slab 세션이 제어하는 센서의 수동 버튼은 시나리오 연동 중으로 비활성화합니다. Slab 진행 패널에서 해당 실행을 제어하세요.
- 저장 목록 재실행의 출력은 재생 목록에서 별도로 선택하며 기본은 모두 꺼짐입니다.

실시간 PCD는 Binary PCD 고정입니다. 파일 저장 포맷과 다릅니다. Raw TCP는 완료 프레임 전체와 매 프레임 receipt를 사용하므로 호환 모드용 전송 간격·receipt 표본 입력을 숨깁니다. PCD 필터와 큐·오류 세부 정보는 전송 필터·상세 정책에서 확인합니다.

Broker 수락(receipt), 실제 소비자 검증, 로컬 파일 완료는 서로 다른 상태입니다. 접수됐다는 이유로 파일 저장이나 소비자 처리가 완료된 것으로 표시하지 않습니다.

### 파일 저장: 세 동작의 차이

| 동작 | 대상 프레임 | 측정과 실패 정책 |
|---|---|---|
| 새 프레임 저장 | 요청 이후에 완료되는 새 프레임 | 측정 실행 중이어야 합니다. 비동기로 기다리며 새 프레임 확보에 2초 이상 걸리면 실패합니다. 이전 프레임으로 대체하지 않습니다. |
| 현재 프레임 저장 | 요청 때 고정한 마지막 완료 프레임 | 추가 측정을 만들지 않습니다. 해당 출력으로 저장할 프레임이 없으면 실행할 수 없습니다. Camera 프레임 읽기 대기는 최대 10초입니다. |
| 주기 저장 | 매 저장 deadline의 최신 완료 프레임 | 별도 스캔/캡처를 반복하지 않습니다. busy·새 프레임 없음·같은 FrameId는 저장 주기를 생략하며 누적 작업을 쌓지 않습니다. |

출력 선택은 현재 센서 종류에 맞춰 표시합니다.

- Camera: JPEG 이미지 / Camera JSON.
- LiDAR: 포인트 클라우드 / LiDAR JSON.
- Point Cloud를 선택했을 때만 CSV/JSONL/PCD/LAS/LAZ 파일 형식을 표시합니다.
- 파일 탭을 통합했지만 이전 Capture/Export enum과 Blueprint API는 유지합니다. 기존 현재 프레임 내보내기 포맷과 새 프레임·주기 캡처 포맷 저장값도 구분해 복원합니다.

저장 요청은 **Actor, SensorId, 출력 선택, 선택된 acquisition snapshot 및 측정 설정 revision**을 고정합니다. 요청 후 모니터 센서를 바꿔도 저장 대상이 바뀌지 않습니다. 자연스러운 센서/Crane 이동은 이미 고정한 측정 프레임을 취소하지 않으며, 센서 삭제·중요 측정 설정 변경·월드 종료는 취소 사유입니다.

주기 저장은 시작 당시 Actor·출력·간격을 고정합니다. 간격은 0.05~3600초이며 Topic 전송 주기와 독립적입니다. 센서 주기 사용도 UI의 허용 범위 안에서 적용합니다. UI 값을 바꾸어 실행 중인 주기를 바꾸지 말고 중지 후 새로 시작하세요.

주기 상태의 **완료 프레임**은 파일 개수가 아니라 저장 요청/센서 프레임 수입니다. 한 프레임을 JPEG+JSON 두 파일로 저장해도 완료 프레임은 1입니다. **저장 주기 생략**은 busy/hitch/중복·미완료 프레임으로 생략한 저장 deadline 수이며 센서 acquisition 누락 수가 아닙니다. 중지하면 새 접수만 멈추고 이미 접수한 파일은 마무리합니다.

### 외부 Camera JSON과 LAZ 제한

- 검증된 외부 Camera JSON만 있고 로컬 JPEG snapshot이 없다면 **Camera JSON만** 저장할 수 있습니다. 원문 JSON과 그 취득 시각·FrameId를 보존합니다.
- 그 상태에서 JPEG를 선택하면 대체 RenderTarget 이미지나 가짜 JPEG를 만들지 않습니다. 주기 저장은 선택 출력 미지원으로 대기·생략합니다. JPEG를 해제한 새 세션을 시작하세요.
- 완료 프레임 자체가 없음, 새 프레임 없음, writer busy, 선택 출력 미지원을 구분합니다. 입력 파싱 오류는 센서의 원래 진단을 유지하며, 실제 파일 파싱·직렬화 실패를 단순 대기로 바꾸지 않습니다.
- LAZ에는 실제 외부 압축 실행 파일과 `{input}`, `{output}` 인수가 필요합니다. LAS 바이트에 확장자만 붙여 성공 처리하지 않습니다.
- LAZ는 외부 프로세스·디스크 비용이 있으며 고주기 저장 성능 보장 대상이 아닙니다. 현재 외부 압축 프로세스에는 별도 실행 시간 제한이 없으므로 정상 동작이 확인된 도구를 사용하세요. **2초는 새 프레임 확보 대기 제한이지 압축·디스크 완료 시간 보장이 아닙니다.**

### 저장 서비스 API

기존 Monitor의 호환 캡처 API는 유지하지만, 등록된 새 데이터 UI는 다음 월드 서비스에 파일 작업을 맡깁니다. 서비스는 Widget 수명·현재 선택·창 열림 상태에 의존하지 않습니다.

| 클래스 | 공개 호출 |
|---|---|
| `UVirtualSensorFileSaveSubsystem` | `RequestSave(NewFrame/CurrentFrame, Actor, Selection)`, `GetRequestStatus`, `GetRecentRequests`, `CancelRequest`, `CanSaveCurrent`, `OnRequestUpdated` |
| `UVirtualSensorPeriodicFileSaveSubsystem` | `StartPeriodicSave(Actor, Selection)`, `StopPeriodicSave(SessionId)`, `GetPeriodicStatus`, `GetRecentPeriodicSessions`, `OnPeriodicUpdated` |
| 데이터 패널 호환 진입점 | `RequestFileSave(Mode)`, 기존 캡처·내보내기 함수와 optional binding |

C++ 연동용 파일 서비스에는 `GetAvailableFrameId`, `GetSensorSaveRevision`, `HasPendingSave`, `CanAcceptSave`, `RequestSaveInCaptureSession`도 있습니다. 새 요청 ID가 반환됐다고 성공으로 간주하지 말고 **Accepted → Waiting → Saving → Succeeded/Failed/Cancelled** 상태를 확인합니다.

파일 writer는 월드당 최대 2개, 센서별 진행 중 요청은 1개입니다. 주기 저장은 동일 Actor의 중복 세션을 거부하고 진행/마무리 중 세션 최대 16개·최근 기록 최대 32개를 유지합니다. busy 시 최신 프레임 확인 의도 플래그만 1개 유지하며 프레임 배열 대기열을 쌓지 않습니다.

주기 저장 위치는 다음과 같습니다.

- `Saved/SensorCaptures/LocalTimedCapture/<UTC session>/Camera`
- `Saved/SensorCaptures/LocalTimedCapture/<UTC session>/Lidar`

저장 위치·최근 결과의 폴더 열기·최근 파일·경로 복사는 공통 도구입니다. **파일 전송 도구 → 최근 파일 수동 전송**은 로컬 저장과 별도 동작입니다.

### 연결·진단

기본은 적용된 서버·backend 요약입니다. 서버 연결 설정, Broker 실제 수신 검증, 전송 기록·호환 도구를 필요할 때 엽니다. 비밀번호·Bearer token은 세션에만 유지합니다.

Settings의 **외부 측정 프레임 입력**은 센서를 구동할 입력 Source이며, 여기의 **Broker 실제 수신 검증**은 송신 결과를 확인하는 진단입니다. 두 기능의 시작/중지 버튼을 같은 연결 기능으로 취급하지 않습니다. 수신 진단을 중지해도 센서 측정·발행과 Broker receipt 처리를 중지하지 않습니다.

## 배치·저장·영향 격리

`UVirtualSensorToolWorkspaceSubsystem`은 다음 Host가 명시적으로 등록한 인스턴스만 관리합니다.

- `AVirtualSensorUiHostActor`: 모니터·설정·데이터.
- `ASlabScenarioReplayUiHostActor`: 재생 목록.
- `ASlabSimulationUiHostActor`: Slab 차트·진행.

Main AddWidgetPanel을 우선 사용하고 없으면 Viewport에 배치합니다. private Canvas 안에서 우리 패널의 순서만 바꾸며, 클릭 시 DTCore의 다른 OpenWidgets 목록으로 ZOrder를 덮어쓰지 않습니다. 공통 부모를 상속했거나 이름이 비슷하다는 이유로 자동 등록하지 않습니다.

배치·크기·접힘·열림·글자 배율은 `Saved/SaveGames/MA0T10_SensorToolWorkspace_v1.sav`에 저장합니다. 기존 enum 순서와 저장 슬롯은 유지하며 이전 파일·센서 설정·동료 데이터를 삭제하거나 일괄 재저장하지 않습니다. 초기 Canvas geometry가 준비되기 전의 임시 최소 크기는 저장하지 않고, 실제 배치 후 복원합니다. Editor 테스트 월드에서 배치 파일을 자동 작성하지 않습니다.

등록된 native 컨트롤과 `RegisterSensorTextControl`/`RegisterSensorInputControl`로 명시적으로 등록한 컨트롤만 폰트를 변경합니다. 중첩된 다른 UserWidget 내부는 순회하지 않습니다. 전용 brush·button·input·combo 스타일은 우리 소유 UI에만 적용하며 엔진·DTCore 전역 스타일을 수정하지 않습니다.

사용자 제작 WBP Designer와 optional binding, 기존 Blueprint 함수, 미등록 native 패널의 기존 UI 경로는 유지합니다. 제공된 빈 Designer WBP는 간소화된 native fallback UI를 사용합니다. 패널 표시/숨김은 캡처나 재생을 끊는 API가 아닙니다.

공개 Workspace API는 `RegisterOwnedPanel`, `SetPanelOpen`, `BringOwnedPanelToFront`, `ResetOwnedPanelLayout`, `ResetOwnedWorkspaceLayout`, `SetOwnedPanelFontScale`입니다. 미등록 Widget에는 적용되지 않습니다.

## 이번 재구성의 검증 상태

### 2026-09-22 가독성·자동 실행 후속 변경

- 우리 모니터의 기본 화면에는 영상, SensorId·프레임·Hz, 한 줄 경고만 남깁니다. 투영·색상·포인트 표시·듀얼 카메라·범례·진단은 `보기 설정`에서 사용하며 기존 API와 선택값은 유지합니다.
- 우리 데이터 패널은 새 PIE의 첫 생성에서 `실시간 전송 → 시나리오 연동`으로 열립니다. 실행 중 선택한 탭·숨김·재호스팅 상태는 유지하고 기존 SaveGame의 탭 값은 삭제하지 않습니다.
- 신규 Slab 출력 기본값은 PCD만 켜짐입니다. Actor에 명시된 출력 선택은 보존하고 화면을 여는 것만으로 송신하지 않습니다. 기본 화면의 `설정 준비됨`은 Broker 수락이나 실제 수신 완료라는 뜻이 아닙니다.
- 송신 설정이 부족한 신규 벌크는 센서 제어 없는 관찰 fallback으로 움직임을 유지하며 이유를 표시합니다. 중복·잘못된 UUID·현재 실행 중 수신은 자동 실행하지 않습니다. 이식 설명과 3D 표시 설정은 `slab_simulation.ko.md`를 참고하세요.
- 후속 검증 로그·실제 밝은 장면 화면은 `Saved/Reports/SlabVisibility`에 기록합니다. 아래 2026-09-21 기록은 이전 작업의 증거로 보존합니다.

2026-09-21, `5d3a735` → `codex/sensor-ui-simplification`의 확인된 결과입니다. 실행 증거는 커밋하지 않는 `Saved/Reports/UiSimplification/report.ko.md`와 해당 폴더의 로그·PNG·JSON에 있습니다. 아래 이전 Workspace 구현의 결과와 구분합니다.

최종 격리 보완 후 전체 자동화 재실행과 포인트 전용 모드의 직접 마우스 확인을 마쳤습니다. 포인트 전용 상태에서 **frame 1509 → 1849 → 2227, 검출점 32,256개 유지**를 확인하고 `2D+월드 포인트 → 2D` 복원도 확인했습니다. `manual_point_only_isolated.png`에 실제 화면을 남겼습니다. 이제 플레이어 뷰만 숨기며 원본 Component의 전역 가시성과 센서 SceneCapture는 변경하지 않습니다.

### 직접 조작한 범위

- Computer Use로 New Editor Window PIE를 직접 실행했습니다. 1280×720과 1920×1080을 요청했지만 실제 확보한 viewport는 **1174×683**입니다. 목표 두 해상도를 모두 검증했다고 표시하지 않습니다.
- 85/100/125/150% 글자 배율, 차트·데이터 창 resize, 접기/펼치기 크기 복원, 패널 숨김/다시 열기, Camera→LiDAR 설정 동기화를 조작했습니다. 작은 창의 150% 글꼴에서는 스크롤 또는 창 확대가 필요합니다.
- 재생 목록의 전체 UUID와 선택 행 동작, 제목 드래그를 확인했습니다. 30초 관찰 시나리오를 **21.40초에 일시정지·중단**하고 세 차트가 같은 시점까지만 표시되며 중단 뒤 곡선이 유지되는 것을 확인했습니다.
- 마우스로 현재 PCD를 저장해 실제 파일 생성을 확인했습니다. 0.1초 주기 저장을 시작한 뒤 데이터 창을 숨겨도 파일 생성이 계속됐고, 이후 중지했습니다. 접수 메시지만을 파일 저장 성공으로 판정하지 않았습니다.
- 주요 화면 증거는 `manual_charts_125.png`, `manual_progressive_charts.png` 및 실제 viewport를 적은 각 `.md`입니다. 자동화 화면 캡처와 직접 마우스 검증은 구분합니다.

### 자동화·자산 검사

| 기록 | 실제 수행 통과 | 실패 | 조건부 skip | 비고 |
|---|---:|---:|---:|---|
| `all_final_verified.log` | 154 | 1 | 10 | 총 165개 완료 기록. 엔진 Success 164건에는 skip 10건이 포함됨 |
| 같은 최종 로그의 `MA0T10.SensorFiles` 부분집합 | 9 | 0 | 0 | 실제 D3D12 저장·격리·취소 검사 포함. 전체 수에 중복 합산하지 않음 |

이전 기록인 `all_automation_v3.log`는 실제 통과 149·실패 1·skip 10, `files_final.log`는 7/7 통과였습니다. 최종 결과에는 `CameraCaptureDispatchIsolation`, `CancelDuringAdmission`, `RealRhiWorkflow`를 포함한 파일 테스트 9개와 `MA0T10.SensorWorkspace.PointCloudViewIsolation`의 통과가 확인됩니다. 새 결과로 이전 실행의 미수행 항목을 소급 통과 처리하지 않습니다.

유일한 전체 자동화 실패는 `MA0T10.EditorSmoke.MapSensorComposition`의 `LiDAR is placed at realistic test height`입니다. 운영 `SensorTestMap`에 LiDAR Z=150±1cm가 있어야 한다는 기존 fixture 검사이며, 이를 통과시키기 위해 사용자 로컬 운영맵을 덮어쓰지 않았습니다. 로그에는 실제 Z값이 없으므로 이 오류를 UI 기능 실패로 바꾸어 설명하지 않습니다.

파일 집중 검사에는 선택 센서를 바꾼 뒤에도 저장 Actor·FrameId가 유지되는지, 실제 JPEG와 JSON 이미지가 같은지, Binary PCD 메타데이터, 숨김 중 주기 저장, 중복 방지와 설정 변경 취소가 포함됩니다. `final_related.log`는 파일·UI·Workspace 집중 실행 기록이며 `file_save_rhi.json`은 **가장 최근 실행으로 덮어쓰는 보고서**이므로 다른 시각의 로그와 같은 실행으로 혼동하지 않습니다.

`assets_final.log`에서 최종 빌드의 6개 WBP를 메모리에서 컴파일하고 native parent를 확인했으며 오류는 0건입니다. 자산은 저장하지 않았습니다. `validate_sensor_v2_refactor.ps1 -RequireAssets`도 통과했습니다. 기본 `validate_sensor_tool_widgets.py`의 원래 4개 검사만으로 6개 전부를 검증했다고 주장하지 않고 Slab 차트·진행을 포함한 별도 검사 결과를 사용합니다.

### Artemis 변경 전후 비교

같은 SaveGame과 Slab 테스트맵, **ML-X Native LiDAR 한 대·PCD 전용** 구성에서 10초 워밍업 후 30초 시나리오를 두 번 실행했습니다. 자동화 viewport는 **753×403**이며 1920×1080 성능 또는 수동 마우스 검증의 대체가 아닙니다.

| 항목 | 변경 전 | 변경 후 |
|---|---|---|
| PCD 제출 / Broker receipt / 내부 소비자 검증 | 각 1,200건 | 각 1,200건 |
| 독립 외부 구독자 수신 | 600 + 600건 | 600 + 600건 |
| 실제 30초 구간 수신율 | 각 20Hz | 각 20Hz |
| 평균 engine FPS, 1회 / 2회 | 59.999 / 59.996 | 59.999 / 59.999 |
| 1% low, 1회 / 2회 | 59.946 / 59.689 | 59.974 / 59.978 |
| engine frame p95 | 두 실행 모두 16.667ms | 두 실행 모두 16.667ms |
| invalid / gap / duplicate / overflow | 모두 0 | 모두 0 |

근거는 `before_external.json`, `after_final_external.json`, `before_perf.log`, `after_final_perf.log`와 `Saved/Reports/Slab/<실행 UUID>.json`입니다. 최종 격리 수정까지 빌드한 뒤 성능 시험을 다시 실행했습니다. 이전 중간 결과 `after_external.json`도 별도로 보존합니다. 외부 보고서의 전체 65초 평균 약 18.46Hz는 유휴 5초를 포함하므로 실제 실행 구간 20Hz와 구분합니다. engine FPS는 World DeltaSeconds 기반이며 수신 callback wall-clock 간격과 같지 않습니다. **독립 UI CPU 타이밍과 전송 e2e p95는 수집하지 않았으므로 통과로 표기하지 않습니다.** 이 결과를 Camera/LiDAR/PCD 3-stream 성능 결과로 확대하지 않습니다.

두 목표 해상도×네 배율의 모든 옵션 전수 조작, 실제 동료 프로젝트 Widget, 실제 외부 LAZ 압축 도구 및 장시간 soak는 완료하지 않은 별도 범위입니다. 동료 Widget의 공통 부모·미등록·중첩 소유권 격리는 자동 검사했지만 제공되지 않은 동료 화면까지 직접 검증한 것은 아닙니다. 포인트 전용 숨김은 진입 당시 primitive에 적용하며, 실행 중 새로 추가한 Actor까지 추적하는 기능은 이번에 확장하지 않았습니다.


## 이전 검증 기록

아래 항목은 이번 간소화 이전 Workspace 구현의 기록입니다. 이번 UI의 수동 조작·변경 후 성능 통과 결과로 재사용하지 않습니다.

- `MA0T10.SensorWorkspace.IsolationAndStorage`: 공통 부모 기반 외부 Widget 등록 거부, 다른 UserWidget 내부 글꼴 등록 거부, 미등록 폰트/크기/위치 보존, 기존 SaveGame 바이트 보존, 같은 인스턴스의 숨김/복원.
- `MA0T10.SensorWorkspace.Runtime`: 실제 PIE에서 숨긴 모니터의 JPEG 저장 완료, 같은 Slate 인스턴스 복원, 설정 숨김 시 조작 종료, 여러 도구 열기, 재생 Host의 지연 생성, 확대 후 초기화.
- `Scripts/validate_sensor_tool_widgets.py`: 네 WBP를 컴파일하고 native parent를 검사합니다. 맵이나 자산을 저장하지 않습니다.
- 전체 자동화의 조건부 RHI/외부 서버 검사는 생략 표시와 실제 별도 실행 결과를 구분합니다.
- 로컬 Artemis RHI 결과는 `Saved/Reports/sensor_workspace_pcd.*` 및 최종 재검증 보고서에 기록합니다. 첫 검증은 PCD 1,200건 제출/receipt/수신 일치, gap/invalid/duplicate 0, 제출 20.01Hz, 평균 엔진 FPS 59.96, 엔진 frame p95 16.67ms였습니다. callback wall pacing p95는 별도 값 22.63ms입니다.
- 이전 보관 보고서와 비교할 때 센서 구성과 측정 구간은 확인해야 합니다. 같은 날 원본 커밋을 다시 빌드한 엄격한 A/B 비교는 수행하지 않았습니다.
- Computer Use에서 도구 막대와 기본 모니터 배치를 확인했지만 도중 모니터 캡처가 0x80070057로 실패했습니다. 1920×1080/1280×720의 전체 직접 조작, 모든 보조 패널의 시각적 clipping, 실제 동료 Widget 인스턴스 검증은 완료로 표시하지 않습니다. 자동 RHI 캡처는 직접 마우스 검증의 대체가 아닙니다.
