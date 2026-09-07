# 센서 도구 UI Workspace

모니터·센서 설정·데이터·Slab 재생 네 패널만 정리한 런타임 UI입니다. 동료가 공통 부모를 상속해 만든 차트/진행률 Widget에는 자동 적용하지 않습니다. 센서 측정이나 송신 계약은 바꾸지 않습니다.

## 사용 방법

- 첫 실행은 모니터 중심입니다. 상단에서 센서를 선택하고 `설정`, `데이터`, `재생`을 열 수 있습니다. 여러 도구를 동시에 열어 두어도 됩니다.
- 제목을 드래그하고 우하단 모서리로 크기를 조절합니다. `접기`는 제목 영역만 남기고, `숨김`은 도구 막대에서 다시 열 수 있습니다.
- `초기화`는 해당 패널의 크기를 먼저 복원하고 위치를 다시 계산합니다. 기존 확대 후 위치 오차를 이 경로에서 수정했습니다.
- `UI 표시`에서 85/100/125/150%를 선택합니다. 등록된 네 패널만 바뀌며 본문은 줄바꿈/스크롤을 사용합니다. 센서 도구 전체 초기화도 네 패널의 배치·열림·글자 배율만 초기화합니다.
- 설정을 숨기면 센서 조작 모드만 종료합니다. 캡처·송신·시나리오 재생은 패널 숨김과 별개로 계속됩니다.

| 패널 | 주요 구성 |
|---|---|
| 모니터 | 큰 미리보기, 프레임/주기/경고, 접이식 표시 옵션·범례·상세 진단 |
| 설정 | 기본 설정 / 위치·회전 / 고급 설정, 항목별 도움말 |
| 데이터 | 실시간 전송 / 캡처 / 파일 내보내기 / 연결 진단, 공통 저장 결과 영역 |
| 시나리오 | 간결한 목록, 선택 항목 상세, 고정된 재생·PCD 조작부 |

주 카메라는 상단 센서 선택을 사용하고 듀얼 보기의 보조 카메라는 모니터 표시 옵션에서 선택합니다. LiDAR 투영·색상·오버레이·포인트 클라우드 모드는 유지됩니다. 캡처는 새 프레임을 요청하는 작업이고 파일 내보내기는 기존 결과를 저장하는 작업이므로 두 기능을 삭제하거나 합치지 않았습니다.

전송 카드에서는 대상 SensorId, backend, 입력/제출 Hz, Broker 수락과 소비자 검증을 구분합니다. PCD 카드가 먼저 표시됩니다. 상세 큐/오류와 호환 모드 간격 옵션은 펼쳐서 확인합니다. Raw TCP 고성능 모드와 Engine STOMP 호환 형식을 혼동하지 않도록 안내를 정리했습니다.

## 배치와 저장

`UVirtualSensorToolWorkspaceSubsystem`은 월드마다 하나이며 명시적으로 등록한 패널만 관리합니다. 첫 기본 배치는 오른쪽 모니터와 왼쪽의 어긋난 도구 배치입니다. 작은 화면은 최소 크기와 화면 경계를 우선하며, 동시에 여러 창을 열면 사용자가 원하는 위치로 옮길 수 있습니다.

`Saved/SaveGames/MA0T10_SensorToolWorkspace_v1.sav`에 네 패널의 위치·크기·접힘·열림 상태와 폰트 배율만 저장합니다. 첫 생성 때 기존 센서 글자 배율을 읽어 올 수 있지만 이전 UI 저장 파일은 삭제하지 않습니다. 센서 프로필·출력 설정·동료 패널 데이터는 기존 경로에 유지합니다. 에디터 테스트 월드에서는 전용 배치 파일도 자동으로 쓰지 않습니다.

패널을 숨겼다 열거나 앞에 가져올 때 인스턴스를 새로 만들지 않습니다. 부모 Main Canvas가 바뀌면 루트만 재호스팅하며 Slate 참조를 유지합니다. 모니터의 대기 캡처/readback 처리는 Workspace의 월드 tick에서 처리하므로 UI가 숨겨져도 완료됩니다.

## 동료 Widget 보호와 WBP 연결

- 기존 `UDxWidget`, Main 및 공용 스타일을 전역 변경하지 않습니다. 공통 부모의 새 연결점은 미등록 상태에서 작동하지 않습니다.
- 등록은 `AVirtualSensorUiHostActor`의 세 패널과 `ASlabScenarioReplayUiHostActor`의 재생 패널 생성 경로에서 수행합니다. 공통 부모를 상속했다는 이유로 검색/등록하지 않습니다.
- `RegisterOwnedPanel`은 역할과 구체적인 패널 타입을 검사합니다. 일반 공통 부모 기반 Widget은 거부합니다.
- private 루트 Canvas 안에서 우리 패널의 앞뒤 순서만 바꿉니다. Main의 다른 자식이나 동료의 ZOrder를 재정렬하지 않습니다.
- 글꼴은 등록된 native 컨트롤 및 `RegisterSensorTextControl`/`RegisterSensorInputControl`로 직접 등록한 UMG 컨트롤만 변경합니다. 다른 UserWidget의 내부 TextBlock은 등록할 수 없습니다.
- 기존 WBP 이름·native parent·Blueprint 함수·optional binding은 유지합니다. 제공된 빈 Designer WBP는 새 native fallback UI를 사용합니다. 직접 Designer를 만든 WBP는 원래 Designer 배치를 유지하며 임의로 재작성하지 않습니다.

기존 센서 Host를 사용하는 맵은 재생성하지 않아도 적용됩니다. 재생 패널은 상단 `재생`에서 처음 열 때 Host를 한 번 생성합니다. adapter나 시나리오가 없으면 재생 버튼만 비활성화합니다. 실제 동료의 재생 adapter 연결은 [시나리오 가이드](slab_scenario_replay.ko.md)를 따릅니다.

공개 API는 `RegisterOwnedPanel`, `SetPanelOpen`, `BringOwnedPanelToFront`, `ResetOwnedPanelLayout`, `ResetOwnedWorkspaceLayout`, `SetOwnedPanelFontScale`입니다. `SetPanelOpen(..., false)`는 데이터 작업의 중지 API가 아닙니다.

## 검증과 제한

- `MA0T10.SensorWorkspace.IsolationAndStorage`: 공통 부모 기반 외부 Widget 등록 거부, 다른 UserWidget 내부 글꼴 등록 거부, 미등록 폰트/크기/위치 보존, 기존 SaveGame 바이트 보존, 같은 인스턴스의 숨김/복원.
- `MA0T10.SensorWorkspace.Runtime`: 실제 PIE에서 숨긴 모니터의 JPEG 저장 완료, 같은 Slate 인스턴스 복원, 설정 숨김 시 조작 종료, 여러 도구 열기, 재생 Host의 지연 생성, 확대 후 초기화.
- `Scripts/validate_sensor_tool_widgets.py`: 네 WBP를 컴파일하고 native parent를 검사합니다. 맵이나 자산을 저장하지 않습니다.
- 전체 자동화의 조건부 RHI/외부 서버 검사는 생략 표시와 실제 별도 실행 결과를 구분합니다.
- 로컬 Artemis RHI 결과는 `Saved/Reports/sensor_workspace_pcd.*` 및 최종 재검증 보고서에 기록합니다. 첫 검증은 PCD 1,200건 제출/receipt/수신 일치, gap/invalid/duplicate 0, 제출 20.01Hz, 평균 엔진 FPS 59.96, 엔진 frame p95 16.67ms였습니다. callback wall pacing p95는 별도 값 22.63ms입니다.
- 이전 보관 보고서와 비교할 때 센서 구성과 측정 구간은 확인해야 합니다. 같은 날 원본 커밋을 다시 빌드한 엄격한 A/B 비교는 수행하지 않았습니다.
- Computer Use에서 도구 막대와 기본 모니터 배치를 확인했지만 도중 모니터 캡처가 0x80070057로 실패했습니다. 1920×1080/1280×720의 전체 직접 조작, 모든 보조 패널의 시각적 clipping, 실제 동료 Widget 인스턴스 검증은 완료로 표시하지 않습니다. 자동 RHI 캡처는 직접 마우스 검증의 대체가 아닙니다.
