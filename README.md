# UE-DT-Project — 가상 센서와 Slab 시뮬레이션

기준: **`codex/architecture-skeleton` (분기 `fda25e0`, Runtime `2c453d2`) / DTCore `b2504d1`, 2026-10-12**. 작업·빌드·검수·Editor 실행은 **`C:\Unreal Projects\ma0t10_dt`** 하나에서 한다(`m7at10_dt`는 무관한 다른 프로젝트). Unreal Engine 5.3 C++ 프로젝트로 Camera·LiDAR 측정/미리보기/송신과 Slab 벌크 시나리오의 보간 이동·분석 표시·차트·재실행을 제공한다.

**아직 독립 센서 플러그인이 아니며, 이번 버전에서는 플러그인 제작을 시작하지 않는다.** 현재 개발은 기능 추가보다 **프로젝트 안의 아키텍처 뼈대 재정비(ARCH 단계)**가 우선이다. 동작·wire 계약·Blueprint API를 바꾸지 않고 레이어 경계·폴더 책임·검증 진입점을 정리한다. 상세 순서는 [로드맵 A단계](docs/ROADMAP.md#a단계--아키텍처-뼈대-재정비-현재-진행)와 [개선 목록 ARCH 항목](docs/IMPROVEMENTS.md#0-arch--아키텍처-뼈대-재정비-현재-최우선)을 따른다.

## 관리 문서

| 문서 | 읽는 목적 |
|---|---|
| [AGENTS.md](AGENTS.md) | **작업 전 필수.** 권한·보호 범위·레이어 규칙·검증/완료 규칙·에이전트 협업 방식 |
| [README.md](README.md) | 현재 기능, 실행·연동 방법, 입출력 계약, 아키텍처 뼈대, 현재 검증 요약 |
| [보완 필요 사항](docs/IMPROVEMENTS.md) | 항목 ID별 우선순위·근거·완료 조건 |
| [최종 목표 로드맵](docs/ROADMAP.md) | 단계 순서(A → R0~R6)와 단계별 gate |
| [검증 이력 보관](docs/VALIDATION_HISTORY.md) | 과거 날짜별 검증 기록 원문(규칙 문서 아님) |

그 밖의 `docs/` 문서는 경로를 보존한 **고정 참고자료**다(맨 아래 목록). 오래된 UI/성능 설명보다 현재 코드와 이 README를 우선한다. 과거 기록을 현재 보장으로 읽지 않는다.

## 1. 현재 상태 한눈에 보기

| 영역 | 현재 상태 | 남은 핵심 조건 (ID) |
|---|---|---|
| 아키텍처 | 단일 Runtime 모듈 안에 센서·Slab·UI가 섞여 있고 역방향 의존이 있다(§10.2 표). ARCH-01 레이어 gate 도입 | 폴더 재배치·역의존 제거 (ARCH-02~08) |
| 화면 구성(UX) | 흐름 바·배치 정리, 데이터 5탭·신규/재생 출력 편집 단일화 구현(UXR-01~04), Slab 정보 카드·표면 프리셋(UXR-08) | UXR-02 최종 삭제 1건·UXR-03 직접 조작은 보류(열린 전송 화면 성능 검증 완료), UXR-04/08 직접 조작 보류, UXR-05~07 미착수; 산화 스케일은 현재 재질 미지원 |
| 센서 배치·위치 저장 | PIE 추가/복제/소유 삭제·북마크·Undo/Redo·전체 배치 저장/복원 구현, 자동화 통과 | 물리 마우스·첫 사용자 수용·DPI 전수 (UX-01 70%, UI-03 40%) |
| PCD 송신 | 실제1920×1080 2+2 PCD-only 10분·10회 반복 통과, 사전 점검 구현 | 대표 장면·4+4·60분·부하 환경 (RT-05 70%, RT-08 40%) |
| Slab 시나리오 | 수신·보간 이동·차트·재실행·데이터 필수 실행·초기 위치 복귀 구현, 실제 Artemis 통과 | Engine/WSS 필수 모드·장시간 (DATA-01 90%), 재생 p95 20ms 목표 미달 이력 |
| 데이터 정확성·수명 | Raw receipt 상한, Camera FrameId-픽셀 정합, Esc 단일 종료, 0Hz 진단 구현 | 최종 범위 정리·직접 조작 (RT-01/04 90%, RT-02/03 70%, LIFE-01 70%) |
| 자동화 | Runtime `7a63971`: D3D12 전체 `MA0T10` 실제227 Success/조건부 skip20/실패0, UXR-03 기준선241개 이름별 상태 유지+신규6개, WBP7개 메모리 컴파일, UXR-08 자동 렌더24장 | UXR-03 직접 조작 보류(전송 화면 성능 보완 검증 완료), 표준 검증 진입점 (QA-01, ARCH-07) |
| 플러그인·이식 | 미착수. 소스·자산·설정을 함께 옮기는 방식만 가능 | 이번 버전 범위 밖 (PL-01~04는 ARCH 이후) |

UXR-08 근거 보고서: `Saved/Reports/UXR/UXR-08/RESULT.md`(집중33 Success, 전체227 Success/skip20, 렌더24장, 성능2% gate 통과; 직접 조작 보류).

UXR-04 근거 보고서: `Saved/Reports/UXR/UXR-04/RESULT.md`(집중 28 Success, 렌더 8장, 성능 2% gate 통과; 직접 조작 보류).

최신 데이터 UI 근거 보고서: `Saved/Reports/UXR/UXR-03/supplement/RESULT.md`(UXR-03 보완), 기존 5탭/전체 회귀는 `Saved/Reports/UXR/UXR-03/RESULT.md`(커밋하지 않음). 과거 수치는 [검증 이력](docs/VALIDATION_HISTORY.md).

## 2. 현재 가능한 것과 아닌 것

| 영역 | 현재 구현 | 아직 보장하지 않는 것 |
|---|---|---|
| 배치·조작 | PIE 장비 추가/복제/소유 삭제, 기즈모/키보드 조작, pose 북마크·Undo/Redo, 전체 배치 저장/명시적 복원 | Editor 영구 설치·일반 프로젝트 저장 workflow, 모든 종료 경로·대규모 무지연 |
| 측정 | Camera SceneCapture, CPU LiDAR chunk, GPU depth projection, immutable snapshot, 비동기 파생 처리 | 모든 backend의 물리 동등성, 실제 ML-X/D455 패킷·depth stream 동일성 |
| 출력 | JPEG/telemetry/Binary PCD, Raw TCP STOMP worker, 호환 STOMP/HTTP, 비동기 로컬 파일 저장 | 네트워크 장애 중 영구 무손실, 모든 소비자의 업무 처리 완료, 고성능 Raw TCP TLS |
| Slab | 벌크 수신, 보간 이동, 단위·Track 설정, 3D 표시, 3중 점진 차트, 메모리 보관/재실행 | 녹화 영상/과거 월드 복원, 디스크 영구 시나리오 보관, AI 사행 감지 |
| UI | 명시적 소유 패널 7개(배치 포함), 글자 배율·drag·resize·접기·숨김·Main/Viewport 배치 | 타 프로젝트 Widget 전수 검증, 모든 해상도·DPI·3D 라벨 겹침 자동 해결 |
| 이식 | 같은 DTCore 기반 프로젝트에 소스·자산·설정을 함께 옮길 수 있음 | 플러그인 복사 후 클래스 하나만 배치하는 완성된 설치 방식 |

수정 후보와 검증 공백은 [IMPROVEMENTS](docs/IMPROVEMENTS.md)에 있다. 이미 있는 Scheduler·snapshot·비동기 전송을 다시 만드는 대신 경계 조건과 확장성을 보강한다.

## 3. 빌드와 안전한 시작

에셋 복구 재현: Editor 종료 후 `UnrealEditor-Cmd.exe <uproject> -run=RepairTestMapMonitorReference`로 점검하고, 대상 확인 후에만 `-Apply`를 추가한다. 전체 맵 재생성 스크립트는 실행하지 않는다.

- Engine: **UE 5.3** / Editor target: `ma0t10_dtEditor Win64 Development`.
- 모듈: `ma0t10_dt`, `ma0t10_dtEditor`, 조기 설정용 `ma0t10_dtBootstrap`.
- DTCore는 필수 submodule이다. 이번 승인된 작업은 동기화본 `b22af0b`의 새 API를 유지하고 공통 안정화 커밋을 별도 생성한다. parent gitlink는 검증한 plugin 커밋으로 고정한다. enum 반환형 소비 코드는 이관이 필요하며 무수정 호환이라고 설명하지 않는다. [main 최소 수정 계약](Plugins/DTCore/docs/MINIMAL_MAIN_MIGRATION.md)를 따른다.
- 기존 worktree의 DTCore·Game.ini·운영맵·PixelStreaming 변경은 보존한다.

Editor/Live Coding을 종료한 뒤 프로젝트 루트에서:

```powershell
& "C:/Program Files/Epic Games/UE_5.3/Engine/Build/BatchFiles/Build.bat" ma0t10_dtEditor Win64 Development "-Project=$($PWD.Path)/ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE
```

빌드 후 **이미 커밋된 테스트맵을 열고** PIE를 시작한다. 테스트를 위해 운영맵을 재생성하거나 덮어쓰지 않는다.

| 맵 | 용도 |
|---|---|
| `/Game/MA0T10/Maps/Tests/SensorRefactorTestMap` | 센서/카메라 V2 UI·조작·출력 확인 |
| `/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap` | synthetic Slab, 차트·진행·PCD 연계 검증 |
| `/Game/MA0T10/Maps/Tests/LidarSemanticValidationMap` | 같은 재질·태그 없는 판/물체의 형상 구분 |
| `/Game/MA0T10/Maps/Tests/SensorScaleStressMap` | 10,000 정적/1,000 이동 proxy 스트레스 fixture; 성능 보증 아님 |
| `/Game/MA0T10/Maps/SensorTestMap` | 기존 운영 테스트맵; 로컬 사용자 변경 보호 |

맵 생성 스크립트는 신규 fixture가 필요할 때만 대상·기존 자산을 확인하고 실행한다. `SlabScenarioValidationRig`는 센서 ID·프로필·로컬 Broker 등을 바꾸는 **테스트 도구**이므로 운영맵에 옮기지 않는다.

DTCore 계약 메모: DTCore는 main `b22af0b`에서 필요한 오류·종료 안전성만 적용한 `b2504d1`을 사용한다. 기존 센서·Slab 기능을 유지하는 별도 연동 브랜치에서 검증했다. `OnConnected`는 성공/실패를 포함한 구독 완료 callback 집계이며 모든 구독 성공을 보증하지 않는다. custom 수신 binding을 보존하고, 공통 5초 timeout·all-ready 상태기계·World cleanup의 GI 전역 큐 폐기는 포함하지 않는다. 이전 안정화 기록의 해당 API 설명은 현재 계약으로 사용하지 않는다.

## 4. 다른 맵에서 현재 기능 연결

### 센서만 사용

1. `AVirtualSensorCoordinator` 하나와 필요한 `AVirtualCameraSensorActor`/`AVirtualLidarSensorActor`를 배치한다.
2. 고유한 SensorId, Transform, 장비/품질/backend를 지정한다.
3. `AVirtualSensorUiHostActor`를 배치한다. 제공 WBP 또는 native fallback을 사용한다.
4. 외부 입력/자체 수신 진단이 필요할 때만 `AVirtualSensorExternalSourceHostActor`를 추가한다.
5. 서버 송신은 데이터 패널에서 설정을 적용하고 필요한 출력만 시작한다. Widget을 여는 것만으로 송신하지 않는다.

### Slab도 사용

- `ASlabTrackReferenceActor`: 로컬 X 진행, Y 오른쪽, Z 위쪽 및 레일 내부면 기준.
- `ASlabActor`: TrackReference, ReceiverId, 치수/위치 단위, TargetSensorIds, 출력 정책 지정.
- `ASlabSimulationUiHostActor`: 대상 Slab와 차트·진행 WBP 연결.
- 재생 목록은 `ASlabScenarioReplayUiHostActor` 또는 도구 막대의 재생 메뉴로 연다. 운영 Slab가 replay adapter를 등록한다.
- 센서 없는 관찰 실행도 가능하다. 송신을 선택할 때만 센서/Coordinator/STOMP 구성이 필요하다.

현재 맵 저장 예약은 **SensorTestMap 전용 경로가 남아 있다**. 다른 맵의 영구 저장까지 일반화됐다고 생각하지 말고, 대상 검증/비활성화 또는 별도 승인된 이식 작업이 필요하다.

### 이식 시 누락하기 쉬운 항목

- C++ 파일 외에 Build.cs/module API macro/include 경로/`/Script/ma0t10_dt` 경로, DTCore 의존성, TC DataTable 등록을 함께 검사한다.
- Unreal **Migrate**로 WBP/Niagara/material 및 의존 자산을 옮긴다. 수동 파일 복사나 일괄 문자열 치환만으로 완료되지 않는다.
- 센서 자산: 세 센서 WBP, `NS_VirtualLidarPointCloud`, `M_VirtualLidarPointSprite`, GPU 분류용 `M_LidarSemanticId`.
- Slab 자산: 재생·차트·진행 WBP, 기본 표면/분석 재질, `M_SlabAnalysisReadable`, `M_SlabTextReadable`, `F_SlabDiagnostics`.
- 진단 폰트는 offline atlas라 실행 PC에 글꼴 설치가 필요 없다. 제작용 재생성 스크립트는 Malgun Gothic을 사용한다.
- 공용 설정 객체와 단일 Bootstrap 진입점은 아직 구현되지 않았다. 자세한 분리 계획은 [ROADMAP](docs/ROADMAP.md)을 따른다.

## 5. UI와 조작

소유 상단 작업 흐름 바는 **장비 ▾ / 배치 / 미리보기 / 시나리오 / PCD 전송 / 결과 / 도구 ▾**다. 장비 선택에는 종류·SensorId·측정/정지/정체/오류 상태가 표시되고 배치 창과 같은 선택을 공유한다. 기존 `장비 추가·위치 저장` 진입점은 **배치**로 짧게 표시한다. Host가 명시적으로 등록한 인스턴스만 관리하며 다른 Widget의 스타일·입력·저장값을 건드리지 않는다.

- **시나리오**는 기존 진행과 보관 목록 인스턴스를 함께 연다. 새 기본 배치는 나란히 놓이며, 저장된 개인 배치가 우선한다. 진행 창의 `시나리오 차트` 버튼은 기존 차트를 열고 다시 누르면 숨긴다. `도구 → 시나리오 차트` 접근도 유지한다. 3D 표시·단위·현재 외형은 진행 창의 **표시 설정** 접힘에 모았다. 출력 정책은 이 화면에서 편집하지 않는다.
- Slab 정보는 **소재 / 진행 / 자세·위치**의 라벨·값 그리드다. 기본 카드의 현재 표시 위치·속도와 접힌 상세의 원본 행 위치·속도를 구분하며, 일시정지 속도는 `0.0 cm/s (일시정지)`로 표시한다. 치수는 천 단위 구분·불필요한 소수 제거, 각도는 소수 2자리, cm·진행률은 1자리다. 장비 클릭 마커는 일반 화면의 흰 구체를 끄고 기존 DTCore hover 강조·클릭 영역을 유지한다. UXR-08 보완 CLI 증거는 `Saved/Reports/UXR/UXR-08/supplement/`이며 직접 마우스 검수는 보류다.
- 상단 작업 버튼은 **토글**이다. 열린 같은 화면을 다시 누르면 숨기고, 닫힌 화면은 열어 앞으로 가져온다. PCD 전송/결과처럼 같은 패널의 다른 화면은 먼저 전환하고, 그 화면을 다시 누를 때 숨긴다. 열린 화면은 강조한다. 숨김은 측정·송신·저장·재생을 멈추지 않는다(배치/설정의 조작 모드만 종료).
- 장비 선택기는 `Camera SensorId ● 상태` 형식과 LiDAR/Camera 묶음을 사용한다. 선택만으로 패널을 열거나 측정을 시작하지 않는다. 긴 ID는 말줄임+전체 ID 툴팁, 하단 `+ 장비 추가 (배치)`는 배치 창 진입이다. 전역 상태 줄은 hover 가능한 칩이며 긴 경고도 전체 문장 툴팁을 제공한다.
- UXR-02 배치 정리는 완료했으며 마우스 최종 삭제 1건은 보류다(환경: 모니터 없음). 확인/취소 직접 검수와 실제 삭제 자동화는 완료했고 사용자 복귀 후 재검수한다. CLI 재검증은 `Saved/Reports/UXR/UXR-02/cli-closure/`에 있다.
- **PCD 전송**은 데이터의 `전송`, **결과**는 독립 `결과` 탭을 연다. 다른 탭이 열려 있으면 전환하고 같은 작업 버튼을 다시 누르면 숨긴다.
- **도구**에는 센서 상세 설정, 파일 저장, 서버 연결 설정, 진단, 고급·호환, 시나리오 차트, UI 표시가 있다. 파일 저장/서버 연결/진단은 데이터의 각 독립 탭, 고급·호환은 기존 Settings 고급 영역을 연다. 기능과 기본값은 바꾸지 않는다.
- 바 아래 **전역 상태 줄**은 장비 수/측정 수·PCD 전송·시나리오·서버와 첫 차단 사유를 최대 5Hz로 표시한다. 각 항목을 누르면 관련 화면을 연다. 상태 조회나 화면 열기는 측정·송신·파일 저장을 시작하지 않는다. 여기서 서버는 PCD 출력 연결 상태이며 외부 소비자 처리 완료를 뜻하지 않는다.

| 패널 / WBP | 현재 역할 |
|---|---|
| 모니터 / `WBP_VirtualSensorMonitorPanel` | 영상 + SensorId/프레임/Hz/경고. 투영·색상·듀얼 카메라·범례·진단은 **보기 설정** |
| 센서 설정 / `WBP_VirtualSensorSettingsPanel` | 장비/품질·측정값, 위치/회전, 고급 식별/Source/디버그/저장 예약 |
| 데이터 / `WBP_VirtualSensorCaptureExportPanel` | **전송 / 결과 / 파일 저장 / 서버 연결 / 진단**. 새 소유 인스턴스는 전송 탭 |
| 재생 / `WBP_SlabScenarioReplayPanel` | 전체 UUID/내부 보관 ID, 선택 항목 재실행·복사·삭제, 이번 재생 출력 요약 + `PCD 전송에서 변경` |
| 차트 / `WBP_SlabChartsPanel` | 기울기·중심 이탈·Margin 3개를 진행 시점까지만 표시 |
| 진행 / `WBP_SlabProgressPanel` | 상태·진행률·일시정지/재개/중단/초기 위치 복귀, 차트 토글, **표시 설정**(3D·단위·외형) |
| 배치 / `WBP_VirtualSensorPlacementPanel` | 장비 추가·선택·조작, 저장 위치·전체 배치 저장/불러오기 |

- 제목 drag, 우하단 resize, 실제 영역 접기, 숨김/다시 열기를 지원한다. 숨김은 측정·파일 저장·송신·재생 중지가 아니다.
- `더보기 → 이 창 위치·크기 초기화`는 해당 패널만, `도구 → UI 표시 → 센서 도구 UI 초기화`는 소유 패널 배치·열림·폰트만 초기화한다.
- 글자 배율은 85/100/125/150%. 작은 창은 스크롤하거나 확대한다. 외부 UserWidget 내부를 재귀 변경하지 않는다.
- 듀얼 카메라는 기존 RenderTarget을 공유한다. 보조 카메라는 보기 전용이며 추가 capture/readback을 만들지 않는다.
- UXR-01 자동 렌더 검수: 실제 client 1280×720·1920×1080, 글자 100%·150%, window DPI 1.0. 캡처/이름별 회귀 비교는 `Saved/Reports/UXR/UXR-01/`에 있다. 레거시 툴바·사용자 Designer 경로는 유지하며 직접 마우스 검토는 별도다.
- Esc 종료 요청·종료 버튼·선택 변경·패널 숨김/파괴는 조작을 시작한 Actor의 설정과 이전 실행 상태를 복원한다. 드래그 완료만으로 조작 모드를 끝내지는 않는다. 2026-10-12 Computer Use에서 정밀 이동 후 **첫 Esc는 조작만 종료하고 PIE를 유지**함을 확인했다. 기록은 `Saved/Reports/UXR/UXR-02/manual/`에 있다.
- **3D 클릭 선택에는 DTCore 기반 입력 구성이 필요하다.** 기존 `BP_DxGameMode`는 `BP_DxPlayerControllerTest`(native 부모 `DxPlayerControllerTest`, SingleRelease)와 `BP_DxPlayerTest`를 사용한다. 기본 `Engine.PlayerController + DefaultPawn`에는 DTCore 클릭 경로가 없으므로 UI 목록 선택을 사용한다. 프로젝트는 Controller/GameMode를 임의로 교체하지 않는다. 검수는 기존 BP 구성을 테스트 PIE에만 임시 적용했으며 맵을 저장하지 않았다.

| 저장 파일(`Saved/SaveGames`) | 내용 |
|---|---|
| `MA0T10_SensorToolWorkspace_v1.sav` | 소유 창 배치·크기·접힘·열림·글자 배율 |
| `MA0T10_VirtualSensorUI_v6.sav` | 센서 보기·캡처/출력/연결의 비밀정보 아닌 옵션; 이전 슬롯 migration |
| `MA0T10_SensorToolAppearance_v1.sav` | 기존 폰트 설정 호환용 |
| Slab 시나리오 | **파일 저장 없음**. GameInstance 메모리에 최근 최대 10개 |

센서 Transform/장비 설정을 UI SaveGame에 저장하지 않는다. PIE 변경의 원본 맵 반영은 명시적 저장 예약으로만 수행한다.

### LiDAR 보기의 차이

- **태그 기반 의미 분류:** Actor Tag/클래스/이름 규칙. 태그가 없으면 같은 회색일 수 있다.
- **자동 형상 구분:** XYZ의 기준면/돌출 분석. 파랑/주황 표시이며 Slab 인식 AI나 개별 물체 추적이 아니다. 모호한 기준면은 높이 색상 fallback으로 표시한다.
- **높이 색상:** 센서 로컬 Z 또는 월드 Z의 연속값. 수직 설치에서 현장 높이가 필요하면 월드 Z를 사용한다.
- **월드 XY 조감도:** 설치 회전과 무관한 평면도. 로컬 XY 조감도는 센서 기준이며 수직 센서에서는 다른 단면처럼 보일 수 있다.
- **방사 거리-높이:** X=`sqrt(local X²+Y²)`, Y=local Z. 좌우 방향이 합쳐진다.
- **전방 수직 슬라이스:** 회전한 로컬 X-Z 평면의 설정 두께 내 점. 방향 분석에 사용한다.
- 거리 영상/Split 전용 격자·깊이 경계와 조감도의 축/거리 원은 별개다.
- 표현은 **2D / 2D+월드 포인트 / 포인트 전용**. 전용 보기는 플레이어 뷰만 숨겨 센서 SceneCapture를 보존한다. 현재 진입 이후 새 Actor 자동 추적은 별도 보완 대상이다.
- GPU 측정과 Niagara 표시 renderer는 별개다. Niagara가 실패하면 CPU ISM fallback/사유를 표시하며 자산 로드만으로 성공 판정하지 않는다.

GPU 의미 분류는 별도 proxy 장면으로 불투명 StaticMesh/ISM/HISM을 지원한다. 깊이 일치·설정 revision을 검사한다. 투명/마스크/WPO/미지원 geometry는 추정하지 않는다. “GPU는 무조건 태그 정보를 가질 수 없다”는 오래된 설명은 현재와 다르며, 필터 사용 전 해당 프레임의 semantic 지원 상태를 확인한다.

## 6. Slab 데이터·자동 실행·재실행

- 입력 Topic: **`topic.scenario`**, MESSAGE_ID: **`IFactory-agent`**. DTCore `WebSocketTopics`에 구독되어야 한다. 다른 업무 Topic을 삭제하지 않는다.
- TC는 백그라운드 parse 후 게임 스레드에서 `ReceiverId`로 DataSync를 찾는다. 기본 ID는 `SlabScenario.Main`이며 Actor와 handler가 같아야 한다.
- 수신 경로는 **DTCore→TC→DataSync** 또는 **`ASlabActor::SubmitScenarioJson(Json)`** 하나다. Actor 호출 전에 archive 등록 API를 별도로 호출하면 중복 UUID로 자동 실행이 막힐 수 있다.
- 직접 API는 게임 스레드에서 호출한다. `SubmitScenarioJson`의 `true`는 비동기 파싱 요청 접수이며 실행 성공이 아니다. 실제 결과는 `GetLastScenarioAdmissionStatus()`와 실행 상태·경고로 확인한다.

| 필드 | 현재 해석 |
|---|---|
| frame_no | 원본 Slab 상태 번호. 센서 FrameId/렌더 프레임과 독립 |
| mtl_no | 한 시나리오의 소재 식별자 |
| slab_len/wth/thk | 형상 치수. 입력 단위를 cm로 변환 |
| center_x | Track 로컬 X 위치 |
| left_skew_angle | 실제 회전각(도); right는 비교 차트 |
| elapsed_sec | 시간 기준 보간 |
| mov_pos / center_y | 이동에는 사용하지 않고 원문 보존 |
| slab_wt | 표시 정보; 물리 질량 아님 |
| _meta.UUID | 원본 시나리오 ID. 신규 입력에서 누락하면 내부 ID 생성 |
| _meta.duration_sec | 있으면 종료 시각. 없으면 마지막 시각+sample period(기본 .05초) |

**치수·위치 기본 단위는 cm다.** `slab_len=10830`은 기본값에서 108.3m다. 검증맵은 치수 mm/위치 cm이므로 길이 10.83m이다. 단위를 추정하거나 원문을 변환해 덮어쓰지 않는다. 원본 행이 건너뛰어도 행을 만들어 넣지 않고 자세만 보간한다.

신규 벌크는 보관 후 자동 실행한다. 단, 중복 UUID/잘못된 입력은 거절하고 기존 실행·pause·drain 중 새 벌크는 **보관만** 한다. 나중에 자동 큐로 실행하지 않는다.

- 기본 출력: PCD만. Camera 이미지/LiDAR 정보는 선택 사항이며 이미 시작한 실행에는 소급 적용하지 않는다.
- 송신 구성이 부족하면 **unbound 관찰 fallback**으로 Slab 이동만 진행한다. 기존 독립 스트림을 중지·재예약하지 않고 `TransmissionWarning`을 남긴다.
- 준비 검사는 설정 검사이지 Broker 연결/receipt 보장이 아니다. 실행 중 설정을 복구해도 해당 관찰 실행에서 송신을 저절로 켜지 않는다.
- `GetLastScenarioAdmissionStatus()`는 자동 실행/송신 없이 실행/보관만/중복/거절과 이유·요청/적용 출력을 제공한다.
- 목록 재실행은 원문/시나리오 ID를 유지하고 **새 RunUUID**를 만든다. 기본 관찰 전용이다. 움직임 종료와 승인된 데이터의 송신 drain(최대 10초)은 별도 상태다.
- 준비·재생·pause·drain 항목은 삭제하지 않는다. 목록 삭제는 원본 파일/Broker/Slab Actor를 삭제하는 기능이 아니다.

### Slab 정보 카드와 표면 표현

소유 시나리오 진행 창의 **Slab 정보**는 소재번호·치수 L×W×T/단위·중량·행/시간/진행률·입력 위치/행 속도·좌우 기울기·행 기준 중심 이탈/여유·원본 사행 여부를 표시한다. 아래에는 현재 Track 위치(cm)와 실제 재생 속도를 따로 표시한다. 원본 Y 입력은 기존처럼 이동에 사용하지 않으며, 중량 단위는 입력에서 지정하지 않아 **원본 단위**다. 없는 원본 Y/사행은0/아니요로 추정하지 않고 `—`다. 기본 판정 색·임계값·AI 판정은 추가하지 않았다. 카드 갱신은 최대5Hz이며 숨김/접기에는 문자열 조립을 멈춘다.

DTCore 입력이 구성된3D 화면에서 Slab를 클릭하면 기존 진행 인스턴스를 선택 Slab에 바인딩하고 카드 맨 위로 연다. 새 패널/위젯 관리자나 입력 루프는 없다. 기존 Host/UDxWidget/전면·입력 차단 경로를 유지한다. 기본 Engine.PlayerController에는 이 클릭 경로가 없으며 설정·맵·Controller를 자동 교체하지 않는다. 개인 저장 배치와 사용자 Designer Root는 보존한다. 기존 `WBP_SlabProgressPanel` 부모(`SlabProgressPanelWidget`)와 `ASlabSimulationUiHostActor.SlabActor` 연결을 유지하며 native 카드용 새 BindWidget·자산·Level Blueprint 배선은 없다. 사용자 Designer에는 자동으로 카드를 삽입하지 않는다.

**표시 설정**의 표면 선택은 `기본(현재 값)`/`고온 적열`/`산화 스케일`/`냉각 강판`/`분석용 무광`이다. 현재 `M_SlabSurfacePlain`은 **산화 스케일 미지원**(Oxidation 파라미터 없음)이며 선택하지 못한다. 다른 네 선택은 기존 MID 파라미터만 바꾸고 기본은 첫 선택 전 값을 복원한다. 사용자 슬롯 재질은 기존 우선순위를 유지하며 자동 교체하지 않는다. 새 머티리얼/기존 자산/운영맵 저장은 없다.

- `LiDAR 반사율도 함께 적용`은 기본끔이다. 켰을 때만940nm 반사율을 적용하고 기존 gain·검출 확률·양면·포화 설정은 유지한다. 꺼서 opt-in 직전 값을 복원하고 자신이 생성한 응답 컴포넌트만 정리한다. 프리셋 적용과CPU응답 캐시 무효화는 변경 시1회이며 센서 시작/중지/추가 스캔을 하지 않는다.
- GPU는 기존 형상 분류 보기나 물체 필터의ID/depth buffer가 요청되는 지원 불투명 geometry에서만 연동한다. 해당 버퍼가 없으면 요청을 거절하고 안내한다. 보기/필터/geometry가 이후 바뀌면 지원 범위를 다시 확인한다. 추가 캡처/readback이나 자동 설정 변경은 없다. 광학 값은 예시 프리셋이며 실소재/실장비 보정 인증이 아니다.
- 실행 결과`ma0t10.slab-run.v1`에는 사용한 경우에만 optional `surfaceAppearance`를 추가한다. 초기/마지막 프리셋·연동·외형/광학 값, 사용한 프리셋 목록(최대5종), 변경 수를 기록하고 기존 필드·의미·PCD bytes/Blueprint API/저장 슬롯은 유지한다. 이 필드는 광학 설정의 provenance이며 실측 intensity 평균·외부 소비자 처리 완료를 뜻하지 않는다. 실제 intensity 변화는RHI회귀에서 대조했다.
- 검증: 결정적BaseColor SceneCapture 픽셀·실제CPU/GPU intensity·고정 envelope identity의PCD bytes가 기본/link-off 전후 정확히 같았고, 외형만 바꿔도LiDAR/PCD는 유지됐다. link-on에서CPU61945.47→24778.19, GPU62546.36→25018.54로 변했고JSON에0.08반사율/연동이 기록됐다. production FinalColor/JPEG의 bitwise 동일성이나 실장비 물리성을 이 시험으로 보장하지 않는다. 마우스 검수는 보류다.

### 데이터 필수 실행과 실행 결과

**PCD 전송 → 전송**에서 `신규 시나리오`와 `보관 목록 재생`의 출력을 나란히 편집한다. 각 열은 PCD·추가 출력(Camera 이미지/LiDAR 정보)·**연결 확인 후 시작**을 제공한다. 신규=PCD, 재생=관찰 전용 기본값은 유지한다. 변경은 다음 실행에 적용하며 재생 패널에서는 요약과 `PCD 전송에서 변경` 링크만 제공한다. 최초 전송 화면 진입은 기존 Workspace 경로로 재생 옵션 소유 패널을 숨김 상태로 준비하며 재생을 시작하지 않는다.

기본값 `ObservationAllowed`는 기존 관찰 fallback이고, `연결 확인 후 시작`의 내부 값 `RequireData`는 대상 센서/출력 검사와 실제 STOMP CONNECTED 확인 후 움직임을 시작한다. 준비 시간은 기본 10초(1~120초), 준비 중에는 현재 자세를 유지하며 `중단`으로 취소할 수 있다. 출력이 하나도 선택되지 않은 필수 실행은 거절한다. `점검 대상:` 세그먼트에서 신규/재생/직접 전송 중 하나를 선택하면 한 줄 준비 상태·첫 차단 사유와 관련 화면을 여는 해결 버튼을 표시한다. 상세는 접힘, 필터·간격·receipt 표본은 `고급`이다.

- 연결 끊김·최종 receipt 실패·과부하·센서 삭제/정체·필수 스트림 중지 때 움직임과 신규 접수를 중단하고 최대 10초 동안 승인된 데이터를 정리한다. 자동 재개하지 않으며 재실행은 새 RunUUID다.
- 준비/실행/drain 동안 해당 센서의 규격 편집과 경량 조작 모드는 잠근다. 센서 설정 적용은 원래 실행/정지 상태를 유지하고 Scheduler로 갱신하며, 설정 적용 때문에 동기 캡처·스캔·파일 저장을 추가하지 않는다. NaN/무한대는 거절한다.
- 결과는 `준비 실패 / 사용자 중단 / 부분 전송 / Broker 수락 완료 / 관찰 완료`로 표시한다. Broker 수락은 해당 실행의 승인 프레임 receipt 기준이며 측정률·자체 검증·외부 업무 처리는 별개다.
- 진행 패널과 데이터의 **결과** 탭에서 최근 20개 요약을 확인한다. 초기 위치 복귀·새 실행·목록 삭제에도 이전 요약을 유지한다. 결과 폴더에는 작은 JSON만 저장하며, 기본 100개를 넘으면 이 기능의 schema/파일명에 해당하는 파일만 정리한다. 저장 실패는 전송 실패와 구분한다.
- 결과 탭은 전체 실행/시나리오 UUID, 요청/적용 출력, 종료 사유와 같은 시나리오의 확정 실행 A/B 비교를 보여 준다. 대상 센서·설정 revision·측정률·단계별 집계·종료 snapshot·receipt와 상세 A/B는 **도구 → 진단 → 선택 실행 상세**에서 확인한다. 새 실행과 패널 숨김은 선택을 유지한다. 선택 항목이 20개 범위에서 빠지면 최신 결과를 선택하고 안내한다. 이전 PIE의 파일은 목록으로 불러오지 않는다.
- `실행 UUID 복사`와 `선택 결과 폴더 열기`를 제공한다. 저장 실패 시 `요약 저장 재시도`는 같은 확정 JSON만 다시 저장한다. 중복 클릭/저장 중/미확정 결과는 거절하며 시나리오 재생이나 Topic 재전송은 시작하지 않는다.
- 종료 JSON의 `terminalSnapshot`은 정리 직전 측정·출력 준비·socket 대기/기록·receipt 대기를 구분한다. `uniqueUnfinished`는 요청 식별자를 합쳐 중복을 제거한 값이다. 단계별 수를 단순 합산하지 않는다. `consumerSnapshotUtc` 이후 도착한 자체 검증은 이미 확정한 결과를 바꾸지 않는다.
- 같은 연결에서 재생할 때 종료된 상세 진단은 회수하고 최근 종료 집계만 20개 보관한다. 재사용/늦은 프레임을 차단하는 최소 UUID·generation 기록은 별도로 유지한다. 이는 65,536건 실행별 ledger 제한을 없애거나 장시간 수집을 인증하는 변경이 아니다.
- 파일: `Saved/Reports/ScenarioRuns/slab-run-<RunUUID>.json`, schema `ma0t10.slab-run.v1`. 원본 PCD/JPEG·인증 정보는 포함하지 않는다. 실행 중 진단 식별자는 65,536건으로 제한하며 초과를 명시적 오류로 처리한다. 장시간 수집 지원 인증은 별도다.
- API: `FSlabExecutionOptions`, `ASlabActor::SetExecutionOptions`, `RequestScenarioReplayWithExecutionOptions`, `USlabRunResultsSubsystem::GetRecentRunResults/GetRunResult/OpenRunReport`. 기존 재생 API는 기본 관찰 정책으로 유지한다. 사용자 WBP는 데이터 패널의 `SetLiveScenarioExecutionOptions/GetScenarioRunResults/OpenScenarioRunReport`를 사용할 수 있다.
- 추가 API: 결과 Subsystem의 `CanRetryRunReportSave/RetryRunReportSave/GetResultsRevision`, 데이터 패널의 `SelectScenarioRunResult/GetSelectedScenarioRunResult/GetSelectedScenarioRunId/RetrySelectedScenarioRunReport`. 기존 함수·schema·PCD/JPEG 계약은 유지한다.

### 초기 배치로 복귀

진행 패널의 **`초기 위치로 복귀`**를 누르면 PIE 시작 시 레벨에 배치되어 있던 Slab의 월드 위치·회전으로 즉시 돌아간다. 시나리오 첫 행이나 Track 원점으로 돌아가는 기능이 아니다.

- 현재 형상·스케일·재질과 최대 10개의 보관 목록을 유지한다. 진행률은 0%, 현재 프레임은 `—`, 세 차트와 이전 3D 분석 라벨은 대기로 초기화한다. 표시 선택·선/문자 크기·창 배치는 유지한다.
- 실행·일시정지·입력 파싱·재생 준비·센서 송신 정리 중에는 비활성화되며 같은 사유를 API에서도 반환한다. 먼저 중단하거나 완료를 기다리고 송신 정리까지 끝낸 뒤 사용한다.
- 복귀 자체는 새 RunUUID, 센서 세션, 캡처/스캔이나 자동 송신을 만들지 않는다. 이전 receipt·실패 기록은 연결·진단에 남는다. 이후 신규 UUID는 정상 자동 실행하며 기존 목록의 같은 시나리오도 재실행할 수 있다.
- C++/Blueprint: `ASlabActor::CanResetToInitialPlacement(OutReason)` / `ResetToInitialPlacement(OutError)`. 사용자 WBP는 `USlabProgressPanelWidget::CanResetSlabToInitialPlacement` / `ResetSlabToInitialPlacement`에 버튼을 연결한다. Designer와 기존 binding은 자동 변경하지 않는다.

진행 패널의 **표시 설정**을 펼치면 3D 분석·단위·현재 외형을 바꿀 수 있다. 실행 중 단위 변경 제한과 표시 선택은 기존대로 유지한다. 이 영역을 접거나 창을 숨겨도 시나리오가 중지되지는 않는다. 진행 패널의 3D 분석은 선 기본 4px(2~12), 글자 28px(18~56), 어두운 배경판, signed Margin과 `[침범]`을 제공한다. 음수 Margin은 기하학적 침범이며 안전 기준/AI 판정이 아니다. 보조 표시는 센서 측정에서 제외한다.

테스트 발행(로컬 Broker가 이미 정상 실행되고 입력 Topic을 구독한 경우):

```powershell
node Tools/Artemis/publish_slab_scenario.mjs --topic topic.scenario
node Tools/Artemis/publish_slab_scenario.mjs --input "C:/TestData/scenario.json" --topic topic.scenario
```

## 7. 측정 프로필과 출력 계약

### 프로젝트 프리셋

| FullSpec | 설정된 규격 |
|---|---|
| D455 Camera | 1280×720 / 30Hz / JPEG 기본 품질 80 |
| Livox 계열 기본 LiDAR | 360×60 / 10Hz |
| ML-X(80) Integration 200 | 200×56 / 20Hz / 최대 150m |
| ML-X(80) Native | 576×56 / 20Hz / 최대 150m / 공개 사양 해석 기반 밀도 |

프로필 값과 **실제 완료율**은 다르다. ML-X Native는 제조사 원시 패킷/실장비 보정 완료가 아니며 D455 SceneCapture는 실장비 depth stream이 아니다. CPU/GPU의 geometry·Echo 지원 차이, 캘리브레이션 근거와 fidelity 상태를 함께 확인한다. enum에 HardwareRayTracing/SDK 이름이 있다고 실장비 backend가 완성된 것은 아니다.

### 측정률·정체 진단

- 시작/재등록 후 `max(1초, 실제 적용 주기의 3배)`는 시작 유예다. 이후 진행이 이 시간보다 오래 없으면 과거 양수 Hz가 남아 있어도 유효 측정률은 0Hz다.
- Settings 상세 부하 요약에서 센서별 시작 유예·측정 중·일시정지·조작용 경량 미리보기·정체를 구분한다. 처리 실패 이력이 있는 정체는 별도 문구로 표시한다.
- World 시간으로 신선도를 계산하므로 PIE 일시정지는 정체 시간에 포함하지 않는다. 일시정지 중에는 진단만 갱신하고 측정하지 않는다.
- `SensorRates`와 `StarvedSensorIds`를 공개한다. 기존 공정성 수치를 읽는 Blueprint/C++는 `bCameraFairnessEvaluable`/`bLidarFairnessEvaluable`도 확인해야 한다. 판정 불가 수치 0은 정상 공정성을 뜻하지 않는다. 조작용 임시 주기는 정격 공정성 통과로 처리하지 않는다.
- Camera acquisition 신선도는 SceneCapture 제출 정의다. 스트리밍 readback은 해당 capture 직후의 복사와 고정 snapshot을 사용한다. slot 부족/합쳐진 capture는 명시적 파생 실패로 기록하며 나중의 픽셀을 옛 FrameId에 붙이지 않는다. acquisition·JPEG 완료·receipt·소비자 검증 Hz는 서로 구분한다.
- 성능 보고서는 워밍업 이후의 0Hz 표본 및 요청 센서 누락을 평균으로 숨기지 않고 실패로 판정한다.

### Topic/Body

| 방향·용도 | 기본 Topic | Body / schema |
|---|---|---|
| Slab 입력 | topic.scenario | JSON, MESSAGE_ID=IFactory-agent |
| Camera 출력 | topic.virtual.sensor.camera.0 | 고성능 원본 JPEG / virtual-camera.jpeg.v1 |
| LiDAR 값 출력 | topic.virtual.sensor.lidar.0 | point 배열 없는 통계 JSON / virtual-lidar.telemetry.v1 |
| PCD 출력 | topic.virtual.sensor.export.0 | 완전한 PCD v0.7 DATA binary / virtual-pointcloud.pcd.v1 |

Raw TCP STOMP worker가 binary I/O·receipt·자체 수신 검증을 담당한다. 기존 JSON `virtual-camera.v1`/`virtual-lidar.v1`은 호환 경로에 남는다. HTTP JSON/raw file POST도 별도 지원한다. `wss://`는 Engine STOMP 호환 경로이며 Raw TCP TLS 지원으로 보지 않는다.

**receipt는 Broker 수락이다.** 내부/외부 소비자 수신 검증, 최종 업무 처리 ACK, 로컬 파일 완료는 각각 별개다. 정상 연결 중 FIFO여도 네트워크 단절 중 프레임을 디스크에 영구 보존하는 기능은 아니다. Raw worker reservation은 입력부터 receipt/최종 실패까지 유지된다. 기본 전역 128MiB·스트림 64MiB 및 Camera 8/기타 20프레임을 넘으면 명시적 과부하로 거부한다. 재시도는 같은 body를 중복 집계하지 않는다.

### PCD 소비자가 알아야 할 최소 계약

```text
FIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence
SIZE 4 4 4 2 2 2 1 1 8 1 4
TYPE F F F U U U U U I U F
COUNT 1 1 1 1 1 1 1 1 1 1 1
DATA binary
```

- 한 점 33바이트, little-endian. XYZ는 센서 로컬 **m**, X 전방/Y 좌측/Z 위쪽이다. 12/36/40바이트 stride로 읽으면 안 된다.
- `# MA0T10_META {JSON}` 주석에 sensor_id, sensor_frame_id, timestamp_utc 및 선택적 run_uuid/scenario_uuid/mtl_no/frame_no/elapsed_sec가 있다. Slab frame_no와 sensor_frame_id는 다른 번호이며 식별 정수는 문자열로 보존한다.
- snapshot pose가 있으면 `sensor_to_world_m`(row-major 4×4, column vector 적용)로 UE 월드 meter를 복원한다. 센서를 회전한 경우 로컬 Z에 센서 높이만 더하면 틀린다.
- `POINTS 0`도 정상 프레임이다. checksum/content-length는 주석을 포함한 전체 파일 바이트 기준이다. 일반 PCD 라이브러리 재저장은 사용자 주석을 제거할 수 있다.
- 실시간 PCD는 Binary 고정이다. 수동 ASCII/기존 API의 주석 포함 여부를 동일하다고 가정하지 않는다.
- Tag/Semantic/ROI 필터는 측정 출력 조건이다. `PointCloudTarget`/Semantic은 Digital Twin 정보이며 ML-X 고유 기능이라고 표기하지 않는다. `mtl_no`가 붙어도 모든 점이 그 Slab 점이라는 뜻은 아니다.

```powershell
python Scripts/read_pcd_context.py received.pcd
python Scripts/inspect_binary_pcd.py received.pcd --plate-z-m 0
```

### 파일 저장과 외부 입력

- 새 프레임 저장: 실행 중인 센서의 요청 후 완료 프레임을 최대 2초 기다린다. 정지 센서를 몰래 켜거나 이전 프레임으로 대체하지 않는다.
- 현재 프레임 저장: 기존 완료 snapshot을 고정하며 추가 측정하지 않는다. 없는 출력은 비활성/실패 안내한다.
- 주기 저장: .05~3600초, 최신 완료 프레임을 비동기 저장한다. 같은 FrameId·busy·새 프레임 없음은 저장 기회 생략으로 집계한다. Topic 주기와 독립적이다.
- 기본 루트 `Saved/SensorCaptures`, 주기 저장 `LocalTimedCapture/<UTC>/Camera|Lidar`, Recorder `Saved/SensorRecordings`.
- 요청 대상·출력·revision을 고정하고 실제 파일 생성 후 완료 이벤트를 보낸다. LAZ는 외부 compressor가 필요하며 프로세스 timeout/cancel 보완이 남아 있다.
- 외부 Source(CSV/JSONL replay, Camera buffered JSON, LiDAR HTTP/UDP)는 센서 입력이다. 서버 수신 진단은 출력 검사다. ROS2/Livox/RealSense SDK 클래스는 현재 stub/확장 지점이며 실제 SDK 지원 완료가 아니다.

## 8. 센서 배치와 저장 위치

**위치 저장을 찾는 경로:** 상단 **`배치 → 3. 저장 위치`**다. 배치 창은 `1. 장비 / 2. 선택 장비 / 3. 저장 위치 / 4. 전체 배치`를 직접 소유하며 Settings 창을 열 필요가 없다. 위치 이름을 입력하고 `현재 위치 저장`을 누른다. 목록의 `이동`으로 적용하며 단순 선택은 자세를 변경하지 않는다. 숫자 입력은 별도의 접힌 `위치·회전 숫자 입력`에 둔다. Settings의 `배치 작업공간` 버튼도 같은 창을 연다. 본문 전체를 스크롤하며 하단 결과 문구는 고정한다.

- 장비 행은 종류·SensorId·레벨/추가·색 상태 점과 선택 강조를 표시한다. `측정 시작/중지`는 실제 측정을 제어하고, `이동·회전 시작/종료`는 경량 조작 모드를 제어한다. 모드·좌표·입력은 현재 선택이 보이는 세그먼트형 토글이다.
- 전체 배치는 **이름 입력 → 저장 / 목록 선택 → 변경 미리보기 → 적용 또는 취소** 순서다. `선택 배치 덮어쓰기`는 목록 선택 후에만 보인다. 개별 위치와 전체 배치는 기존 파일 형식을 유지한다.
- 소유 Settings의 기존 북마크·조작 Blueprint API는 배치 창과 같은 `SensorPlacementInteraction` adapter 및 기존 PlacementSubsystem을 사용한다. `BindPlacementControls`도 호환용으로 유지하지만 배치 UI 구성/작동에 Settings를 요구하지 않는다.
- DTCore 객체 선택용 표식은 소유 Host 등록 장비에만 붙는 transient 구성이다. 센서 trace 채널은 무시하고 Camera/GPU LiDAR·의미 분류·그림자/반사에서는 제외한다. 창 전면 처리는 DTCore API를 순간 등록 adapter로 사용하며 일반 닫기/모드 전환 소유권을 넘기지 않는다.

처음 사용하는 순서는 **PIE 실행 → 관찰 시점 이동 → LiDAR/Camera 추가 → 위치·회전 조정 → 현재 위치 저장 → 전체 배치 저장**이다. 다음 PIE에서는 전체 배치를 선택해 변경 미리보기 후 적용한다. 개별 북마크는 선택 장비의 자세만 저장하므로, 추가 장비를 재생성하려면 전체 배치 저장을 사용한다. PCD 송신은 별도로 선택한다.

맵에는 `VirtualSensorCoordinator`가 정확히 하나, `VirtualSensorUiHostActor`가 하나 필요하다. Host의 `bAutoCreateOnBeginPlay`, `bAutoCreateToolPanels`를 켜고 `SensorManager`를 연결한다. `PlacementWidgetClass`를 지정하거나 `bUseNativeToolWidgetFallback`을 켠다. Settings는 선택적 상세 설정 화면이다. WBP를 사용할 때 부모는 `VirtualSensorPlacementPanelWidget`이며 native 구성을 쓰는 wrapper에는 Designer Root, 별도 컨트롤 이름 또는 `Is Variable` 설정이 필요하지 않다. Level Blueprint에서 배치 Widget을 중복 생성하지 않는다. 늦은 준비는 0.5초 간격·최대 10초 재시도하며 실패 사유를 표시한다. 생성되지 않은 창의 열림 상태를 성공으로 저장하지 않는다.

DTCore 소스와 gitlink는 `b2504d1`로 유지한다. 일반 PIE의 기존 `ws://localhost:31000` 접속 정책도 유지한다. 2026-10-10 확인 당시 31000 listener가 없어 일반 연결은 실패했고 프로젝트 기본 `WebSocketTopics`도 비어 있었다. Slab 수신을 사용할 때는 해당 서버를 실행하고 `Project Settings → DTCore Settings → WebSocketTopics`에 `topic.scenario`를 등록하거나 프로젝트의 기존 명시적 구독 경로를 연결해야 한다. 데이터 패널의 센서 출력 Broker와 업무 전문 수신 설정은 별개다. 별도 로컬 시험에서 Artemis `61616`의 `topic.scenario` 전문이 실제 DTCore → TC → DataSync → Slab로 전달되어 frame 5까지 진행했다. 이는 일반 실행의 31000 서버가 연결됐다는 뜻이 아니다. 서버·구독 기본값은 자동 변경하지 않았다.

전체 배치 A를 미리보기한 뒤 B를 선택하면 A 요청은 취소된다. B를 다시 미리보기해야 적용할 수 있다. `CancelLayoutRestorePreview`와 배치 Widget의 선택/미리보기/적용 전달 API를 사용자 WBP에서도 사용할 수 있다. 북마크 이동은 선택 ID와 편집 이름·삭제 확인 상태를 함께 갱신한다.

북마크 저장을 기다리는 동안 다른 항목을 선택하거나 이름을 편집해도 이전 저장 완료가 새 편집 대상을 덮지 않는다. 저장 자체는 고정한 대상·내용으로 마무리하며 파일 결과는 기존 상태 API에서 조회한다.

새 native 패널과 `WBP_VirtualSensorPlacementPanel`은 우리 Host가 명시적으로 등록한다. 기존 Designer가 있는 사용자 WBP를 자동 변경하지 않으며 공개 저장/조회 API로 연결할 수 있다. 개별 저장 위치와 전체 배치 저장은 서로 다른 기능이다.

`LiDAR 추가`와 `Camera 추가`는 현재 관찰 시점의 위치·방향에 Native ML-X/D455 장비를 만들고 선택·미리보기를 시작한다. 조작 중에는 기존 경량 미리보기를 사용하며 Esc로 원규격에 복귀한다. 복제는 측정 설정만 복사하고 새 ID를 부여한다. Topic 송신·녹화·외부 Source·실행 UUID는 복사하지 않는다. 삭제는 이 도구가 만든 장비만 가능하다. 수집 잠금이나 동종 wildcard 송신이 있으면 이유를 안내하고 거절한다. 레벨에 있는 기존 장비를 자동으로 삭제하지 않는다.

**전체 배치:** `배치 → 전체 배치`에서 이름을 지정해 저장하고, 저장 목록을 선택해 `불러오기 변경 미리보기 → 확인 후 적용`으로 복원한다. 별도 `Saved/SensorLayouts/*.layout.v1.json`이며 실제 쓰기 완료 후 저장 성공을 표시한다. 같은 맵만 지원한다. 기존 레벨 장비는 정규화된 정확한 Actor 경로·종류, 도구 장비는 배치 GUID로 대응한다. 미포함 기존 레벨 장비는 유지하고 미포함 도구 장비만 정리한다. 실패·오래된 미리보기·busy·ID 충돌은 거절한다. 저장된 미리보기 상태는 복원하지만 송신·녹화·파일 저장은 시작하지 않는다. 새 PIE에서는 저장 구성이나 북마크를 자동 적용하지 않는다. `.umap`과 기존 북마크 파일은 재저장하지 않는다.

**PCD 사전 점검:** 데이터·재생 화면에서 원래 수집 규격의 대상/주기, 보수 추정 body·MiB/s, 실제 연결 상태와 제출 예산을 확인한다. PCD-only 2 LiDAR×20Hz는 40건/s이며 미리보기 Camera는 JPEG 송신량에 포함하지 않는다. `MaxSubmissionsPerFrame × 관측 FPS`는 이론적 상한이다. FPS가 없으면 60FPS 가정으로 표시하며 실제 성능을 보증하지 않는다. echo/backend가 불확실하면 명시하고 추정 용량 경고만으로 관찰 fallback이나 데이터 필수 정책을 바꾸지 않는다. 기본 예산은 2이며 Coordinator의 StreamPublisher 설정을 명시적으로 조정하고 재검증한다. 자동으로 4나 저규격으로 바꾸지 않는다.

점검은 활성 개별 설정이 wildcard를 우선하는 실제 송신 규칙을 사용한다. 수동 PCD 시작은 기존 독립 출력과 합산하고, 다음 시나리오는 제어 대상의 출력을 선택 정책으로 교체한 뒤 비대상 독립 출력만 더한다. `PCD 건/s`와 `전체 출력 건/s`, Raw/Engine 연결 상태를 구분한다. 단일 프레임이 bucket 용량보다 크면 평균 MiB/s가 낮아도 계속 대기할 수 있어 별도 경고한다. 요약에 첫 차단 사유를 우선 표시하며 호환 JSON 상태는 현재 선택 LiDAR 한 대의 상태다. 기존 `Inspect` API는 유지하고 새 `InspectForPurpose`가 호출 목적을 명시한다.

공개 경계: `UVirtualSensorLayoutSubsystem`은 장비 수명·전체 배치, 기존 `UVirtualSensorPlacementSubsystem`은 북마크·자세 이력·새 위치 프레임 확인, `UVirtualSensorOutputPreflight::Inspect/Evaluate`는 읽기 전용 snapshot·순수 평가다. 이번 지원 검증의 근거는 `Saved/Reports/SensorPlacementWorkspace`에 보관하며 과거 2+2 결과를 새 기능의 최종 합격으로 대체하지 않는다.

- `배치 → 이동·회전 시작`: 기본 연속 이동 125cm/s·회전 62.5°/s. WASD/QE·방향키·Z/C, Shift×5·Ctrl×0.2와 Local/World를 사용한다. 정밀 단위는 키 1회당 10cm·5°이며 자동 반복을 무시한다. 기존 Settings Blueprint의 속도·단위 API는 유지한다.
- 소유 viewport에서 RMB를 누르면 관찰자 탐색으로 전환하고 센서는 움직이지 않는다. 프로젝트 Pawn/Controller는 초당 속도를 사용하며, 기본 DefaultPawn 맵도 해당 gesture에만 적용되는 경로로 지원한다. 입력창·외부 Widget·다른 창은 센서 이동 단축키 대상이 아니다.
- 조작 중 실행 센서의 미리보기는 Camera 640×360/15Hz·LiDAR 120×24/10Hz이며 저장·Topic 출력에서 제외한다. 정지 센서는 자동 시작하지 않는다. Esc·선택 변경·숨김·종료는 현재 자세를 유지하며 원규격·원실행 상태를 복원한다.
- `현재 위치 저장`: 센서별 이름 있는 위치·회전 20개를 `Saved/SensorPlacements/*.poses.v1.json`에 보관한다. 파일 쓰기 완료 후 저장 완료를 표시한다. 새 PIE는 원래 맵 배치에서 시작하고 `이동` 버튼으로 직접 적용한다. 목록 선택만으로 위치가 바뀌지 않으며 현재 스케일·프로필·해상도·주기·출력을 유지한다.
- 위치의 키는 PIE 접두사를 제거한 맵 경로·센서 종류·SensorId다. 다른 맵·중복/변경된 ID·누락 센서는 추측해 연결하지 않는다. 이름 변경·명시적 덮어쓰기·삭제 확인/취소를 제공한다. 기존 UI 저장값과 맵 저장 예약은 별도로 유지한다.
- 센서별 현재 PIE의 50개 자세 이력을 `되돌리기/다시 적용` 또는 Ctrl+Z/Ctrl+Y로 조작한다. drag 1회·연속 키 gesture·숫자 입력·북마크 이동은 각각 1개 이력이다. 입력창은 기존 텍스트 Undo를 사용한다.
- 해당 센서의 시나리오 준비/실행/pause/drain·수동 스트림(wildcard 포함)·녹화·주기/미완료 파일 저장은 위치와 SensorId 변경을 잠근다. 비대상 센서는 계속 조작할 수 있다. 조작 중 수집 시작은 gesture 확정·원규격 복원 후 접수한다. 조회와 현재 위치 저장은 가능하다.
- 실행 결과/JSON의 sensor `placement`에는 시작 맵·월드 자세(cm/quaternion/scale)·북마크 ID/이름/revision·고정 시각과 외부 변경 여부를 기록한다. 기존 `ma0t10.slab-run.v1`, PCD/JPEG 본문과 프레임별 측정 자세는 유지한다. 이후 이동·이름 변경으로 과거 시작 배치를 덮어쓰지 않는다.
- 공용 API는 `UVirtualSensorPlacementSubsystem`의 저장 요청/상태 조회·목록·이동·이름 변경/삭제·Undo/Redo·현재 snapshot이다. Settings의 선택 센서 전달 API도 사용 가능하다. 동료 WBP Designer는 자동 변경하지 않는다.

### 배치 미리보기 확인과 A/B 비교

- 맵 값 복원은 적용 성공 후에만 완료로 표시하며 자세 변경을 Undo/Redo에 기록한다. 측정 설정 자체를 Undo하지 않는다. SensorId 변경 시 북마크 목록·선택·삭제 확인과 자세 이력을 이전 ID에 연결하지 않는다.
- 북마크·Undo/Redo·숫자 위치·조작 종료 뒤에는 새 측정의 시작 시각·자세·규격·generation을 확인한다. 이전 위치에서 시작한 늦은 프레임으로 준비 완료가 되지 않는다. Camera는 기존 capture 뒤의 GPU fence를 사용하며 이 확인 때문에 JPEG나 readback을 추가하지 않는다. 정지 센서는 계속 정지한다.
- 미리보기 대기는 `max(1.5초, 주기×3)`이며 PIE pause 시간은 제외한다. 준비 완료·정지·지연·취소를 표시한다. 지연 때문에 센서를 자동 재시작하지 않는다. `GetPlacementPreviewStatus()`로 Blueprint에서도 조회할 수 있다.
- `데이터 → 연결·진단 → 실행별 전송 결과`에서 확정된 실행을 `A로 선택`·`B로 선택`한다. 같은 시나리오 UUID의 서로 다른 실행을 비교하며 현재 PIE의 최근 20개 결과만 사용한다. 선택이 보관 범위를 벗어나면 비교를 중지하고 이유를 표시한다.
- 센서별 시작 위치·최단 회전각·스케일·북마크·설정과 요청/기록 시점 Hz를 비교한다. 기존 **실행 전체 카운터**와 새 **센서·출력별 상세**를 구분해 표시한다. 상세가 없는 과거 기록은 확인 불가다. 기록 시점 Hz는 실행 평균이 아니며 receipt 비율은 승인 0건에서 N/A다.
- 신규 실행은 원문 UTF-8 SHA-1을 선택적 `ScenarioContentHash`로 기록한다. 설정 JSON은 필드 순서 대신 값으로 비교하고 revision만 다른 경우 설정 차이로 판정하지 않는다. 원문·출력·맵·설정·진행 구간·외부 배치 변경이 다르면 참고 비교이며 누락 정보는 확인 불가다. 전송 수로 배치 우열·가림률·점밀도를 판정하지 않는다.
- `GetRunComparison()`은 저장된 두 요약만 읽는다. 비교·표시·해제는 측정·이동·재생·Topic 전송을 발생시키지 않는다. 확정된 결과에는 늦은 측정률·중단 알림도 적용하지 않는다. 기존 `ma0t10.slab-run.v1`과 PCD/JPEG 계약은 유지한다.

## 9. LiDAR 출력 정책과 센서별 전달 결과

- 배치 도구로 새로 추가한 LiDAR는 `WhenRequested` 정책으로 시작한다. PCD·Raw telemetry·미리보기만 사용할 때는 큰 호환 JSON을 생성하지 않는다. 기존 클래스·레벨 장비·구형 전체 배치 파일은 `LegacyAlways`를 유지한다. 복제와 새 전체 배치 저장/복원은 원본 정책을 보존한다.
- 정책은 `센서 설정 → 고급 설정 → LiDAR 호환 JSON 생성`에서 명시적으로 바꾼다. 정책만 변경하면 측정이나 조작 모드를 재시작하지 않는다. 기존 수집 잠금은 적용된다. JSON 생략은 실패/프레임 유실로 세지 않고 데이터 패널에 `미사용`으로 표시한다.
- 호환 LiDAR 스트림, 실제 JSON 녹화, 기존 Blueprint JSON 완료 구독, 등록된 native 소비자와 수동 요청은 JSON을 생성한다. `GetLastJsonPayload()`를 직접 polling하는 새 소비자는 `RegisterCompatibilityJsonConsumer(Owner)`/해제를 사용하거나 `LegacyAlways`를 선택한다. preview에는 기존 acquisition 이벤트를 사용할 수 있다.
- Native 수동 JSON 전송은 완료 snapshot의 비동기 1회 생성과 metadata를 사용하며 센서를 추가 측정하지 않는다. 작업 슬롯이 사용 중이거나 설정/종료 전의 오래된 snapshot이면 사유를 안내한다. 기존 bool 전송 API는 완료된 JSON과 그 JSON의 SensorId/FrameId를 사용하고 반환 의미를 유지한다.
- 생성 상태 조회는 `GetCompatibilityOutputStatus()`이며 작업 수·미사용 수와 queue/encode/completion 시간을 제공한다. 이전 post-process 시간은 acquisition부터 완료까지의 값으로, 순수 JSON encode 시간과 다르다. 자동 파일 출력과 수동/외부 입력의 기존 기능은 유지한다.

- 호환 LiDAR JSON은 센서별 처리 중 1개·최신 대기 1개, 전역 생성 작업 4개다. 새 측정이 도착했다는 이유만으로 같은 generation의 정상 완료 결과를 버리지 않는다. 실제 대기 교체, generation 폐기, 생성 실패는 별도 집계한다. Stop·설정 재시작·삭제 후 결과는 적용하지 않는다.
- `OnDerivedFrameCompletedNative`는 JSON과 측정 당시 FrameId·UTC·pose·설정·Slab context·point snapshot을 함께 제공한다. 기존 `OnScanCompleted` Blueprint 이벤트와 수동/외부 입력 API는 유지한다. 이벤트의 ViewTexture는 기존 현재 미리보기이며 과거 프레임 영상 snapshot을 새로 제공하는 것은 아니다.
- 파생 JSON 완료는 호환 출력에만 전달한다. PCD와 Raw telemetry는 acquisition 경로에서 처리하고 다시 발행하지 않는다. 선택된 호환 JSON 작업만 세션의 최대 10초 drain에 포함하며 PCD 전용 실행은 불필요한 JSON 완료를 기다리지 않는다.
- **결과**에서 실행/A/B를 선택하고 **도구 → 진단**에서 센서별 측정 완료/실패, SensorId+출력별 승인·제출·receipt·실패·승인 후 receipt 미확인·자체 검증을 확인한다. 재시도와 중복 callback은 기존 request ledger에서 한 번만 센다. 이전 결과에 상세 집계가 없으면 `확인 불가`다. 기존 합계와 `ma0t10.slab-run.v1`은 유지한다.
- **시나리오 없이 직접 전송**은 전송 탭 상단의 접힘 헤더를 펼쳐 출력별 대상 장비를 확인한 뒤 시작/중지한다. **파일 저장** 탭은 새 프레임/현재 프레임/주기 저장, **서버 연결** 탭은 방식·주소·계정·적용/연결 시험, **진단** 탭은 수신 검증·전송 기록·보고서 저장·호환 JSON 수동 전송을 제공한다. 숨김은 작업 중지가 아니며 숨긴 탭의 표시용 갱신은 멈춘다. Designer WBP와 기존 Blueprint API·저장 슬롯/탭 enum·기본 출력 정책은 유지한다.
- 측정 Hz, 호환 JSON 생성 Hz, 실제 PCD/JPEG 전달 Hz는 다르다. `MeasuredOutputRateHz`는 LiDAR 호환 파생 출력의 최근 간격 값이며 PCD 송신률이 아니다. 실행 상세의 Hz는 기록 이벤트 간격이고 실행 전체 평균이나 외부 소비자의 저장·학습 처리 완료를 뜻하지 않는다.
- 실제 선택 출력에 필요한 센서만 측정률 미달 경고의 대상이다. PCD 전용 실행에 포함된 비선택 Camera의 정지/0Hz는 실패로 판정하지 않는다. 해당 Camera 이미지를 선택하면 0Hz 경고는 계속 유지한다.
- wildcard 전송의 FrameId 연속성은 센서별로 관리하며 전달 카운터는 센서 합계다. 공유 전역 보관 bytes는 중복 합산하지 않고 지연 p95는 보수적인 **센서별 최대값**으로 표시한다. 송신 round-robin은 한 순회에서 동일한 시작 기준을 사용한다.
- UE 학습 데이터 묶음 저장은 추가하지 않았다. 기존 수동 파일 내보내기·작은 실행 결과 JSON은 유지하며, 데이터 보관과 학습 처리는 외부 수신 프로그램의 역할이다.

재현 명령: `Scripts/run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -SensorPairs 2 -ClientWidth 1920 -ClientHeight 1080 [-PointCloudOnly] [-SubmissionBudget 4]`. PCD 전용에서도 독립 구독자는 세 Topic을 모두 확인해 선택하지 않은 출력 0건을 검사한다. 성능 보고서는 `AcquisitionPass`, `RequestedDerivedOutputPass`, `TransportPass`를 분리하며 검사하지 않은 단계는 `NotTested`다. `run_fullspec_performance_evidence.ps1`의 기본 `AcquisitionOnly` 통과를 송수신 통과로 해석하지 않는다.

기존 `MaxSubmissionsPerFrame=2`는 60FPS에서 최대 120건/s이므로 2 Camera+2 LiDAR의 전체 출력 약140건/s을 처리하지 못한다. `SubmissionBudget=4`는 **시험에서 기존 Actor 속성을 명시적으로 설정**하는 옵션이며 기본값이나 운영맵을 자동 변경하지 않는다. 수집 규격을 낮추거나 큐를 늘리는 조치가 아니다. 각 보고서의 적용 예산과 선택 출력을 확인한다. UI 대조도 준비 중 누적 카운터를 직접 비교하지 않고 측정 구간에 승인된 각 request가 실제 TC/UI 검증까지 도달했는지 확인한다.

## 10. 아키텍처 뼈대

### 10.1 목표 레이어

한 Runtime 모듈(`ma0t10_dt`) 안에서 폴더를 레이어로 사용한다. **의존은 위에서 아래로만** 허용한다. 플러그인 분리(R5)는 이 경계가 gate로 고정된 뒤의 별도 단계이며 이번 버전에서 시작하지 않는다.

| 레이어 | 폴더 (`Source/ma0t10_dt/MA0T10/`) | 책임 | include 가능 |
|---|---|---|---|
| L3 UI | `UI/` (Workspace Subsystem 포함 예정) | 소유 패널·Host·Gizmo·창 배치·UI 저장값 | L0~L2, DTCore |
| L2 Host·업무 | `Slab/`, `Scenario/`(예정), `WebSocket/TC/`, 기존 `Crane/` `Manager/` `Player/` `Api/` `InteractableActor/` `UI/Main/` | Slab 시나리오·실행 결과·재생, 업무 전문 TC, 센서 context adapter | L0~L1, DTCore |
| L1 센서 Runtime | `Sensor/`, `Camera/`, `Core/`(센서 공용 서비스만) | 센서 편집 상태 타입(`Sensor/VirtualSensorEditableTypes.h`), 측정·스케줄·코덱·출력·송신·파일 저장·배치/레이아웃 서비스 | L0~L1, DTCore |
| L0 공통 계약 | `Contracts/` | Topic/슬롯/경로 리터럴 상수, L1 타입에 의존하지 않는 중립 acquisition context(ARCH-06 예정) | 엔진·DTCore 중립 타입만 |
| 외부 | `Plugins/DTCore` (submodule) | 공통 API/WebSocket/Data/UI 기반 | 수정 금지 |

- 테스트(`*/Tests/`)와 Editor 모듈(`ma0t10_dtEditor`)은 레이어 검사에서 제외하지만 production 코드가 테스트를 include하면 안 된다.
- 기존 업무 폴더(Crane 등)는 L2로 분류만 하고 이번 단계에서 수정하지 않는다.
- 폴더 이동은 같은 모듈 안의 이동이므로 `/Script/ma0t10_dt` reflection 경로·Blueprint·SaveGame 직렬화에 영향이 없어야 한다. 클래스/구조체/enum 이름·순서·UPROPERTY는 바꾸지 않는다.

### 10.2 현재 위반 (2026-10-12, `7a63971` Runtime 기준, ARCH-04 gate 유지)

gate는 `Tests/`를 제외한 `.h/.cpp` 221개에서 **고유 source → resolved target include 쌍**을 센다. 같은 쌍의 반복 include와 줄 수는 중복 집계하지 않는다. 현재 **18쌍 = allowlist 18개**다. ARCH-04에서 Coordinator→Monitor 1쌍·Slab→UI Host 1쌍·Player→Gizmo 2쌍을 해소했고 UXR-03 보완·UXR-04·UXR-08 신규 위반은 없다. L1→L3·L2→L3 include 위반은 0이다.

| 방향 | include 쌍 수 | 위치 | 해소 항목 |
|---|---:|---|---|
| L1→L2 | 9 | `Scenario/VirtualSensorSlabContextSubsystem.h`, `VirtualSlabFrameContext.h`, `SlabRunResultsSubsystem.h`를 Camera 1·Sensor 3·Core 5쌍이 include. `Core/VirtualSensorStreamPublisherComponent.cpp`가 2쌍이며 나머지는 각 1쌍 — 이동 전에는 같은 L1이었다 | ARCH-06 |
| L1→L2 | 9 | `Core/VirtualSensorHighThroughputTransportSubsystem` 4·`VirtualSensorPcdReceiveContract.cpp` 1·`Sensor/VirtualSensorExternalSourceHostActor` 4개가 `WebSocket/TC` 수신 진단 헤더를 include | ARCH-05 |
| **합계** | **18** | 모두 L1→L2. L1→L3 0 + L2→L3 0 | ARCH-01의 38개 이하 유지 |

Coordinator는 L1 `IVirtualSensorMonitorView`, Player는 L1 `IVirtualSensorPlacementInputOwner`로 호출한다. 호환용 `BindMonitorWidget`과 기존 Player의 Gizmo getter/register 시그니처는 그대로이며 본문은 `UI/` 번역 단위에 있다. `BoundMonitorWidget`의 concrete 타입·`UPROPERTY(Transient)`·GC 보유도 유지하므로 UI 타입 forward 선언/Blueprint 핀까지 없앤 것은 아니다. `Slab/SlabUiRegistry`는 약한 Actor 존재 정보와 활성 등록을 분리해 생성·로드·복제·BeginPlay 전 및 EndPlay 후 유효 Actor의 기존 판정을 유지한다. 센서→Scenario 9쌍은 ARCH-06, TC 관련 9쌍은 ARCH-05에서 해소한다. 미사용 Public `PixelStreaming*` 의존 3개는 include gate 외 ARCH-08 항목이다.

실행: `Scripts/validate_architecture_layers.ps1` (`-Json`은 상세 결과). 신규 위반과 stale allowlist 모두 exit 1이며, 경로별 원본은 `Scripts/architecture_layers_allowlist.txt`다. `-SourceRoot`·`-AllowlistPath`는 `Saved` 안의 음성 대조 fixture용이며 일반 gate에서는 생략한다.

### 10.3 구성 요소 책임 (현재)

| 구성 | 책임 |
|---|---|
| VirtualSensorActorBase / Camera·LiDAR Actor | 실행·편집 상태·interaction·외부 프레임 API(`SubmitExternalFrame`) |
| Capture / Scan / Analysis / Visualization / Export | 측정과 파생 처리 분담 |
| VirtualSensorSchedulerSubsystem | 자동 작업 분배; 센서 수 기반 60/30 FPS 예산은 지원 보증이 아님 |
| IVirtualSensorAcquisitionBackend | 기존 GPU LiDAR 경계; 아직 Camera/SDK 전체의 범용 확장 계약은 아님 |
| Output / StreamPublisher / Transport / Recorder / HighThroughputTransport | 프레임 라우팅·직렬화·송수신·기록, Raw TCP STOMP worker |
| FileSave / PeriodicFileSave | Widget 수명과 독립적인 bounded 파일 작업 |
| Placement / Layout / OutputPreflight | 북마크·자세 이력, 장비 수명·전체 배치, 읽기 전용 PCD 사전 점검 |
| SlabContext / Replay / RunResults / SlabActor | 업무 세션·보관·이동·실행 결과 (L2) |
| ToolWorkspace / PanelHost / UiHost | 명시적 소유 UI·Main/Viewport 재호스팅 (L3) |

`UVirtualSensorIntegrationSettings`/`UVirtualSensorDtCoreBridgeSubsystem`/Bootstrap Actor는 **소스에 없다**(과거 제안). 구현된 것으로 오인하지 않는다.

## 11. 검증

- 문서만 바꾸면 링크·경로·코드 일치와 `git diff --check`만 확인한다.
- C++ 변경은 Editor/Live Coding 종료 후 빌드하고 관련 그룹 → 전체 `MA0T10` 순서로 실행한다. ARCH 단계에서는 매 단계 **레이어 gate**와 **보호 경로 불변**도 확인한다.
- 표준 진입점 `Scripts/verify.ps1 -Level quick|feature|release`와 `Scripts/validate_architecture_layers.ps1`은 ARCH-01/07에서 추가된다. 추가 전에는 아래 명령을 사용한다.
- opt-in 시험은 `MA0T10_*` 환경변수로 켠다. skip을 pass로 세지 않는다. 변수 목록은 ARCH-07에서 문서화한다.

검증 명령/그룹은 사용 목적에 맞게 선택한다. 기본 quick check가 운영맵 생성이나 전체 성능 테스트를 뜻하지 않는다.

```powershell
& Scripts/validate_sensor_v2_refactor.ps1 -RequireAssets
& "C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$($PWD.Path)/ma0t10_dt.uproject" -d3d12 -unattended -nosplash -nop4 "-ExecCmds=Automation RunTests MA0T10;Quit" "-TestExit=Automation Test Queue Empty"
```

실제 RHI/Artemis 그룹은 해당 환경변수·Broker·외부 구독자가 필요하다. skip을 pass로 세지 않는다. `run_sensor_map_stream_rhi_smoke.ps1`, `run_fullspec_performance_evidence.ps1`, `run_lidar_geometry_stream_test.ps1`의 인자를 먼저 확인한다.

과거 날짜별 검증 수치·조건은 [검증 이력 보관](docs/VALIDATION_HISTORY.md)에 원문 그대로 있다.

## 고정 참고자료

아래 문서는 이전 상세 명세/작업 기록이며 현재 UI·지원 성능의 기준 문서는 아니다. 경로를 참조하는 스크립트를 위해 보존한다.

- 계약: [Camera v1](docs/camera_payload_schema.md), [LiDAR v1/v2](docs/lidar_payload_schema.md), [전송](docs/server_transport_contract.md), [과거 스트리밍 상세](docs/sensor_streaming.ko.md)
- 센서: [테스트맵](docs/sensor_test_map_setup.ko.md), [높이·좌표](docs/lidar_height_semantic_coordinates.ko.md), [태그 없는 형상](docs/lidar_geometry_validation.ko.md), [V2 migration](docs/sensor_v2_migration.ko.md)
- Slab/UI: [시뮬레이션 상세](docs/slab_simulation.ko.md), [재생 상세](docs/slab_scenario_replay.ko.md), [센서 연동](docs/sensor_slab_integration.ko.md), [Workspace 상세](docs/sensor_tool_workspace.ko.md)
- 이력/범위: [PR16 병합 기록](docs/sensor_pr16_merge_notes.ko.md), [Editor smoke 이력](docs/editor_smoke_test.md), [실장비 adapter 제안](docs/real_sensor_adapter_plan.md), [PixelStreaming 참고](docs/pixel_streaming_setup.md)
