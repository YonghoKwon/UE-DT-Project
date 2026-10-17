# UE-DT-Project — 가상 센서와 Slab 시뮬레이션

기준: **`feat/uxr-16` (통합 기준 `5dd9327`, UI `b6f2dc8`) / DTCore `b2504d1`, 2026-10-16**. 작업·빌드·검수·Editor 실행은 **`C:\Unreal Projects\ma0t10_dt`** 하나에서 한다(`m7at10_dt`는 무관한 다른 프로젝트). Unreal Engine 5.3 C++ 프로젝트로 Camera·LiDAR 측정/미리보기/송신,명시적 IMU 부착과 Slab 벌크 시나리오의 보간 이동·분석 표시·차트·재실행을 제공한다.

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

TESTSET-01(사용자2026-10-16, 임시 기능): 검증 Runtime4202a91, 제품 동작40049ba. 기본 소재9999·목표100을 원본 보관/worker 파이프라인으로 생성하고, 일반 시나리오 우선·완료 후 선택→보기 전용 재생을 제공한다. 실제100개 두 실행·각351프레임·release14단계(고유290 Success/skip37/실패0)·WBP7·golden·Python69·layer0 통과. 순차32.689초/파이프라인28.524초, engine/GT/callback150ms 초과0. 9baf759 대비 offscreen1+1 ABAB12회2% gate 통과(FPS 저하−0.00234%·p95동일·GT−1.66243%). 직접 UI는 모니터 캡처0x80070057로 입력0·환경 보류이며 표시형 성능/선택 모달리티 전 조합은 미수행이다. 전송·다중 프로세스는 제외했고 상세 증거·최초 실패·정리 제한은 `Saved/Reports/TESTSET-01/RESULT.md`를 따른다.

UXR-16 v1.1 통합 점검(`b6f2dc8`, 기준 `5dd9327`): API/일괄·IMU 활동은 기존 경고와 함께 표시하고 문구·꺼짐·숫자 글자 배율·변환 선행 안내를 정리했다. Editor/Game Development·release14단계(고유290 Success/조건부skip34/실패0)·WBP7·어휘114/기본34영역/허용7영역·위반0·layer0/0·전후108장씩·통합/관련 집중9개가 통과했다. 오프스크린1+1 ABAB12회2% gate는 FPS 저하 +0.0022%·engine p95 +0.0000%·GT -0.6700%. **Windows 잠금으로 직접 재시도는 입력0·보류이며 표시형 성능은 미수행**이다. 수용 진행률70%, 상세/초기 resize 실패·skip 이름·보호/개인 복원은 `Saved/Reports/UXR/UXR-16/RESULT.md`, 후속은 [통합24행/70분 점검표](docs/MANUAL_CHECKLIST.md)다. 아래 이전 항목의 직접/성능 결과를 이번 SHA의 수용으로 합치지 않는다.

IMU-01(사용자 2026-10-16, 로드맵 #14): `feat/virtual-imu`, Runtime `a5de63c`, 기준 `f5a55d2`. 선택 장비에 세션 전용 IMU를 명시적으로 부착한다. 집중 D3D12 6 Success, 직접 부착·400Hz·노이즈·기록·CSV·전송 선택 점검, 실제 Artemis 445메시지/1484표본(gap/invalid/duplicate 0), Python 38 Success를 확인했다. release14단계(고유289 Success/조건부skip34/실패0)와 기본 꺼짐 ABAB12회2% gate가 통과했다(FPS 저하 −0.008%,p95 0%,GT −0.487%).2개400Hz 켜짐 비용은GT+0.223ms(+3.41%)·p95동일이며,전체 제한은 `Saved/Reports/IMU-01/RESULT.md`를 따른다. 아래 이전 항목의 검증 SHA·수치를 IMU 검증 결과로 읽지 않는다.

| 영역 | 현재 상태 | 남은 핵심 조건 (ID) |
|---|---|---|
| 가상 IMU (IMU-01) | 세션 전용 명시 부착·100~400Hz 비력/각속도/정답 자세·가정 노이즈·5Hz 그래프·독립 출력/CSV/선택 라벨/API/ROS adapter. 집중6·직접 주요흐름·Artemis1484표본·Python38·release14단계 통과 | 기본 꺼짐 ABAB12회2% 통과(FPS −0.008%·p95 0%·GT −0.487%);2개400Hz 켜짐GT+0.223ms(+3.41%)·p95동일. 상세 `Saved/Reports/IMU-01/RESULT.md`. WSL 실측은 요청대로 단위 테스트로 대체;실제 deskew·실장비 교정·장시간·독립검토는 별도 |
| 아키텍처 | ARCH-01~08 완료: L0 Contracts → L1 센서 → L2 Scenario/Slab/TC → L3 UI 단방향, layer gate 위반·allowlist **0** | DTCore 변경 후보 정리 후 DTCore 수정 단계, 이후 플러그인화(R5) |
| 화면 구성(UX) | UXR-16: 자동화 그룹·API/일괄·IMU 활동/경고 공존, IMU 숫자 배율·현재 꺼짐·데이터셋 변환 선행 안내. 동일4조합 전후108장씩, 캡처 기반 큰 설정 구조 목업은 제안 | Windows 잠금으로 직접 재시도 입력0·환경 보류. [24행/70분 점검표](docs/MANUAL_CHECKLIST.md), 표시형 성능·첫 사용자·기존 자유 배치 FAIL·외부 수용은 남음. `Saved/Reports/UXR/UXR-16/RESULT.md` |
| 새 맵 적용 (MAP-01) | Rig 하나·Slab 없음 안내·DTCore 맵 점검·새 맵 2개, Editor/Game·release 14단계·242 Success/skip20·16장·성능2% gate 통과 | 직접 마우스·DPI 수용은 UXR-07로 이관, DTCore 업무 연결 조회 API 제한; 독립 검토 전 90% |
| 센서 배치·위치 저장 | PIE 추가/복제/소유 삭제·북마크·Undo/Redo·전체 배치 저장/복원 구현, 자동화와 주요 직접 저장/복원 확인 | 자유 배치 입력·전체 직접 조합·첫 사용자 수용·DPI 전수 (UX-01 70%, UI-03 70%) |
| PCD 송신 | PCD-OPT 기본 자동 전송률 조절·생략 집계·소켓 대기 구현. 실제1080p 제한 링크20Hz 5분 과부하0·gap95ms, 시나리오 변경12회도 과부하0/hitch0. Artemis20Hz 생략0 | 구현·CLI gate 완료. 291b4f9 대비 perf: FPS −0.01%·p95 동일·GT +1.77% 통과. 운영 PIE 멈춤 원인 미확정; 직접 UI는 Windows 보안 팝업으로 보류. 대표 장면·4+4·60분·부하 환경 (RT-05 70%, RT-08 40%) |
| 절대 성능 (PERF-01) | 미송신 표시/오프스크린 1+1·2+2 ABAB×3 및 초기 GT 초과 조건 재측정 완료. FPS·engine p95 회귀 없음; 재측정 GT +1.63/+1.52/+1.42%로 기준 안. Editor/Game·quick8·D3D12 242 Success/skip22·WBP7 통과 | 초기 GT 초과 원본·이분 탐색·Insights 근거 보존. 실제 2+2 PCD 송신은 Broker 환경 보류; 기존 송신 포함 기록과 조건 다름. `Saved/Reports/PERF/PERF-01/RESULT.md` |
| 사용자 검수 F11·F12 (INSP-FIX-1) | `48d93d6`: 신규 PCD 선택을 덮어쓰던 관찰 검증 버튼 수정, TCP 연결 시험·대기/연결/단절 표시·첫 차단 사유 정리. release14단계 Success294/조건부skip38/실패0·Editor/Game·WBP7·어휘·golden·layer0. 로컬 실제 PCD100프레임/106,567,882bytes·invalid0, 표시형1+1 ABAB12샘플2% gate 통과 | 직접1280×720·DPI1·글자150%에서 기본값/토글·차단 이동·연결 시험 확인. M13/M15 전체·M14 제한 링크/실제 단절 조합은 별도. `Saved/Reports/INSP-FIX-1/RESULT.md` |
| Slab 시나리오 | 수신·보간 이동·차트·재실행·데이터 필수 실행·초기 위치 복귀 구현, 실제 Artemis 통과 | Engine/WSS 필수 모드·장시간 (DATA-01 90%), 재생 p95 20ms 목표 미달 이력 |
| 카메라 정답 데이터 (CAM-GT-01) | 기본 꺼짐·Depth/semantic/Actor ID/bbox·snapshot·비동기 저장 유지. UXR-15 CAM-GT-FIX로 체크 즉시 실제 적용·끄기 후 RGB 복귀, UI→4모드/4파일·프로필/배치 디스크 복원 회귀 추가 | 1920×1080·DPI1·글자100%의 직접 on/off·4모드·저장 재검수 완료. 다른 직접 조합/반복 수용은 통합 점검표, 서버 송신/ROS2/3D bbox는 2단계 제안. 원본 `Saved/Reports/CAM-GT-01/RESULT.md` |
| LiDAR 사실성 (LIDAR-FID-01) | 모두 기본 꺼짐. UXR-15에서 실제 체크3개→Scan 상태·프로필/배치 복원·CPU trace/PCD 출력·UI dataset RHI 반영 회귀 확인 | 1920×1080·DPI1·글자100% 직접3개 on/off 완료. 다른 직접 조합/이동 Slab은 통합 점검표. 기존 켜짐 비용 GT +2.658ms(+50.20%)·p95 +0.510ms(+3.06%)와 가정 모델 한계는 `Saved/Reports/LIDAR-FID-01/RESULT.md` 유지 |
| 데이터셋 생성 (DATASET-01) | 구현·자동 검증 완료. UXR-15에서 UI 시작→정답/사실성 on/off 산출물·범위15개 sampled 값 회귀와 실제3장면 직접 생성 완료·각7파일 확인 | 기본 꺼짐·세션 draft/명시 시작 유지. 직접 범위15개·포함3개 on/off 완료, 취소/재개·전체 직접 조합·장시간은 통합 점검표. 초기 cold61ms peak·geometry/동일 PIE resume 제한·기존100장면 검증은 `Saved/Reports/DATASET-01/RESULT.md` 유지 |
| 표준 데이터셋 내보내기 (EXPORT-01) | Runtime eeff0c1: GPU20장면·COCO80마스크·KITTI407519점 재로딩/투영, Python19·release285 Success/skip32·2% perf 통과 | 기본 없음. UXR-16 직접 재시도도 Windows 잠금으로 입력0·보류(이전 Windows Security 이력 보존). KITTI 기울기 근사·custom class 제한은 [사용법](Tools/DatasetTools/README.md), 상세는 `Saved/Reports/EXPORT-01/RESULT.md` |
| 외부 ROS2 변환 (ROS2-01) | Python adapter·전용 WSL2 도구. Windows25·WSL23 Success/skip2·ROS 직렬화3·실제 Artemis adapter4, UE PCD 20/10Hz 각5분 송신 검증 통과 | 20Hz 브리지6200/ROS6119, 10Hz UE3099/브리지3098/ROS3083. JPEG Image 디코더 결정·직접 3D 수용 보류(80%). [ROS2로 보기](Tools/Ros2Bridge/README.md), `Saved/Reports/ROS2-01/RESULT.md` |
| 데이터 정확성·수명 | Raw receipt 상한, Camera FrameId-픽셀 정합, Esc 단일 종료, 0Hz 진단 구현 | 최종 범위 정리·직접 조작 (RT-01/04 90%, RT-02/03 70%, LIFE-01 70%) |
| 자동화 | UXR-18 `4c14a82`: Editor/Game Development·release14단계, D3D12 고유293 Success/skip37/실패·NotRun0·WBP7·어휘114/기본36기록(고유33)/허용7·위반0·golden·layer0 | `ff1ec9a` 대비 표시형 perf는 두 시도 모두0xC000013A 제어 종료로 중단되어2% 미판정. 직접6행은0x80070005/0x80070057·입력0·환경 보류. 상세는 `Saved/Reports/UXR/UXR-18/RESULT.md` |
| 플러그인·이식 | 미착수. 소스·자산·설정을 함께 옮기는 방식만 가능 | 이번 버전 범위 밖 (PL-01~04는 ARCH 이후) |

UXR-08 근거 보고서: `Saved/Reports/UXR/UXR-08/RESULT.md`(집중33 Success, 전체227 Success/skip20, 렌더24장, 성능2% gate 통과; 직접 조작 보류).

산화 스케일 보완: `Saved/Reports/UXR/UXR-08/oxide/RESULT.md`(새 자산 1개, 집중40 Success/skip6/실패0, Editor·Game Development/quick/WBP7/최소 Windows cook 통과, 5종 동시 비교·근접2장·성능2% gate; 직접 조작 미수행).

UXR-04 근거 보고서: `Saved/Reports/UXR/UXR-04/RESULT.md`(집중 28 Success, 렌더 8장, 성능 2% gate 통과; 직접 조작 보류).

최신 데이터 UI 근거 보고서: `Saved/Reports/UXR/UXR-03/supplement/RESULT.md`(UXR-03 보완), 기존 5탭/전체 회귀는 `Saved/Reports/UXR/UXR-03/RESULT.md`(커밋하지 않음). 과거 수치는 [검증 이력](docs/VALIDATION_HISTORY.md).

UXR-17 미리보기 카드는 새 Camera의 첫 영상까지 **영상 준비 중…**, 5초 뒤 센서 상태 확인 안내를 표시하고 유효 영상에서 즉시 해제한다. 카드에 **일반/깊이/종류/개체**와 깊이0–50m 색 막대·공통 분류 색·개체별 색 안내를 표시한다. 이름만 말줄임하고 상태·Hz를 우선 보존하며 전체 이름은 툴팁으로 확인한다. LiDAR 점군만 보기도 배지로 구분한다. 기존 보기 설정 메뉴와 측정/출력/저장 계약은 같다. 검증·직접 환경 제한은 `Saved/Reports/UXR/UXR-17/RESULT.md`를 따른다.

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
| `/Game/MA0T10/Maps/Tests/PortableSetupMap` | Rig + 바닥 + TrackReference/Slab/조명만 있는 새 맵 적용 fixture |
| `/Game/MA0T10/Maps/Tests/PortableNoSlabMap` | Rig + 바닥만 있는 센서 전용 fixture |
| `/Game/MA0T10/Maps/Tests/LidarSemanticValidationMap` | 같은 재질·태그 없는 판/물체의 형상 구분 |
| `/Game/MA0T10/Maps/Tests/SensorScaleStressMap` | 10,000 정적/1,000 이동 proxy 스트레스 fixture; 성능 보증 아님 |
| `/Game/MA0T10/Maps/SensorTestMap` | 기존 운영 테스트맵; 로컬 사용자 변경 보호 |

맵 생성 스크립트는 신규 fixture가 필요할 때만 대상·기존 자산을 확인하고 실행한다. `SlabScenarioValidationRig`는 센서 ID·프로필·로컬 Broker 등을 바꾸는 **테스트 도구**이므로 운영맵에 옮기지 않는다.

DTCore 계약 메모: DTCore는 main `b22af0b`에서 필요한 오류·종료 안전성만 적용한 `b2504d1`을 사용한다. 기존 센서·Slab 기능을 유지하는 별도 연동 브랜치에서 검증했다. `OnConnected`는 성공/실패를 포함한 구독 완료 callback 집계이며 모든 구독 성공을 보증하지 않는다. custom 수신 binding을 보존하고, 공통 5초 timeout·all-ready 상태기계·World cleanup의 GI 전역 큐 폐기는 포함하지 않는다. 이전 안정화 기록의 해당 API 설명은 현재 계약으로 사용하지 않는다.

<a id="4-다른-맵에서-현재-기능-연결"></a>

## 4. 새 맵에 적용하기

같은 프로젝트 안의 새 맵에는 **`VirtualSensorToolRig` 하나**를 배치한다. 별도 플러그인·새 모듈·WBP·Level Blueprint 배선은 필요하지 않다. Rig는 L3 조립 Actor이며 측정·통신·환경 배치를 소유하지 않는다.

- [ ] 프로젝트 공통 설정 확인: 이 프로젝트는 DTCore 활성화, `BP_DxGameInstance`, 기본 `DTCore.DxGameMode`, 공통 센서/Slab 자산과 TC DataTable을 이미 사용한다. 새 맵의 World Settings가 이를 override하는지 확인한다. Rig가 GameMode/GameInstance/Main/Player를 교체하지 않는다. PixelStreaming 의존·플러그인도 유지한다.
- [ ] 맵에 `VirtualSensorToolRig`를 **하나만** 넣는다. 기존 Coordinator·센서 UI Host·Slab UI Host·Replay UI Host가 각각 하나 있으면 같은 인스턴스를 재사용하며, 빠진 도구 구성만 실행 중 생성한다. 중복 도구 구성은 거절한다. 기존 Host의 fallback/자동 생성 선택은 덮어쓰지 않는다. 새 Host는 기존 자동 생성·native fallback 기본값을 사용한다.
- [ ] Slab을 쓸 때만 `SlabTrackReferenceActor`와 `SlabActor`를 직접 배치하고 Slab의 `TrackReference`를 연결한다. 치수/위치 단위·레일·`ReceiverId`·출력 대상도 확인한다. 여러 Slab은 Rig의 `TargetSlab`을 명시하고 `ReceiverId`를 각각 다르게 지정한다. 배경·조명·관찰 위치는 해당 맵에서 별도로 세팅한다.
- [ ] PIE 실행 후 `배치`에서 Camera/LiDAR를 추가하고 선택·이동·현재 위치 저장을 사용한다. Rig가 장비나 Slab을 미리 생성하거나 서버/전송을 자동 시작하지 않는다. Slab 없는 맵은 센서 기능을 그대로 쓰며 시나리오와 Slab 카드에서 `Slab이 없습니다 · 배치 방법`을 안내한다. 실행 중 나중에 배치한 유일 Slab도 1초 간격으로 연결한다.
- [ ] **도구 → 맵 점검**에서 ✓/⚠와 한 줄 해결 방법을 확인한다. Rig/Coordinator/화면, Slab 대상·TrackReference·ReceiverId, `IFactory-agent` TC, DTCore `WebSocketTopics`의 `topic.scenario`, 업무/출력 서버, DTCore GameMode, `DT_DxLevel` 등록을 읽기 전용으로 검사한다. 열려 있을 때만 최대 1Hz로 검사하며 맵·설정·구독을 자동 수정하지 않는다.
- [ ] Slab 전문을 받을 경우 기존 업무 서버와 `topic.scenario` 구독을 직접 확인한다. 센서 출력 서버는 별도 `도구 → 서버 연결 설정`에서 연결한다. DTCore `b2504d1`에는 현재 업무 서버 연결 상태의 공개 조회 API가 없어 이 항목은 **확인 불가 경고**와 로그 확인 방법을 표시한다. `OnConnected` 이력만으로 현재 연결 성공을 추정하지 않는다.
- [ ] 패키징/레벨 전환에 사용할 맵은 DTCore Settings의 `LevelDataTable`(`DT_DxLevel`)에 `IsUse` 행으로 등록하고 cook 대상도 확인한다. Rig를 놓는 것만으로 DataTable이나 패키징 설정에 등록되지 않는다. 새 fixture 생성도 이 표를 자동 저장하지 않는다.
- [ ] 새 맵의 저장은 Editor에서 명시적으로 한다. 실행 중의 **맵 저장 예약은 SensorTestMap 전용**으로 유지하며 다른 맵에서는 버튼을 비활성화하고 직접 API도 거절한다. 실행 중 배치는 기존 `저장 위치`/`전체 배치` 기능으로 저장한다. Rig는 맵을 저장하지 않으며 EndPlay에서 자신이 만든 도구 구성만 정리하고 재사용한 연결을 복원한다.

재현 스크립트는 `Scripts/setup_portable_sensor_maps.py`다. 승인된 위 두 fixture 중 하나라도 이미 존재하면 쓰기 전에 중단한다. 생성 외의 일반 빌드/테스트에는 실행하지 않는다. 자동 PIE·렌더 증거는 `Saved/Reports/MAP/MAP-01/`, 직접 마우스 확인은 UXR-07의 `MANUAL_RUNBOOK.md`와 MAP-01 `manual/CHECKLIST.md`에서 별도로 수행한다.

### 기존 수동 연결 (호환)

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

맵 저장 예약은 **SensorTestMap 전용**이며 다른 맵에서는 비활성화된다. 신규 맵의 영구 저장 정책은 자동으로 일반화하지 않았다.

### 이식 시 누락하기 쉬운 항목

- C++ 파일 외에 Build.cs/module API macro/include 경로/`/Script/ma0t10_dt` 경로, DTCore 의존성, TC DataTable 등록을 함께 검사한다.
- Unreal **Migrate**로 WBP/Niagara/material 및 의존 자산을 옮긴다. 수동 파일 복사나 일괄 문자열 치환만으로 완료되지 않는다.
- 센서 자산: 세 센서 WBP, `NS_VirtualLidarPointCloud`, `M_VirtualLidarPointSprite`, GPU 분류용 `M_LidarSemanticId`.
- Slab 자산: 재생·차트·진행 WBP, 기본 표면/분석 재질, `M_SlabAnalysisReadable`, `M_SlabTextReadable`, `F_SlabDiagnostics`.
- 진단 폰트는 offline atlas라 실행 PC에 글꼴 설치가 필요 없다. 제작용 재생성 스크립트는 Malgun Gothic을 사용한다.
- 공용 설정 객체와 단일 Bootstrap 진입점은 아직 구현되지 않았다. 자세한 분리 계획은 [ROADMAP](docs/ROADMAP.md)을 따른다.

### 패키지 배포: 서버 연결 설정 (PKG-01)

Shipping과 Development 패키지는 SaveGame 위치가 다르다. Development는 패키지 안 `Saved`, Shipping은 `%LOCALAPPDATA%\ma0t10_dt\Saved`를 쓴다. 그래서 앱의 "서버 연결" 탭에서 저장한 값이 Shipping으로 넘어가지 않았다. 이제 전송 설정은 **프로젝트의 `Config/SensorTransport.ini`** 하나로 관리한다. 에디터가 이 파일을 읽고, **패키징하면 pak 밖의 일반 파일로 `<패키지>\ma0t10_dt\Config\SensorTransport.ini`에 자동 복사**된다(Build.cs `RuntimeDependencies` NonUFS, Development·Shipping 동일). 프로젝트 파일을 채워 두면 패키징 결과가 바로 그 값으로 동작하고, 대상 PC별로 다르게 하려면 패키지 옆 파일만 고친다. 패키징 때 UE가 ini 주석 줄은 지우지만 값은 그대로 남긴다(`tcp://…` 확인).

```ini
; 프로젝트 Config/SensorTransport.ini (패키징 시 <패키지>\ma0t10_dt\Config\ 로 복사)
[MA0T10.SensorTransport]
TransportMode=StompWebSocket      ; 필수: 비우면 맵 기본값(LogOnly)이라 송신하지 않는다
BrokerUrl=tcp://127.0.0.1:61616   ; PCD 고속 TCP. ws://host:port 도 가능. 따옴표는 있어도 되고 없어도 된다
ExportTopic=                      ; PCD Topic (비우면 기존 값 유지)
LidarTopic=
CameraTopic=
AckTopic=
UserName=
Passcode=                         ; 평문. 실제 값은 커밋하지 말고 대상 PC의 패키지 옆 파일에 넣는다
HttpEndpoint=
```

- **비어 있는 키는 기존 값**(맵·앱 저장값)을 그대로 쓴다. `//`가 들어간 값도 잘리지 않는다(직접 파싱).
- 찾는 순서: `SensorTransport.ini` → 같은 섹션의 `Game.ini`(호환) → `DefaultGame.ini`(이때는 URL을 따옴표로 감싼다). 각 파일은 DTCore와 같은 폴더 순서(`ProjectDir/Config` → `LaunchDir/Config` → …)로 찾고, 값이 있는 첫 파일만 쓴다.
- **`Config/Game.ini`·`SensorTransport.ini`는 pak에 넣지 않는다**(`DefaultGame.ini [Staging] +DisallowedConfigFiles`). 이전에는 프로젝트의 빈 `Game.ini`가 pak에 들어가 패키지 옆 파일을 가렸다. 그래서 DTCore `[DTCoreRuntimeOverride]`(WebSocketUrl·Login·Passcode)도 이제 패키지 옆 `Game.ini`로 바꿀 수 있다. 비어 있으면 `ws://localhost:31000`을 쓴다.
- 적용되면 Shipping에서도 DTCore 로그(`<실행 폴더>\Logs\CustomLogs\DxLog_날짜.log`)에 `[SensorTransport] applied N deployment keys from … (broker=…, passcode=set/unchanged)`가 남는다. 비밀번호 값은 기록하지 않는다. 실행 폴더는 exe를 실행한 현재 폴더다.
- 이 설정이 적용 중이면 앱의 서버 연결 탭은 파일 값을 보여 주고, 전송 시작 때 옛 저장값이나 빈 비밀번호로 덮어쓰지 않는다. 화면에서 바꾼 값은 그 세션에만 적용된다.

## 5. UI와 조작

소유 UI의 기본 화면은 **왼쪽 작업 사이드바**다. 아이콘 레일에서 **배치 / 미리보기 / 시나리오 / PCD / 결과**를 선택하고, 하단 **도구 → UI 표시**에서 글자 배율과 **창 자유 배치 (이전 방식)**를 선택한다. 왼쪽 본문은 한 번에 작업 하나만 표시하며 같은 작업 버튼을 다시 누르면 접힌다. 미리보기는 오른쪽 아래, Slab 요약은 오른쪽 위 카드이고 전역 상태는 하단 한 줄이다. 기존 7개 패널 인스턴스·Blueprint API·장비 선택과 작업 상태를 재사용한다. Host가 명시적으로 등록한 native UI만 관리하며 사용자 Designer·레거시 툴바·외부 Widget은 변경하지 않는다.

화면에서는 장비의 수집을 **측정 시작/측정 중지**, 서버로 보내는 작업을 **PCD 전송**, 로컬 기록을 **파일 저장**, JPEG/telemetry 선택을 **추가 출력: Camera 이미지·LiDAR 정보**로 구분한다. 서버 연결의 `서버 주소`/`출력 주소`는 기존 Broker URL/Topic 값의 표시 이름이다. 결과의 **서버 수락**은 외부 프로그램의 저장·처리 완료가 아니다. 기술 원문은 진단·고급·호환 영역에 남는다. UXR-07 현재 직접 판정은 `Saved/Reports/UXR/UXR-07/manual/session-20261013T0106/CHECKLIST.md`다. 주요 화면의 실제 1280×720·1920×1080 × 글자100/125/150% 접근을 확인했으나 모든 패널·조작의 전수 통과는 아니다. 사용자 체험은 같은 보고서 그룹의 `USER_TRIAL.md`를 따른다.

- 미리보기 제목은 장비·상태·Hz만 표시하고 경고는 별도 한 줄/전체 툴팁으로 제공한다. 긴 ID는 제목에서 줄이되 툴팁과 공개 상태 API의 전체 ID는 유지한다. 장비가 없으면 선택 장비/조작 영역은 숨기고 추가 안내를 제공한다.
- 배치 조작의 미리보기 상태 줄은 고정 높이여서 늦은 프레임 완료가 이동/회전 버튼 위치를 밀지 않는다. 관찰자 바로 앞의 기즈모도 거리 비례 크기를 유지한다. 이동·회전량, 원규격 복원과 측정/송신 정책은 바꾸지 않는다.
- **현재 제한:** 자유 배치 제목 드래그가 실제 마우스 시험에서 창 이동을 만들지 않았다. 후보 hit-test 변경은 복구 효과가 없어 반영하지 않았다. 기본 사이드바의 클릭·전환과 별도로 후속 입력 진단이 필요하며 전체 UI 검수 완료로 표시하지 않는다.

- **시나리오**는 사이드바에 진행 화면을 연다. `보관 목록`과 `시나리오 진행 →`으로 같은 작업 영역에서 전환하고, `시나리오 차트`는 기존 3중 차트로 이동한다. 자유 배치 모드에서는 진행·보관 목록의 동시 표시와 기존 개인 창 배치를 유지한다. 3D 표시·단위·외형은 진행의 **표시 설정** 접힘에 있고 출력 정책은 **PCD 전송**에서 편집한다.
- Slab 정보는 **소재 / 진행 / 자세·위치**의 라벨·값 그리드다. 기본 카드의 현재 표시 위치·속도와 접힌 상세의 원본 행 위치·속도를 구분하며, 일시정지 속도는 `0.0 cm/s (일시정지)`로 표시한다. 치수는 천 단위 구분·불필요한 소수 제거, 각도는 소수 2자리, cm·진행률은 1자리다. 장비 클릭 마커는 일반 화면의 흰 구체를 끄고 기존 DTCore hover 강조·클릭 영역을 유지한다. UXR-08 보완 CLI 증거는 `Saved/Reports/UXR/UXR-08/supplement/`이며 직접 마우스 검수는 보류다.
- 작업 레일과 자유 배치의 상단 버튼은 **토글**이다. PCD/결과처럼 같은 패널의 다른 탭은 먼저 전환하고, 같은 탭을 다시 누를 때 숨긴다. 미리보기 카드의 `×`는 카드만 닫고 레일에서 다시 열 수 있으며 `...`는 보기 설정이다. 숨김은 측정·전송·저장·재생을 멈추지 않는다(배치/설정의 조작 모드만 종료).
- 사이드바의 **배치 → 장비 목록**과 자유 배치의 **장비 선택기**는 같은 Coordinator 선택을 공유한다. 자유 배치 선택기는 `Camera SensorId ● 상태` 형식과 종류별 묶음을 사용한다. 선택만으로 패널을 열거나 측정을 시작하지 않는다. 긴 ID는 말줄임+전체 ID 툴팁이다. 하단 상태바의 긴 경고도 전체 문장 툴팁을 제공한다.
- UXR-02 배치 정리는 구현됐으며 이번 직접 점검에서 시험 장비/북마크 삭제 의도와 취소를 확인했다. 최종 삭제 클릭은 action-time 확인 답변이 없어 보류했다. 정상 PIE 종료로 장비가 사라진 것을 UI 최종 삭제 통과로 세지 않는다. 과거 CLI 근거는 `Saved/Reports/UXR/UXR-02/cli-closure/`에 보존한다.
- **PCD 전송**은 데이터의 `전송`, **결과**는 독립 `결과` 탭을 연다. 다른 탭이 열려 있으면 전환하고 같은 작업 버튼을 다시 누르면 숨긴다.
- **도구**에는 센서 상세 설정, 파일 저장 · 데이터셋, 서버 연결 설정, 진단, 자동화, 시나리오 차트, UI 표시가 있다. 파일 저장/서버 연결/진단은 데이터의 각 독립 탭, 자동화는 기존 Settings의 자동화 탭을 연다. 호환 출력·관리는 센서 상세 설정의 고급 안에 접혀 있다. 기능과 기본값은 바꾸지 않는다.
- **하단 상태바**는 장비 수/측정 수·PCD 전송·시나리오·서버와 첫 차단 사유를 최대 5Hz로 표시한다. UXR-16은 API/일괄 실행과 IMU 기록·전송을 별도 활동 칩으로 함께 표시한다. 켜진 API의 요청 대기와 실제 접수 작업을 구분하고 활동으로 기존 경고를 가리지 않는다. 경고만 노란 칩이며 각 항목을 누르면 관련 화면으로 이동한다. 연결 경고는 서버 연결 설정을 연다. 상태 조회나 화면 열기는 측정·전송·파일 저장을 시작하지 않는다. 서버 연결은 외부 소비자의 처리 완료를 뜻하지 않는다.

UXR-15는 최근 추가 옵션의 상태와 저장 작업을 정리했다. **도구 → 파일 저장 · 데이터셋**에서 일반 프레임 저장과 접힌 데이터셋 생성을 찾는다. 데이터셋 대상 버튼은 누를 때마다 다음 대상을 고르며, 랜덤화는 항목명 아래의 **왼쪽 최솟값 / 오른쪽 최댓값**을 입력한다. Camera 정답과 LiDAR 사실성은 현재 켜짐/꺼짐을 표시한다. 정답 미리보기는 **보기 설정 → RGB / 깊이 / 의미 분류 / 객체 구분**, 로컬 파일은 **정답 데이터 1회 저장**이다. 직접 조작 보류를 묶은 [통합 점검표](docs/MANUAL_CHECKLIST.md)는 준비된 환경에서 약70분 순서와 조건부 보류를 제공한다.

**CAM-GT-FIX:** 정답 체크가 편집값만 바꾸던 결함은 승인된 범위 추가로 수정했다. 체크 즉시 다른 설정과 같은 Actor 검증·적용 경로로 실제 Camera에 반영하며, 기존 전체 배치 v1 저장·복원과 프로필 재적용에도 일치한다. 정답 모드를 보다가 끄면 기존 RGB 영상으로 돌아간다. 새 저장 슬롯·기본값·출력 계약은 없다. 최근 체크27개는 UI 콜백→실제 상태→출력·기존 저장/복원 정책을 자동 검증하고 직접 on/off를 확인했다. 실제 정답4모드·4파일 저장, 정답/사실성을 포함한 데이터셋3장면 완료 근거는 `Saved/Reports/UXR/UXR-15/TOGGLE_AUDIT.md`와 `manual-fixed/`, `manual-closure/`, `manual-inventory/`에 있다. 전체57분 사용자 수용과 원격/서버 환경 점검은 별도다.

하단 경고는 아래 순서의 **첫 한 가지**만 표시한다. 장비 수·PCD 상태·시나리오·서버 칩은 각 상태를 별도로 유지하고, PCD 칩은 전송 중/중지/재개 대기만 짧게 표시한다. 전송률 조절 문장을 두 칩에 반복하지 않는다. 숨김이 작업 중지를 뜻하지 않는 것처럼, 경고가 다른 경고에 가려져도 진단의 개별 집계는 유지된다.

| 우선순위 | 표시 조건·문구 | 확인 경로 |
|---:|---|---|
| 0 | 장비0: 장비 없음 · 배치에서 장비를 추가하세요 | 장비 칩 → 배치 |
| 1 | 연결 여부와 무관한 사전 점검 차단: 첫 차단 사유. 예: PCD 출력 주소가 없습니다 · 서버 연결 설정에서 입력하세요 | 경고 칩·PCD 칩 → 전송의 사전 점검·해결 버튼 |
| 2 | 설정 없음: 연결 설정 필요 · 서버 연결 설정을 확인하세요. 설정 후 연결 시험·전송 요청 전에는 경고 없음 | 경고 칩 → 서버 연결 설정 |
| 3 | 연결 시도/활성 전송 중 단절5초 미만: 서버 재연결 중 · 잠시 기다려 주세요. 5초 이상: 서버 연결 끊김 · 서버 연결 설정에서 확인하세요 | 경고 칩 → 서버 연결 설정 |
| 4 | PCD 과부하 일시 중단: PCD 과부하 · 잠시 후 자동 재개 | PCD 칩 → 전송, 경고 칩 → 진단. 쿨다운·연결·예약 배수 조건 충족 후 재개 |
| 5 | PCD 전송률 조절 중 · 서버 처리량에 맞춤 (n Hz) | PCD 전송 → 고급 → PCD 전송률 자동 조절. n은 조절 중 스트림의 합산 Hz |
| 6 | 현재 과부하/Raw 전송 대기: 전송 지연 · 전송량과 서버 수신 상태를 확인하세요 | 경고 칩 → 진단 |
| 7 | 실행의 필수 출력 오류: 전송 상태 확인 필요 · 진단에서 확인하세요 | 경고 칩 → 진단 |
| — | 위 조건이 없으면 경고 칩 숨김. 과거 누적 실패 수만으로 현재 경고를 만들지 않음 | 결과·진단에서 과거 집계 확인 |

UXR-16의 활동 표시는 위 경고 순서와 독립이다. 경고 칩은 기존 첫 경고를 유지하며, 아래 활동 칩은 경고색을 쓰지 않는다.

| 활동 표시 순서/조건 | 하단 문구·동시 규칙 | 확인 경로 |
|---|---|---|
| 1 · 일괄 실행 활성 | `일괄 실행 중 · n/N · 작업`. 같은 제어기의 API 작업/대기보다 우선 | 활동 칩 → 센서 상세 설정 → 자동화 |
| 2 · 일괄 없이 접수 작업 진행 | `API 작업 중 · 프레임 저장/데이터셋 생성/시나리오 실행/PCD 전송/장비 설정`. API를 꺼도 이미 접수한 작업은 계속 표시 | 같은 자동화 그룹의 주소·진행 상태 |
| 3 · API 켜짐, 접수 작업 없음 | `API 켜짐 · 요청 대기`. 제어 중이나 외부 제어 lease를 뜻하지 않음 | API 토글/주소. 꺼짐·유휴이면 활동 칩 숨김, 설정에는 꺼짐 표시 |
| 독립 · IMU 기록 또는 전송 활성 | `IMU 기록 n · 전송 m`. 전체 등록 장비의 현재 IMU 수이며 측정률/수신 완료가 아님. 위 활동·PCD·시나리오·첫 경고와 함께 유지 | IMU 칩 → 배치 → 기존 IMU 영역 |
| 출처/실제 수신 | UE에는 ROS2 출처/lease가 없어 `ROS2 제어 중`을 추정하지 않음. IMU 전송 선택을 Broker/소비자 수신 완료로 표시하지 않음 | ROS2 피드백·진단/실수신은 별도 확인 |

구현 원본은 `SensorToolToolbarWidget.Status.cpp::RefreshOwnedStatus`와 `SensorToolUiText::PcdWarning`이다. 이는 **표시 우선순위**이며 재연결·자동 조절·과부하 중단/재개 정책을 변경하지 않는다. 서버 연결/수락을 소비자 처리 완료로 표시하지 않는다. 모든 상태 칩은 최대5Hz 갱신·긴 문장 전체 툴팁을 유지한다. 경고 칩은 연결 경고이면 서버 설정, 사전 점검 차단이면 PCD 전송, 과부하·전송 지연이면 진단을 연다.

INSP-FIX-1(F11·F12): 서버 칩은 **서버 설정 필요 / 서버 설정됨 · 전송 대기 / 서버 연결됨 / 서버 재연결 중 / 서버 연결 안 됨**을 구분하고 미연결일 때도 표시한다. 서버 연결 설정의 연결 시험은 `tcp://`·지원되는 `ws://`에서 PCD와 같은 Raw TCP worker를 사용하며 성공은 화면에 `연결 시험 성공 · 서버 연결됨`으로 표시한다. `wss://`는 기존 보안 연결을 유지한다. 스트림이 없다는 이유만으로 설정 부족 경고를 만들지 않는다. 연결 시험은 PCD 프레임을 보내지 않으며 성공이 외부 소비자의 수신·처리 완료를 뜻하지 않는다.

| 패널 / WBP | 현재 역할 |
|---|---|
| 모니터 / `WBP_VirtualSensorMonitorPanel` | 영상/점군 + 장비·상태·측정Hz·실제 경고1줄. 중지/장비 없음 안내. 투영·색상·듀얼·범례·개발자 정보는 **보기 설정** |
| 센서 설정 / `WBP_VirtualSensorSettingsPanel` | **기본 / 고급 / 자동화**. 기본=프로필·품질·주기·거리·미리보기 점·측정 시작/중지. 고급=Camera 정답·LiDAR 사실성·해상도/FOV/채널·다중 검출·IMU 기존 배치 화면 연결·접힌 호환 출력·관리. 자동화=API/일괄 상태·기존 데이터셋/변환 형식/TESTSET 화면 연결·스캔 자동 파일 저장 |
| 데이터 / `WBP_VirtualSensorCaptureExportPanel` | **전송 / 결과 / 파일 저장 / 서버 연결 / 진단**. 새 소유 인스턴스는 전송 탭 |
| 재생 / `WBP_SlabScenarioReplayPanel` | 전체 UUID/내부 보관 ID, 선택 항목 재실행·복사·삭제, 이번 재생 출력 요약 + `PCD 전송에서 변경` |
| 차트 / `WBP_SlabChartsPanel` | 기울기·중심 이탈·Margin 3개를 진행 시점까지만 표시 |
| 진행 / `WBP_SlabProgressPanel` | 상태·진행률·일시정지/재개/중단/초기 위치 복귀, 차트 토글, **표시 설정**(3D·단위·외형) |
| 배치 / `WBP_VirtualSensorPlacementPanel` | 장비 추가·선택·조작, 저장 위치·전체 배치 저장/불러오기·세션 IMU 부착/설정/기록 |

- 기본 사이드바는 고정 배치다. **도구 → UI 표시 → 창 자유 배치 (이전 방식)**에서는 제목 drag·우하단 resize·접기·숨김을 계속 사용할 수 있다. 두 모드는 같은 인스턴스를 사용하며 전환이 측정·파일 저장·전송·재생을 중단하지 않는다. 사이드바 사각형은 Workspace v1의 기존 자유 배치 저장값을 덮어쓰지 않는다. 표시 모드 선택은 현재 PIE에만 적용되고 새 PIE는 사이드바로 시작한다.
- `더보기 → 이 창 위치·크기 초기화`는 해당 패널만, `도구 → UI 표시 → 센서 도구 UI 초기화`는 소유 패널 배치·열림·폰트만 초기화한다.
- 글자 배율은 85/100/125/150%. 작은 창은 스크롤하거나 확대한다. 외부 UserWidget 내부를 재귀 변경하지 않는다.
- **센서 상세 설정**은 UXR-18의 기본·고급·자동화 3탭이다. 새 패널은 기본으로 시작하고 같은 인스턴스의 재열기·선택 변경·재호스팅은 마지막 탭을 기억한다. 기존 UI 저장 구조에 탭 필드가 없어 세션 내 기억만 사용하며 SensorUI v6/Workspace v1 형식을 확장하지 않는다. 고급/자동화 이름 아래 `n개 켜짐`을 표시하고 0이면 숨긴다. 선택 장비의 정답·사실성·다중 검출·부착 IMU와 자동 제어 묶음·데이터셋 사용·변환 형식·자동 파일 저장·미완료 테스트셋을 세며 세부 랜덤화는 중복 합산하지 않는다. 기준은 탭 툴팁에서 확인한다. 한 문장 도움말·성능 영향·`ⓘ`, 기존 BP API·수집 잠금·즉시 적용/복원·저장값은 유지한다. [직접 점검표](docs/MANUAL_CHECKLIST.md#uxr-18-센서-상세-설정-추가-직접-점검)와 `Saved/Reports/UXR/UXR-18/RESULT.md`에서 자동 검증과 직접 수용을 구분한다.
- 듀얼 카메라는 기존 RenderTarget을 공유한다. 보조 카메라는 보기 전용이며 추가 capture/readback을 만들지 않는다.
- **미리보기**는 중지 장비에 `측정 중지 상태입니다 · 측정 시작`, 장비가 없으면 `장비 추가`를 제공한다. 시작은 기존 배치의 잠금·조작 종료 경로, 추가는 기존 배치 창 열기다. 정상 메시지/경고 없음에 경고색을 쓰지 않는다. 숨김/접힘과 닫힌 개발자 정보·범례는 문자열 조립을 멈춘다. 소유 native 보기는 기존 프레임만 표시하며 숨은 동기 측정을 부르지 않는다. 기존 명시 BP 호환 캡처/저장·Designer 경로는 유지한다. UXR-06의 자동 Slate/RHI 검사·렌더20장·성능·전체 이름별 비교는 `Saved/Reports/UXR/UXR-06/`; 마우스는 보류다.
- UXR-01 자동 렌더 검수: 실제 client 1280×720·1920×1080, 글자 100%·150%, window DPI 1.0. 캡처/이름별 회귀 비교는 `Saved/Reports/UXR/UXR-01/`에 있다. 레거시 툴바·사용자 Designer 경로는 유지하며 직접 마우스 검토는 별도다.
- Esc 종료 요청·종료 버튼·선택 변경·패널 숨김/파괴는 조작을 시작한 Actor의 설정과 이전 실행 상태를 복원한다. 드래그 완료만으로 조작 모드를 끝내지는 않는다. 2026-10-12 Computer Use에서 정밀 이동 후 **첫 Esc는 조작만 종료하고 PIE를 유지**함을 확인했다. 기록은 `Saved/Reports/UXR/UXR-02/manual/`에 있다.
- UXR-09의 사이드바·호환·렌더·마우스·전체 회귀 검증은 수행했으나 **성능 2% 수용 기준은 실패**했다. 전체 완료로 표시하지 않으며 수치와 제한은 `docs/IMPROVEMENTS.md`의 UXR-09 및 `Saved/Reports/UXR/UXR-09/RESULT.md`를 따른다.
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

**표시 설정**의 표면 선택은 `기본(현재 값)`/`고온 적열`/`산화 스케일`/`냉각 강판`/`분석용 무광`이다. 산화 스케일만 승인된 새 `/Game/MA0T10/Slab/Materials/M_SlabSurfaceOxideScale`을 MID 부모로 사용한다. 단일 레벨 계산 Gradient Noise로 짙은 회갈색·청회색 얼룩과 밝은 강판의 벗겨짐을 표현하며 외부 텍스처는 없다. `Hotness/Oxidation/Roughness/EmissiveStrength` 계약을 유지한다. 다른 네 선택은 원래 `SurfaceMaterial`의 MID를 사용하고 기본은 첫 선택 전 외형 값과 같은 MID 인스턴스를 복원한다. 두 MID는 선택 변경 때만 생성/재사용하며 `SurfaceMaterial` 저장값을 바꾸지 않는다. 사용자 슬롯 재질은 기존 우선순위를 유지한다. 기존 자산·운영맵은 재저장하지 않았다.

재현 생성 스크립트는 `Scripts/setup_slab_oxide_scale_material.py`다. 대상 자산이 이미 있으면 생성/저장 전에 오류로 중단한다. native soft path의 cook 누락을 막기 위해 `Config/DefaultGame.ini`의 `DirectoriesToAlwaysCook`에 `/Game/MA0T10/Slab/Materials`를 포함한다. 개인 `Config/Game.ini`는 수정하지 않는다. 동시 비교·근접 캡처·검증 보고서는 `Saved/Reports/UXR/UXR-08/oxide/`에 있다.

- `LiDAR 반사율도 함께 적용`은 기본끔이다. 켰을 때만940nm 반사율을 적용하고 기존 gain·검출 확률·양면·포화 설정은 유지한다. 꺼서 opt-in 직전 값을 복원하고 자신이 생성한 응답 컴포넌트만 정리한다. 프리셋 적용과CPU응답 캐시 무효화는 변경 시1회이며 센서 시작/중지/추가 스캔을 하지 않는다.
- GPU는 기존 형상 분류 보기나 물체 필터의ID/depth buffer가 요청되는 지원 불투명 geometry에서만 연동한다. 해당 버퍼가 없으면 요청을 거절하고 안내한다. 보기/필터/geometry가 이후 바뀌면 지원 범위를 다시 확인한다. 추가 캡처/readback이나 자동 설정 변경은 없다. 광학 값은 예시 프리셋이며 실소재/실장비 보정 인증이 아니다.
- 산화 스케일의 opt-in 940nm 반사율은 기존 가정값 `0.12`다(냉각 강판 `0.45`보다 낮음). 어두운 산화막을 표현하기 위한 시뮬레이션 선택값이며 940nm 실측 보정이나 특정 강종/온도/산화 두께의 검증값은 아니다. 실제 RHI 회귀에서 산화 외형만 적용하면 intensity/PCD가 유지되고, 연동 시 CPU 61945.47→37167.23 / GPU 62546.36→37527.77로 감소했다. 해제 후 원래 Camera BaseColor 픽셀·intensity·PCD bytes가 정확히 복원됐다.
- 실행 결과`ma0t10.slab-run.v1`에는 사용한 경우에만 optional `surfaceAppearance`를 추가한다. 초기/마지막 프리셋·연동·외형/광학 값, 사용한 프리셋 목록(최대5종), 변경 수를 기록하고 기존 필드·의미·PCD bytes/Blueprint API/저장 슬롯은 유지한다. 이 필드는 광학 설정의 provenance이며 실측 intensity 평균·외부 소비자 처리 완료를 뜻하지 않는다. 실제 intensity 변화는RHI회귀에서 대조했다.
- 검증: 결정적BaseColor SceneCapture 픽셀·실제CPU/GPU intensity·고정 envelope identity의PCD bytes가 기본/link-off 전후 정확히 같았고, 외형만 바꿔도LiDAR/PCD는 유지됐다. link-on에서CPU61945.47→24778.19, GPU62546.36→25018.54로 변했고JSON에0.08반사율/연동이 기록됐다. production FinalColor/JPEG의 bitwise 동일성이나 실장비 물리성을 이 시험으로 보장하지 않는다. 마우스 검수는 보류다.

### 데이터 필수 실행과 실행 결과

**PCD 전송 → 전송**에서 `신규 시나리오`와 `보관 목록 재생`의 출력을 나란히 편집한다. 각 열은 PCD·추가 출력(Camera 이미지/LiDAR 정보)·**연결 확인 후 시작**을 제공한다. 신규=PCD, 재생=관찰 전용 기본값은 유지한다. 변경은 다음 실행에 적용하며 재생 패널에서는 요약과 `PCD 전송에서 변경` 링크만 제공한다. 최초 전송 화면 진입은 기존 Workspace 경로로 재생 옵션 소유 패널을 숨김 상태로 준비하며 재생을 시작하지 않는다.

API·일괄·TESTSET 및 검수맵의 `검증: 30초 관찰/PCD`는 해당 실행의 출력만 선택한다. 신규 시나리오의 사용자 선택을 덮어쓰지 않는다. 신규 출력 선택은 Slab Actor 값이며 SensorUI SaveGame에 저장하지 않는다. INSP-FIX-1 이전 검증 버튼은 관찰 실행 후 신규 PCD도 껐으므로 해당 PIE에서 꺼진 값은 `신규 시나리오 → PCD 켬`으로 다시 선택한다. 의도적으로 끈 값과 자동화가 끈 값을 구분할 저장 이력이 없으므로 강제 복원하지 않는다. 맵·클래스 기본값은 PCD 켬이다.

기본값 `ObservationAllowed`는 기존 관찰 fallback이고, `연결 확인 후 시작`의 내부 값 `RequireData`는 대상 센서/출력 검사와 실제 STOMP CONNECTED 확인 후 움직임을 시작한다. 준비 시간은 기본 10초(1~120초), 준비 중에는 현재 자세를 유지하며 `중단`으로 취소할 수 있다. 출력이 하나도 선택되지 않은 필수 실행은 거절한다. `점검 대상:` 세그먼트에서 신규/재생/직접 전송 중 하나를 선택하면 한 줄 준비 상태·첫 차단 사유와 관련 화면을 여는 해결 버튼을 표시한다. 상세는 접힘, 필터·간격·receipt 표본은 `고급`이다.

- 연결 끊김·최종 receipt 실패·과부하·센서 삭제/정체·필수 스트림 중지 때 움직임과 신규 접수를 중단하고 최대 10초 동안 승인된 데이터를 정리한다. 자동 재개하지 않으며 재실행은 새 RunUUID다.
- 준비/실행/drain 동안 해당 센서의 규격 편집과 경량 조작 모드는 잠근다. 센서 설정 적용은 원래 실행/정지 상태를 유지하고 Scheduler로 갱신하며, 설정 적용 때문에 동기 캡처·스캔·파일 저장을 추가하지 않는다. NaN/무한대는 거절한다.
- 결과는 `준비 실패 / 사용자 중단 / 부분 전송 / 서버 수락 완료 / 관찰 완료`로 표시한다. 서버 수락은 해당 실행의 승인 프레임 receipt 기준이며 측정률·자체 검증·외부 업무 처리는 별개다.
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

테스트 발행(로컬 Broker가 이미 정상 실행되고 입력 Topic을 구독한 경우, 계정은 `MA0T10_ARTEMIS_USER`/`MA0T10_ARTEMIS_PASSWORD` 환경변수로 설정):

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

### LiDAR 사실성 옵션 (LIDAR-FID-01)

사용자 요청(2026-10-15)에 따른 **점별 시간차·움직임 왜곡·재질 반사**와 ⓘ 설명은 UXR-18에서 `도구 → 센서 상세 설정 → 고급 → LiDAR 사실성`으로 옮겼다. 모두 기본 꺼짐이다. 움직임 왜곡을 켜면 시간차도 켜고, 시간차를 끄면 왜곡도 끈다. 수집 잠금과 기존 Actor 설정 transaction을 따른다. 미리보기 색 목록의 **반사 강도·측정 시각**은 기존 5Hz 이하 cache를 사용한다.

- 시간차는 기존 PCD `time_offset_ns`(33-byte little-endian 레코드)에 기록한다. ML-X 비회전 행 순차는 **가정**이며 실제 제조사 firing 순서가 아니다. 주기 안의 `[0,T)` 범위이고 기본 출력은 계속 0이다. Generic 회전형은 column→ring 순서, Livox는 기존 raster 근사로 실제 non-repeating pattern을 재현하지 않는다.
- 움직임은 acquisition 시점의 linear/angular velocity·표면 평면으로 예측한다. Slab은 기존 보간의 속도를 중립 provider로 제공하고 정지/일시정지는 0이다. 센서 자체 속도는 제공된 속도 또는 두 번째 acquisition부터 pose 차분으로 추정한다. 점군은 deskew하지 않으며 추가 광선 추적을 하지 않는다. 가속·형상 경계·가려짐 변화·변형은 지원하지 않는다.
- 재질은 기존 SurfaceResponse가 우선이며, 없으면 공통 semantic의 Slab .20/Roller .35/Conveyor .12/Floor .25, Oxidized tag .08을 사용한다. **모두 프로젝트 가정값**이다. 거리·입사각·반사율에 따른 강도/확률적 미검출/거리 노이즈에 기존 seed와 ±30mm cap을 재사용한다. GPU는 기존 ID/depth snapshot과 이웃 depth 법선을 이용하므로 추가 proxy capture/readback 비용이 있고 경계 법선은 근사한다. 물리 multi-echo/제조사 raw intensity 동등성은 주장하지 않는다.

구현·정확도·기본 꺼짐 성능·수용 상태와 2단계 제안은 `Saved/Reports/LIDAR-FID-01/RESULT.md`, 추적 항목은 [IMPROVEMENTS](docs/IMPROVEMENTS.md#lidar-fid-01--lidar-사실성-1단계)를 따른다. 새 자산·플러그인·DTCore 변경은 없다.

검증(기준 `e0a6901` → Runtime `eea0268`): CPU 평판 최대오차0.00005126cm, 실제 GPU0.00917cm. 전체 D3D12 **272 Success/skip29/실패·NotRun0**, WBP7 메모리 컴파일·golden14파일 바이트·layer0 통과. 1920×1080/DPI1/폰트100% 자동 강도·시각2장은 직접 수용과 구분한다. 기본 꺼짐은 표시형1+1·10초 준비+30초·ABAB12회 중앙값의2% gate 통과. 같은 구현본의 이동 Slab opt-in ABAB4회에서 FPS59.9989→59.7187, p95 16.6669→17.1774ms, GT5.2945→7.9523ms였으며 각30초에Camera900/LiDAR600프레임·획득 실패/큐초과0을 유지했다. 송신 없는 측정+미리보기 조건이며 외부 수신률 보장은 아니다.

### 측정률·정체 진단

- 시작/재등록 후 `max(1초, 실제 적용 주기의 3배)`는 시작 유예다. 이후 진행이 이 시간보다 오래 없으면 과거 양수 Hz가 남아 있어도 유효 측정률은 0Hz다.
- Settings 상세 부하 요약에서 센서별 시작 유예·측정 중·일시정지·조작용 경량 미리보기·정체를 구분한다. 처리 실패 이력이 있는 정체는 별도 문구로 표시한다.
- 소유 Settings의 상세 부하 문자열은 **고급의 열린 도움말**에서만 조립한다. 닫힌 도움말과 숨김/접힘에서는 상세 조립을 멈추며 기존 Blueprint 상세 조회 API는 유지한다.
- World 시간으로 신선도를 계산하므로 PIE 일시정지는 정체 시간에 포함하지 않는다. 일시정지 중에는 진단만 갱신하고 측정하지 않는다.
- `SensorRates`와 `StarvedSensorIds`를 공개한다. 기존 공정성 수치를 읽는 Blueprint/C++는 `bCameraFairnessEvaluable`/`bLidarFairnessEvaluable`도 확인해야 한다. 판정 불가 수치 0은 정상 공정성을 뜻하지 않는다. 조작용 임시 주기는 정격 공정성 통과로 처리하지 않는다.
- Camera acquisition 신선도는 SceneCapture 제출 정의다. 스트리밍 readback은 해당 capture 직후의 복사와 고정 snapshot을 사용한다. slot 부족/합쳐진 capture는 명시적 파생 실패로 기록하며 나중의 픽셀을 옛 FrameId에 붙이지 않는다. acquisition·JPEG 완료·receipt·소비자 검증 Hz는 서로 구분한다.
- 성능 보고서는 워밍업 이후의 0Hz 표본 및 요청 센서 누락을 평균으로 숨기지 않고 실패로 판정한다.
- 소유 미리보기의 한 줄 Hz는 기존 신선도 판정을 그대로 사용하며 프레임·Payload·공정성·렌더러 세부 정보는 열린 **개발자 정보**에서 확인한다. 숨김은 센서 측정·송신·저장을 멈추지 않는다.

### Topic/Body

| 방향·용도 | 기본 Topic | Body / schema |
|---|---|---|
| Slab 입력 | topic.scenario | JSON, MESSAGE_ID=IFactory-agent |
| Camera 출력 | topic.virtual.sensor.camera.0 | 고성능 원본 JPEG / virtual-camera.jpeg.v1 |
| LiDAR 값 출력 | topic.virtual.sensor.lidar.0 | point 배열 없는 통계 JSON / virtual-lidar.telemetry.v1 |
| PCD 출력 | topic.virtual.sensor.export.0 | 완전한 PCD v0.7 DATA binary / virtual-pointcloud.pcd.v1 |

Raw TCP STOMP worker가 binary I/O·receipt·자체 수신 검증을 담당한다. 기존 JSON `virtual-camera.v1`/`virtual-lidar.v1`은 호환 경로에 남는다. HTTP JSON/raw file POST도 별도 지원한다. `wss://`는 Engine STOMP 호환 경로이며 Raw TCP TLS 지원으로 보지 않는다.

**receipt는 Broker 수락이다.** 내부/외부 소비자 수신 검증, 최종 업무 처리 ACK, 로컬 파일 완료는 각각 별개다. 정상 연결 중 FIFO여도 네트워크 단절 중 프레임을 디스크에 영구 보존하는 기능은 아니다. Raw worker reservation은 입력부터 receipt/최종 실패까지 유지된다. 기본 전역 128MiB·스트림 64MiB 및 Camera 8/기타 20프레임을 넘으면 명시적 과부하로 거부한다. 재시도는 같은 body를 중복 집계하지 않는다.

**PCD 전송률 자동 조절(PCD-OPT, 사용자 승인 2026-10-15):** Raw TCP PCD는 기본으로 예약 프레임/bytes와 준비 큐를 감시하며, 25% 이상 누적될 때 직렬화 전 신규 프레임을 균등하게 생략해 배수 처리량에 맞춘다. 여유가 생기면 원래 전송률로 복귀하며 측정 주기는 유지한다. `bAutoAdjustPointCloudSendRate` 기본값은 `true`; 데이터 패널의 **고급 → PCD 전송률 자동 조절**에서 끌 수 있다. 상태바는 **"PCD 전송률 조절 중 · 서버 처리량에 맞춤 (n Hz)"**, 진단/로그는 별도 생략 수를 표시한다. 이미 접수한 프레임의 FIFO와 예약 상한은 유지하며, 과부하 중단·자동 재개는 최후 안전장치다. 생략은 기존 Slab 출력 실패로 기록하므로 **데이터 필수 실행은 실패 판정을 유지**한다. receipt를 업무 성공으로 바꾸거나 생략을 성공 처리하지 않는다. wire/SaveGame 버전은 유지한다.

**PCD 과부하 자동 재개(PCD-RESUME, 사용자 결정 2026-10-14):** PCD 발행 큐 과부하는 사용자 중지와 구분해 일시 중단한다. `UVirtualSensorStreamPublisherComponent::bAutoResumePointCloudAfterOverload`는 기본 `true`이며, 초기 쿨다운 `PointCloudOverloadResumeDelaySeconds=2`초, 연속 과부하는 2→4→8→16→30초로 늘어난다(`PointCloudOverloadMaxResumeDelaySeconds=30`, 실제 상한 30초). 쿨다운 뒤 이전 직렬화와 해당 PCD의 worker 예약(소켓 쓰기·receipt 포함)이 모두 비고, 전역 예약 bytes가 한도의 절반 이하이며, 연결·전송 지연이 회복되어야 재개한다. 전체 센서 PCD 설정은 모든 PCD 예약이 비어야 한다. 끊김 중에는 보류하고 연결 회복 뒤 다시 확인한다. WSS는 기존 Engine STOMP 연결과 해당 receipt 배수 조건을 사용한다.

재개는 새 acquisition 프레임부터이며 frame id·시퀀스·PCD bytes를 초기화하거나 폐기한 프레임을 재전송하지 않는다. 중단 때 폐기한 프레임은 Slab failure와 과부하 폐기 수에 남긴다. 실제 receipt가 30초간 계속 진행되면 백오프를 초기화한다(`PointCloudOverloadBackoffResetSeconds=30`); 연결된 채 대기한 시간만으로 초기화하지 않는다. 사용자 중지·송신 끄기·스트림 재설정·종료는 재개 예약을 취소한다. 옵션을 끄면 재시작 전까지 중지하는 기존 정책이다. 상태바는 **"PCD 과부하 · 잠시 후 자동 재개"**, 진단은 최소 대기시간·재개 횟수·폐기 수를 표시하며 실제 연결 경고와 구분한다. 설정은 component UPROPERTY, 재개 상태는 실행 중 임시값이며 기존 Blueprint 함수·세션 SaveGame 버전은 유지한다.

### PCD 소비자가 알아야 할 최소 계약

**ROS2로 보기(ROS2-01, 사용자 2026-10-15):** [외부 브리지 안내](Tools/Ros2Bridge/README.md)에 WSL2 설치·기존 PCD 송신·Foxglove 연결·정상 종료/삭제 절차가 있다. UE C++·wire·Broker 설정을 바꾸지 않는다. PCD 로컬 XYZ는 이미 ROS와 같은 m·오른손 축이므로 그대로 사용하고 acquisition TF의 UE 월드 Y만 변환한다. binary JPEG에는 pose/FOV가 없어 CameraInfo는 미보정이며, CAM-GT 정답 데이터의 새 서버 송신과 구분한다. UE UI의 ROS2 상태 표시는 2단계 제안이다.

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

### 카메라 정답 데이터 (CAM-GT-01)

출처: 사용자 2026-10-15. **기본 꺼짐**인 카메라별 선택 기능이다. `도구 → 센서 상세 설정 → 고급 → 정답 데이터`를 체크하면 즉시 적용되며, 측정 중인 카메라의 `미리보기 → 보기 설정`에서 RGB / 깊이(Depth) / 의미 분류(Segmentation) / 객체 구분(Instance)을 선택한다. 화면이 요청하는 정답 데이터는 최대 5Hz이며 숨김/접힘 후 표시 수요가 만료된다. RGB 선택만으로 정답 캡처를 계속하지 않는다. 기존 JPEG·JSON·PCD 송신 bytes와 topic/schema는 유지한다.

- **정답 데이터 1회 캡처:** FileSave 서비스의 admission을 거쳐 다음 카메라 acquisition에서 비동기 처리한다. 측정을 몰래 시작하지 않으며 busy/미지원/실패를 표시한다. `Saved/CameraGroundTruth/<고유 ID>/`에 `depth.exr`(float32 미터), `segmentation.png`(uint8 클래스 ID), `instance.png`(무손실 24-bit Actor ID), `camera_info.json`을 저장한다. JSON은 모든 이미지 저장 뒤의 완료 표식이다. 데이터셋·주기/대량 생성·서버 송신은 추가하지 않았다.
- **Depth:** SceneDepth의 광학축 Z를 cm→m로 변환한다. 광선 방향의 유클리드 거리와 구별하며 변환식을 JSON에 기록한다. 0은 배경/무효, 최대 10,000m다. 화면 컬러맵의 0–50m 범위는 저장 float 값을 자르지 않는다.
- **보정:** 이상적인 pinhole, 왜곡 0. `fx=fy=width/(2 tan(HFOV/2))`, 정수 픽셀 중심 기준 `cx=(width-1)/2`, `cy=(height-1)/2`. 실제 해상도·FOV·UTC·frame id·revision과 UE 월드 pose(m), optical→UE world 4×4 행렬을 같은 acquisition snapshot으로 고정한다. UE 카메라 `(X 전방,Y 우측,Z 위)` → 광학 `(x 우측,y 아래,z 전방)`은 `(Y,-Z,X)`다. 실장비 D455 depth/보정 완료를 뜻하지 않는다.
- **분류:** `Sensor/VirtualSensorSemanticLabels.*`가 LiDAR와 카메라의 기본 규칙·팔레트 원본이다. ID 0=배경/무효/미지원, 1=Other, 2=Slab, 3=Roller, 4=Conveyor, 5=Floor, 6=Sensor. Actor Tag뿐 아니라 기존 이름·클래스·컴포넌트 규칙 우선순위를 보존한다. 기존 개별 LiDAR 사용자 규칙은 유지되며 카메라로 자동 복제하지 않는다.
- **지원 경계:** 기존 LiDAR ID 재질을 격리 proxy 장면에서 재사용한다. 불투명 StaticMesh/ISM/HISM 중 WPO 없는 geometry만 분류하고 실제 depth와 일치할 때만 ID를 채운다. masked/translucent/skeletal/procedural geometry는 추정하지 않고 미분류 수를 표시한다. Actor별 ID는 해당 실행·카메라에서 안정적이며 같은 Actor의 ISM 인스턴스는 같은 ID다. bbox는 보이는 마스크의 instance/class별 inclusive 픽셀 경계이며 3D/가려진 전체 물체 박스가 아니다. 기본 원근 투영만 지원한다.

1+1 표시형1920×1080에서 기본 꺼짐 성능 gate는 FPS −0.0011%·p95 동일·GT +0.7332%로 통과했다. 같은 소스에서 정답 미리보기 수요를 켠 비용은 GT 평균 +0.4635ms, GPU frame 평균 +0.3537ms였다(1280×720·최대5Hz, JPEG encode 포함·미송신). 정확도는 알려진9.9m 앞면 최대 오차0.00007381m, EXR/PNG 무손실 round-trip 통과다. 직접 마우스는 보안 팝업 미조작으로 보류했다.

현재 검증 수치·직접 화면·성능 gate와 제한은 `Saved/Reports/CAM-GT-01/RESULT.md`, 진행 상태는 [CAM-GT-01](docs/IMPROVEMENTS.md#cam-gt-01--카메라-정답-데이터-1단계-p1)을 따른다.

### IMU-01 — 선택 장비에 IMU 부착 (기본 없음)

**배치 → 장비 선택 → IMU · 부착·설정·미리보기 → 선택 장비에 IMU 부착**을 누른다. Camera/LiDAR의 기존 Actor 루트에 `UVirtualImuComponent`를 붙이며 새 Actor·자산·맵·플러그인을 만들지 않는다. 주파수(100~400Hz, 기본200), 산업용 MEMS 가정 노이즈/이상적 측정, seed를 설정한다. 실시간 비력·각속도·자세와 XYZ 그래프는 영역을 펼쳤을 때만 최대5Hz로 갱신한다. 접기·패널 숨김은 IMU 측정이나 기록을 중지하지 않는다.

부착·설정은 **이번 PIE 세션에만** 적용되며 기존 장비 프로필/전체 배치 SaveGame에 저장되지 않는다. 새 장비를 추가하거나 배치를 복원해도 IMU는 자동 생성되지 않는다. 다른 움직이는 Actor/장면 컴포넌트에는 Blueprint에서 `VirtualImuComponent`를 명시적으로 추가·부착하거나 C++의 `UVirtualImuComponent::Attach(Actor)`를 사용한다. 기존 `VirtualSensorMotionProvider`가 부착 체인에 있으면 센서 원점의 rigid velocity(레버암 포함)를 사용하며, 없으면 세 프레임의 위치와 쿼터니언에서 운동을 재구성한다. 일반 Actor의 출력에는 기존 Recorder/Publisher 참조를 명시적으로 공급한다. 새 DTCore 관리자나 구독 루프는 없다.

좌표는 **센서 로컬 FLU: X 전방/Y 좌측/Z 위쪽**, 월드 기준은 UE 월드 Y를 반전한 오른손 좌표다(지리 ENU를 의미하지 않는다). 비력은 `R⁻¹(a−g)` **m/s²**, 각속도는 오른손 **rad/s**, 자세는 센서→월드 **xyzw 쿼터니언**이다. 정지·수평·무잡음은 `(0,0,+9.81)`, 자유낙하는0이다. 자세는 시뮬레이터 정답이며 IMU 적분·AHRS 추정값이 아니다. UE→FLU에서 벡터는 `(x,-y,z)`, 각속도는 `(-x,y,-z)`, 쿼터니언은 `(-x,y,-z,w)`로 변환한다.

표본 격자는 게임 시간의 고정 스텝이다. 게임 프레임 사이의 위치를 2차 보간/속도 provider로 재구성하고 자세는 최단 호 SLERP를 사용한다. 첫 구간은 준비 구간이며, 250ms 초과 관측 공백·시간 역행·한 프레임100m 초과 이동은 재준비하고 누락/불연속을 센다. 게임 관측 주파수를 넘는 진동이나 서브프레임 물리를 새로 시뮬레이션하지 않는다. `simulationSeconds`가 고정 주기 검증 기준이고 `timestampUtc`는 관측 프레임의 실제 UTC 사이 보간(100ns 저장 해상도)이다. pause/시간 배율에서 UTC와 게임 시간을 혼동하지 않는다. 기존 LiDAR acquisition UTC+`time_offset_ns`와 IMU pose를 연결할 데이터만 제공하며 실제 deskew는2단계 제안이다.

출력은 각각 따로 켠다.

| 선택 | 결과 |
|---|---|
| IMU 기록 | 기존 Recorder 안의 별도 표본 버퍼. 센서당24000표본, 최대16개 IMU. 가득 차면 새 표본을 받지 않고 초과 수 표시. 기존 JSON 녹화 시작을 자동 호출하지 않음 |
| 기록한 IMU CSV 저장 | 기존 FileSave의 비동기·취소·weak ownership·설정 revision 검사 사용. `Saved/SensorCaptures/<장비ID>.imu/IMU/`. configuration_revision, sequence, simulation_seconds, UTC ticks(서기1년 기준100ns), 비력/각속도/자세/위치/분산 열. 설정 변경은 새 revision이며 버퍼에 이전 표본도 유지 |
| IMU 전송 | 기존 StreamPublisher의 bounded latest-batch/Engine STOMP JSON 경로. 별도 `topic.virtual.sensor.imu.0`, `schemaVersion=virtual-imu.v1`, `<장비ID>.imu`. 한 게임 프레임의 고정 스텝 표본을 묶음 전송하며 교체/오류/receipt는 기존 진단에 표시. 기존 PCD·JPEG·telemetry wire/golden 불변 |
| 데이터셋 → IMU 값 포함 | 기본false. 선택 Camera/LiDAR에 부착된 IMU의 최신 값/독립 시각을 `label.json.imu[]`에 추가. 없으면 빈 배열. 장면 저장 접수 시점의 최신 표본이며 동시 하드웨어 획득을 뜻하지 않음 |
| AUTO-01 `GET /v1/sensors` | 부착된 장비에만 `imu` 객체: 주파수, 기록/전송, 표본/누락/불연속 및 최신 값. API 기본 꺼짐·인증·listener 계약 유지 |
| ROS2 bridge `--imu` | 추가 topic 구독을 명시적으로 켜고 `/ma0t10/<sensor_key>/imu`의 `sensor_msgs/Imu`와 독립 IMU TF 발행. [IMU 브리지 설명](Tools/Ros2Bridge/README.md#imu-01). 이번 단계의 WSL 실측은 Windows 단위 테스트로 대체 |

새 JSON의 `samples[]`는 sequence·configurationRevision·simulationSeconds·timestampUtc·specificForceMps2[3]·angularVelocityRadps[3]·orientationXyzw[4]·positionWorldFluM[3]·accelVariance·gyroVariance를 담는다. 분산은 백색 잡음과 누적 bias random walk의 대각 근사이며 고정 scale/정렬 오차의 추정 불확도는 포함하지 않는다. ROS 자세 covariance의0배열은 미지정이다. noise=false/Ideal은 모든 오차를 우회한다. 실제1Hz·20cm 이동의 무잡음 위치fallback 가속도는RMSE0.337m/s²·절대오차p950.670m/s²였다(시작0.3초전환제외). 고정표본률과물리정확도를구분한다. 가정값·근거·허용 오차와 실측 그래프는 `Saved/Reports/IMU-01/RESULT.md`에 있다.

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
- **결과**에서 확정된 실행을 `A로 선택`·`B로 선택`한다. 같은 시나리오 UUID의 서로 다른 실행을 비교하며 현재 PIE의 최근 20개 결과만 사용한다. 단계별 상세는 **도구 → 진단**에서 확인한다. 선택이 보관 범위를 벗어나면 비교를 중지하고 이유를 표시한다.
- 센서별 시작 위치·최단 회전각·스케일·북마크·설정과 요청/기록 시점 Hz를 비교한다. 기존 **실행 전체 카운터**와 새 **센서·출력별 상세**를 구분해 표시한다. 상세가 없는 과거 기록은 확인 불가다. 기록 시점 Hz는 실행 평균이 아니며 receipt 비율은 승인 0건에서 N/A다.
- 신규 실행은 원문 UTF-8 SHA-1을 선택적 `ScenarioContentHash`로 기록한다. 설정 JSON은 필드 순서 대신 값으로 비교하고 revision만 다른 경우 설정 차이로 판정하지 않는다. 원문·출력·맵·설정·진행 구간·외부 배치 변경이 다르면 참고 비교이며 누락 정보는 확인 불가다. 전송 수로 배치 우열·가림률·점밀도를 판정하지 않는다.
- `GetRunComparison()`은 저장된 두 요약만 읽는다. 비교·표시·해제는 측정·이동·재생·Topic 전송을 발생시키지 않는다. 확정된 결과에는 늦은 측정률·중단 알림도 적용하지 않는다. 기존 `ma0t10.slab-run.v1`과 PCD/JPEG 계약은 유지한다.

## 9. LiDAR 출력 정책과 센서별 전달 결과

- 배치 도구로 새로 추가한 LiDAR는 `WhenRequested` 정책으로 시작한다. PCD·Raw telemetry·미리보기만 사용할 때는 큰 호환 JSON을 생성하지 않는다. 기존 클래스·레벨 장비·구형 전체 배치 파일은 `LegacyAlways`를 유지한다. 복제와 새 전체 배치 저장/복원은 원본 정책을 보존한다.
- 정책은 `도구 → 센서 상세 설정 → 고급 → 호환 출력·관리 → LiDAR 호환 JSON 생성`에서 명시적으로 바꾼다. 정책만 변경하면 측정이나 조작 모드를 재시작하지 않는다. 기존 수집 잠금은 적용된다. JSON 생략은 실패/프레임 유실로 세지 않고 데이터 패널에 `미사용`으로 표시한다.
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
| L0 공통 계약 | `Contracts/` | Topic/슬롯/경로 리터럴 상수, L1 타입에 의존하지 않는 중립 acquisition context·provider와 기존 reflected 값 타입의 호환 adapter | 엔진·DTCore 중립 타입만 |
| 외부 | `Plugins/DTCore` (submodule) | 공통 API/WebSocket/Data/UI 기반 | 수정 금지 |

- 테스트(`*/Tests/`)와 Editor 모듈(`ma0t10_dtEditor`)은 레이어 검사에서 제외하지만 production 코드가 테스트를 include하면 안 된다.
- 기존 업무 폴더(Crane 등)는 L2로 분류만 하고 이번 단계에서 수정하지 않는다.
- 폴더 이동은 같은 모듈 안의 이동이므로 `/Script/ma0t10_dt` reflection 경로·Blueprint·SaveGame 직렬화에 영향이 없어야 한다. 클래스/구조체/enum 이름·순서·UPROPERTY는 바꾸지 않는다.

### 10.2 현재 위반 (2026-10-16, UXR-18·UXR-17·UXR-16· UI-02·UXR-10·UXR-11·UXR-12·PCD-RESUME·PCD-OPT·CAM-GT-01·ROS2-01·LIDAR-FID-01 적용)

UXR-18은 기존 L3 Settings의 기본/고급/자동화 재배치와 기존 소유 IMU/데이터셋/TESTSET 진입만 정리한다. 센서 revision과 기존 툴바5Hz 활동 캐시를 읽고 숨김/접힘 갱신을 멈춘다. L0~L2·DTCore·Blueprint/SaveGame/wire·새 모듈/자산 변경 없이 레이어 위반/allowlist **0→0**이다.

UXR-17은 기존 L3 소유 미리보기의 프레임 대기 안내·모드 범례·헤더 배치만 바꾼다. 기존 acquisition/정답 texture 캐시와5Hz 상태 갱신을 읽으며 숨김/접힘 갱신을 멈춘다. 새 capture/readback·타이머·저장값·wire·모듈·자산·DTCore 변경 없이 레이어 위반/allowlist **0→0**이다.

TESTSET-01은 L2 `Scenario/AgentTestSetSubsystem`에서 수신 분기·큐·DATASET-01 프레임 계획을 조립하고 L1 FileSave의 별도 TestSets 허용 루트를 재사용한다. L3의 소유 `UDxWidget`은 기존 Workspace/DTCore 등록·닫기·전면·입력 차단을 사용한다. 기존 패널 enum/저장값·DTCore·자산·모듈 변경 없이 레이어 위반/allowlist **0→0**이다.

UXR-16은 기존 L3 소유 UI의 문구·글자/줄바꿈·활동 표시와 L2 Automation의 읽기 전용 표시 accessor만 정리한다.5Hz 기존 상태 갱신/센서 순회를 재사용하며 새 모듈·Tick/timer·측정·직렬화·DTCore·자산 변경 없이 레이어 위반/allowlist **0→0**이다. 당시 설정 구조 제안은 별도 사용자 승인 후 UXR-18에서 구현했다.

IMU-01은 L1 `Sensor/VirtualImuTypes`·`VirtualImuComponent`와 기존 Recorder/StreamPublisher/FileSave를 확장한다. L0 motion provider를 읽고, L2 Dataset/AUTO-01과 L3 기존 DTCore 기반 배치 패널이 값을 소비한다. 레이어 위반·allowlist **0→0**, 새 모듈·자산·DTCore 변경 없음. IMU 스트림은 기존 acquisition envelope와 별도 접수되어 Camera/LiDAR·Slab PCD 기본 정책에 편입되지 않는다.

EXPORT-01은 L2 Dataset의 추가 OBB 라벨·선택적 외부 Python 프로세스와 기존 L3 소유 데이터 패널의 세션 선택만 추가한다. Python은 완료된 파일만 읽고 UE 객체·측정·송신 경로를 참조하지 않는다. L2→L3 include·새 모듈·자산·DTCore 변경 없이 위반/allowlist **0/0**이다.

ROS2-01은 `Tools/Ros2Bridge/`의 외부 adapter만 추가한다. UE 소스·모듈·플러그인·자산·DTCore 변경 없이 layer allowlist/위반 **0/0**을 유지하며, 센서 플러그인화나 UE 측 ROS2 연결 UI를 시작하지 않는다.

gate는 `Tests/`를 제외한 `.h/.cpp` 286개(TESTSET-01 포함)에서 **고유 source → resolved target include 쌍**을 센다. 현재 **위반 없음: 0쌍 = allowlist 0개**다. ARCH-06에서 남은 Scenario 의존 9쌍을 해소했으며 신규·stale도 0이다. MAP-01의 `UI/VirtualSensorToolRig`와 `UI/SensorMapCheckWidget`는 L3에서만 L1/L2를 참조하며 새 모듈/플러그인은 없다. UI-02의 `Slab/SlabLabelLayout`은 L2의 중립 screen geometry helper이며, L3 Workspace가 실제 소유 UI 사각형을 기존 Slab StatusVisualizer에 전달한다. L2→L3 include와 새 widget 관리자는 없다. CAM-GT-01은 Camera·Sensor 공통 semantic·Core FileSave(L1)를 재사용하고 UI(L3)가 이를 표시하며, 신규 역방향 의존 없이 위반/allowlist 0을 유지했다.

LIDAR-FID-01은 L0 `VirtualSensorMotionProvider`를 L2 SlabMotion이 구현하고 L1 센서가 snapshot으로 읽는다. L1 `VirtualLidarFidelity`의 순수 모델을 CPU/GPU가 공유하고 기존 L3 소유 설정/미리보기가 옵션을 표시한다. 신규 역방향 의존 없이 위반·allowlist **0/0**, DTCore·모듈·자산·PCD 레코드 불변이다.

UXR-15는 기존 L3 소유 화면의 문구·그룹·표시 우선순위만 정리하며, 승인된 Camera 정답 미적용 수정도 기존 Actor의 검증·적용 경로를 호출한다. 신규 회귀는 Editor 모듈에 두고 테스트 전용 접근만 추가했다. Editor의 JsonUtilities 의존 외 새 모듈·플러그인·위젯 관리자·런타임 역방향 include는 없으며 위반·allowlist **0/0**을 유지한다.

PCD-RESUME의 과부하 예약·재개·배수 확인은 기존 L1 StreamPublisher/HighThroughputTransport에 두고, L3 상태바·진단은 L1의 추가 실행 상태를 읽는다. 새 모듈·플러그인·자산·DTCore 변경 없이 위반·allowlist **0/0**을 유지한다. 기존 Blueprint 시그니처와 SaveGame 버전, PCD/wire golden 14파일 바이트는 유지하며 검증 범위는 `Saved/Reports/PCD-RESUME/RESULT.md`를 따른다.

PCD-OPT의 전송률 조절·소켓 대기·hitch 구간 계측도 기존 L1 Core에 두며, L3 UI가 조절 상태를 표시한다. opt-in 재현 조립은 L3의 시험용 probe이고 기본 실행에서는 생성하지 않는다. DTCore·새 모듈·플러그인·자산 추가 없이 layer allowlist **0**을 유지한다. 운영 PIE 화면 멈춤의 원인 확정 여부와 제한 링크/저코어 비교 증거는 `Saved/Reports/PCD-OPT/RESULT.md`를 따른다.

UXR-10의 resize·하위 화면 복귀는 L3의 기존 `UDxWidget` 패널/Workspace 안에서 처리한다. Runtime 파일 수와 위반·allowlist **0/0**을 유지하고 새 모듈·플러그인·자산·저장 schema를 추가하지 않는다. 집중 D3D12 2 Success, 관련 회귀 49 Success/3 조건부 skip, WBP7 메모리 컴파일·quick8 통과이며 직접 마우스·release·성능은 요청에 따라 보류다. 근거는 `Saved/Reports/UXR/UXR-10/RESULT.md`다.

UXR-11의 실제 마우스 라우팅은 L3 `UI/VirtualSensorToolWorkspaceSubsystem.Input.cpp`(Slate 입력 pre-processor)와 기존 패널 base 안에서 처리한다. 위반·allowlist **0/0** 유지.

UXR-12의 표현 토큰·레일·머리·폼·카드는 기존 L3 소유 native UI 안에서만 바꾼다. 새 모듈·자산·DTCore 변경·저장 schema·Tick/invalidation root 없이 위반·allowlist **0/0** 유지. Runtime `399ea46`의 집중 회귀58 Success/skip3·WBP7·quick8·두 해상도/세 글자 배율 전후 렌더를 확인했다. 직접 마우스·통합 release·상대 성능은 인계 상태이며 `docs/IMPROVEMENTS.md` UXR-12와 `Saved/Reports/UXR/UXR-12/RESULT.md`를 따른다.

| 방향 | include 쌍 수 | 위치 | 해소 항목 |
|---|---:|---|---|
| L1→L2 | 0 | 위반 없음. acquisition 값·provider·기존 reflected 값 호환 타입은 L0 `Contracts/`, Slab 세션·결과 adapter는 L2 `Scenario/` | ARCH-06 완료(9→0) |
| L1→L2 | 0 | 수신 타입·codec registry는 L0 `Contracts/`, codec 등록·ExternalSource 진단 hosting 본문은 L2 `WebSocket/TC/` | ARCH-05 완료 |
| L1→L3·L2→L3 | 0 | 위반 없음 | ARCH-04 완료 |
| **합계** | **0** | **위반 없음** | allowlist 비움 |

Coordinator는 L1 `IVirtualSensorMonitorView`, Player는 L1 `IVirtualSensorPlacementInputOwner`로 호출한다. 호환용 `BindMonitorWidget`과 기존 Player의 Gizmo getter/register 시그니처는 그대로이며 본문은 `UI/` 번역 단위에 있다. `BoundMonitorWidget`의 concrete 타입·`UPROPERTY(Transient)`·GC 보유도 유지하므로 UI 타입 forward 선언/Blueprint 핀까지 없앤 것은 아니다. `Slab/SlabUiRegistry`는 약한 Actor 존재 정보와 활성 등록을 분리해 생성·로드·복제·BeginPlay 전 및 EndPlay 후 유효 Actor의 기존 판정을 유지한다. ARCH-06의 센서 frame/request·worker는 문자열·숫자만 있는 `FVirtualSensorAcquisitionContext`를 전달한다. 월드별 provider는 게임 스레드에서만 조회하며 SlabContext가 약한 소유권으로 등록·해제한다. 기존 `FVirtualSlabFrameContext`·출력 선택의 이름/UPROPERTY와 공개 Blueprint/C++ API는 L0 호환 값/변환으로 유지하고, 세션 상태·Slab 실행 정책은 L2에 남긴다. 송신 시점에는 기존 generation/run 승인만 검사하며 acquisition context를 최신 Slab 상태로 덮어쓰지 않는다. Transport는 module startup에서 L2가 등록한 stateless codec의 thread-safe snapshot으로 기존 파서를 worker에서 호출한다. ExternalSource Actor 클래스·BP 계약은 유지하고 진단 hosting 본문만 L2로 이동했다. ARCH-08에서 미사용 Public `PixelStreaming*` 의존 3개를 제거했고 `.uproject` 활성화는 유지했다.

ARCH-08의 Monitor/Scan/Publisher는 같은 클래스의 12개 `.cpp`(249~887줄)로 나뉘며 순수 LiDAR JSON helper는 acquisition 번역 단위가 한 번만 include하는 `.inl`에 있다. 함수·helper 331개 원문·mutable helper 단일 인스턴스를 보존했다. 모든 Runtime `ForTests/ForTesting` hook은 `WITH_DEV_AUTOMATION_TESTS`로 제한된다. production 직렬화는 `SerializePointCloudFrame`으로 부르고 Development의 `SerializePointCloudForTesting` 호환 이름은 유지한다(비반영 native 함수).

현재 검증(Runtime `90a0758`): Editor·Game Development, `verify.ps1 -Level release` 12단계, layer **0/0**, WBP7 메모리 컴파일 통과. 변경 전 golden 커밋 `ba635b8`의 PCD 전체·호환 LiDAR JSON·telemetry·Camera/PCD STOMP 헤더 **14개 바이트 동일**. D3D12 전체 **233 Success / skip20 / 실패·NotRun0**이며 기존 249개 이름별 결과 차이0 + 신규 context/golden 4개 통과다. 실제 client 1920×1080·1 Camera+1 LiDAR·10초 준비+30초·D3D12 RenderOffscreen 비교에서 callback FPS 47.06→47.42, engine frame p95 29.690→29.607ms, game thread 평균 6.276→6.013ms, 2%gate 통과. 실제 Artemis 및 직접 마우스 점검은 수행하지 않았다. 개인 SaveGame 9개 복원·보호 경로 hash 불변, 독립 검토는 별도다. 근거: `Saved/Reports/ArchitectureSkeleton/A06/RESULT.md`, `summary.json`, `release-manifest.json`, `test-comparison.md`, `performance-comparison.json`. ARCH-05 enum→수신 kind의 명시적 switch 보완도 `90a0758`에 포함한다.

실행: `Scripts/validate_architecture_layers.ps1` (`-Json`은 상세 결과). 신규 위반과 stale allowlist 모두 exit 1이며, 경로별 원본은 `Scripts/architecture_layers_allowlist.txt`다. `-SourceRoot`·`-AllowlistPath`는 `Saved` 안의 음성 대조 fixture용이며 일반 gate에서는 생략한다.

### 10.3 구성 요소 책임 (현재)

AUTO-01은 기존 모듈의 L2 `Scenario/Automation`에서 하위 센서/출력과 같은 L2 재생/데이터셋 서비스를 호출하며, L3 소유 설정 화면만 이 adapter를 참조한다. 새 모듈·플러그인·위젯 관리자를 만들지 않고 layer/allowlist0/0을 유지한다. SCN-RELEASE-01(`9fa04be`)은 terminal snapshot 확정과 기존 소유 작업 정리 뒤 live 센서 제어만 해제한다. 이전 run의 늦은 프레임은 계속 거부하고 데이터 필수 잠금·terminal 결과 계약은 보존한다.

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
| AutomationControl / LoopbackServer / Protocol | 명시적 job·loopback API(L2), 기존 서비스 호출·게임 스레드 직렬화·결과 기록 |

`UVirtualSensorIntegrationSettings`/`UVirtualSensorDtCoreBridgeSubsystem`/Bootstrap Actor는 **소스에 없다**(과거 제안). 구현된 것으로 오인하지 않는다.

### DATASET-01 로컬 학습 데이터셋 — 명시적 opt-in

출처: 사용자 2026-10-15. 기본 출력은 PCD 송신이다. `데이터 → 파일 저장 → 데이터셋 생성 · 기본 꺼짐`을 펼쳐 사용자가 직접 켜고 조건을 정한 뒤 시작한다. 이름·장면 수(1–10000)·seed·안정화 프레임, Slab/LiDAR/Camera와 선택한 Movable DirectionalLight, 항목별 켜기/끄기·범위를 지정한다. 시작/일시 중지/재개/중단/결과 폴더와 선택 규격 기반 비압축 환산 용량을 표시한다. PNG/EXR의 실제 압축량은 장면에 따라 다르다. 선택 센서 측정·송신·저장과 Slab 시나리오는 먼저 중지한다. 기존 작업을 자동 중지하거나 운영맵을 저장하지 않는다.

| 파일 | 스키마·단위·인코딩 |
|---|---|
| `Saved/Datasets/<이름>_<UTC시각>_<충돌방지ID>/dataset.json` | `ma0t10.dataset.v1`: seed, requested/completed_scenes, settle_frames, camera_truth/lidar_fidelity, enabled/min/max 범위 표와 range_order, 프로젝트 commit SHA, map, class/instance 표. 마지막 확정 장면 수만 원자 교체한다. EXPORT-01은 additive `schema_minor=1`, 기본 none인 `export_format`, `export_upright_envelope`를 추가한다. |
| `scene_XXXXX/lidar.pcd` | 기존 immutable PCD0.7 binary writer·33-byte little-endian 레코드. 센서 로컬 RH meter(X전방/Y좌측/Z위쪽), 기존 MA0T10_META acquisition transform. 송신 wire에 라벨 필드를 추가하지 않는다. |
| `lidar_labels.bin` | PCD 검출점 순서의5-byte little-endian `uint8 semantic_id, uint32 instance_id`, padding/헤더 없음. 파일 크기/5 = PCD POINTS. 표준 PCD 레이아웃을 유지하며 라벨만 분리해 읽기 위한 선택이다. |
| `rgb.png` | 선택 Camera RGB target의8-bit 무손실 PNG. |
| `depth.exr`, `seg.png`, `instance.png` | Camera GT 포함 선택 시에만 생성. float32 광학축 Z meter(0 invalid), uint8 class ID, ID=`R+256G+65536B`. GT off에서는 이 세 파일과 내부 depth/semantic readback을 생략한다. |
| `label.json` | `ma0t10.dataset.scene.v1`: scene_index/seed, 실제 Slab pose·크기·표면, immutable sensor pose, Camera K/해상도, LiDAR 샘플 수/FOV/수직각/range/주기·profile/calibration/backend, 선택 조명 pose/lux, random_values 객체, request_engine_frame, 독립 acquisition frame ID/UTC, PCA yaw valid/angle/점 수 및 관찰용 memory/pump 값. 장면 완료 marker로 마지막에 쓴다. EXPORT-01의 additive `schema_minor=1`은 실제 선택 Slab mesh OBB `boxes_3d`(instance/class, center/size meter, quaternion/yaw, 좌표계)를 추가하며 기존 필드는 유지한다. |

pose는 UE world meter(X전방/Y우측/Z위쪽), quaternion xyzw와 pitch/yaw/roll degree다. Camera optical은 x우측/y아래/z전방이며 이상적 pinhole·zero distortion과 CAM-GT-01의 opaque static geometry 제한을 따른다. class0=미지원/미분류, 1=Other, 2=Slab, 3=Roller, 4=Conveyor, 5=Floor, 6=Sensor. instance0=대응 없음. 시작 시 Actor 이름을 정렬한 session ID를 PCD 점과 Camera mask에 함께 대응시킨다. ISM/HISM은 Actor 단위이며 전역 영구 ID가 아니다. 개별 LiDAR의 사용자 semantic override가 canonical 표에 없는 경우 class0로 남는다.

랜덤 기본 범위(각 항목 별도 off): yaw±10°/pitch·roll±3°, Slab world X/Y±20cm·Z±5cm, 크기0.9–1.1배, 표면0–4, roughness0.2–0.9/oxidation0–1, 조명1000–10000lux·pitch−70~−20°·yaw±180°, 센서 각 축 위치±1cm·각도±0.2°. 크기는 현재 mesh bounds의 균일 배율이다. 표면0은 세션 시작 시 실제 표면 값을 기준으로 하며 pause/resume 뒤에도 같은 결과다. seed+장면 번호의 독립 채널을 사용해 다른 옵션을 켜도 기존 채널을 이동시키지 않는다. 재현성은 같은 초기 장면·설정의 정답/랜덤 값에 적용하며 UTC·frame ID·성능 관찰값이나 임의 동적 배경/RGB byte 동일성을 뜻하지 않는다.

순서: 장면 적용 → 지정 engine frame 안정화 → 같은 게임 프레임의 두 센서 요청 → immutable 획득 → worker encode/write → 완료 marker/manifest → 다음 장면. CPU LiDAR chunk는 여러 engine frame에 걸쳐도 장면을 고정하고 acquisition snapshot을 보존한다. 하드웨어 동시 timestamp를 주장하지 않는다. 장면/저장 작업1개, Camera≤1920×1080·LiDAR≤25만 잠재 반환점·입력 추정≤256MiB로 admission을 제한한다. 같은 크기의 readback은 재사용한다. pause는 완료 장면 경계에서 원상 복원하고 같은 PIE에서 재개한다. 프로세스 재시작 후 디스크 resume는2단계다. 종료·중단은 새 작업을 멈추고 부분 파일을 제거하며 다른 작업이 바꾼 상태를 덮지 않는다. 화면 숨김/장비 선택은 진행 중인 생성을 중단하지 않는다.

채점: `python Tools/DatasetTools/score_dataset.py <데이터셋 폴더> --output <보고서.json>`. 기존 world-XY PCA 주축 yaw를180° 주기로 비교하고 평균·p95(선형 보간)·최대를 계산한다. invalid 분석은 별도 집계한다. `python -m unittest discover -s Tools/DatasetTools -v`로 도구를 검사한다(내보내기 테스트는 아래 requirements 설치 필요). 기존 생성 검증 조건·성능·메모리·수용 제한은 `Saved/Reports/DATASET-01/RESULT.md`에 기록한다. 배치/headless 실행은 AUTO-01 범위를 따른다. ROS bag/디스크 resume는 후속 제안이다.

### EXPORT-01 표준 형식 내보내기 — 로드맵 #12

출처: 사용자 2026-10-15. [설치·CLI·형식별 매핑·가정](Tools/DatasetTools/README.md), 고정 [requirements](Tools/DatasetTools/requirements.txt)를 따른다. `python export_coco.py <dataset> <out>` / `python export_kitti.py <dataset> <out>`는 새 출력 폴더에 생성하며 원본은 바꾸지 않는다. COCO는 공식 pycocotools의 lossless RLE·visible bbox·공통 semantic category다. KITTI는 실제 K/획득 pose로 P2/Tr_velo_to_cam을 계산하고 같은 점을 XYZI float32로 쓴다. 선택 Slab의 실제 mesh OBB만 label_2에 쓸 수 있다. 커스텀 클래스이므로 학습 도구의 class 설정이 필요하다.

기존 scene minor 0은 COCO에서 지원하고 KITTI에서는 mesh pivot/3D truth 부족을 명시적으로 거부한다. minor 1은 기존 `schema_version`을 유지하고 별도 `schema_minor=1`·`boxes_3d`를 추가한다. KITTI yaw-only로 표현되지 않는 pitch/roll은 기본 거부하며 `--upright-envelope`를 직접 선택하면 camera-upright 포위 박스와 원본 OBB/기울기·부피 비율을 기록한다. 선택적 `--semantic-kitti`는 custom semantic IDs 0..6의 uint32 점 라벨 저장 형식이며 정규 도로 클래스/odometry 데이터셋을 의미하지 않는다.

UI **생성 후 내보내기 형식**은 기본 없음이다. 선택한 경우에만 원본 생성·상태 복원이 끝난 뒤 설정된 Python에서 변환하고 sibling `<dataset>_export/`에 저장한다. `MA0T10_DATASET_PYTHON`은 requirements 환경의 절대 python.exe 경로이며 Editor 시작 전에 설정한다. 설치를 UI에서 실행하지 않는다. [AutomationClient 예제](Tools/AutomationClient/dataset_export.example.json)의 `dataset.start.export_format`도 기본 none이며 변환 실패는 job 실패로 보고한다. 정합 수치와 실제20장면·UI·전체 회귀·성능 수용 상태는 `Saved/Reports/EXPORT-01/RESULT.md`를 따른다.

### TESTSET-01 AI Agent 시나리오 테스트셋 — 임시 기능, 로드맵 #17

출처: 사용자 2026-10-16. `Config/DefaultGame.ini`의 `[AgentTestSet]`에서 `Enabled=True`, `TriggerMtlNo=9999`, `TargetScenarios=100`을 정한다. `mtl_no`는 문자열로 비교하며 양끝 공백만 제거한다. `Enabled=False`면 기존 일반 경로를 사용한다. 기존 TC → DataSync → Slab의 검증된 수신 지점에서 분기하고, 트리거는 일반 보관10개·PCD 자동 전송·실시간 재생에 넣지 않는다. 일반 소재가 오면 테스트셋 캡처/Slab 상태를 먼저 복원해 기존 실행에 양보하고, 일반 시나리오·센서 세션 정리 후 생성에 복귀한다.

`도구 → AI 시나리오 테스트셋`에서 트리거·목표·등록/완료 수·시나리오별 상태·프레임 수·시간·예상 남은 시간을 본다. 첫 등록 때도 이 소유 화면을 연다. 일시 중지/재개, 실패 재시도, 결과 폴더를 제공한다. 세트 초기화는 8초 안에 다시 눌러 확인하며 기존 파일을 보존하고 현재 목록을 비운다. 숨김은 생성 중지가 아니다. 목표·파일 검증·manifest 저장 완료 후 **시나리오 선택**이 활성화된다. 선택하면 기존 Slab 시뮬레이션을 **보기 전용·전송 꺼짐**으로 실행하며 중지/다른 선택을 지원한다. 생성/PCD 전송을 다시 시작하거나 일반 보관 목록을 밀어내지 않는다. 전송 버튼·Sender·임시 전송·ActiveMQ 알림은 이번 범위에서 제외했다.

대상은 수신 Slab과 같은 World의 LiDAR다. `LidarSensorId`가 비어 있으면 LiDAR가 정확히1개여야 하며, 여러 개면 ID를 지정한다. 선택 센서의 측정·출력·저장을 먼저 중지한다. `SampleStride=1`은 원본 모든 행, `SettleFrames=1`은 적용 뒤 engine frame1회 안정화다. 실제 재생과 같은 첫 행 치수·단위·TrackReference·BuildPose를 사용하고 실시간 재생 시계는 돌리지 않는다. 공유 World의 Slab을 이동시키므로 화면에는 샘플 자세가 보일 수 있으나 시간에 맞춘 보간 재생은 하지 않는다.

기본은 **LiDAR PCD + 라벨**이다. `CaptureCamera`, `CameraTruth`, `IncludeImu`, `LidarFidelity`는 모두 false이며 켠 경우 DATASET-01 대응 기능을 사용한다. 카메라 포함 시 `CameraSensorId`도 단일 대상 규칙을 따른다. `ExportFormat=none`이 기본이며 `coco`/`kitti`/`coco,kitti`에는 CameraTruth와 기존 EXPORT-01 Python 환경이 필요하다. `ExportUprightEnvelope`도 기존 정책을 따른다.

| 경로 | 내용 |
|---|---|
| `Saved/TestSets/<세트 UUID>/testset.json` | `ma0t10.testset.v1`: 트리거·목표/완료 수·선택 옵션·시나리오 ID/상태/프레임/시간/RunUUID/폴더·프로젝트 SHA/빌드. 단일 worker가 원자 교체한다. |
| `scenario_<번호>_<시나리오 UUID>/original.json` | 수신 원본 JSON. 별도 디스크 보관으로 일반 메모리 보관10개 제한에 영향을 주지 않는다. |
| `scenario_.../run_<RunUUID>/scene_XXXXX/` | DATASET-01 binary33 PCD·점별 라벨·`label.json`. 실제 Slab pose/치수/표면·immutable 센서 pose/FrameId/UTC에 `scenario_id`, `run_uuid`, 원본 `mtl_no`, `scenario_frame_no`, `scenario_elapsed_s`를 추가한다. |
| `scenario_.../summary.json` | 성공 RunUUID·프레임 경로·개수·시간·PCD bytes. 파일 검증 후 작성하며 재시도는 새 RunUUID/폴더를 사용한다. |

`Pipeline=True`는 캡처를 순차로 두고 이전 프레임의 PCD 직렬화·이미지 인코딩·라벨 JSON·파일 쓰기와 다음 캡처를 겹친다. writer1건 + 캡처1건, 다음 시나리오 파싱/자세 준비1건, 원본 저장2건으로 제한한다. 입력 대기64MiB, 두 장면 메모리 추정 합계256MiB 이하, Camera1920×1080·LiDAR25만 잠재 반환점 상한을 적용한다. CPU trace는 기존 분할 경로를 유지한다. World/Actor·semantic 조회를 포함한 루프를 임의 worker ParallelFor로 옮기지 않는다. `Pipeline=False`로 순차 처리와 비교하며 다중 UE 프로세스 생성은 구현하지 않았다.

기존 수신에서 허용한 UUID 없는 JSON도 최초 내부 보관 ID를 생성·라벨·보기 재생에서 유지한다. 원본 JSON을 다시 쓰지 않으며 디스크 재파싱 시 원본 해시를 검사한다.

목록은 실행 중 메모리 상태이며 프로세스 재시작 후 자동 복원/재개는 없다. 원본과 완료 산출물은 디스크에 남는다. AUTO-01을 켠 경우 `GET /v1/testset`으로 읽는다. 새 listener·ROS2 경로·전송은 추가하지 않는다. 검증·처리량·직접 UI·성능 수용은 `Saved/Reports/TESTSET-01/RESULT.md`를 따른다.

### AUTO-01 배치 실행과 로컬 제어 API — 기본 꺼짐

사용자 재개 결정에 따라 **Python 실행기가 공식 종료 상태를 판정**한다. UE5.3 Windows의 graceful exit는 비0 코드를 보장하지 않는다. UE의 `RequestExitWithStatus(false, code)`와 teardown/config flush는 유지하며 새 작업 실행 경로에서 Force/TerminateProcess는 쓰지 않는다. 이전 실패·수정 및 최종 검증 근거는 `Saved/Reports/AUTO-01/RESULT.md`를 따른다.

검증 Runtime `f6af018`: Editor/Game Development·Development 패키지의 GPU20장면+PCD30초(598submit/receipt)·Python12개·실행기/API/인증·release14단계(전체284실행/skip31/실패0)·실제 WireGolden·WBP7·어휘·layer0·기본 꺼짐 ABAB12샘플2% gate 통과. FPS−0.0252%·engine p95동일·GT+0.5897%. **설정의 API 토글/주소/배치 상태 직접 마우스 점검은 Windows Security 허용창으로 보류**이며 팝업 입력은0이다. 자동 검사로 직접 수용을 대체하지 않는다. 기존 UE 표준 자동화/perf의 Force 종료는 새 작업 실행기의 정상 teardown 증거와 별도로 RESULT에 기록했다. D3D12 offscreen의 초기 handled ensure22로그 줄, 기존 맵 PCA20장면 invalid 등 실제 제한은 RESULT를 따른다.

사용자 2026-10-15 승인 범위다. 기존 모듈 L2 `Scenario/Automation`이 Coordinator·Actor 편집 API·Replay·Dataset·비동기 FileSave·PCD Publisher를 호출한다. DTCore `UDxApiSubsystem`은 HTTP 클라이언트, `UDxWebSocketSubsystem`은 STOMP 클라이언트라 수신 서버로 재사용할 수 없다. UE Sockets worker는 UObject를 읽지 않으며 명령은 게임 스레드에서 직렬 처리한다. DTCore·기본 PCD·wire·SaveGame·맵은 유지한다.

미사용 시 listener·ticker·프레임 계측·파일 생성이 없다. CLI는 standalone Game world에만 적용한다. 기존 Coordinator **정확히1개**, 중복 없는 SensorId, 시나리오 playback adapter가 필요하다. 장비·Slab·조명·맵을 자동 배치하지 않는다.

```powershell
python Tools/AutomationClient/run_job.py `
  --executable 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
  --project 'C:/Unreal Projects/ma0t10_dt/ma0t10_dt.uproject' `
  --map /Game/MA0T10/Maps/Tests/SlabScenarioValidationMap --job C:/jobs/job.json `
  --timeout 600 -- -game -d3d12 -RenderOffscreen -unattended -nosplash -NoLoadingScreen
```

Development 패키지는 `--executable <package>/ma0t10_dt.exe`로 지정하고 `--project`를 생략한다. PowerShell의 `Tools/AutomationClient/run_job.ps1 -Executable ... -Job ... -Project ... -Map ...`도 같은 Python 실행기를 호출하고 같은 종료 코드를 반환한다. 두 실행기 모두 UE의 정상 teardown을 유지한다.

실행기는 결과 경로를 lock으로 보호하고 기존 결과는 `.previous-<실행ID>`로 보존하여 오래된 성공을 재사용하지 않는다. child exit0 **및** 새 `ma0t10.job.result.v1`의 `ok=true`, 전체 단계의 성공/순서 일치가 필요하다. 결과 없음/실패/스키마 오류/child 실패는 exit1, 입력·경로 충돌은2, timeout은124, 사용자 중단은130이다. 별도 `<job>.runner.json`에 child 종료 상태·판정·경과 시간·로그 위치·완료된 엔진 로그의 Error/Fatal 줄 수(부팅 포함)를 기록하며 인증 값/argv/환경변수를 출력하지 않는다. handled ensure의 로그 줄 수를 고유 오류 수나 job 실패 여부와 혼동하지 않는다.

timeout/중단은 실행별 `-MA0T10JobCancelFile=<경로>` marker로 게임 스레드에 정상 취소를 요청하고 기본30초 기다린다. UE가 완전히 응답하지 않으면 강제 종료하지 않고 실행기만 비0으로 돌아오며 `child_running`, PID, marker와 lock 경로를 보고한다. 해당 child가 끝나기 전에는 lock을 자동 해제하지 않는다. 직접 `-MA0T10Job=...` 실행도 가능하지만 Windows UE exit 코드만으로 성공을 판정하면 안 된다.

job은 UTF-8 JSON·최대256KiB·1~1000단계다. 상대 경로는 **job 폴더 기준**이다. 문서 전체의 필드/타입/범위를 먼저 검사하고 대상/충돌은 단계 직전 검사한다. 실패 시 후속 단계는 실행하지 않는다. 각 비동기 작업의 완료를 기다린다. 기본 단계 timeout300초·전체3600초, 명시적 timeout은1~86400초다.

```json
{
  "schema": "ma0t10.job.v1", "result_path": "result.json", "timeout_sec": 600,
  "steps": [
    {"op": "sensor.stop", "sensor_id": "VCAM-001"},
    {"op": "sensor.stop", "sensor_id": "SLAB-LIDAR-001"},
    {"op": "sensor.set", "sensor_id": "VCAM-001", "profile": "IntelRealSenseD455", "quality": "FullSpec"},
    {"op": "scenario.start", "json_path": "scenario.json", "timeout_sec": 180},
    {"op": "dataset.start", "name": "AUTO01", "scenes": 20, "seed": 22015,
     "slab": "SlabActor_0", "lidar_id": "SLAB-LIDAR-001", "camera_id": "VCAM-001",
     "camera_truth": true, "lidar_fidelity": true, "ranges": {"yaw": [-10, 10]}},
    {"op": "pcd.run", "sensor_id": "SLAB-LIDAR-001", "seconds": 30, "timeout_sec": 90}
  ]
}
```

`wait`는 `seconds`(.01~86400), `pcd.run`은 `seconds`(.1~86400)와 충분한 timeout을 지정한다. 다른 op는 아래 API 경로의 마지막 이름에 대응한다(`sensor.set`, `scenario.start` 등). 무한 `pcd.start`는 job에서 거부한다. PCD는 기존 배포 설정/인증을 사용하며 기존 활성 exact/wildcard 스트림이 있으면 거부한다. 자신이 시작한 스트림/측정만 종료하고 원래 스트림 설정을 복원한다. receipt0 또는 전달 오류는 실패다. receipt는 소비자 업무 ACK가 아니다. job은 전체 설정 rollback이나 디스크 resume transaction이 아니다.

`result_path`에 `ma0t10.job.result.v1`을 임시 파일→교체 방식으로 기록한다. 공식 실행기는 새 결과와 child 종료 상태를 함께 검사한다. 유효한 결과 경로를 얻기 전 읽기/파싱 실패는 `Saved/Automation/job-result.json`에 남기며, 기대한 결과가 없어도 실행기는 실패로 판정한다. 결과에는 단계별 성공/오류·시간·산출물·PCD input/submit/receipt/error/overload 수·소스 SHA·맵·RHI/offscreen 여부가 있다. 입력과 제출은 별개이며 지정한 중지 경계의 프레임은 미제출일 수 있다. receipt>0을 획득 손실0으로 표현하지 않는다. hitch는 활성 제어 구간의 world tick 간 wall150ms 초과 수/최대/평균이며 GPU frame/부팅 시간과 다르다. `log_error_count`는 계측 이후 Error 로그 줄 수이므로 부팅 로그도 별도 보관한다.

API는 `-MA0T10Control` 또는 소유 **센서 설정 → 호환 → 외부 제어 API** 체크로 켠다. 기본 `http://127.0.0.1:8099`, 끄면 listener를 닫는다. 세션 토글·주소/실패·배치 진행은 기존 소유 설정 화면에서 최대5Hz로 갱신한다. 숨김/접힘 때 문자열 갱신을 멈추며 토글은 SaveGame에 저장하지 않는다. API를 꺼도 이미 접수한 작업은 계속 진행한다.

선택적 `-MA0T10ControlConfig=C:/private/control.json`의 허용 키는 `port`(1024~65535 정수), `token`뿐이다. 예: `{"port":8099,"token":"<local-secret>"}`. 토큰은 `MA0T10_AUTOMATION_TOKEN`으로도 지정하며 파일 token 키가 우선한다(ASCII33~126·최대256자). 토큰을 명령줄 값/로그/상태 응답/SaveGame에 넣지 않는다. 둘 다 비어 있으면 같은 PC의 프로세스가 인증 없이 접근한다. 외부 bind 주소 옵션·TLS·다중 사용자 권한 분리는 없다.

HTTP/1.1·JSON만 지원한다. Host는 정확한 `127.0.0.1:<port>`이며 Origin·chunked·pipelining·중복 헤더를 거부한다. 본문256KiB·헤더8KiB·수신2초·OS backlog16, 소켓1건씩 처리한다. Bearer 인증은 조회/변경 모두 적용한다.200은 즉시 완료,202는 접수,400은 형식,401은 인증,404는 경로,409는 충돌/대상/적용 실패,413/431은 크기,405는 메서드 오류다. 자동 재전송·영구 이력·CORS는 없다.

| 메서드·경로 | 본문/동작 |
|---|---|
| GET `/v1/status` | operation_id/phase/terminal/success/error/detail, 배치·데이터셋 진행/폴더·재생 상태·Slab/조명 Actor 이름 |
| GET `/v1/sensors` | SensorId·Actor·종류·측정 여부·revision·주기·프로필·품질·마지막 FrameId |
| POST `/v1/sensor` | `sensor_id`와 `interval_sec`(.033~60), 또는 `profile`/`quality`의 기존 enum 이름. 프리셋과 주기 override는 별도 요청. 식별자/자세/임의 UPROPERTY 쓰기는 없음 |
| POST `/v1/sensor/start`, `/v1/sensor/stop` | `sensor_id`; 기존 Actor 측정 시작/중지 |
| POST `/v1/capture` | `sensor_id`; **측정 중인** 센서의 새 프레임1개를 FileSave로 비동기 저장. Camera JPEG 또는 LiDAR binary PCD; 완료 detail에 경로/FrameId |
| POST `/v1/scenario/start` | `json_path` 또는 보관된 `uuid` 중1개. 선택 `send_pcd`(기본false)와 대상 `sensor_id`. 파일8MiB, worker 파싱 후 기존 archive/replay admission |
| POST `/v1/scenario/stop` | `{}`; 이 제어기가 시작한 재생만 취소 |
| POST `/v1/dataset/start` | 예제의 이름/대상/장면 수(1~10000). 선택 seed, settle_frames(1~600), camera_truth/lidar_fidelity(기본false), light Actor 이름, ranges. 대상 센서/시나리오가 정지해 있어야 함 |
| POST `/v1/dataset/stop` | `{}`; 소유 생성 취소. 기존 Dataset 서비스가 writer drain/복원을 소유 |
| POST `/v1/pcd/start`, `/v1/pcd/stop` | 시작 `sensor_id`, 중지 `{}`. 소유 스트림만 종료/정리; batch는 `pcd.run` |
| POST `/v1/shutdown` | `{}`; idle standalone game 정상 종료. PIE·배치·진행 중 operation은409 |

긴 작업은1개만 접수한다. 동일 operation_id의 terminal을 기다려야 한다. 배치 중 API 변경은409, 조회는 가능하다. 취소는 terminal 실패이며 하위 서비스의 drain 상태는 계속 조회한다. dataset ranges는 DATASET-01 단위/상한의 `yaw,pitch,roll,x,y,z,size,surface,roughness,oxidation,light_intensity,light_pitch,light_yaw,sensor_position,sensor_angle`에 `[min,max]`를 지정한다. 생략한 항목은 랜덤화하지 않는다.

같은 파일을 다시 재생하면 동일한 immutable 보관본에서 새 RunUUID를 만든다. 같은 UUID의 내용이 다르면 덮어쓰지 않고 실패한다. API PCD 중 UI가 해당 스트림/센서 설정을 바꾸면 변경을 감지해 API 동작을 실패 종료하며 사용자 새 값을 과거 값으로 복원하지 않는다.

`scenario.start`의 `send_pcd`는 그 실행의 출력이다. `false`로 관찰해도 신규 시나리오 PCD 기본 켬과 사용자가 직접 선택한 다음 실행 값은 유지된다. 이는 `json_path` 비동기 수신과 보관 UUID 재생에 동일하게 적용된다(INSP-FIX-1).

```powershell
python Tools/AutomationClient/client.py status
python Tools/AutomationClient/client.py sensors
python Tools/AutomationClient/client.py call /v1/sensor request.json
python Tools/AutomationClient/client.py capture VCAM-001  # 먼저 sensor/start 호출
python Tools/AutomationClient/client.py make-job C:/jobs/job.json --scenario C:/jobs/scenario.json --slab SlabActor_0 --lidar SLAB-LIDAR-001 --camera VCAM-001
# job으로 시뮬레이터 실행 후 생성→채점:
python Tools/AutomationClient/client.py check C:/jobs/result.json
python Tools/AutomationClient/client.py score C:/jobs/result.json --output C:/jobs/score.json
python -m unittest discover -s Tools/AutomationClient -v
```

Python 표준 라이브러리 클라이언트는127.0.0.1 주소만 만들고 프록시·redirect를 사용하지 않으며 인증 값/요청 헤더를 출력하지 않는다. `integration_check.py`는 명시한 **검증용 runtime**의 센서를 변경·캡처·생성·취소하는 예제다. 실제 지원 조합·초기 실패/제한·회귀는 `Saved/Reports/AUTO-01/RESULT.md`를 따른다. `-NullRHI`의 Camera/GPU 데이터셋·캡처는 거부한다. 디스플레이 없는 서버/다른 driver·Shipping·무제한 장면 수 지원을 Development offscreen 결과로 추정하지 않는다.

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


### 11.1 표준 진입점

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/verify.ps1 -Level quick
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/verify.ps1 -Level feature -Tests MA0T10.SensorReceiver
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/verify.ps1 -Level release
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/verify.ps1 -Level perf -BaseCommit <기준 커밋> -PerfPairs 1,2 -PerfDisplay Both
```

`perf`(QA-02)는 quick 뒤에 `Scripts/run_perf_abab.ps1`로 기준/현재 revision을 별도 archive checkout에 빌드하고, PERF-01 계측(`MA0T10.PERF01.AcquisitionAndPreview`, SlabScenarioValidationMap, 1920×1080 D3D12, 10초 준비+60초)을 **ABAB×3** 실행해 revision별 중앙값으로 FPS 감소·engine p95 증가·game thread 증가를 각각 2% 기준으로 판정한다. 결과는 `Saved/Reports/verify/<시각>/perf/manifest.json`(revision·DTCore·계측기·맵 hash, GPU/driver, 표시 방식, 판정). 모든 Editor·PIE·마우스 점검·빌드가 끝난 뒤에만 실행한다. 기존 결과 재판정: `run_perf_abab.ps1 -SummarizeOnly <runs 폴더>`. 자동 실행 설치(승인 후): `Scripts/install_verify_automation.ps1 -PreCommitHook`(커밋 전 quick) / `-NightlyFeature`(야간 feature).

`quick`은 기존 diff/guard/layer/payload 검사를 호출하고 보호 경로의 index·commit 변경 및 아래 변수표 일치를 확인한다. `feature`는 Editor Development 빌드와 지정 그룹, `release`는 전체 D3D12 `MA0T10`과 WBP 7개 메모리 컴파일을 더한다. 기존 `run_smoke_tests.ps1`의 기본 NullRHI 동작은 유지하며 verify에서는 D3D12 보고서 모드를 명시한다. 테스트 이름을 `-Tests`에 전달한다(여러 그룹은 PowerShell 스크립트 호출에서 배열로 지정).

`Saved/Reports/verify/<UTC>/manifest.json`은 commit/submodule SHA·worktree·Source hash·단계/테스트별 상태·skip 이름/원인 변수·소요 시간·엔진/RHI 환경을 남긴다. 비밀 환경변수 값은 수집하지 않는다. Success 안의 skip을 별도 집계하며 opt-in은 자동으로 켜지 않는다. 개인 SaveGame은 실행 전 백업하고 종료 후 복원한다. 맵 fixture는 현재 HEAD의 커밋본을 Saved에 추출하며 사용자 맵을 저장하지 않는다.

보호 commit 비교의 기본은 `HEAD^..HEAD`다. 여러 커밋을 검토할 때 `-BaseCommit <검토 시작 SHA>`로 범위를 명시한다. 기존 unstaged 사용자 변경은 실패 사유로 삼지 않고 실행 전후 hash를 대조한다. feature/release 전에는 Editor·Live Coding을 정상 종료한다. quick에는 git·PowerShell이, feature/release에는 UE 5.3 빌드 도구와 실제 D3D12가 필요하다.

### 11.2 MA0T10 환경변수 inventory

`Source/`의 literal `GetEnvironmentVariable(TEXT("MA0T10_*"))` 전체를 포함한다. 활성화 flag 외 경로·시간·정책 selector도 포함하며 저장값/기본 정책을 바꾸는 권한은 아니다. flag의 미설정 또는 1 이외 값은 opt-in 비활성으로 처리한다. `[OPT-IN SKIP] <변수> unset or !=1` 로그는 미실행이며 실제 RHI 부족은 별도 skip이다. `validate_opt_in_inventory.ps1`이 소스와 표의 이름 집합을 자동 대조한다.

<!-- MA0T10_ENV_TABLE_BEGIN -->

| 변수 | 활성화/설정 대상 시험·fixture | 값 의미·필요한 환경 |
|---|---|---|
| `MA0T10_AUTOMATION_TOKEN` | AUTO-01 local control API | Optional Bearer token (ASCII 33..126, max 256 chars); does not enable API; never logged or saved |
| `MA0T10_PCD20HZ_CASE` | MA0T10.SensorStream.Pcd20Hz.SocketFaults | healthy/slow/heartbeat/silent/error/reset/observe; Tools/StompFaultServer 로컬 TCP fault 서버 |
| `MA0T10_PCD20HZ_SECONDS` | MA0T10.SensorStream.Pcd20Hz.SocketFaults | 8~600초; 합성 socket 부하 시간 |
| `MA0T10_PCD20HZ_INTERVAL` | MA0T10.SensorStream.Pcd20Hz.SocketFaults | 0.05~0.2초; 합성 프레임 간격 |
| `MA0T10_PCD20HZ_LIDARS` | MA0T10.SensorStream.Pcd20Hz.SocketFaults | 1~2; 합성 스트림 수, 실제 acquisition 검증과 구분 |
| `MA0T10_PCD20HZ_NATIVE_INTERVAL` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | PCD-20HZ 전용 0.05~0.2초; Actor 편집 API로 시험 주기 적용, 상태바·Raw·Engine 시간축 기록 |
| `MA0T10_ACCEPTANCE_LEDGER` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke / RepeatThreeStreamAcceptance | 수용 ledger 출력 경로; 쓰기 가능한 Saved 하위 경로 |
| `MA0T10_ACCEPTANCE_RES_X` | FinalAcceptanceTestSession 사용 RHI/PIE 시험 | client viewport 폭 정수; 실제 RHI/PIE |
| `MA0T10_ACCEPTANCE_RES_Y` | FinalAcceptanceTestSession 사용 RHI/PIE 시험 | client viewport 높이 정수; 실제 RHI/PIE |
| `MA0T10_ARTEMIS_PASSWORD` | SensorStream / SensorV2 / SlabScenarioReplay 및 ExternalSource receiver | 인증 비밀번호; Broker 시험 스크립트·Node probe 필수(없으면 중단), 비밀값은 기록·커밋하지 않음 |
| `MA0T10_ARTEMIS_RAW_URL` | MA0T10.SensorStream.HighThroughput.RawTcpArtemisLoopback | 기존 Raw TCP STOMP URL; Raw Broker |
| `MA0T10_ARTEMIS_URL` | SensorStream / SensorV2 / SlabScenarioReplay 및 ExternalSource receiver | 기존 STOMP/WS URL; 시험에 맞는 Broker·외부 구독자 |
| `MA0T10_ARTEMIS_USER` | SensorStream / SensorV2 / SlabScenarioReplay 및 ExternalSource receiver | 인증 사용자 이름; Broker 시험 스크립트·Node probe 필수(없으면 중단), 비밀값은 기록하지 않음 |
| `MA0T10_CAMERA_MARKER_RHI` | MA0T10.SensorFiles.CameraMarkerRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_DIAGNOSTIC_HIDE_OWNED_UI` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; 진단 fixture 소유 UI 숨김 |
| `MA0T10_DTCORE_SLAB_BROKER` | MA0T10.DTCoreIntegration.SlabBroker | 1; 실제 Broker + 외부 synthetic scenario publisher |
| `MA0T10_GEOMETRY_BASELINE` | MA0T10.LidarGeometry.StreamPerformance | 비교할 이전 geometry 성능 JSON 경로 |
| `MA0T10_GEOMETRY_PERF` | MA0T10.LidarGeometry.StreamPerformance | 1; 실제 RHI + local Artemis + geometry fixture |
| `MA0T10_INPUT_ACCEPTANCE_RHI` | MA0T10.SensorV2.UI.InputAcceptanceRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_LAYOUT_API_FIXTURE` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke / SlabScenario.ProductionRuntime / SlabReliability.RepeatedRuntime | 1; 기존 layout API fixture 설정 |
| `MA0T10_LIDAR_SEMANTIC_BENCHMARK` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; 실제 RHI semantic benchmark fixture |
| `MA0T10_MANIPULATION_RHI` | MA0T10.SensorV2.UI.ManipulationRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_OUTPUT_SENSOR_PAIRS` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke / SlabScenario.ProductionRuntime / SlabReliability.RepeatedRuntime | 1 또는 2; Camera+LiDAR 쌍 수 선택·실제 RHI fixture |
| `MA0T10_OUTPUT_SUBMISSION_BUDGET` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 프레임당 submit 예산 정수; 운영 기본값은 변경하지 않음 |
| `MA0T10_PCD_DEMAND_ACCEPTANCE` | ContinuousThreeStreamSmoke / SlabScenario.ProductionRuntime / SlabScenarioValidationRig | 1; 기존 PCD demand fixture·실제 RHI |
| `MA0T10_PCD_JSON_POLICY` | PCD demand / stream / Slab fixture의 기존 JSON 정책 선택 | 기존 policy selector; 관련 runner의 인자/소스 참조 |
| `MA0T10_PCD_ONLY` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; stream fixture PCD-only 선택 |
| `MA0T10_PLACEMENT_AB_RHI` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_PLACEMENT_PRESENTATION_RHI` | PlacementUi.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_PLACEMENT_RHI` | MA0T10.Placement.InputOwnershipRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_RAW_PROBE` | MA0T10.SensorStream.RawReceiptProbe | 1; Tools/Artemis/raw_receipt_probe.mjs의 local fault endpoint |
| `MA0T10_REPEAT_STREAMS_RHI` | MA0T10.SensorV2.Runtime.RepeatThreeStreamAcceptance | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_REPLAY_REPORT` | MA0T10.ScenarioReplay.Runtime | replay 보고서 경로; 쓰기 가능한 Saved 하위 경로 |
| `MA0T10_REPLAY_RHI` | MA0T10.ScenarioReplay.Runtime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_RESOURCE_MANUAL_FIXTURE` | SensorResource.ManualFixture | 1; 사용자 승인된 직접 조작 세션·화면 필요 |
| `MA0T10_RUN_ARTEMIS_SMOKE` | MA0T10.SensorStream.ArtemisIntegration<br>MA0T10.SensorStream.HighThroughput.RawTcpArtemisLoopback | 1; 실행 중 Broker + 인증 + 외부 구독자 |
| `MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; 실제 RHI + 커밋본 맵 + Broker + 외부 검증 |
| `MA0T10_RUN_SLAB_SCENARIO_SMOKE` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 1; 실제 RHI + Slab scenario fixture + Broker |
| `MA0T10_SCENARIO_FAULT_CYCLES` | MA0T10.SlabReliability.TcpFaultRuntime | 반복 횟수 정수; local TCP fault fixture |
| `MA0T10_SCENARIO_FAULT_MODE` | MA0T10.SlabReliability.TcpFaultRuntime | 기존 fault 모드 문자열; local TCP fault fixture |
| `MA0T10_SENSOR_FILES_RHI` | MA0T10.SensorFiles.RealRhiWorkflow | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SENSOR_RESOURCE_DIAGNOSTICS` | 기존 Slab/수용 fixture (관련 runner 참조) | 1; 기존 fixture 진단 수집 |
| `MA0T10_SENSOR_RESOURCE_MEMREPORT` | 기존 Slab/수용 fixture (관련 runner 참조) | 1; 기존 fixture 메모리 보고 수집 |
| `MA0T10_SENSOR_RESOURCE_PREVIEW_OFF` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; resource fixture 미리보기 숨김 |
| `MA0T10_SENSOR_RESOURCE_REPORT_DIR` | 기존 Slab/수용 fixture (관련 runner 참조) | resource 보고서 폴더; 쓰기 가능한 Saved 하위 경로 |
| `MA0T10_SLAB_ANALYSIS_OFF` | 기존 Slab/수용 fixture (관련 runner 참조) | 1; Slab fixture 분석 표시 숨김 |
| `MA0T10_SLAB_ANALYSIS_RHI` | MA0T10.SlabAnalysis.CameraGpuIsolationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_EXACT_VIEWPORT` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; 설정한 acceptance 해상도를 엄격히 사용 |
| `MA0T10_SLAB_LOCAL_BROKER` | 기존 Slab/수용 fixture (관련 runner 참조) | 1; Slab fixture의 기존 local Broker 선택 |
| `MA0T10_SLAB_OUTPUT_ALL` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; Slab fixture 추가 출력 선택 |
| `MA0T10_SLAB_PORTABLE` | MA0T10.SlabScenario.PortableMapAndLatePlayer | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_REPEAT_COUNT` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 실행 반복 횟수 정수; Slab fixture |
| `MA0T10_SLAB_REPEAT_RHI` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_REQUIRE_DATA` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; Slab fixture 데이터 필수 출력 선택 |
| `MA0T10_SLAB_RESET_RHI` | MA0T10.SlabReliability.RepeatedRuntime<br>MA0T10.SlabScenario.ProductionRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_RHI` | MA0T10.SlabScenario.ProductionRuntime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_SLAB_RUNTIME` | MA0T10.SlabScenario.ProductionLifecycle | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_STREAM_MEASURE_SECONDS` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 측정 시간(초); 실제 RHI stream fixture |
| `MA0T10_STREAM_WARMUP_SECONDS` | MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke | 준비 시간(초); 실제 RHI stream fixture |
| `MA0T10_UXR01_PRESENTATION_RHI` | UXR01.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR02_MANUAL` | UXR02.Manual | 1; Computer Use 직접 조작 세션·화면 필요 |
| `MA0T10_UXR02_RENDER` | UXR02.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR03_RENDER` | UXR03.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR04_RENDER` | UXR04.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR05_RENDER` | UXR05.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR06_RENDER` | UXR06.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR08_RENDER` | UXR04.PresentationRhi<br>UXR08.PresentationRhi | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR_PERF` | UXR02.Performance | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_UXR_PERF_NAME` | UXR02.Performance | 성능 비교 run 이름 |
| `MA0T10_UXR_PERF_VIEW` | UXR02.Performance | preview/transfer/settings/scenario/scenario-monitor; 미설정은 기본 배치 화면 |
| `MA0T10_UXR_REPORT_ROOT` | UXR02.Performance / UXR02~06/08 렌더 fixture | UXR 증거 출력 폴더; 쓰기 가능한 Saved 하위 경로 |
| `MA0T10_WORKSPACE_RHI` | MA0T10.SensorWorkspace.Runtime | 1; 실제 D3D12/PIE, 해당 fixture. 렌더 fixture는 직접 마우스 수용과 구분 |
| `MA0T10_WORKSPACE_SCREENSHOT` | MA0T10.SensorWorkspace.Runtime | screenshot 파일 경로; 실제 RHI + 쓰기 권한 |
| `MA0T10_DATASET_SCENES` | MA0T10.Dataset.RealRhiGeneration | 명시한 10–100장면 실제 RHI 생성; Saved/Datasets에 시험 파일을 만드는 opt-in |
| `MA0T10_DATASET_PYTHON` | EXPORT-01 선택적 오프라인 변환 | requirements를 설치한 python.exe의 절대 경로. export_format 기본 none에서는 읽거나 실행하지 않는다. |
| `MA0T10_STOMP_USER` | IMU-01 Editor acceptance fixture only | Existing broker session username, read only with explicit `-ImuAutomated -ImuBroker`; never logged/saved; does not enable production output |
| `MA0T10_STOMP_PASSWORD` | IMU-01 Editor acceptance fixture only | Existing broker session passcode, inherited process environment only with explicit `-ImuAutomated -ImuBroker`; never logged/saved; no broker configuration change |
<!-- MA0T10_ENV_TABLE_END -->

## 고정 참고자료

아래 문서는 이전 상세 명세/작업 기록이며 현재 UI·지원 성능의 기준 문서는 아니다. 경로를 참조하는 스크립트를 위해 보존한다.

- 계약: [Camera v1](docs/camera_payload_schema.md), [LiDAR v1/v2](docs/lidar_payload_schema.md), [전송](docs/server_transport_contract.md), [과거 스트리밍 상세](docs/sensor_streaming.ko.md)
- 센서: [테스트맵](docs/sensor_test_map_setup.ko.md), [높이·좌표](docs/lidar_height_semantic_coordinates.ko.md), [태그 없는 형상](docs/lidar_geometry_validation.ko.md), [V2 migration](docs/sensor_v2_migration.ko.md)
- Slab/UI: [시뮬레이션 상세](docs/slab_simulation.ko.md), [재생 상세](docs/slab_scenario_replay.ko.md), [센서 연동](docs/sensor_slab_integration.ko.md), [Workspace 상세](docs/sensor_tool_workspace.ko.md)
- 이력/범위: [PR16 병합 기록](docs/sensor_pr16_merge_notes.ko.md), [Editor smoke 이력](docs/editor_smoke_test.md), [실장비 adapter 제안](docs/real_sensor_adapter_plan.md), [PixelStreaming 참고](docs/pixel_streaming_setup.md)
