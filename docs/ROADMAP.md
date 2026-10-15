# 최종 목표를 위한 로드맵

기준: **`codex/architecture-skeleton` (분기 `fda25e0`) / DTCore `b2504d1`, 2026-10-11 아키텍처 재정비 착수**. 작업·빌드·검수는 `C:\Unreal Projects\ma0t10_dt`에서 진행하고 별도 worktree를 새로 생성하지 않는다. 검증 수치는 해당 시험의 원래 날짜·조건을 따른다.
이 문서는 후속 개발 순서와 승인 기준이다. 플러그인 생성이나 모든 단계의 구현을 지금 승인하는 문서는 아니다. **이번 버전에서는 센서 플러그인 제작(R5)을 시작하지 않는다.**

[AGENTS](../AGENTS.md) · [현재 구현](../README.md) · [개별 보완 백로그](IMPROVEMENTS.md) · [검증 이력](VALIDATION_HISTORY.md)

## 1. 최종 사용자 경험

| 사용자 목표 | 최종적으로 제공할 동작 | 현재와의 차이 |
|---|---|---|
| 여러 Camera/LiDAR를 자유롭게 배치 | `센서 배치` 패널에서 추가·복제·선택·이동, 저장 위치와 전체 배치 복원 | PIE 핵심 workflow·저장/복원은 구현·검증. 전체 직접/DPI·첫 사용자 수용·Editor 영구 설치는 별도 |
| 시나리오에 맞춰 PCD 생성·전송 | 준비 상태 확인, 독립 측정 주기, 소재/프레임 context, 종료 drain과 실행 결과 | 2+2 PCD10분/10회 조건 통과. 다른 장면·운영 환경·4+4/60분 인증은 남음 |
| 장비별 플러그인 관리 | DTCore + 센서 플러그인, 데이터 기반 ProfileId registry, backend/SDK 확장 경계 | 현재 프로젝트 모듈·enum/resolver·Slab 직접 의존 |
| 다른 프로젝트에 쉽게 설치 | 플러그인 활성화 후 Bootstrap Actor 하나와 setup asset으로 preview/UI 확인 | 현재 여러 Host·Coordinator·자산·TC 설정을 수동 이식 |

“느려짐 없이”는 무제한 센서·어떤 PC에서도 60FPS를 의미하지 않는다. **고정 장비/장면/출력 조합의 지원 매트릭스**를 만들고, 한계 초과를 미리 알리며 요청 규격을 몰래 낮추지 않는 것을 제품 기준으로 삼는다.

DTCore는 필수 의존성이며 현재 `b2504d1`을 유지한다. 당면 배치·UI·PCD 개선은 프로젝트 코드에서 진행하고, 공통 수정이 불가피한 재현 사례만 별도 최소 변경안으로 다룬다. AI 모델 학습/실장비 SDK 전체 구현은 센서 플랫폼 완성의 자동 포함 범위가 아니다.

송신 고도화의 주 대상은 **Binary PCD**다. Camera는 추가 배치·방향·미리보기와 관련 정합성을 계속 다룬다. 일반 CSV/JSONL/LAZ/JPEG 및 호환 JSON의 신규 기능·최적화는 현재 보류하고 기존 계약의 회귀만 유지한다. **사용자 2026-10-15 승인으로 DATASET-01의 opt-in 로컬 학습 데이터셋 범위는 예외**다. 화면에서 사용자가 직접 켜고 조건을 정한 때만 생성하며 기본 PCD 송신 정책을 유지한다. 장비 위치/전체 배치 저장은 데이터 파일 형식 개선과 별개의 사용자 기능이다.

## 2. 권장 순서와 단계별 결과물

**AUTO-01 (사용자 2026-10-15, 로드맵 #7, 기준 d45f27e): 1단계 구현·집중 검증 완료, 통합 검증 중.** 공식 Python/PowerShell 실행기가 결과+child 상태로 종료 코드를 보장하고 UE는 정상 종료한다. 일반 UI의 terminal 제어 잔류 버그는 SCN-RELEASE-01(`9fa04be`)로 분리 수정했다. offscreen 결합 job은 시나리오→데이터셋20→PCD30초·599submit/receipt로 성공했고 로컬 API·토큰·실행기/취소도 검사했다. [명세](../README.md#auto-01-배치-실행과-로컬-제어-api--기본-꺼짐), [현재 gate](IMPROVEMENTS.md#auto-01--화면-없는-배치-실행과-로컬-제어-api-1단계-로드맵-7), `Saved/Reports/AUTO-01/RESULT.md`를 따른다. 기본 PCD·wire·저장값·맵·DTCore를 유지하며 디스크 resume/외부 노출/포맷 변환·플러그인화는 시작하지 않는다.

**DATASET-01 (사용자 2026-10-15, 기준 f499d57): 90% — 구현·자동 검증 완료, 직접 마우스 환경 보류.** CAM-GT/LIDAR-FID 위에서 domain randomization·장면 라벨·채점·owned data UI를 제공한다. 재개 승인에 따라 일반 동시 캡처의 cached shader binding 수명/texture 전환 버그를 **11번 GPU-CRASH-01**로 분리 수정(`337dd10`)했고, 데이터셋 GPU→CPU 강제 전환 없이 일반4경로100회와 FullSpec100장면×3·정답 재현/취소·release278 Success/skip30·Game/WBP7/quick/layer0·기본 꺼짐ABAB×3의2% gate를 통과했다. 직접 Computer Use는캡처0x80070057로 미조작 보류이며 자동1920×1080 진행 화면과 구분한다. [상태와 gate](IMPROVEMENTS.md#dataset-01--학습용-데이터셋-자동-생성-1단계), `Saved/Reports/DATASET-01/RESULT.md`를 따른다. 기본 PCD 정책과 명시적 opt-in을 유지한다. 2단계 KITTI/COCO·ROS bag·7번 연계 대량 headless는 제안이다. DTCore 수정·플러그인화와 분리한다.

2026-10-15 사용자 승인 범위 **LIDAR-FID-01**은 현재 프로젝트 모듈 안의 센서 사실성 1단계다. 기존 시간 필드·SurfaceResponse·장비 프로필을 감사하고, 기본 꺼짐인 시간차·rigid motion 평면 근사·재질/입사각 반사와 소유 UI를 보완한다. 기준은 `e0a6901`, Runtime `eea0268`이며 전체 회귀·기본 꺼짐 2% 성능·opt-in 비용·직접 화면 수용 결과는 [IMPROVEMENTS](IMPROVEMENTS.md#lidar-fid-01--lidar-사실성-1단계)와 `Saved/Reports/LIDAR-FID-01/RESULT.md`를 따른다. 발사 순서/반사율 계수의 가정을 실장비 보정으로 표시하지 않는다. **2단계** 빔 발산·혼합 픽셀·물리 multi-echo·ROS2 시간차 활용은 제안이며 구현 승인이 아니다. 플러그인화·DTCore 수정 단계와 분리한다.

현재1단계는 **90%**: Editor/Game·전체release272 Success/skip29/실패0·기본 꺼짐2% gate·opt-in 비용·자동 화면 검증 완료다. 직접 마우스는 Computer Use 모니터 캡처0x80070057로 환경 보류하며 자동 캡처를 직접 수용으로 바꾸지 않는다. 속도 provider의 DTCore 공용화는 DTC-LIDAR-FID01 후보 기록만 추가했고 DTCore는 고정했다.

현재 착수 순서: **A단계(아키텍처 뼈대 재정비) → ① 센서·카메라 추가 배치와 저장 위치 전용 패널 수용 → ② 작업 흐름 중심 UI/UX 재구성 → ③ PCD 송신 사전 점검·예산 안내 → ④ 다중 센서 PCD 장기 검증·조작 성능 → ⑤ 남은 R4 의존성 정리**.

R0~R6는 최종 목표의 영역·완료 gate를 나타낸다. A단계는 R4의 일부를 **같은 모듈 안에서** 앞당겨 수행하는 구조 정리이며, 그 뒤 R2 사용성 작업을 재개한다. 남은 장기 검수 전체가 끝나야 다음 단계를 시작할 수 있다는 순서로 해석하지 않는다. 세부 진행률과 보류 항목은 IMPROVEMENTS를 따른다.

DTCore 작업 완료를 기다리느라 프로젝트 측 안정화를 멈출 필요는 없다.

### A단계 — 아키텍처 뼈대 재정비 (현재 진행)

**왜 지금인가:** 배치·PCD·Slab 기능이 빠르게 쌓이면서 센서 Runtime이 UI 타입과 Slab context, 업무 TC를 직접 참조하게 됐다(README §10.2). 이 상태로 다중 센서 확대나 플러그인화를 시작하면 변경마다 영향 범위를 가늠하기 어렵고, 나중의 분리 비용이 계속 커진다. 기능을 늘리기 전에 **레이어 경계를 세우고 자동 gate로 고정**한다.

**범위:** 한 Runtime 모듈 `ma0t10_dt` 안에서만 정리한다. L0 `Contracts/` → L1 센서 Runtime(`Sensor/` `Camera/` `Core/`) → L2 Host·업무(`Scenario/` `Slab/` `WebSocket/TC/` 기존 업무 폴더) → L3 `UI/`의 단방향 의존을 목표로 한다. 새 모듈·`.uplugin`·자산 mount 이동은 하지 않는다.

| 순서 | 항목 | 결과물 | gate |
|---:|---|---|---|
| 1 | ARCH-01 | 레이어 검사 스크립트·allowlist, DTCore guard 고정값 수정, 변경 전 기준선 | 현재 HEAD에서 gate 통과, 신규 위반 차단 확인 |
| 2 | ARCH-02 | Slab 서비스 `Core/`→`Scenario/`, Workspace Subsystem `Core/`→`UI/` | 이동만, 회귀·자산 불변 |
| 3 | ARCH-03 | `Contracts/` 편집 타입·기본값 상수 | L1의 UI 타입 의존 0, SaveGame/enum 호환 |
| 4 | ARCH-04 | Coordinator/SlabActor의 UI 직접 호출 역전 | L1→L3·L2→L3 0, BP 시그니처 불변 |
| 5 | ARCH-05 | 송신 자체 검증의 TC 의존 분리 | L1→`WebSocket/` 0 |
| 6 | ARCH-06 | 중립 acquisition context + Slab provider | golden byte 동일, L1→Slab 0 |
| 병행 | ARCH-07 | `verify.ps1`·opt-in 변수표·Scripts 분류 | manifest 자동 생성 |
| 마지막 | ARCH-08 | 미사용 모듈 의존·테스트 hook·대형 파일 분할 | 이동만으로 구성된 diff |

**A단계 완료 gate:** layer gate allowlist가 0(또는 사용자가 승인한 명시적 예외만 남음), 전체 `MA0T10`·WBP 컴파일·wire golden fixture가 기준선과 같고, 보호 경로가 불변이다. 이 gate를 통과하면 R4의 남은 일은 설정 객체(PL-02 2단계)와 ProfileId registry(PL-03)다.

**하지 않는 것:** 센서 플러그인/새 UE 모듈 생성, DTCore 수정, wire/SaveGame/Blueprint 계약 변경, 성능 최적화, 새 사용자 기능.

**순서 조정(2026-10-11 사용자 결정):** ARCH-01~04 → **U단계** → ARCH-05~08. UI 경계가 정리된 직후 UX를 진행하고, UI 파일을 건드리지 않는 송신·Slab 내부 정리는 뒤로 미룬다.

### U단계 — 사용자 흐름 중심 UI 재정비 (ARCH-04 직후)

초기 방향(모니터·JSON Payload·파일 내보내기)에서 현재 방향(배치 → 시나리오 → PCD 전송 → 결과)으로 바뀌면서 화면 진입점·중복 설정·개발자 용어가 뒤섞였다. 주 사용자인 현장·업무 담당자 기준으로 화면을 다시 묶는다. 기능·API·저장값·기본 정책은 바꾸지 않고 **위치·문구·노출 수준**만 바꾼다.

| 순서 | 항목 | 결과 |
|---:|---|---|
| 1 | UXR-01 | 상단 작업 흐름 바 `배치·미리보기·시나리오·PCD 전송·결과·도구`, 전역 상태 줄, 용어 상수 |
| 2 | UXR-02 | 배치 창이 장비 목록·조작·저장 위치·전체 배치를 직접 소유, `측정 시작/중지` |
| 3 | UXR-03 | PCD 전송 한 화면에서 신규/재생 출력 설정, 결과·파일·연결·진단 탭 분리 |
| 4 | UXR-04 | 시나리오 화면 정리, 출력 정책은 요약+이동 |
| 5 | UXR-05 | 센서 상세 설정 기본/고급/고급·호환, 레거시 기능 이동 |
| 6 | UXR-06 | 미리보기 상태 1줄, 상세 진단 분리 |
| 7 | UXR-07 | 문구 전수 점검, 렌더 캡처·직접 조작·첫 사용자 시나리오, 사용자 확인 |
| 8 | UXR-08 | Slab 정보 카드·3D 클릭 선택·표면 표현 프리셋(LiDAR 반사율 연동은 명시 선택) |
| 9 | UXR-09 | A안 작업 사이드바 시각 리프레시(디자인 토큰·하단 상태바·한 번에 작업 1개), 산화 스케일 외형 개선 |
| 10 | MAP-01 | 새 맵에 "센서 도구 Rig" 하나로 설치, Slab·배경은 맵에서 별도 세팅, 맵 점검 화면, 맵 하드코딩 제거 |

2026-10-12 사용자 결정: A안 선택(시안 https://claude.ai/artifact/SHhmPcxxrBuSCLkE4xoZ3s). 실제 적용은 신규 맵에 도구를 넣고 Slab·배경을 따로 세팅하므로 MAP-01을 U단계에 포함한다. 순서: UXR-09 → MAP-01 → UXR-07(최종 수용).

2026-10-12 보완: 상단 버튼은 토글(다시 누르면 숨김), 장비 선택기 단순화, Codex가 매 UI 항목에서 마우스 기능 점검과 성능 전후 비교를 수행한다. 진행률 보고는 IMPROVEMENTS의 "개편 프로그램 진행률" 표를 따른다.

**U단계 완료 gate:** 첫 사용자가 안내 없이 `PIE 시작 → 장비 추가 → 배치·위치 저장 → 시나리오 실행 → PCD 전송 → 결과 확인`을 끝낸다. 기본 화면에 개발자 용어·레거시 항목이 없고, 같은 설정이 두 화면에 없으며, 기존 자동화·WBP 컴파일·SaveGame 호환이 유지된다. 상세는 IMPROVEMENTS 0-U절.

### R0 — 재현 가능한 기준선

**현재:** 핵심 문서 4개와 자동화/실측 근거가 있다. Runtime `81b56ba`의 최종 전체 MA0T10은217 Success 중실제215·opt-in skip2다. `ma0t10_dt` 단일 작업 폴더에서 빌드·검수하고 이전 worktree의 자료/이력을 보존·정리했다. clean 설정·운영 환경과 직접 마우스 검수의 재현성은 계속 보완한다.

- 코드/DTCore SHA, Engine/toolchain/GPU/driver, actual viewport/DPI, profile/backend/output/UI 상태를 manifest로 기록한다.
- DTCore `b2504d1`을 고정하고 격리 host/프로젝트 검증 이력을 유지한다. 공통 의존성 업데이트가 필요하면 별도 변경과 검증으로 분리한다. 타 실제 프로젝트 검증은 별도다.
- 운영맵 사용자 변경을 검증 fixture에서 분리한다. map regeneration을 quick-start 필수 단계에서 제거하고 임시/커밋된 test scene을 사용한다.
- 최신 단일 ML-X PCD 기준선을 재현하고 각 test의 실제 수행/skip과 원본 보고서를 연결한다.

**완료 gate:** 개인 Game.ini/SaveGame 없이 빌드·WBP·계약·핵심 RHI 재현, 운영맵/보호 경로 hash 불변. 관련 백로그: QA-01.

### R1 — 확장 전에 데이터·수명 정확성 확보

진행: RT-02(`d2024bf`)·RT-03(`59d1027`)와 RT-01(`a120ba6`)·RT-04(`017d20f`) 구현·집중 검증. 공통 DTCore 종료·로그·registry·구독·Widget 계약도 보강했다. LIFE-01 전체 fault 조합과 전체 R1 gate는 아직 미완료이며 수동 검증 범위는 최신 README/보고서를 따른다.

1. 구현된 Esc 단일 종료 경로와 원설정/실행 상태 복원을 새 배치 패널에서도 사용한다. Drag commit과 mode exit를 구분한다.
2. 구현된 Raw frame/byte 상한과 0Hz/정체 진단을 새로 추가한 센서에도 적용한다. 사용자에게 잠금·정지·준비 이유를 표시한다.
3. 기존 Camera 픽셀·metadata 정합성 검증을 배치/미리보기 변경의 회귀로 유지한다. JPEG 전송 고도화를 당면 목표에 추가하지 않는다.
4. 추가/복제/삭제·Stop·설정 변경·PIE 종료에서 관련 generation·cancel·drain과 소유권을 검사한다.

**완료 gate:** RT-01/02/03/04, LIFE-01의 관련 회귀가 통과하고 같은 frame에 다른 픽셀이 붙거나 다른 owner 자원을 정리하지 않음. 정상1+1과 검증된 PCD 조건을 유지한다. 센서 수 확대는1→2대부터 단계적으로 검증한다.

### R2 — 다중 센서 배치와 성능 제품화

1. **배치 진입점과 전용 패널:** 구현된 `장비 추가·위치 저장` 진입점과 `센서·카메라 배치` 패널의 첫 사용자/직접 조작 수용을 닫는다. 장비 목록·추가/복제/삭제·Transform·기즈모·저장 위치·Undo/Redo와 전체 배치 복원을 연결했고, 첫 사용은 배치+미리보기다. 프로필/품질은 Settings, PCD는 Data로 연결하며 사용자 창 저장값은 보존한다.
2. **PIE/runtime 추가 배치:** LiDAR/Camera 추가 시 고유 ID·프로필·생성 위치 확인→Actor 생성→Coordinator/출력 서비스 등록→목록/미리보기 동기화 순서를 지킨다. 추가만으로 Topic을 송신하지 않는다. 도구가 생성한 장비의 삭제와 기존 레벨 Actor의 보호를 구분하며, 수집 중 대상/설정을 임의 변경하지 않는다.
3. **명확한 저장 기능:** 선택 센서 pose 북마크와 전체 장비 구성을 구분한다. 기존20개 북마크·50개 PIE 이력을 재사용하고, 전체 배치에는 stable ID/종류/프로필/pose를 포함해 명시적 불러오기로 복원한다. 새 PIE에 자동 적용하거나 `.umap`을 자동 저장하지 않는다. Editor 영구 배치·Editor Undo는 별도 후속이다.
4. **소유 UI/UX 정리:** 배치 → 미리보기 → 시나리오 → PCD → 결과의 직접 진입점, 일관된 대상/상태/주요 조작, 읽기 쉬운 다크 스타일을 제공한다. 새 패널은 명시적 소유 등록과 저장 호환을 갖추고, 기존6개·미등록/동료 Widget은 보존한다.
5. **PCD 사전 점검과 검증:** 선택 출력만으로 예상 제출량/대역폭을 계산하고 부족한 설정을 안내한다. Camera 미리보기는 별도 렌더 부하로 기록한다. 사용자가 실제 추가한1→2 LiDAR의 단기·반복·장기 시험 후 다음 규모를 판단한다.

- 기존 경량 조작 API를 유지하며 조작 중 입력→Transform→화면 지연을 계측한다. 선택 센서의 preview 우선순위가 타 센서의 acquisition을 독점하지 않게 한다.
- 사전 점검 이후의 전체 작업량 기반 배분은 확인된 병목에 따라 설계한다. 센서 개수뿐 아니라 pixels/s·rays/s·echo·CPU/GPU·semantic·PCD 직렬화·네트워크 비용을 사용한다. Camera staging 재사용과 다른 포맷 최적화는 현재 보류다.
- 설정할 규격, admission된 규격, 실제 acquisition/output률을 별도로 보여준다. 지원 불가 조합은 사전 경고하고 명시적인 profile/preview 선택으로 해결한다.
- 우선 Camera 미리보기를 포함한1+1/2+2 PCD 조건의 지원표를 작성한다. 4+4는 후속,8+8은 탐색값이다. 스트레스 fixture를 실 공장 장면 대표로 자동 간주하지 않는다.
- UI의 실제 해상도/DPI·저장 복원·입력 소유권을 점검하고 3D 라벨 겹침은 측정 파이프라인과 독립적으로 개선한다.

**완료 gate:** RT-05/06/08, UX-01, UI-01/03. 사용자가 도움 없이 추가→선택→배치→저장/불러오기→PCD 실행→결과 확인을 수행하고, 지원 조합에서 입력 반응·측정 완료율·송수신·자원 상한이 함께 만족한다. 모든 지원 수치에 실제 조건/보고서를 붙인다.

### R3 — 시나리오 기반 신뢰할 수 있는 수집

- host의 **관찰 가능/데이터 필수** 정책은 이미 구현됐다. 기본 관찰 fallback과 opt-in 데이터 필수 실행을 유지하고 PCD 운영/장기 검증을 보완한다.
- preflight는 설정/자산/필요 센서 확인, 연결 준비는 실제 transport 상태 확인, 실행은 acquisition, drain은 승인된 데이터 완료로 구분한다.
- 중립적인 run manifest에 scenario/run ID, target sensors, profile/calibration/backend, 요청/완료/누락/전달 수, 출력 경로·checksum·종료 이유를 기록한다.
- 원본 Slab frame과 센서 frame을 일대일 강제하지 않는다. acquisition 시작 context가 직렬화/전송/저장까지 보존되는 것을 검증한다.
- 소비자 업무 완료가 필요한 배포에만 application ACK·멱등성·누락 대조를 추가한다. Broker receipt로 학습 데이터셋 저장 완료를 선언하지 않는다.
- 네트워크 장애 중 영구 보존은 선택 기능이다. 추가할 경우 bounded 디스크 spool·회수·용량 경고·재전송 규칙을 별도 계약으로 정하며 무제한 backlog를 만들지 않는다.
- LAZ 등 PCD 외 형식의 timeout/cancel 고도화는 보류한다. PCD 공통 경로·기존 기능에 영향을 주는 회귀는 관련 변경에서 확인한다.

**현재 PCD gate:** DATA-01/LIFE-01의 관련 장애·완전/부분/실패 판정, 선택하지 않은 출력과 비대상 스트림 보호, PCD 기존 소비자 호환. RT-07 업무 ACK와 IO-01 다른 포맷 개선은 별도 후속이며 현재 PCD 단계의 필수 착수 조건으로 묶지 않는다. 공통 코드 변경 시 기존 JPEG/JSON의 관련 회귀는 유지한다.

### CAM-GT-01 — 카메라 정답 데이터의 첫 검증 경로

사용자 2026-10-15 요청으로 기존 카메라 RGB 옆에 기본 꺼짐인 Depth·Semantic·Actor ID·2D bbox와 보정 snapshot을 추가한다. 1단계는 소유 미리보기의 확인과 `Saved/CameraGroundTruth` 1회 검증 파일만 다루며, 기존 wire·서버·저장 schema와 파일 형식 개선 보류 정책을 유지한다. LiDAR의 공통 기본 label/팔레트와 기존 ID 재질을 재사용하고 새 자산·UE 모듈·플러그인을 만들지 않는다. 진행·정확도·성능·직접 수용은 IMPROVEMENTS의 CAM-GT-01과 해당 RESULT를 따른다.

현재 구현·검증(2026-10-15): Depth/semantic/Actor ID/visible 2D bbox/보정 및 최소 저장과 자동 4모드 렌더를 완료했다. 기본 꺼짐 ABAB×3은 FPS −0.0011%·p95 동일·GT +0.7332%로2% gate를 통과했다. 직접 마우스는 Windows 보안 팝업 미조작으로 보류하고 자동 검증과 구분한다. PCD-OPT의 기존 UXR03 기대값 누락은 별도 승인으로 단독 수정·재검증했다.

후속 **2단계 제안(미승인/미구현)**: 기존 JPEG와 분리한 새 topic/schemaVersion의 정답 frame·calibration 연계, ROS2 `Image`/`CameraInfo` adapter, optical 좌표계의 3D bbox/pose. KITTI/COCO 등 데이터셋·대량 생성은 별도 범위이며 이번 최소 저장에서 확대하지 않는다.

### ROS2-01 — 외부 ROS2 소비 경로 (사용자 2026-10-15 승인)

기존 Artemis PCD/LiDAR/JPEG wire를 외부 rclpy 노드에서 ROS2로 변환한다. `Tools/Ros2Bridge`는 전용 Ubuntu 24.04 WSL2의 Jazzy·Foxglove 테스트 환경을 생성하고 검증 뒤 unregister로 삭제한다. 기존 WSL 배포판·기본값·Broker/방화벽 설정·UE 코드·플러그인 경계는 유지한다. PCD PointCloud2와 acquisition TF가 우선이며, Camera는 기존 payload에 실제로 있는 metadata만 사용한다. `/clock`·미전달 calibration/pose를 추정하지 않는다.

현재80%: Windows/WSL golden·ROS 직렬화·Artemis4종 adapter·두 주기 PIE5분 송신 회귀와 빈 WSL 설치/삭제를 검증했다. 10Hz 브리지 미수신1, ROS best-effort 추가 누락81/15, Image 디코더 결정과 직접 Foxglove 3D/축 수용은 남아 있다. 원본/정확한 범위는 IMPROVEMENTS ROS2-01 및 `Saved/Reports/ROS2-01/RESULT.md`를 따른다. UE의 ROS2 상태 UI와 CAM-GT 새 wire 송신은 후속 2단계 제안이다. 이 경로는 R5 센서 플러그인 생성을 시작하지 않는다.

### R4 — 프로젝트 안에서 분리 준비

**이 단계는 아직 클래스/자산을 플러그인으로 이동하지 않는다.** 경계를 먼저 정리한다. 아래 표의 편집 타입·Coordinator·Workspace·Slab context 항목은 A단계(ARCH-02~06)가 같은 모듈 안에서 먼저 처리한다. A단계 완료 후 남는 R4 범위는 설정 객체·soft reference, ProfileId registry, 분석 extension이다.

| 현재 결합 | 프로젝트 내 선행 리팩토링 |
|---|---|
| frame/request의 `FVirtualSlabFrameContext`, Capture/Scan의 SlabContext 호출 | 중립 immutable acquisition context + provider/completion observer. host Slab adapter가 기존 wire metadata 의미를 제공 |
| CaptureExport가 SlabActor 탐색·출력 설정 | host-owned scenario UI extension/policy adapter. 일반 저장/송신 UI와 분리 |
| Workspace의 Slab panel type 검사/Host spawn | 등록형 panel descriptor/factory. 소유권 token과 기존 저장 key adapter 유지 |
| Lidar의 업무용 slabAnalysis/기준 yaw | 일반 분석 extension 또는 host analyzer로 이동; 기존 Payload adapter 유지 |
| Actor가 `UI/VirtualSensorControlTypes.h`에 의존 | 편집 상태·명령 계약을 Runtime public 영역으로 이동, UI→Runtime 단방향 |
| Coordinator의 Monitor 타입·직접 bind/보기 제어·OwningPlayer 조회 | 선택/모드 변경은 Runtime delegate/interface, Widget binding·플레이어 뷰 처리는 UI adapter로 분리 |
| `/Game/MA0T10`, SensorTestMap, MA0T10 저장 슬롯/Topic | settings/soft references와 explicit override 우선순위 도입 |
| device enum/switch | built-in enum adapter + 안정적인 ProfileId registry |

- 현재 이름만 제안됐던 IntegrationSettings/DtCoreBridge를 이미 있는 것으로 가정하지 않는다. 필요한 최소 설정/구독 façade를 구현한다.
- 일반 DTCore JSON 구독은 owner/generation별로 정리하되 공유 연결·다른 Subscription을 끊지 않는다. Binary 경로는 Raw worker로 유지한다.
- context provider는 게임 스레드에서 snapshot을 만들고 worker는 immutable 값만 읽는다. UObject/host 상태를 worker가 역참조하지 않는다.
- serialization byte fixture와 Blueprint compatibility를 경계마다 검증한다. 클래스 이름 변경과 계약 변경을 같은 대규모 작업에 섞지 않는다.

**완료 gate:** PL-01/02/03. 센서 Runtime→Slab/host/UI include·타입·module dependency가 없는지 검사하고, Slab/UI adapter를 연결하면 기존 metadata/동작이 돌아온다. 아직 plugin 설치 완료라고 표시하지 않는다.

### 개편 마무리와 후속 순서 (2026-10-13)

개편(ARCH+UXR+MAP)은 UXR-07 사용자 체험·확인으로 닫는다. 그 뒤 권장 순서: **PERF-01 절대 성능 재확인·UXR-08 산화 외형 확인 → UI-02 3D 진단 라벨 정리 → OPS-01 에이전트 환경(새 Codex 대화, 점검 전용 계정/VM) · QA-02 verify 자동 실행·성능 측정 표준화 → 아래 DTCore 수정 단계 → 센서/카메라 플러그인화(R5)**. FONT-01(한글 UI 글꼴)은 선택. 2026-10-13~14 야간: UXR-08 후속·UI-02·QA-02(대부분)·OPS-01(병행 검수 복사본)·UXR-10(사용자 피드백)·UXR-11(실제 마우스 결함) 구현. 남은 것은 직접 마우스 재검수, 통합 release, `verify -Level perf`, UXR-07 사용자 15분 체험. 상세 근거·완료 조건은 IMPROVEMENTS "개편 후속 개선" 절.

### 개편 이후 예정 단계 (2026-10-12 사용자 예고)

1. **DTCore 수정 단계:** IMPROVEMENTS의 "DTCore 변경 후보" 목록을 근거로 DTCore 쪽 변경을 별도 승인·별도 저장소 커밋으로 진행하고, 프로젝트는 검증된 gitlink만 갱신한다. 우회 코드는 그 뒤 정리한다.
2. **센서/카메라 플러그인화(R5):** ARCH(레이어 gate 0)와 남은 R4 정리가 끝난 뒤 착수한다.

이 두 단계는 이번 ARCH+UXR 개편 범위가 아니다. 개편 중에는 DTCore를 수정하지 않고 플러그인 생성도 시작하지 않는다.

### R5 — DTCore 의존 플러그인과 한 클래스 진입점

**이번 버전에서는 착수하지 않는다.** 아래는 A단계·R4 이후의 목표 구조다.

권장 최종 구조(모두 **신규 제안**):

| 구성 | 포함 | 포함하지 않음 |
|---|---|---|
| `DTVirtualSensorRuntime` | 센서 Actor/Component, scheduler/backend, profile registry, codec, output/transport/record/file, 중립 context/extension 계약 | Slab 클래스·업무 Topic·차트, host module, UI 타입 역참조 |
| `DTVirtualSensorUI` | 센서 Monitor/Settings/Data, gizmo, panel host/workspace core, 소유 UI 상태 | Slab panel factory/업무별 메뉴를 hardcode한 구현 |
| `DTVirtualSensorEditor` | 배치·설정 검증·명시적 맵 반영·자산 제작 도구 | packaged runtime 의존 |
| plugin Content | 센서 WBP/Niagara/semantic material·기본 profile/setup assets | 운영맵·Slab 업무 자산 |
| host `ma0t10_dt` | Slab 시나리오/이동/분석/차트/재생, TC 등록·context/UI adapter | 센서 코어의 사본 |

- `.uplugin`에 DTCore 필수 의존을 명시한다. host Build.cs를 그대로 복사해 PixelStreaming·업무 API·Player/Manager 의존을 끌고 오지 않는다.
- 자산 mount는 `/DTVirtualSensor/...`로 옮기고 Asset Registry·cook 의존성을 검사한다. `M_LidarSemanticId` 같은 간접 로드 자산을 누락하지 않는다.
- 기존 자산을 이어 쓰는 경우에만 정확한 CoreRedirect를 제공한다. enum 순서·Blueprint 함수·SaveGame은 adapter/migration으로 검증하며 파일을 일괄 삭제하지 않는다.
- `ADTVirtualSensorBootstrapActor`(제안)는 기존 구성 요소의 **façade**다. 구현을 한 거대 Actor로 합치지 않는다.

Bootstrap의 확정할 기본 동작:

1. 명시적인 Coordinator/Host가 있으면 재사용, 없으면 소유 인스턴스만 생성한다. 초기 버전은 World당 한 sensor rig/coordinator로 제한하고 중복은 오류로 안내한다.
2. setup DataAsset에서 **기존 센서 발견** 또는 **목록 기반 생성**을 선택한다. 생성 시 ID/설정 검증→output service 연결→등록→UI bind→측정 시작 순서를 제어한다.
3. 초기 기본값은 로컬 preview만이다. Broker credentials/업무 Topic·송신은 explicit opt-in 또는 host 시나리오 정책으로 시작한다.
4. GameMode/GameInstance/Player/Main를 강제 교체하지 않는다. Main Canvas가 있으면 사용하고 없으면 Viewport fallback, 지연 PlayerController는 제한된 재시도로 처리한다.
5. EndPlay는 자신이 만든 Actor·timer·delegate·subscription만 정리한다. UI 숨김과 작업 중단은 별개다. dedicated server에서는 UI를 만들지 않는다.
6. Slab는 Bootstrap을 몰라도 되고 Bootstrap도 Slab 클래스를 몰라야 한다. host가 adapter를 명시적으로 등록한다.

**초기화 시점 주의:** 현재 `ma0t10_dtBootstrap`의 WebSocket 1ms 설정은 PostConfigInit에서 적용된다. 이를 Actor BeginPlay로 옮기는 것은 동등하지 않다. 우선 host 정책으로 남기고 플러그인은 암묵적으로 전역 값을 바꾸지 않는다. 필요하면 별도 opt-in 조기 모듈을 비교 검증한다.

**완료 gate:** PL-04. 빈 UE5.3 consumer 프로젝트에 DTCore+plugin만 넣고 Bootstrap 하나로 실행. host 소스/자산/local Game.ini 없이 Editor와 packaged Development 빌드/cook, UI/미리보기/파일/PCD/RHI/종료 회귀 통과. 다른 GameMode/Main 유무·다중 PIE에서 동료 자원 불변.

### R6 — 장비와 운영 범위 확장

- 새 장비의 물리 규격·품질·지원 capability를 Profile DataAsset으로 추가한다. backend/SDK가 필요하면 해당 extension을 구현하고 unsupported 기능은 비활성+이유를 표시한다.
- enum 기존값은 built-in ProfileId로 매핑한다. ML-X Native/Integration200을 합치지 않는다. 프로필 적용은 하나의 설정 transaction/재예약으로 유지한다.
- 실장비 SDK·패킷/캘리브레이션·raw sample이 확보된 모델만 hardware-calibrated/protocol-verified 단계로 올린다.
- 보안 배포 프로필·TLS 경로·Broker 장애·소비자 ACK·장시간 soak를 지원 조합별로 인증한다.
- DTCore의 별도 개발이 완료되면 **의존성 업데이트 PR**에서 API/수명/Widget/구독 contract 시험을 돌린다. 센서 기능 PR에 gitlink 변경을 묶지 않는다.

**완료 gate:** 신규 profile 등록에 코어 switch 편집이 필요 없고, golden dataset/실측 지원 표/릴리스 노트/호환 매트릭스가 장비별로 존재한다. 아직 자료 없는 장비는 공개 사양 에뮬레이션으로 남긴다.

## 3. 성능·신뢰성 gate 제안

아래는 최종 목표를 수치화하기 위한 **제안값**이며 현재 지원 선언이 아니다. 구현 착수 시 대상 PC·driver·scene·정격·출력을 고정하고 승인한다.

| 구성 | 화면 목표 | 정격/데이터 목표 | 현재 상태 |
|---|---|---|---|
| D455 1 + ML-X Native 1 | 평균≥55 FPS, 1% low≥45, frame p95≤20ms | LiDAR/PCD≥19Hz; Camera 배치·미리보기, JPEG는 선택 시 기존 계약 회귀 | 2026-10-08 실제1920×1080 전체 출력30/20Hz·평균59.98FPS·p9516.67ms 통과. 현재 초점은PCD |
| 2 + 2 | 같은 60FPS 계열 기준 | 새 배치 WhenRequested·PCD 정격·Camera1280×720/30Hz 미리보기 | 표시형1920×1080·60초3회/10회반복/600초 통과. 반복최소low55.966,600초low59.58·각PCD≥19.9967Hz. 기본 제출예산2. 대표장면·숨김·4+4/60분 별도 |
| 4 + 4 | 후속 목표, 착수 시 조건 확정 | PCD 정격 유지 가능 범위를 실측하고 한계를 명시 | 현재 미검증·후속 |
| 8 + 8 이상 | 탐색 시험 후 등급 설정 | Best Effort/지원/미지원 구분 | 보장 없음 |

이전 배치 작업공간 근거는 `Saved/Reports/SensorPlacementWorkspace/FINAL_STATUS.md`다. Camera도1280×720/30Hz로측정·미리보기했지만Camera/LiDAR정보Topic은0건이었다. 당시부하환경실패와재시험을보존한다. 전체 출력의이전60초시험은 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`의명시적4건/프레임조건으로만보존한다. 선택하지않은출력의0Hz를PCD실패로판정하지않는다.

현재 근거는 `Saved/Reports/PcdDemandPlacementAcceptance/FINAL_STATUS.md`다. 같은전체JSON stride1/max0·맵/pose/UI에서정책을교차비교해WhenRequested를검증했다. PCD는전체검출점·FrameStride1이다. 기존클래스/레벨/구형layout의LegacyAlways는유지하고명시적으로전환할수있다. 숨긴/offscreen과Legacy전체JSON의성능제한은남으며,새정책직접UI검수도Windows입력접근제약으로미완료다.

공통 gate:

- 같은 모델/설정의 센서 간 완료 횟수 max/min≤1.2, 유예 후 0Hz 센서는 정상 평균에서 제외하지 않음.
- 실제 client 1280×720/1920×1080, 고정 scene 및 preview/표시/출력 조합, 최소10초 warmup(셰이더/자산 준비 포함), 60초 측정.
- 동일한 **승인된 센서 프레임 집합**의 제출/receipt/소비자 검증 일치. Slab 600행=센서600프레임으로 가정하지 않음.
- 정상 지원 조건 invalid/unexpected gap/overflow/silent replacement 0. budget miss와 미확인 소비자 결과는 별도 집계.
- 입력→화면 p95≤50ms, 조작 종료 coherent 프레임≤1.5초를 제안한다. 경량 preview와 물리 output을 섞지 않는다.
- 메모리·큐는 count와 byte 모두 상한, 10분/60분 안정 구간에서 누적 증가가 없어야 한다. 무조건적 0지연/0메모리를 요구하지 않는다.

### 네트워크는 물리적 한계가 있다

현재 33-byte PCD의 Native 32,256점은 header 제외 약1.064MB/frame, 20Hz에서 약21.29MB/s다. 4대면 약85.16MB/s에 PCD/STOMP overhead가 추가된다. JPEG·telemetry의 추가 대역폭은 해당 출력을 명시적으로 선택했을 때만 계산한다. 모든 ray가2 Echo이면4대 약170.31MB/s로 **1Gbps의 이론적125MB/s도 초과**한다.

따라서 센서 수 증가에는 측정/직렬화 최적화만이 아니라 NIC/Broker 디스크/소비자 처리량 검증이 필요하다. 가능한 해결은 더 빠른 망, 승인된 ROI/출력 선택, 별도 수집 노드 등이며 PCD 포맷/정격을 몰래 바꾸는 것이 아니다. 현재 코드의 bandwidth budget이 backend별 어느 경로에 적용되는지도 RT-01/08에서 확인한다.

## 4. 단계 운영과 출시 원칙

- 첫 구현 묶음인 **UX-01 배치 패널·전체 배치·PCD 사전 점검**은 구현됐다. 다음은 첫 사용자/남은 직접 입력·DPI 수용, 대표 장면·부하 환경의 PCD 검증, 동적 예산과 의존성 정리 순이다. RT-01/02/03/04·LIFE-01·QA-01의 관련 회귀를 각 묶음에서 수행한다.
- 각 PR은 기능/계약/자산 이동 중 하나를 중심으로 만든다. 관련 빌드/테스트 후 커밋하고, 전체 회귀·실 RHI/네트워크 검증은 통합 gate에서 수행한다.
- schema/enum/SaveGame migration은 old fixture를 유지한다. wire contract를 바꾸는 기능은 별도 opt-in/version으로 제공한다.
- 성능 미달은 기능적 데이터 오류와 구분한다. 요청 규격을 충족하지 못하면 지원 표를 낮추거나 후속 최적화를 하고 달성했다고 표시하지 않는다.
- rollback은 신규 branch/commit으로 계획하고 보호된 사용자 작업을 reset하지 않는다. 주요 module/asset 이동 전에는 별도 worktree/깨끗한 consumer fixture로 검증한다.
- 문서 완료, 코드 완료, 자동화 완료, 실제 현장 완료를 구분한다. 실제 회사 맵/consumer가 제공되지 않으면 현장 완료 gate는 남는다.

### 단계 착수 시 확정할 입력

| 입력 | 필요한 이유 |
|---|---|
| 목표 PC/GPU/driver 및 실제 센서 수 | 지원 매트릭스·GPU 메모리/trace/encode budget |
| 장면 대표성·동적 물체·geometry 종류 | GPU depth/semantic/CPU의 측정 차이와 proxy 비용 |
| 필수 output·소비자 ACK 필요성 | 현재 주 출력은PCD,Camera이미지/다른포맷개선은보류. 업무ACK는추가요구시범위확정 |
| 배치 실행·저장 범위 | 현재 PIE/runtime 추가·조작·저장위치 우선. 전체배치 명시적복원과 Editor영구맵저장은단계를구분 |
| 대상 consumer 프로젝트·DTCore 승인 revision | 충돌 없는 drop-in 검증과 재현 가능한 릴리스 |
| 장비 SDK/calibration/golden data | 제조사 동일성의 검증 근거 |

현재 저장소에서 해결 가능한 배치/UI 작업을 진행하면서 필요한 입력을 단계별로 확정한다. 전체 장비 배치 저장 방식·대규모 지원 조건 등 영향이 큰 선택은 해당 구현 직전에 구체화한다.

## 5. 최종 완료의 정의

1. 승인된 지원 구성에서 여러 센서를 설치·이동·복원하고, 무응답/상태 잔류 없이 조작할 수 있다.
2. 시나리오 실행마다 무엇을 측정·전송·소비했는지 데이터와 증거로 확인할 수 있으며, 관찰 fallback이나 부분 실패를 성공으로 오인하지 않는다.
3. 센서·카메라·공통 UI가 DTCore 의존 플러그인으로 배포되고 장비 추가가 데이터/extension 중심으로 이루어진다.
4. 깨끗한 다른 프로젝트에서 Bootstrap 하나로 검증 가능하며, 기존 GameMode/Main/Widget/Subscription/사용자 Actor를 침범하지 않는다.
5. 소스 빌드만이 아니라 자산/cook/실 RHI/외부 소비자/수명/성능/이식 gate를 통과한 범위만 release support로 공개한다.
