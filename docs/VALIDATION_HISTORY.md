# 검증 이력 보관

이 문서는 2026-10-11 문서 재구성 때 README·IMPROVEMENTS·ROADMAP에 날짜별로 섞여 있던 **과거 검증 기록을 원문 그대로 옮긴 보관본**이다. 현재 보장·규칙이 아니며, 현재 상태는 [README](../README.md)와 [IMPROVEMENTS](IMPROVEMENTS.md)를 따른다. 문장 안의 SHA·수치·보고서 경로는 당시 조건이다.

[작업 지침](../AGENTS.md) · [현재 기능](../README.md) · [백로그](IMPROVEMENTS.md) · [로드맵](ROADMAP.md)

새 검증 기록은 아래 "README에서 옮긴 이력" 절보다 앞에 날짜 절로 추가한다. 옮긴 원문 문장은 수정하지 않는다.

## 2026-10-15 PCD-OPT — 전송률 자동 조절·혼잡 대기·PIE hitch 조사

- 사용자2026-10-15: 적용 대상은 운영 서버 PC의 Editor PIE이며 화면 전체/Slab 멈춤이다. 로그/시나리오/사양 제공 없이 근거 있는 추정 수정을 승인했다. 폴더 `C:\Unreal Projects\ma0t10_dt`, 기존 `perf/pcd-transport-opt`, 기준291b4f9 / DTCore b2504d1. 새 branch/worktree·push/PR/merge 없음.
- `7275b3d`: 기본 on/off PCD admission 조절, 균등 생략 카운터·진단·집계 로그·상태바, Slab 기존 출력 실패 경로, 소켓/이벤트 대기와 반복 로그 제한. `2d18939`: .05→.06처럼 작은 설정 변경도 여유를 즉시 회복하는 회귀. `8b8114a`는 사전 hitch/소켓 계측, `2b9de02`는 실제 affinity 유지·종료 경합 보완, `2b1388a`는 실제 viewport/글자 배율 기록 및 크기 assertion이다. 기존 wire/golden·Blueprint 시그니처·SaveGame 버전·Raw20프레임/64MiB/128MiB 상한은 유지했다.
- 1280×720·글자100%의 동일 계측 binary에서 기존 대기/admission 복원과 조절을 비교했다. 제한18.5MB/s·receipt RTT10ms .05초5분: 기존 중단8/8.997Hz·9.485MB/s → 조절 중단0/17.303Hz·18.345MB/s/최대 receipt 공백96ms. .06/.1은 양쪽 모두 생략0/16.667Hz·17.664MB/s,10Hz·10.598MB/s. Artemis .05초5분 양쪽20Hz·21.196MB/s/생략0, .06/.1도 생략0. 이 비교는 순수291b4f9 binary나1080p 증거와 다르다.
- 새 PIE helper가 해상도 환경변수 없이1280×720을 사용한 것을 직접 창 관찰로 확인해 실행기를 수정했다. **실제1920×1080 재확인:** .05초5분 중단/과부하/hitch0,17.353Hz·18.398MB/s,최대 receipt 공백95ms,생략 누계818. 4논리 CPU·시나리오 변경12회5분도 중단/과부하/hitch0,16.957Hz·17.985MB/s,공백218ms,생략802. 실제 viewport와 글자100%를 result.json에서 확인했다.
- 64KiB 실제 송신 소켓·6초/10초 read blackout·시나리오 변경12회·미리보기의1080p 시험: 2논리 CPU hitch0/max124.70ms·과부하0, 4논리 CPU+호환 JSON hitch0/max93.88ms·과부하0. 블랙아웃의6초 receipt 공백은1초 링크 기준과 구분한다. 저코어 acquisition은 낮아져20Hz 처리량 증거로 세지 않는다. 기존1280 폴링2CPU에서326ms(GC313ms) hitch1회, 재시험0회였으므로 운영3초 주기 원인으로 확정하지 않았다. would-block은 같은1280/2CPU 비교에서562만→10,198회; 전체 CPU는1.757→1.767개 상당으로 비슷했고 wall frame5064→5418이었다.
- mutex 내부 socket I/O0, GT mutex 대기 sub-ms, 주기 변경 중 StopWorker/CancelRun0을 기록했다. Native API 경로의 EditorProperty/SettingsUi/UiSave0을 실제 Editor Details/직접 UI 검증으로 확대하지 않는다. 데이터 필수는 잠금4회 거부·실패 후8회 적용/hitch0이며 생략을 성공 처리하지 않았다. 관찰 실행은 매초 Slab 시간 진행을 확인했다.
- 관련 D3D12 회귀45 Success / opt-in skip8 / 실패0(`Saved/Reports/verify/20261015T013234510Z/manifest.json`), 새 조절6·재개7·SlabReliability·WireGolden 포함. Node11, `run-faults.ps1` 기존8+bandwidth9개 Success. Native ledger Artemis1200/제한 링크1044 accepted 모두 receipt·소비자 수신. socket-only NullRHI와 실제 RHI를 구분했다. 최종 Editor/Game Development/Shipping 빌드와 quick8(`20261015T020411650Z`) 통과.
- **직접 UI 보류:** computer-use로 PIE를 관찰했으나 Windows 보안 UnrealGame 네트워크 권한 팝업 때문에 입력0. 보안 권한 UI 조작을 금지한 스킬 규칙에 따라 사용자 수동 처리/보류를 요청했다. `Saved/Reports/PCD-OPT/manual/phase2/CHECKLIST.md`, blocked-security-0.png. 자동 API12회는 마우스 통과가 아니다.
- `verify.ps1 -Level perf -BaseCommit 291b4f9` Success(`Saved/Reports/verify/20261015T020419113Z/{manifest.json,perf/manifest.json}`): candidate2b1388a /1Camera+1LiDAR/실제표시1920×1080 D3D12/10초+60초/ABAB×3/12회유효. 중앙값 FPS59.9905→59.9847(−0.0096%),engine p95 16.6669→16.6669ms(0%),GT6.2053→6.3153ms(+1.7716%); 각2% gate 통과. 테스트 종료 후 archive 소유 폴더 정리, Editor0. 문서 커밋은 같은 runtime code의 결과다.
- Artemis는 세션 소유 시작 후 정상stop·AMQ221002·Java 종료·61616 해제를 확인했다. 기존 batch NO_ARTEMIS_OOME_DUMP stderr는 cmd 경유 정상stop으로 처리했고 인스턴스 파일을 편집하지 않았다. 설정 modification time 변화0. 자격 증명/body 기록 없음. 보호300중299 hash불변, Claude 전용 메모1 외부 변경은 보존했다. DTCore/gitlink clean, Config/Game.ini·PixelStreaming·운영맵·빈 값 SensorTransport.ini(550bytes) 보존, 개인SaveGame9/9복원·추가0, Editor UI설정 복원·61616/61618 listener0(`phase2/final-audit.json`).
- 크기 축소는 제안만: miss point 이미 제외, XYZ16bit1cm는18.2%/약.867MB 산술. 메모리 합성자료 zlib1/6은57.4%·14.48ms /49.8%·21.34ms, 압축 불가능 대조군100.03%·19~20ms였다. 실제 장면·서버 CPU·소비자 비용은 미측정이며 wire에는 구현하지 않았다. 상세 `Saved/Reports/PCD-OPT/RESULT.md`.

## 2026-10-14 UXR-12 CLI 시각 다듬기 — 직접 마우스·release·성능 인계

- 기준 폴더 `C:\Unreal Projects\ma0t10_dt`, 브랜치 `codex/post-program-followups`, 시작 `fe20bcbd7c3668f721a285309d51361e2bb1c6f8`, Runtime `399ea46`, DTCore `b2504d1dfea5659f832ff0d750515edee146a28e`. `7b83864`(토큰·레일·머리) → `9c07108`(폼·카드·버튼·관련 기대값) → `399ea46`(배치 저장 동작 묶음). 소스/테스트21개 파일 수정, 이동/새 source/새 자산0. 별도 문서 커밋으로 관리 결과를 기록한다.
- Editor Win64 Development 성공(`Saved/Reports/UXR/UXR-12/build-placement-spacing.log`), 실제 D3D12 RenderOffscreen 관련 회귀 엔진61 Success 중 **58 수행 Success / 조건부 skip3 / 실패·NotRun0**(`spacing-regression-1920x1080/index.json`). UXR10 복귀/resize, UXR11 프로필 재적용 안내, UXR01~09·UI02·SensorMonitor·SensorWorkspace·SensorUi·SensorControl·Slab.UI를 포함한다. skip은 `SensorWorkspace.Runtime`(MA0T10_WORKSPACE_RHI), `UI02.LabelPresentationRhi`(-Ui02Render), `UXR07.FloatingTitleDragRhi`(MA0T10_INPUT_ACCEPTANCE_RHI)다. Windows 커서를 움직이는 `UXR11.NativeCursorCaptureRhi`는 동시 QA 마우스 검수 때문에 요청 목록에서 제외했다. 이전 SHA의 통과로 대체하지 않는다.
- WBP7 Entry/NullRHI 메모리 컴파일 exit0(`widgets/widgets.log`), 자산 저장0. quick8·layer/allowlist/신규/stale0/0/0/0·DTCore guard·용어103 상수와 실제 Slate inventory 통과. quick의 첫 실행은 `core.autocrlf=true` 줄바꿈 경고가 PowerShell 오류로 취급되어 diff-check 단계에서 중단됐다. UTF-8을 유지하며 변경 파일만 CRLF로 맞춘 뒤 `quick-verified.log`에서8단계 통과했다. reflected header 계약 원문 비교·strict UTF-8/U+FFFD0·보호 파일484/484·개인 SaveGame9/9 복원을 `summary.json`으로 대조했다.
- 1920×1080·1280×720 ×100/125/150% ×배치/미리보기/시나리오/PCD/결과/도구/접힌 상태/자유 배치/센서 설정의 before54/after54 자동 렌더. `after-spacing/`은 커밋본 Runtime의 실제 RHI 렌더이며 3px Slate border 포함 PNG(1926×1086/1286×726)를 원문 보존한다. `comparison/`은 client viewport를 crop한 전후 세부 비교5장이다. 레일 라벨의 실제 할당 폭과 설정 컨트롤의 logical 높이32/40/48 검사를 같은 capture fixture에서 통과했다. 개인 UI 상태가 아닌 테스트 fixture가 명시 초기 배치를 사용한다. 센서는 Camera1(정지)+LiDAR1(측정), 기존 테스트맵과 메모리 관찰 시나리오이며 성능용1+1 조건이 아니다.
- 초기 스타일 build에서 SCheckBox의 지원하지 않는 VAlign, 다음 build에서 Slate bracket 오류를 수정했다. 첫 배치 높이 검사는 combo/숫자 입력의2~3px 차이를 실패로 기록한 뒤 공통 고정 높이와 profile slot fill로 수정했다(`iteration2-layout-failure/`). 첫 회귀는 새 레일 문구/세그먼트 구조를 이전 기대값으로 검사한3건이 실패했다(`final-regression-1920x1080/`). `ToolbarRoutes`·`OwnedScreenVocabulary`의 레일 문구, `SingleOutputEditorAndDefaults`의 SCheckBox 수8→11(기존 출력/정책8+점검 대상3)만 바꿨으며 다른 상태/계약 검사는 유지했다. 최종 실패0이다. UE5.3 offscreen MainFrame/SwapChain handled ensure는 baseline과 after 로그에 남으며 무오류 엔진 시작으로 보고하지 않는다.
- 기존 운영 SensorTestMap 변경과 미추적 개인 Config/Game.ini·Samples/PixelStreaming·Claude 문서를 보존했다. DTCore tracked worktree/staged·gitlink, PixelStreaming 의존/uproject, 전체 Content 불변. 운영맵 열기/저장·새 .uasset/.umap/글꼴·검수/Claude 복사본 변경·push/PR/merge/worktree/submodule update 없음. CLI가 생성한 Editor/PIE는 종료했고 개인 SaveGame은 원본 hash로 복원했다.
- 직접 Computer Use0건, 변경 화면 마우스는 앱 Codex로 인계(`manual/CHECKLIST.md`). 전체 release 및 성능 측정은 사용자 요청에 따라 미수행이다. 모든 화면 작업 종료 후 Claude가 `Scripts/verify.ps1 -Level release -BaseCommit fe20bcb`, `-Level perf -BaseCommit fe20bcb`로 통합·ABAB 상대2% gate를 실행한다. 캡처 fixture의 기존 perf 로그를 성능 수용 증거로 사용하지 않는다. 진행률70%, 전체 수용 완료 아님. 상세 `Saved/Reports/UXR/UXR-12/RESULT.md`.

## 2026-10-13 UXR-10 CLI 구현·검증 — 마우스·release·성능 보류

- 기준 `codex/post-program-followups`, 시작 `b4c562d4ecc1192658d8609561c1442930b6c742`, 구현 `70ecc57d10db352ff6981fc3efbec4addae85a67`, DTCore `b2504d1dfea5659f832ff0d750515edee146a28e`. 소스/테스트 16파일 수정·추가(이동0), 관리 문서는 별도 커밋이다. 기존 운영맵 변경과 미추적 개인 설정/PixelStreaming/Claude 문서를 보존했다.
- UE5.3 Editor Win64 Development 성공(`build-final.log`), 실제 D3D12 신규 `MA0T10.UXR10.BackPathsPreserveState`, `ResizePersistenceAndRenderRhi` **2 Success/실패·skip0**. 실제 Slate pointer routing의 최소540×280(글자150%)/설정 최대900×800·viewport 내부·release capture 해제·Workspace v1 저장/복원·접기/모드 전환 복원, 부모 경로·도구 복귀·레일 토글·조작 중 차단·미적용 설정/센서 pose/revision/측정 상태 불변을 확인했다. synthetic Slate 이벤트이며 직접 마우스 수용이 아니다.
- 기존 UXR01~09/SensorUi/SensorWorkspace/SensorControl/Slab.UI/UI02 관련 그룹 최종 **49 Success/3 skip/실패·NotRun0**(엔진 Success52 중 skip3 별도). skip은 `MA0T10.SensorWorkspace.Runtime`, `MA0T10.UI02.LabelPresentationRhi`, `MA0T10.UXR07.FloatingTitleDragRhi`의 기존 opt-in 조건이다. 첫 관련 회귀는46 Success/2 skip/실패1(북마크 저장 `UXR02.SettingsFreeWorkflow`)이었다. 단독 재실행과 최종 순차 실행은 통과했고 초기 실패를 삭제하지 않았다. 최초 렌더 실행의 인자 인용 오류와 최초 구현 빌드의 누락 include 오류도 보존했다. D3D12 RenderOffscreen 시작 시 UE5.3 `SwapChain4` handled ensure가 출력됐지만 테스트/렌더는 완료했으며 이를 무오류 엔진 시작으로 보고하지 않는다.
- Entry 시작, 자동화는 `-RenderOffscreen`·`Start-Process -WindowStyle Hidden`. `/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap`은 읽기/PIE만 사용했다. 실제 client1920×1080/window DPI1.0, 기준/변경 각24장(글자100/125/150%)과 변경 하위 경로9장. raw PNG는 Slate border를 포함한1926×1086, `before-after-1920x1080.png`는 viewport1920×1080 crop 두 장을 나란히 둔 비교이며 원본을 보존한다.
- WBP7 메모리 컴파일 통과/저장0. `verify.ps1 -Level quick -BaseCommit b4c562d` 8단계 Success(`20261013T144325482Z/manifest.json`), layer 위반/allowlist/신규/stale **0/0/0/0**, production 파일249개 유지. `validate_sensor_ui_vocabulary`는 새101개 상수와 이번 실행의 visible-text inventory 모두 통과했다. 함수/Blueprint enum·UPROPERTY·저장 슬롯/버전·wire 계약 변경 없음. Tick/timer/invalidation root·추가 측정/캡처/readback 경로 추가0, 새 처리는 이벤트 기반이다.
- 보호 hash816개 중815개 동일. 나머지1개는 빌드가 갱신한 Git-ignored DTCore UHT `Timestamp`이며 소스·자산 변경이 아니다. DTCore tracked worktree/staged clean/gitlink 고정, 전체 Content·Config/Game.ini·Samples/PixelStreaming·PixelStreaming Build.cs/uproject 불변. `.umap`/`.uasset`/신규 Content 변경0, 개인 SaveGame9개 원본 hash 복원. 검수 복사본/Claude 복사본 변경·push/PR/merge 없음. CLI가 띄운 Editor/PIE는 종료했고 외부 QA UXR07.Manual 프로세스는 건드리지 않았다.
- 직접 마우스·전체 release·성능 측정은 사용자 요청으로 미수행이다. 체크리스트 `Saved/Reports/UXR/UXR-10/manual/CHECKLIST.md`. 모든 Editor 종료 후 Claude가 `Scripts/verify.ps1 -Level perf -BaseCommit b4c562d`로 QA-02 ABAB gate를 실행한다. 핵심 구현/CLI 검증 완료와 전체 수용을 구분하여 UXR-10 진행률70%를 유지한다. 상세 `Saved/Reports/UXR/UXR-10/RESULT.md`, `summary.json`, `protected-and-save-restoration.json`, `focused-final/`, `regression-final/`, `before/`, `after/`.

## 2026-10-13 PERF-01 사전 확인 — Broker 연결 불가로 측정 전 중단

- 기준 `codex/post-program-followups`, HEAD `807f512749deb735a7814199022dd60804cb1b82`, 비교 대상 `fda25e05bd0a1435d80b4eff0a9a121d73f08b9a`, 두 gitlink 모두 DTCore `b2504d1dfea5659f832ff0d750515edee146a28e`. 시작 상태의 운영맵 변경과 미추적 Game.ini/PixelStreaming/Claude 전용 문서를 보존했다.
- 환경: NVIDIA RTX 4070 Ti, driver `32.0.16.1088`, desktop 3840×2160. 시작 Editor/Live Coding 프로세스 없음. 로컬 `127.0.0.1:61616` listener 없음, TCP 연결 2초 timeout. 기존 `run_sensor_map_stream_rhi_smoke.ps1`의 실제 receipt·내부/외부 수신 경로를 사용할 수 없어 사용자 요청의 중단 규칙을 적용했다. Broker 서비스 시작/재시작·설정 변경은 하지 않았다.
- 현재 HEAD의 UE 5.3 Editor Development exit0(Target up to date), Game Development exit0(10 actions). `Scripts/verify.ps1 -Level quick -BaseCommit 807f512`: 8단계 Success/실패0, layer 위반0/allowlist0/신규0/stale0. C++ 자동화는 실행하지 않았으며 gate 8개를 자동화 Success 8개로 세지 않는다. manifest `Saved/Reports/verify/20261013T081047440Z/manifest.json`.
- 성능은 전부 미수행: baseline 임시 checkout/빌드, 표시형1920×1080 1+1·2+2 PCD-only,10초+60초,ABAB×3,오프스크린 비교,이분 탐색,Insights. 회귀 유무·47 FPS 원인·표시형2+2≥55 FPS는 미판정이며 PERF-01 진행률0% 유지. UXR-08 비교/근접 이미지와 UI-02 전후 이미지도 미생성, 해당 후속 두 단계 미착수.
- 원본·재개 조건·QA-02 권장 측정 절차: `Saved/Reports/PERF/PERF-01/RESULT.md`, `preflight.json`, build 로그, quick manifest, 보존 검증. 운영맵을 열거나 저장하지 않았고 Editor/PIE 실행·자산 저장·Computer Use·새 checkout/worktree·push/PR/merge는 하지 않았다. 개인 SaveGame9개는 변경하지 않았다.

## 2026-10-13 MAP-01 CLI 구현·검증 — 직접 마우스는 UXR-07 이관

- 최종 소스 `efcd35340dc57bbbf26574da8c71379ed5dd0f0c`, DTCore `b2504d1dfea5659f832ff0d750515edee146a28e`, UE5.3 Win64 Development. 문서 완료 커밋은 소스를 바꾸지 않는다. 변경 25파일(소스18·스크립트/정책2·승인 새 맵2·관리 문서3), 이동0. 조사 문서/Rig/맵 점검/Slab 없음/저장 예약/맵/테스트/문서로 분리했고 커밋 후 unity의 두 식별자 충돌은 별도 수정했다.
- `Scripts/verify.ps1 -Level release -BaseCommit b16f35c`: 14단계 Success. manifest `Saved/Reports/verify/20261012T151852991Z/manifest.json`, Valid=true. D3D12 고유 **242 Success / 기존 opt-in skip20 / 실패·NotRun0**. 별도 feature 중복까지 포함하면266건(246 Success/20skip)이다. UXR-09의 보관된 최종 보고서에서 기존259개 이름별 결과 차이0 + 신규3개: `MA0T10.MAP01.PortableSetupFlow`, `PortableNoSlabFlow`, `OwnershipDuplicateLateSlab`. 오래된 검증을 현재 통과로 계산하지 않았다. 현재 소스 전 파일 hash는 최종 manifest와 일치한다.
- 집중5 Success: 위3개 + `MA0T10.SensorControl.MapApplyQueue` + `MA0T10.UXR07.OwnedScreenVocabulary`. 새 맵의 추가·선택·조작·비동기 위치 파일 저장/복원·PCD 점검/결과 접근·시나리오 주입/진행·card 연결·EndPlay 생성 helper 0잔존/기존 Actor 보존, 중복 Rig 거절·기존 구성 재사용·복수 Slab/ReceiverId/Track 경고·늦은 Slab·명시 PCD 대상/3D 선택 보존을 확인했다. MapApplyQueue는 보호 맵을 로드/저장하지 않는 메모리 canonical fixture로 바꿨으며 기존 교체/중복 방지 검증을 유지했다.
- Editor 및 Game Development 통과. 마지막 커밋 상태에서 Game unity build도 재확인했다. layer 위반/allowlist/신규/stale **0/0/0/0**, Runtime production247파일. WBP7 읽기/메모리 컴파일·금지어 검사 통과. 새 화면은 `UDxWidgetSubsystem`을 이용하고 Workspace role/저장 schema는 바꾸지 않았다. 정확한 설정 식별자 때문에 `map-check`만 진단 allowlist로 추가하고 실제 Slate 문구를 기록했다.
- D3D12 RenderOffscreen 자동 렌더: 두 맵×실제 client1280×720/1920×1080×글자100/150%×기본/맵 점검 =16 PNG. `Saved/Reports/MAP/MAP-01/Portable*map-check.png`; PNG에는 각 변3px Slate 창 테두리가 포함된다. 맵 점검은 ✓/⚠ 혼합, 150%는 세로 스크롤로 전체 항목에 접근한다. 실제 마우스·OS DPI 수용은 수행하지 않았다.
- 성능: 같은 SlabScenarioValidationMap·D3D12·실제1920×1080·Camera D4551+LiDAR MLX80_NATIVE1 FullSpec 측정·같은 backend/출력/UI·10초준비+30초. baseline `b16f35c` → Rig로 기존 구성 재사용한 `efcd353`. callback FPS60.0034→59.9998(**−0.006%**), callback frame p9520.9281→20.7630ms(**−0.789%**), engine frame p9516.6669→16.6669ms(0%), game thread평균5.9012→5.2673ms(**−10.741%**). 2%gate 통과이며 frame cap/pacing을 측정 처리량과 혼동하지 않는다. `performance-comparison.json`.
- 보호297/297 hash 불변: 운영 SensorTestMap 사용자 수정·Game.ini·Samples/PixelStreaming·Build.cs/uproject. DTCore guard의 고정 gitlink/worktree/index 불변. 승인된 새 .umap2개 외 자산 생성/저장0, 플러그인/모듈0, Claude 파일·push/PR/merge/worktree/submodule update 없음. 모든 자신이 띄운 Editor/PIE 정상 종료, 시작 시 백업한 개인 SaveGame9/9 byte 복원. 증거 `protected-before.json`, `save-restoration.json`, `summary.json`.
- 제한: DTCore 현재 업무 서버 연결/구독 readiness의 공개 조회 API 부재는 확인 불가 경고와 변경 후보로 기록했다. 새 맵의 DT_DxLevel/cook 등록과 실제 외부 업무 수신은 자동 변경/검증하지 않는다. D3D12 RenderOffscreen 시작 전(frame0)에 `WindowsD3D12Viewport.cpp:473 SwapChain4.GetReference()` handled ensure가 실행마다1건 있다. 실제 RHI 테스트와 렌더는 통과했지만 engine 로그가 무오류였다는 뜻은 아니다. 마우스10단계 체크리스트와 UXR-07 런북을 추가했고 MAP-01은90%, 개편 합계는 독립 검토 전87%를 유지한다. 초기 빌드 오류 및 Git EOL 경고로 중단된 진입점은 해결 후 현재 검증을 다시 수행했다. 맵 생성 재실행의 예상 거절(exit−1)을 확인했고 새 맵2개 hash는 그대로다(NullRHI는 이 저장 거절 검사에만 사용).

## 2026-10-13 MAP-01 맵 독립성 조사

- 시작: `codex/architecture-skeleton`, `b16f35c425c440e32e728e1b20258ba8b079b0d2`, origin UE-DT-Project, DTCore `b2504d1`. 기존 SensorTestMap 수정 및 보호된 미추적 파일을 보존한다.
- Source/Config/Scripts 전수 조사 212개 위치와 영향·조치: `Saved/Reports/MAP/MAP-01/hardcoding.md`. Runtime 측정은 맵 경로를 요구하지 않는다. 기본/cook 맵·공통 자산·목적별 테스트 Rig/태그는 근거 있는 예외로 유지한다. 첫 Coordinator/외부 Source 탐색은 기존 호환 경로이며 새 Rig는 단일 구성 확인과 명시 연결로 제한한다.
- SensorTestMap 저장 예약은 고정 경로·영구 태그·EndPIE 자동 저장 전제이므로 다른 맵에서는 UI와 직접 API 모두 거절한다. 신규 맵의 Slab·TrackReference·배경은 사용자 소유다. DTCore 소스/gitlink, 개인 설정, 운영맵 및 PixelStreaming 변경은 범위 밖이다.
- 직접 마우스 확인은 요청된 CLI 예외에 따라 UXR-07로 이관한다. 자동화·렌더·성능·완료 결과는 후속 MAP-01 검증 기록으로 남긴다.

## 2026-10-12 UXR-08 산화 스케일 보완 — 승인된 새 자산 1개

- 기준 branch `codex/architecture-skeleton`, HEAD `767d52fcc7711380446e459c17e648bf0d89a63b`, DTCore `b2504d1dfea5659f832ff0d750515edee146a28e`. 이 HEAD와 요청 범위의 변경으로 빌드·성능·cook을 검증했고, 최종 소스/테스트 커밋 `bbecc00`에서 관련 회귀를 다시 확인했다. 최종 소스/자산 SHA256·커밋 목록·명령은 `Saved/Reports/UXR/UXR-08/oxide/validation-manifest.json`, 상세 결과는 같은 폴더의 `RESULT.md`에 있다.
- `Scripts/setup_slab_oxide_scale_material.py`: 새 `/Game/MA0T10/Slab/Materials/M_SlabSurfaceOxideScale`만 생성. 저장 전 단일 입력 핀 이름 오류 시도 1건은 실패로 보관(`create-material.log`); 수정 후 생성 성공(`create-material-final.log`). Value Noise의 격자 경계를 렌더에서 확인해 새 자산만 GradientALU/scale0.17/levels1로 보완했다(`refine-new-material.log`). 생성 스크립트는 최종 그래프를 재현하며 기존 대상이면 생성/저장 전에 중단한다. 재생성하지 않고 guard AST 앞부분만 실행해 기존 대상 거절을 확인했다(`widgets.log`). 기존 자산·운영맵 저장 없음.
- `run.ps1 -Build`, UE5.3 `Build.bat ma0t10_dt Win64 Development ... -WaitMutex -NoHotReloadFromIDE`: Editor/Game Development 성공. 최초 렌더 fixture의 `TSharedRef.ToSharedRef` 컴파일 오류는 `build-editor.log`, 수정 후 통과는 `build-verified.log`/`build-game-final.log`. `verify.ps1 -Level quick -BaseCommit 767d52f` 8단계 통과. 초기 PowerShell이 Git CRLF 경고를 오류로 처리한 시도와 rg PATH 미설정 V2 실패는 최종 통과와 구분한다. 설치된 VS Code rg를 프로세스 PATH에만 추가했고 기존 파일의 CRLF 형식을 맞췄다. layer0→0·guard·V2 assets 통과.
- `run.ps1 -Group 'MA0T10.UXR08+MA0T10.Slab+MA0T10.Architecture.Slab' -Name focused-verified`: D3D12 SM6 **40 실제 Success/6 조건부 skip/실패0**. skip은 SlabAnalysis.CameraGpuIsolationRhi, SlabReliability.RepeatedRuntime/TcpFaultRuntime, SlabScenario.PortableMapAndLatePlayer/ProductionLifecycle/ProductionRuntime이며 환경/독립 Broker 등의 opt-in 조건은 이번에 설정하지 않았다. 전체 MA0T10 회귀는 미수행. `focused-verified-classified.json`/`focused-verified/index.json`에 이름별 상태를 보관한다.
- 새 자산 로드·MID 부모·3회 전환/복원·같은 MID 재사용·원래 MID의 추가 Metallic 값 보존·사용자 슬롯 거절·명시 광학 값/복원 통과. 실제 CPU/GPU 센서 회귀 7단계씩에서 기본/link-off/산화 외형/복원 intensity와 PCD bytes 정확히 동일(동일 FrameId/UTC envelope). 산화 link-on CPU61945.47→37167.23 / GPU62546.36→37527.77 및 optional 결과 JSON `oxidation-scale`/반사율0.12 기록을 확인했다. BaseColor 픽셀은 기본/복원과 정확히 동일; 과거 기준선 phase0 PNG와도 CPU/GPU SHA256 동일. FinalColor/JPEG bitwise·실소재 광학 보정은 이 시험 범위가 아니다. `sensor-regression/`에 증거를 보관한다.
- 940nm 반사율 `0.12`는 기존 코드의 가정값을 유지했다(냉각 강판0.45보다 낮은 어두운 산화막 표현 목적). 실측 데이터/특정 강종·온도·두께 보정값이나 인증값을 사용했다는 의미가 아니다. gain·검출·양면·포화 정책은 유지한다.
- in-memory scene에서 기본/고온/산화/냉각/무광을 같은 시점·조명으로 D3D12 FinalColorLDR 동시 캡처(2560×1440), 산화 근접2장(1200×900), BaseColor1장. contact sheet는 원본 동시 렌더의 동일 영역을 자르고 라벨만 붙였다. 같은 폴더의 `presets-five-*.png`, `oxide-close-*.png`와 `contact_sheet.py`로 재현한다. 자동 렌더이며 마우스 검수가 아니다.
- shader statistics: UE5.3 D3D12 SM6 `MaterialEditingLibrary::GetStatistics`의 최대 pixel instructions **253→379(+126)**, vertex **131→131**, pixel texture samples/samplers **3→3**, 재질 texture dependencies **0→0**. 단일 계산 노이즈를 모든 출력에서 공유한다. 실제 기본 전후: FPS47.603086→47.537988(-0.137%), engine frame p95 29.682800→29.630397ms(-0.177%), callback wall p95 33.920899→33.671498ms(-0.735%), game thread 평균6.786248→6.711511ms(-1.101%). 같은 최종 바이너리의 기본↔산화: FPS47.537988→47.351931(-0.391%), engine p95 +0.119%, callback p95 +0.308%, game thread -0.010%, 전체 GPU 평균7.250025→7.245054ms(-0.069%). 두 비교 모두 2% gate 통과. 조건: 실제 client1920×1080/font100, `SlabScenarioValidationMap`, 시나리오 진행/모니터+진행+보관 UI, D455+MLX80/FullSpec/GPU, 관찰 출력(송신·파일 저장 없음), 10초 준비+30초. 가상 offscreen 화면의 callback pacing과 engine delta/GGameThreadTime/GGPUFrameTime을 구분하며 GPU 값은 개별 재질의 격리 측정이 아니다. 성능 기준선 첫 실행은 해상도 변수 오지정으로 실패했고 1920 실행만 비교에 사용했다.
- WBP7 메모리 compile/저장0·새 재질 read-only graph inspection 통과(`widgets.log`, `asset-inspection.json`). `UnrealEditor-Cmd ... -run=cook -targetplatform=Windows -map=/Engine/Maps/Entry -NullRHI -OutputDir=.../oxide/cooked-normal`: **3,477 packages, 0 errors/0 warnings**, `DirectoriesToAlwaysCook` 로딩 로그·새 재질의 .uasset/.uexp·Game SM5/SM6 셰이더·AssetRegistry 포함을 확인했다(`cook-normal.log`). 기본/간접 참조의 SensorTestMap·TestMap도 포함된 cook이며 stage/pak/package는 하지 않았다. 앞선 `-SkipSoftReferences -NoDefaultMaps` 축소 cook3회는 명령이 성공해도 기대 자산이 없어 검증 실패로 분리한다(`cook.log`, `cook-final.log`, `cook-verified.log`). 중간 DeveloperToolSettings 섹션 시도에서는 실제 CDO 배열이 비어 있었고, 최종 섹션은 실제 클래스가 읽는 legacy `/Script/UnrealEd.ProjectPackagingSettings`로 확인했다(`cook-settings-old-section.log`). NullRHI는 cook/메모리 compile에만 사용하고 렌더/센서/성능은 실제 D3D12로 검사했다.
- 보호477/477 SHA256 불변·DTCore guard/gitlink/worktree clean·개인 SaveGame9/9 복원·Editor/PIE 정상 종료. 시작 시 사용자가 수정한 `SensorTestMap.umap`, 개인 `Config/Game.ini`, PixelStreaming, Claude 전용 문서, 기존 Content는 변경/stage하지 않았다. 새 .uasset은 산화 재질1개뿐이다. 빈 `Source/m7at10_dt/` 삭제는 Claude가 2026-10-12 사용자 승인으로 완료한 상태를 문서에 반영했으며 Codex가 삭제하지 않았다. 직접 Computer Use는 사용자 요청대로 미수행, 독립 검토/수용은 보류이며 UXR-08 진행률90%를 유지한다.

## 2026-10-12 UXR-07 CLI 준비 — 화면 조작과 사용자 수용은 대기

- 기준 HEAD `7a99a45175295fe5e1ad7f3986eb413a15867083`, branch `codex/architecture-skeleton`, DTCore `b2504d1dfea5659f832ff0d750515edee146a28e`. 검증은 이 HEAD와 UXR-07 변경으로 수행했고 소스 커밋 `8ab6a3a`와 동일한 최종 Source SHA256은 `Saved/Reports/UXR/UXR-07/verified-source-hashes.json`에 보관한다. 커밋 이후 전체 release를 검증했다고 소급 설명하지 않는다.
- `Scripts/verify.ps1 -Level release -Tests MA0T10.UXR -BaseCommit 7a99a45`: `Saved/Reports/verify/20261012T094125797Z/manifest.json` Valid. Editor Development, diff/guard/layer/payload/fixture/opt-in/문구 상수 gate, D3D12 UXR 집중18 Success, 전체 MA0T10 **234 실제 Success/20 조건부 skip/실패0**, WBP7 메모리 컴파일(자산 저장 없음), 같은 실행에서 새로 만든 Slate 문구 inventory gate 통과. 집중18은 전체234에 포함되며 별도 합산하지 않는다.
- 이름별 기준선은 마지막 이전 release `20261012T085458983Z`, source HEAD `90a07588a1feefba9434eb18a0ecf78349b41c0e`다. **253개 기존 이름 상태 변화0·누락0 + 신규 `MA0T10.UXR07.OwnedScreenVocabulary` Success**. 날짜가 다른 기준선을 현재 HEAD 시험처럼 설명하지 않는다. skip 이름·원인은 `Saved/Reports/UXR/UXR-07/skipped-tests.json`, 비교 원본은 `test-comparison.json`/`test-comparison-summary.json`이다.
- 문구: 표시 상수87개, 기본29영역/허용6영역의 실제 native Slate 라벨·힌트·툴팁. 발견18건은 반복된 공통 숨김 툴팁11영역, 결과 재시도1영역, 서버 라벨6개다. 원본 `found-violations.json`과 최종 `vocabulary-final.json`/`visible-text.json` 참조. 별도로 파일 저장 주기 설명과 옛 캡처/내보내기 화면 이름을 정리하고, 동적 이유의 모든 정책 금지어/대문자 형태 및 실제 데이터 패널 메시지 전달을 검사했다. 편집 값·임의 ID/주소/경로·Designer/레거시·미실행 오류 조합은 이 fixture의 검사 대상 밖이다.
- 초기 검사에서 UMG wrapper의 마지막 참조를 버려 NativeDestruct가 소유 등록을 해제하고 레거시 텍스트를 읽는 문제가 있었다. wrapper를 유지하고 실제 소유 툴바·작업 버튼의 positive check를 추가한 뒤 전체 release를 다시 통과했다. 기대값을 완화하지 않았고 기존 테스트 기대값 수정0이다. PowerShell native warning을 오류로 처리한 첫 release의 CRLF 경고 실패도 원본 manifest로 보존했다. 스크립트 negative control은 기본 STOMP 주입 exit1/진단 허용 주입 exit0이다.
- 자동 렌더: UXR02/03/04/05/06/08 PresentationRhi를 두 해상도에서 실행, **12 Success/실패·skip0, 최종94 PNG**. 실제 client1280×720/1920×1080, 글자100/150%, windowDPI1.0. PNG는 창 경계6px을 포함한다. 기존 UXR02 fixture의 고정 출력 경로에서 새 배치6장을 UXR07 폴더에 복사했으며, UXR04/08의 같은 표면 fixture는 같은 이름을 최종 캡처로 갱신한다. `captures.json`은 이름·크기·SHA256을 기록한다. 주요 서버 화면4조합·배치·설정·결과 이미지를 검토했으며 하단 스크롤/툴팁/물리 DPI의 직접 조작 통과로 세지 않는다. 테스트맵 하단의 별도 진단 fixture 문구와 편집 주소 값은 소유 기본 라벨 검사 범위 밖이다.
- 성능: 같은 커밋된 SlabScenarioValidationMap/D3D12/실제1920×1080/font100/전송 탭 열림, 1D455 Camera+1MLX80 Native GPU LiDAR FullSpec 측정, 시나리오·출력·파일 저장 없음,10초 준비+30초. FPS **47.472844→47.738787(+0.560%)**, engine frame p95 **29.619098→29.568899ms(-0.169%)**, callback wall p95 **33.362299→33.675797ms(+0.940%)**, game thread 평균 **6.356517→6.222725ms(-2.105%)**, 2% gate PASS. 0Hz 장비를 제외하지 않으며 로그에 두 장비의 실제 측정이 있다. 가상 디스플레이의 engine pacing/callback wall을 구분하고 절대 p95 20ms/60FPS 목표나 외부 수신을 인증하지 않는다. `performance/comparison.json` 참조.
- 보호:476개 중475개 SHA256 유지. `CLAUDE.md`는 별도로 09:29:05 UTC에 수정되었고 이 작업은 그 파일을 수정·복원·stage하지 않았다(원본/최종 해시 별도 기록). DTCore gitlink/worktree clean, Config/Game.ini, Samples/PixelStreaming, PixelStreaming 모듈 의존/활성, 시작 때 변경돼 있던 운영 SensorTestMap, 전체 Content 자산 불변. 개인 SaveGame9/9 복원. 새 자산/맵 저장, push/PR/merge/worktree/submodule update/서비스 재시작/Broker 설정 변경 없음. 실행한 Editor/PIE 프로세스 정상 종료.
- PixelStreaming 플러그인 활성/Build.cs 의존 유지 및 렌더 로그의 설정 초기화, 해당 로그의 PixelStreaming 오류0을 확인했다. 시그널링/원격 스트리밍은 수행하지 않았고 실제 마우스 PIE 초기화 확인은 런북에 남겼다.
- 수동 런북: `Saved/Reports/UXR/UXR-07/MANUAL_RUNBOOK.md`,55단계/100~130분. UXR-02~08 main/supplement CHECKLIST와 무안내 첫 사용자 흐름, 두 해상도×글자100/125/150%, 시험 장비 삭제·Slab 클릭·명시 광학 연동·수명·개인 값 복원을 합쳤다. 준비용 Python은 문법과 기존 PIE/API 경로를 확인했지만 화면 세션에서는 아직 실행하지 않았다. 서버/실패/빈 장비/재호스팅 fixture가 없는 항목은 환경 보류로 표시하며 사용자 확인까지 UXR-07을 완료 처리하지 않는다.

## 2026-10-12 UXR-02 CLI 마무리와 UXR-03

- 시작: `codex/architecture-skeleton`, HEAD `e9bb72f`, DTCore `b2504d1`. `C:\Unreal Projects\ma0t10_dt`에서만 작업했다. 모니터 부재/WinDisc 1612×816 환경이며 요청에 따라 Computer Use는 수행하지 않았다.
- UXR-02: 기존 상태 칩 변경과 신규 `MA0T10.UXR02.PlacementMarkerIsolationRhi`를 재빌드하고 D3D12 UXR 7개 Success/실패·skip 0을 확인했다. 코드 `bd857a3`, 문서 `75a9f41`. 최종 삭제는 보류(환경: 모니터 없음) — 확인/취소 직접 검수 + 실제 삭제 자동화 검증 완료, 사용자 복귀 후 재검수.
- 통합 중 마커 Camera 픽셀 비교가 실패했다. 셰이더 완료·초기 픽셀 안정 대기를 추가한 `e5e9041` 이후 픽셀 완전 일치가 통과했다. 비교 허용치를 완화하지 않았으며 실제 PCD/CPU/GPU/semantic/ISM/Niagara와 가시/충돌 음성 대조를 유지했다. 이 보완은 테스트 fixture만 변경한다.
- UXR-03 코드 `2c453d2`: 소유 데이터 5탭·출력 편집 단일화·재생 요약·사전 점검/해결 링크·결과/진단 분리 구현(수정 15/추가 2, 이동 0). 검증 시작 HEAD `e5e9041`+작업 소스 17개의 hash가 커밋 소스와 일치함을 확인했다. 모든 실행이 커밋 이후 시작됐다고 주장하지 않는다. 기존 Workspace/UDxWidget/DTCore 전면·입력 차단 경로를 유지한다. 같은 옵션을 새 Subsystem/SaveGame에 복제하지 않고 기존 Slab Actor·재생 패널 상태로 위임한다. 사전 점검의 판단과 상세 형식을 공유하며 접힌 UI는 상세 요약을 만들지 않는다. 명시적 Blueprint 상세 조회는 유지한다. 파일 완료는 표시 polling의 5Hz 제한을 우회하지 않는다.
- Editor Development·layer gate 18/18(신규·stale 0)·DTCore guard·V2 `-RequireAssets`·WBP 7개 메모리 컴파일 통과. 최초 V2 실행은 CLI PATH에 `rg`가 없어 실패했고 설치된 VS Code ripgrep 경로를 프로세스 PATH에 추가한 후 통과했다. 검증 스크립트/시스템 PATH는 변경하지 않았다.
- 집중 자동화 16 Success/실패·skip 0. D3D12 전체 `MA0T10`은 **221 실제 실행 Success / 20 조건부 skip / 실패 0**. 기준선 `Saved/Reports/UXR/UXR-02/automation/index.json`의 238개 이름별 상태가 유지됐다. 추가는 `UXR02.PlacementMarkerIsolationRhi`, `UXR03.SingleOutputEditorAndDefaults`, `UXR03.PreflightDetailBudget` 3개다. skip 목록은 같은 조건부 opt-in 환경을 사용했다.
- 기존 기대값/검사 갱신: `SensorOwnedUiLayoutTests`(파일 탭 표시 index 1→2, legacy enum ordinal 불변), `SensorWorkflowToolbarTests`(진단 독립 탭), `SensorPlacementComparisonTests`(기본 결과 A/B·금지 용어 검사 추가), `SlabRunHistoryTests`(새 결과 필드에서 UUID/출력 검사, 진단에 snapshot/receipt 유지 검사 추가). 최초 전체 실행의 이력 테스트 1건 실패는 옛 필드 위치 기대였으며 실제 저장 실패/재시도·중복 거절·파일 생성 검증은 그대로 유지했다.
- 자동 렌더: 5탭×2해상도×2글자 배율의 20장. 실제 client 1280×720/1920×1080, window DPI 1.0, 글자 100/150%. Slate 창 테두리를 포함한 PNG는 1286×726/1926×1086이다. 본문은 스크롤이며 마우스로 하단 버튼 접근/resize를 확인한 것은 아니다. 최초 전송 진입의 재생 열 활성화는 실제 PIE fixture에서 검사했다. 기본 결과 렌더는 빈 목록이고 A/B·저장 재시도는 별도 자동화 fixture에서 검증했다.
- 성능: 실제 D3D12/1920×1080 client, 검증맵 `SlabScenarioValidationMap`, 1 Camera(D455)+1 LiDAR(MLX80/GPU), FullSpec, 미리보기/배치 기본 UI, 전송·파일 저장 없음, 10초 준비+30초 표본. 기존 계측기 `UXR02.Performance`의 전후 JSON은 `Saved/Reports/UXR/UXR-03/performance/`에 보관한다. `average_fps`/`frame_p95_ms`는 자동화 callback wall pacing이고 game thread는 `GGameThreadTime` cycles다. engine frame p95나 열린 데이터 탭의 전후 수치로 해석하지 않는다.
- 공통 UI 성능 전후: FPS **60.001→60.001**, callback p95 **21.163→20.967ms(-0.926%)**, engine game thread 평균 **6.091→5.634ms(-7.504%)**, 각 1800 표본. 2% 범위 안이며 절대 p95 20ms 목표 통과를 뜻하지 않는다. 열린 데이터 탭의 전후 비교와 물리 화면 수용은 계속 보류다.
- 보류: UXR-03 직접 조작 전체와 데이터 탭을 열어 둔 동일 조건 전후 성능, 물리 화면/DPI 수용. 가상 화면 자동 렌더/공통 성능 측정을 직접 검수로 세지 않았다. 체크리스트는 `Saved/Reports/UXR/UXR-03/manual/CHECKLIST.md`에 있다. UXR-02는 최종 삭제 클릭 1건만 보류다.
- 원본: `Saved/Reports/UXR/UXR-02/cli-closure/`, `Saved/Reports/UXR/UXR-03/RESULT.md`, `validation-manifest.json`, `test-comparison.json`, `skipped-tests.json`, 빌드/자동화/렌더 로그. 보호 파일 **474/474 hash 불변**, 개인 SaveGame/UI 슬롯 **9/9 복원**, 추가 SaveGame 0, Editor/PIE 정상 종료를 확인했다. Broker 재설정·서비스 재시작·DTCore 변경·운영맵/자산 저장·push/PR/merge는 수행하지 않았다.

## README에서 옮긴 이력

### 2026-10-10 단일 폴더·배치 UI 검수 (README 원문)

- 최신 구현은 `ma0t10_dt`에서 직접 빌드·실행한다. 네 별도 checkout 경로와 Git worktree를 정리했고, 이전 자료와 Git 이력은 `Saved/WorkspaceConsolidation`에 보존했다. 현재 Artemis 로그도 이 프로젝트의 보고서 폴더를 사용한다.
- 배치 창 생성/진입·취소된 복원 요청·북마크 선택/이름·늦은 저장 완료를 보완했다. PCD 점검은 exact/wildcard, 다음 시나리오와 수동/현재 출력, backend 연결과 프레임/초 예산을 구분한다.
- Runtime `81b56ba`: UE5.3 Editor Development, 전체 MA0T10 217 Success/실패0 중 실제215·조건부skip2, WBP7개 메모리 컴파일·V2 자산 검사 통과. 사용자 맵 높이 assertion은 원본을 보존하고 커밋본 격리 fixture로 재검증했다.
- 실제1920×1080의2+2 PCD-only 10+60초는 평균59.89FPS·1% low51.13·frame p9516.67ms, 각PCD19.95/19.9667Hz·Camera acquisition29.938/29.954Hz다. 승인2,395건의 내부/UI/외부 누락·비선택Topic·invalid/gap/duplicate/overflow0, 내부/외부e2e p9579/93ms다. 기존 기준선 대비 평균/low 저하는 약0.05/4.13%, frame p95는 같다.
- 두 client 해상도와 네 배율의 렌더 캡처를 확인하고 배치 창 잘림을 보정했다. 실제 Slate/RHI 입력과 물리 마우스는 구분한다. 직접 마우스 검수는 재연결 후에도 Computer Use node kernel 초기화(os error3)가 실패해 미완료다. 최신 결과·스크린샷·제한은 `Saved/Reports/PlacementUiPreflightClosure/FINAL_STATUS.md`다. 이전600초/10회 PCD 증거는 보존했다.

### 2026-10-09 PCD 중심 기본 생성 장비 검증 (README 원문)

- 실제 표시된1920×1080 PIE, 같은 Slab 검증맵·pose·UI에서 JSON stride1/max0를 고정해 정책을 교차 비교했다. 기본 제출 예산2, Camera1280×720/30Hz·Native576×56/20Hz, PCD 전체 검출점·FrameStride1을 유지했다. 2+2 새 정책은10+60초3회 모두 통과: 평균59.99~60FPS·1% low59.17~59.80·p9516.67ms. 기존 전체 JSON 정책의 low29.74~36.02는 실패로 보존한다. 새 정책1+1도60FPS·low59.87로 통과했다.
- 새 정책의 같은 월드·연결30초×10회는11,995개 승인 request의 제출·receipt·내부/UI·외부 수신이 일치했다. 최소 Camera29.9667Hz·LiDAR/PCD19.9667Hz·FPS59.9523·low55.966, 최대frame p9516.669ms다. JSON 작업0, 복귀 추가 제출0, drain 후body/events/active0, PIE 후weak owner16개·Camera global job0을 확인했다.
- 10+600초2+2도 통과: 평균59.99FPS·low59.58·p9516.67ms, 각PCD19.9967/19.9983Hz. 고정 승인 집합23,997건의 외부/내부/UI 누락·clock 오류0, e2e p9580/92ms(내부/외부)다. 비선택 Topic·invalid/gap/duplicate/overflow0이다. JSON 생성이 필요 없을 때만 생략하며 기존 JSON 소비 경로는 유지한다.
- 최종 전체MA0T10은211 Success/실패0 중 실제208·조건부skip3이다. 배치 입력 RHI는 별도 통과(Camera/LiDAR input→GPU end-frame p9534.849/34.753ms, 원규격 preview31.250/62.104ms). Camera marker987개, WBP7개 메모리 compile·자산검사·기존 자산저장0을 확인했다.
- 직접 UI는 Windows 캡처/커서 오류0x80070057/0x80070005로 현재 미완료다. 활성화에 성공했던 조사용 화면의 작업 영역은1440×900이었다. 이전 작업의 직접 조작을 이번 새 정책 UI의 통과로 바꾸지 않는다. 숨긴/offscreen 시험의 낮은 FPS도 보존하며 이 수치를 표시된 PIE 지원으로 확대하지 않는다.
- 재현: `Scripts/run_pcd_demand_acceptance.ps1 -Phase Compare`, `-Phase Repeat`, `-Phase Endurance`. 센서별 기대 수·속도·실행 수·정합성·성능 중 필수 조건이 실패하면 실패 종료한다. 증거와 제한은 `Saved/Reports/PcdDemandPlacementAcceptance/FINAL_STATUS.md`에 있다. DTCore·운영맵·개인 UI 저장값을 보존하고 push는 수행하지 않는다.

### 2026-10-08 출력 신뢰성 검증 (README 원문)

2026-10-08 검증: UE5.3 Editor Development, 최종 D3D12 MA0T10 **222 Success/실패0(실제216·opt-in skip6)**, 별도 실제30초 A/B ProductionRuntime, WBP6개 메모리 compile/자산 검사·저장0. 1+1 전체 출력은30/20Hz·평균59.98FPS·1% low57.86·p9516.67ms. 2+2 PCD 전용은각PCD 약19.9Hz, Camera/LiDAR정보 Topic0건. 2+2 전체 출력은**명시적 제출예산4건/프레임**에서각Camera29.883Hz·각LiDAR/PCD19.917Hz·평균59.76FPS·1% low51.81·p9517.04ms와 승인 request의 receipt/내부/UI/외부 누락0이다. 기본예산2건은Camera큐과부하로실패이며 지원으로표시하지않는다. 실제1920×1080 Camera기즈모·정지유지·LiDAR정밀이동·첫Esc/20Hz복귀, 실제1280×720 PCD재생·실행선택·A/B상세·resize/scroll을 직접 확인했다. 60초2+2를장기/4+4인증으로확대하지않는다. baseline의1%low정의차이와Camera지연p95 증가도 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`에 남겼다.

### 2026-10-09 배치 작업공간 검증 — a78b443 이전 이력

- 최신 D3D12 전체 `MA0T10`은 **209 Success/실패 0, 실제 수행 206·opt-in skip 3**이다. 실제 `topic.scenario` 발행을 준비해 DTCore 수신도 통과했다. skip 중 배치 입력 RHI는 별도 실행으로 통과했고, Geometry 성능·세 출력 반복 opt-in은 이번 PCD-only 범위에서 미수행이다. WBP **7개** 메모리 컴파일·자산 검사 통과, 기존 자산 저장 0이다.
- 배치 API로 만든 1+1·2+2 장비의 실제1920×1080·10초 준비+60초 PCD-only 시험을 통과했다. 최종1+1은 평균60.00FPS·low59.92·p9516.67ms, 승인20Hz·외부수신19.962Hz다. 2+2는 센서별 승인20Hz·외부합산39.925Hz·Camera acquisition30Hz, 평균60.00FPS·1% low59.65·p9516.67ms다. 기본 제출 예산2건/프레임을 유지했고 비선택 Topic 제출은0이다. 이 성능 fixture는 미송신 호환JSON에 기존 stride32/max1024를 적용한다. PCD는 전체 검출점·FrameStride1을 유지하며, 이 결과를 신규 장비의 기본 호환JSON stride1/무제한 조건으로 확대하지 않는다.
- 같은 월드·연결의 **2+2 PCD 10분**은 각 PCD19.993Hz·평균59.98FPS·1% low58.27·p9516.67ms다. 두Camera를모두실행한 최종 **30초 시나리오10회**의 서로 다른 RunUUID에서 총11,938개 PCD request를 제출·receipt·내부/UI·독립 외부 수신과 대조했다. invalid/gap/duplicate/overflow0, 초기 위치 복귀의 추가 제출0, 정리 후 outstanding bytes/pending events/active runs0이다. Slab 행 수와 센서 프레임 수를 같다고 강제하지 않는다.
- **반복 성능은 미완료:** 최종10회는 송수신·수명 시험의 통과이며, 회차별1% low38.42~40.25FPS는45FPS 목표에 미달한다(평균59.26~59.35, p9516.69~16.77ms). 첫Camera가정지했던초기시험11,982건·low36.84~43.96도이력으로보존한다. 반복 fixture의 신규LiDAR는 기본 호환JSON stride1/무제한이며 연속 성능 fixture와 후처리 조건이 다르다. 이 차이와 Slab/preview 비용은 원인 후보이고 인과 분해·동일조건 재시험이 남는다. 실행 종료 후에 생성하는 검증 파일만을 원인으로 단정하지 않는다.
- 직접1280×720에서 장비 추가·즉시 preview·북마크 저장/이동·숫자 입력·Undo/Redo·첫Esc 원규격 복귀·전체 배치 저장을 확인했다. PIE 재시작 후1920×1080에서 전체 배치 변경 미리보기/적용·고유ID/북마크 복원·PCD/결과 진입·150% 배율·접기/drag/resize를 확인했다. 85/125%와 모든 패널의 전체 직접 체크리스트, UI 직접 삭제/복제·전체 DPI는 별도다. 마지막 간결한 PCD 요약의 추가 화면 확인은 Windows 캡처 오류0x80070057로 미완료다.
- 부하가 있던 최초 시험은 기존·신규 장비 모두 약47FPS, 2+2 PCD 약15.7Hz로 실패했다. 사용자가 다른 GPU 작업을 멈춘 후 위 시험은 통과했지만 특정 앱을 원인으로 확정하거나 모든 환경에서 무부하를 보장하지 않는다. 원본 실패와 조용한 환경 재시험을 모두 보존한다.
- 재현: `Scripts/run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -PointCloudOnly -LayoutApiFixture -SensorPairs 2 -ClientWidth 1920 -ClientHeight 1080 -MeasurementSeconds 600`, `Scripts/run_slab_repeat_stability.ps1 -Repeated -LayoutApiFixture -RunCount 10 -ResourceDiagnostics -ReportGroup SensorPlacementWorkspace`. 상세 범위는 `Saved/Reports/SensorPlacementWorkspace/FINAL_STATUS.md`다. DTCore·운영맵·Game.ini·PixelStreaming과 개인 UI 저장값은 보존한다. 4+4·60분 soak·Engine STOMP/WSS 운영 인증·Editor 영구 설치는 포함하지 않는다.

### 2026-10-08 배치 조작 이전 검증 (README 원문)

이전 `7c6ed29` 검증: 30/60/120FPS의 2초 이동·회전 차이 1% 이내, 실제 파일 재로딩·실패 시 보존·20/50개 상한·잠금·시작 snapshot 불변을 확인했다. 당시 전체 회귀는 206개 수행 통과·조건부 skip 6·실패 0이었다. 두 해상도의 북마크·정밀 이동·Undo/Redo·Esc 직접 검증 범위는 `Saved/Reports/SensorPlacementWorkflow/FINAL_STATUS.md`에 보존한다. 후속 새 자세 프레임·A/B 비교의 최신 근거는 아래를 따른다.

### 배치 비교 이전 검증 (README 원문)

이전 배치 비교 검증은 210개 수행 통과·조건부 skip 6·실패 0, WBP 6개 메모리 컴파일은 저장 없이 통과했다. 실제 1280×720·1920×1080 RHI에서 원규격 새 자세 프레임 준비는 Camera 약31ms·LiDAR 약67ms, 입력→viewport GPU end-frame p95는 약34~43ms였다. 정격 세 스트림은 평균59.99FPS·Camera29.983Hz·PCD20Hz, 비교 UI 표시 전후 frame p95는16.67ms였다. 당시 직접 마우스는0x80070005로 미완료였고 이후 SensorOutputReliability에서 추가 직접 검수를 수행했다. 원래 근거는 `Saved/Reports/SensorPlacementComparison/FINAL_STATUS.md`, 최신 범위는 이 문서 상단과 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`를 따른다.

### 2026-10-08 Slab 반복 실행과 결과 조회 검증

이번 자원 수명 보강은 `Saved/Reports/SensorResourceLifecycle`에서 별도로 검증한다. 아래 10회 반복 결과는 이전 소스의 증거다.

- 이번 최종 반복은 실제1920×1080에서 PCD 전용/세 출력 각각30초×30회다. 승인·제출·receipt·내부 실제 MESSAGE·외부 request ID집합 대조 통과: PCD17,989개, 세 출력합계62,979개. 종료 bytes/events/active/retiring0, 결과·종료진단20개, source별 worker/connection 동일. 반복 최소FPS59.82/59.94, 가장 느린1% 평균 low47.80/55.32, frame p95≤16.67ms.10초 준비+60초 변경 전후 각3회도5% 회귀 조건을 통과했다.
- `1d17016` 최종 전체207개 Success 상태(그중15개 경고 포함)에서 명시 opt-in skip6개를 분리해 실제201개 수행으로 기록한다. 이전206개 결과와 원본 실패는 보존한다. 반복시험·실제 topic.scenario Broker 수신은 별도 opt-in 실행에서 통과했다. WBP6개 메모리 컴파일과 자산 검사는 저장0이다. 입력 시험의 최초 실패는 접혀 저장된 Settings를 펼친 뒤 Slate 배치를 기다리는 fixture 보정 후 집중·전체 재시험으로 통과했으며 원본 로그를 보존한다.
- 직접1280×720의 Camera 이동/숫자 회전·정지/실행 첫 Esc·LiDAR 숫자 위치/첫 Esc·패널 조작에 이어, 세션 복구 후 실제1920×1080에서 Camera 기즈모 이동·회전·정지 첫 Esc·Settings 숨김, LiDAR 숫자Z1800→1850·4Hz→20Hz Esc 복귀, 데이터 패널 열기·네 글자 배율·resize/scroll/collapse를 추가 확인했다. 캡처1180×771에는 아래쪽 일부가 보이지 않아 전체1920 checklist·Main/외부 Widget 입력 전수·직접 준비 취소는 미완료다. 정상 반복의 private memory는 실행1→30에서 약15/44MiB 증가했고 전체 allocator/driver/engine 인과 분해는 남는다. **전체 최종 검수 완료로 표시하지 않는다.**
- 직접 정체 시험에서 획득이 멈췄는데 모니터가 마지막20Hz를 계속 표시하는 문제를 재현했다. `1d17016`은 등록된 소유 모니터만 Scheduler의 현재 유효Hz·준비/정지/조작/정체/pause 상태를 표시한다. 실제1280×720에서0Hz·정체 경고→20Hz 복귀, 일시정지→재개를 확인했다. 저장된 측정 이력·송신 계약·주기·비대상 Widget의 표시 방식은 변경하지 않는다.
- 이 표시 수정까지 포함한 최종 source의 실제1920×1080·10+60초 PCD/세 출력 각3회도 통과했다: 평균59.98~60FPS,1% low≥59.96,p9516.67ms,PCD외부수신≥19.912Hz, invalid/gap/duplicate0. 기존 baseline 대비FPS/low/p95의5% 회귀 없음. source별 원문과 구간별 카운터는 `Saved/Reports/SensorResourceLifecycle/final-performance-comparison.json` 및 `Saved/Reports/lifecycle_final_monitor_*`에 보존한다.

- Camera의 readback·encode·완료 순서 대기 전체를 8프레임으로 제한한다. 앞 JPEG만 2초 지연한 실제 D3D12 시험에서 순서 대기46→8, 완료 JPEG44→7을 확인했다. 한도 도달은 명시적 출력 실패이며 측정을 막거나 이전 이미지로 채우지 않는다. 실패한 앞 프레임 뒤의 완료 결과는 해제하고, Stop은 취소된 결과를 발행하지 않는다.
- 진단용 CPU/Niagara 점군과 World debug line batcher를 센서 SceneCapture에서 제외한다. 플레이어의 3D 표시는 유지한다. 실제 RHI의 표시 OFF/ON에서 Camera 픽셀 변화0, GPU 깊이 차이0을 확인했고, 격리를 해제한 대조군은 Camera2,536픽셀·중앙 깊이795.013→200cm의 변화를 검출했다. 사용자 Mesh·머티리얼은 수정하지 않는다.
- 자원 snapshot은 요청 시에만 생성하며 CPU 소유/공유 용량과 GPU 추정을 구분한다. 상세 파일은 `Scripts/run_slab_repeat_stability.ps1 -Repeated -RunCount 30 -ResourceDiagnostics -ReportGroup SensorResourceLifecycle [-AllOutputs]`로 수집하고 `Scripts/analyze_sensor_resource_lifecycle.ps1`로 상한을 대조한다. 공유 bytes를 서로 다른 소유자 사이에서 합산하지 않는다. Camera readback 재사용 최적화는 원인 확인 조건부로 유지한다.

- 같은 월드·Raw worker·연결로 30초×10회 PCD 전용과 세 출력 실행을 검사했다. PCD 전용6,000건, 세 출력 Camera8,999/LiDAR정보6,000/PCD6,000건의 승인 request ID·제출·receipt·실제 내부 검증·독립 외부 수신을 대조했다. invalid/gap/duplicate/overflow0, 복귀 추가 제출0, 종료 outstanding bytes/sequence0이다.
- 실제1920×1080에서 최종 반복은 PCD59.90~60.00FPS·1% low≥52.91, 세 출력59.91~59.99FPS·1% low≥54.27, p95≤16.672ms. 같은 조건 변경 전후 각3회의 중앙값 FPS는 PCD59.96→59.99, 세 출력59.89→59.96이며 p95 증가5% 이내다. 장비는 현재 RTX4070Ti/UE5.3 한 대 구성이다.
- 최초 반복 후반 p95약25ms와 ISM update 최대15.9ms를 trace로 재현했다. 진단 미리보기 인스턴스의 충돌/물리/navigation을 제외한 뒤 같은 후반30초 갱신 inclusive2.122초→0.044초, 최대0.291ms를 확인했다. 점 수·센서 측정 규격은 동일하다. 과거 다른 SHA/조건의24.9ms 기록과 모든 PC 지원은 별도다.
- 최종 D3D12 `MA0T10`203개 목록 중 최초202 Success/1 Fail(외부 publisher 준비 누락), 실제 `topic.scenario` 발행을 준비한 해당 시험 재검증 통과. 명시 opt-in skip5개는 제외해 실제198개 수행으로 구분한다. TCP 장애6종 각2회, 집중 종료/저장/preview isolation 및 WBP6개 메모리 컴파일/저장0을 확인했다.
- 직접 client1174×683에서 결과 선택·숨김 보존·저장 실패→mouse retry→실제 저장 완료·초기 복귀 결과 보존·9.37초 중단/187 receipt·UUID 복사를 확인했다. 목표1280×720·1920×1080 전체 직접 조작 및 직접 준비 취소는 미완료다. 자동화 exact1920과 구분한다.
- 세부 증거·원본 실패·제한: `Saved/Reports/SlabRepeatRunStability/FINAL_STATUS.md`. 재현: `Scripts/run_slab_repeat_stability.ps1 -Repeated [-AllOutputs]`, `Scripts/analyze_slab_repeat_stability.ps1`. DTCore/운영맵/개인 UI 저장값은 보존했다.

### 2026-10-06 main 기반 최소 DTCore 적용

DTCore는 main `b22af0b`에서 필요한 오류·종료 안전성만 적용한 `b2504d1`을 사용한다. 기존 센서·Slab 기능을 유지하는 별도 연동 브랜치에서 검증했다. `OnConnected`는 성공/실패를 포함한 구독 완료 callback 집계이며 모든 구독 성공을 보증하지 않는다. custom 수신 binding을 보존하고, 공통 5초 timeout·all-ready 상태기계·World cleanup의 GI 전역 큐 폐기는 포함하지 않는다. 이전 안정화 기록의 해당 API 설명은 현재 계약으로 사용하지 않는다.

로그 쓰기·parse 종료 안전성, Registry EndPlay/stream-out 정리와 Widget 제거 후 Blueprint hook을 검증했다. SingleRelease와 FPS 숨김은 프로젝트 Controller가 선택하며, 미등록 Widget 정책은 유지한다. 공통 변경 이유와 소비자 이관은 [최소 수정 계약](../Plugins/DTCore/docs/MINIMAL_MAIN_MIGRATION.md)을 참조한다. 검증 결과는 `Saved/Reports/DTCoreMinimalAcceptance/FINAL_STATUS.md`에 있다. 최종 `443b48a` 전체 RHI 자동화184 Success(실제179·opt-in skip5), 신규 실패0이며 독립 host/프로젝트 Editor·Shipping과 Development 패키징을 통과했다. 직접1286×760 PIE에서 조작·숫자 입력창 첫 Esc·폰트·패널·Slab 실행/재생을 확인했다. PCD 전용과 세 stream의 별도1920×1080·10+60초 시험은20/30Hz·평균60FPS·p9516.67ms, 승인 request 집합의 내부/외부 누락0이다. **재생 성능20ms 조건은 기존24.908ms/최소수정24.925ms로 두 버전 모두 실패**하므로 성능을 포함한 최종 검수 완료로 표시하지 않는다. Geometry 성능·10회 stream 반복의 최종 버전 시험은 미수행이다. 이후 발견한 소유 숫자 편집창 Esc는 프로젝트에서 수정했고 전송 계약은 동일하다.

### 2026-10-07 Slab 초기 배치 복귀 검증

- 진행 패널에서 종료·정리 후 초기 위치/회전으로 복귀한다. 형상·보관 목록은 유지하고 진행·차트·3D 분석을 대기로 초기화한다. 사용법은 아래 Slab 절을 따른다.
- UE 5.3 Editor Development 빌드, 전체 `MA0T10` 194건 Success/실패 0(실제 185·조건부 skip 9), 별도 Slab 12건 Success(실제 11·PCD opt-in skip 1), 소유 WBP 6개 메모리 컴파일(저장 0)을 확인했다. 전체 시험의 skip은 실제 통과로 합산하지 않는다.
- 실제 Artemis의 `30초 PCD → drain → 복귀 → 같은 시나리오 30초 재실행`은 각 600건, 총 1,200건 제출/receipt/내부·외부 수신이 일치했고 invalid/gap/duplicate/overflow 0이다. 복귀 후 대기 1초의 추가 제출은 0건이다. 센서와 Slab 프레임 개수가 항상 같다는 보장은 아니다.
- Computer Use로 실제 client **1286×760, 글자 100%**에서 실행/pause 비활성 사유, 완료·중단 후 복귀, 동일 목록 재생, 숨긴 차트 재표시와 3D 라벨 제거를 확인했다. 임시 GUI 관찰 스크립트의 미노출 Python API 오류는 보정 후 재실행했으며 production 결함으로 집계하지 않는다. DTCore·운영맵은 수정하지 않았고 개인 UI 저장값은 복원했다.
- 증거: `Saved/Reports/SlabResetPlacement/FINAL_STATUS.md`. PCD 시험의 실제 viewport는 1068×360이다. 이번 시험을 목표 해상도 전체 성능 검수나 기존 재생 p95 문제 해결로 확대하지 않는다. push/PR은 별도 요청 전까지 하지 않는다.

### 2026-10-07 송신 신뢰성 검증 (README 원문)

2026-10-07 송신 신뢰성 검증: Editor Development 빌드, 최종 D3D12 `MA0T10` **198 Success/실패0(실제188·opt-in skip10)**, 소유 WBP6개 메모리 compile/save0. 별도 TCP fixture의 인증 거절·연결 지연·단절·receipt 누락·과부하·센서 삭제 6종을 통과했다. 실제 Artemis 30초 두 실행에서 PCD 전용1,200건 및 세 출력 Camera1,800/LiDAR정보1,200/PCD1,200건의 제출·receipt·내부 수신이 일치하고 외부 PCD1,200건도 검증했다. 실제 자동화 viewport1068×360, 직접 client1286×760/100%다. 세 출력 평균59.85~59.91FPS·p9516.67~16.69ms는 작은 viewport의 집중 결과이며 목표해상도·기존 replay p95 문제 해결 인증이 아니다. 직접 필수 선택·준비 대기·실패·결과 조회·복귀 후 기록 보존을 확인했다. 상세 증거는 `Saved/Reports/ScenarioDataReliability/FINAL_STATUS.md`. Engine STOMP/WSS 필수 모드의 별도 실사용 시험과 장시간 수집·외부 업무 ACK는 남아 있다.

### 2026-09-29 런처 연계 패키징 점검

- 현재 체크아웃(기준148cc336, DTCore a1b333e)의 Windows Development Build/Cook/Stage/Archive를 실제 완료했다. 최종 성공은 맵 제외 없이 수행했다.
- 게임 빌드를 막던 에디터 전용 테스트11파일의 가드를 보완했다(`760082c`). D3D12 에디터 관련29테스트 실패0/미실행0:12건 정상,17건 격리된 SaveGame 파일 부재 경고 동반. 센서 계산/전송 로직은 변경하지 않았다.
- DT_DxLevel이 참조하는 TestMap의 누락된 구형 모니터 클래스를 사용자 승인 후 현재 WBP_VirtualSensorMonitorPanel로 연결했다. 원본은 Saved/LauncherPackagingBackup에 보존했고 재실행 시 파일 불변을 확인했다. SensorTestMap과 Config/Game.ini의 원래 바이트는 보존했다.
- 패키지328파일/878,409,923바이트. ZIP+외부 release.json을 격리된 런처 배포 서버에 접수·승인하고 사용자 확인 후 실제 GUI로 설치·실행했다. Manifest328파일 해시, SensorTestMap/D3D12 창, 런처 종료/재실행 중 동일 UE bootstrap/자식 생존, 정상 종료 후 Quiescent를 확인했다. 실제 버전 업데이트/복구·Linux/회사 인수까지 완료한 것은 아니다.
- `Scripts/inspect_launcher_package_map.py`는 기본 맵의 출력 설정을 읽기만 한다. 현재 저장된 기본 맵의 센서 출력은 LogOnly이며, 런처 시험은 별도 UserDir와 DTCore 로컬 시험 주소를 사용한다.
- DTCore의 EnhancedInput 플러그인 의존 선언 경고는 남아 있다. DTCore 및 gitlink를 임의 수정하지 않았으며 Shipping/Linux·회사 서버·실장비 검증을 완료로 표시하지 않는다.
- 실제 실행에서 DTCore의 CustomLogs가 설치 폴더에 생성됐다. `-UserDir`만으로 모든 로그가 분리되는 것은 아니며 Program Files·서비스 계정의 로그 쓰기 경로는 후속 점검한다. 이번 시험은 시험 폴더 안에서만 이루어졌다.


### 이전 최종 검수 이력 — 2026-10-03

아래는 당시 production source `2eb30f0`, DTCore `067195b`의 이력이다. 최신 기준은 문서 상단의 `2ec67a2`/`b2504d1`을 따른다. 당시 검수에서는 DTCore 공통 소스·wire schema·센서 규격을 바꾸지 않고 숫자 Transform 편집의 경량 조작 유지, 실제 Slate 포인터 경로, 큰 글자에서 소유 패널 최소 폭을 보완했다.

현재 시험의 `1% low` 표기는 `1000 / frame-time p99` 계산값이다. 최악1% 프레임의 평균을 사용하는 다른 벤치마크 정의와 같다고 해석하지 않는다. engine FApp delta와 실제 callback wall pacing도 별도로 기록한다.

- LiDAR envelope UTC가 측정 완료 시점이던 결함을 `c1b86eb`에서 실제 측정 시작 시점으로 수정했다. 이전 12회 비교와 장시간 시험의 PCD 지연 27~36ms는 completion-to-consumer 기록이며 acquisition-to-consumer 통과 근거가 아니다. 전달 건수·checksum·FrameId 증거와 시각 기준을 분리한다. 독립 baseline에도 같은 시각 변환만 적용해 비교를 정규화했다.
- 정규화 후 기준선에서 간헐적인 성능 실패가 발생했다(1280×720 세 스트림: 평균 56.90FPS/1% low 33.70FPS/p95 28.61ms, 1920×1080 PCD: 평균 59.08FPS/1% low 34.40FPS). 동일 조건의 추적 재시험 3회는 통과했지만 원인은 아직 확정되지 않았다. 실패를 정상 재시험으로 지우지 않으며, 최종 12회 비교·전후 5% gate는 미완료다. 사용자 요청에 따라 다른 앱·서비스를 중지하지 않고 현재 환경에서 분석 중이다.
- `117e6e3`의 1920×1080 세 스트림 집중 시험은 평균 59.79FPS/1% low 59.57FPS/p95 16.67ms, 전체 구간 acquisition-to-consumer p95 Camera 49ms·PCD 66ms·LiDAR telemetry 72ms였다. 제출·receipt·내부 검증·외부 MESSAGE 집합을 대조했다. 한 번의 집중 시험이며 전체 비교 또는 다른 맵/다중 센서 지원 인증이 아니다.
- 현재 환경의 10분 trace에서 렌더 작업 대기와 D3D12 텍스처 생성 지연을 관측했다. 매 LiDAR 프레임의 staging readback 생성을 `0c502af`에서 재사용으로 변경했고, 동일 크기·해상도 변경·진행 중 취소/재시작의 실제 깊이와 generation 시험을 포함한 관련 6개 테스트를 통과했다. 수정 후 1920×1080 세 스트림60초는 평균59.75FPS/1% low55.28FPS/p9516.7ms, PCD19.963Hz·gap/invalid/duplicate0으로 통과했다. 외부 CPU 부하도 함께 관측됐고 개별 RHI 자원 이름은 trace에 없어, 간헐적 지연의 전체 인과 또는 완전 해결을 단정하지 않는다.
- `e6afe4f`는 재사용 버퍼의 이전 GPU fence를 새 완료로 읽지 않도록 nonblocking render 제출 fence를 추가했다. `2eb30f0`는 기즈모 클릭 위치가 viewport인데 전역의 이전 UI hover 때문에 거부되던 회귀를 수정했다. 실제 클릭 위치의 소유 viewport hit path를 사용하고 실제 패널 위 클릭은 거부한다. 의도적으로 이전 패널 hover를 남긴 Slate 회귀를 통과했으며 DTCore의 공통 hover API는 변경하지 않았다.
- Camera는 실제 장면의 64bit acquisition token+CRC16 격자를 완료 JPEG마다 독립 해독해 990개 프레임을 검사했다. 이전 JPEG/새 metadata 음성 대조군, readback 포화·설정 변경·Stop/재시작·삭제를 포함한다. 관측한 JPEG만 폴링하던 이전 시험과 구분한다.
- 재생은 관찰 1회와 30초 PCD 2회, 1,200건 제출/receipt/소비자 검증 일치·context 오류 0·평균 59.93FPS·p95 16.67ms다. 시험 프로세스의 Editor background throttle을 명시적으로 해제하고 복원했으며 사용자 설정은 저장하지 않았다. 이전 25.07ms 결과를 숨기거나 production 최적화 효과로 표시하지 않는다.
- 현재 `2eb30f0`의 전체 `MA0T10` 보고서는190건 Success/실패0이다. 그중185건을 실제 수행했고 geometry 성능·replay runtime·continuous stream·repeat·production Slab의5건은 opt-in skip이다. continuous/repeat는 아래 현재 source의 집중 시험으로 갱신하고, 다른3건의 이전 source 증거는 최신 통과로 합산하지 않는다. 앞선 입력 hover 회귀1건의 실패 원본도 보존했다. 커밋된 운영맵은 Saved에 격리해 기존 assertion을 그대로 검사하고 사용자 맵은 보존한다.
- Slate 입력 시험은 첫 Esc/키 반복 후 PIE 유지, 원설정 복구, Data 패널 geometry, Main 재호스팅과 미등록 Widget font 불변을 확인했다. 이 결과를 직접 마우스 검증으로 대신 표시하지 않는다.
- 현재 source의 실제1920×1080 10분 실행은 PCD/telemetry 각12,000건(20Hz), Camera17,999건(29.998Hz)의 승인 요청/제출/receipt/내부·외부 검증 집합 일치와 missing/invalid/gap/duplicate0을 확인했다. 평균59.99FPS·1% low59.96FPS·frame p9516.67ms, 외부 acquisition-to-consumer p95 PCD73ms·Camera47ms였다. callback wall p9522.75ms는 별도로 기록한다. 첫1분 이후 private bytes8.03~8.13GB에는 전체 측정 ledger 보관 비용도 포함되며, 장기 무누수 인증은 아니다. 호환 JSON 파생 출력 생략·acquisition deadline miss는 승인된 PCD 전달 누락과 별도로 기록한다.
- 현재 source의 45초 실행+정리10회를 통과했다. 모든 회차의 승인 요청/제출/receipt/내부·외부 검증 집합과 missing/invalid/gap/duplicate0, outstanding frame/bytes0 반환을 확인했다. Camera29.956~30Hz·PCD19.956~20Hz, 외부 p95 Camera48~49ms·PCD71~73ms였다. 정리503~507ms, 정리 후 private bytes8.12~8.15GB로 약32MB 증가했으며 이 결과를 장기 무누수 또는 완전한 메모리 plateau로 표현하지 않는다. 중단된 초기 fixture·외부 probe timeout은 별도 보존했다.
- 운영 Slab adapter의 신규 실행+같은 시나리오 재생 2회 및 LiDAR geometry 표시/PCD 전송 시험도 실제 Artemis·외부 구독자로 통과했다. 출력 선택과 서로 다른 RunUUID·원본 scenario UUID·Slab frame의 계약을 유지한다. 전체 시험의 조건부 항목은 이러한 집중 실행 증거와 이름별로 연결하며 skip 자체를 통과로 세지 않는다.
- 현재 source의 프로젝트·독립 최소 host Editor Development/Shipping 빌드, Windows Development Build/Cook/Stage/Archive, 16개 Blueprint 메모리 컴파일(저장0), V2 자산 검사를 통과했다. 독립 host는 별도 복사한 검증된 DTCore source/DLL을 사용하며 다른 실제 소비 프로젝트의 검증을 대신하지 않는다.
- 직접 마우스로 Camera/LiDAR 이동, LiDAR 회전, 첫 Esc의 PIE 유지, 네 글자 배율과 패널 resize/접기를 확인했다. 현재 source에서는 Camera Z170→180 입력 후5Hz 경량 조작 유지, Settings 숨김 후 기존640×360/10Hz 미리보기 복귀, 실제 client1280×720의150% 제목/버튼 가독성을 추가 확인했다. 일반 PIE 전체화면 단축키는 크기를 바꾸지 않았고 앞선 최대 client는1920×998였으므로1920×1080 직접 조작은 환경상 미완료다. 자동화의 정확한1920×1080 측정과 혼동하지 않는다. 남은 직접 조작·최종3개 opt-in 회귀·12회 비교 gate 때문에 **최종 검수 완료가 아니다**. 증거는 `Saved/Reports/DTCoreFinalAcceptance`에 둔다. 개인 UI9개와 테스트 fullscreen 설정은 복원했다.

### DTCore 동기화·센서 출력 안정화 — 2026-10-02

아래는 이전 안정화 단계의 기록이다. 최종 검수의 신규 증거와 제한은 위 항목을 우선한다.

- 동기화본의 `GetType(): int32`, Widget byte API를 유지한다. 구형 Widget 식별자 0~6은 Hidden 호환 항목으로 보존한다. 현재 Crane 생산/소비 코드를 함께 이관했다.
- HTTP UTF-8 Body와 URL 우선순위, 수동 Disconnect 의도·generation·구독 receipt 5초 timeout, parse/log worker 종료와 registry GC 보유를 검증한다. 전송 연결 이벤트와 전체 구독 준비 이벤트를 구분한다.
- 공통 클릭 기본값은 DoublePress, 현재 프로젝트 Controller는 SingleRelease를 명시한다. 소유 패널은 약한 입력 차단자로 등록하며 이 등록은 다른 Widget의 style/close/z-order 소유권을 이전하지 않는다. Blueprint close dispatch는 한 번만 실행한다.
- 로그 `LogDirectory`가 비어 있으면 기존 경로를 보존한다. 상대 경로는 Project Saved 기준이다. `FlushLogs()` 결과를 확인한다. `bPresistent`의 모드 유지 정책은 아직 연결되지 않았다.
- RT-01 fake Broker 실제 SEND/receipt 누락 시험과 RT-04 D3D12 JPEG 색상 marker·pose/UTC/포화 시험을 통과했다. marker 검사는 폴링으로 관측한 완료 JPEG에 대한 검사이며 모든 프레임을 독립 검증했다고 확대하지 않는다.
- 10초 warmup+60초 실제 Artemis 외부 구독: PCD 전용 19.93Hz, 세 스트림 Camera 29.95Hz·LiDAR 19.98Hz·PCD 19.96Hz, invalid/gap/duplicate 0. 엔진 frame 평균 60FPS·1% low 59.98 이상·p95 16.67ms. 세 스트림 callback wall p95는 21.97ms로 별도다.
- RT-01/04 전후 기준선은 공통 DTCore 안정화 이후 `8c04c5d`다. 동기화본 자체가 컴파일되지 않았으므로 동기화 전후 전체 성능 비교로 표시하지 않는다. 실제 PIE viewport는 1274×680이며 요청 1920×1080 성능 인증이 아니다.
- 세부 결과·미수행·보호 파일 hash는 `Saved/Reports/DTCoreSync`에 남긴다. 다른 실제 소비 프로젝트, Linux, 회사 PC, 60분 soak 및 센서 플러그인화는 검증/완료 범위가 아니다.
- 10분 세 스트림 연속 시험: Camera29.99Hz·LiDAR/PCD20.00Hz, PCD 입력/제출/receipt/내부검증12,197건 일치, 외부 측정구간11,999건·invalid/gap/duplicate0. 외부 구독자의 측정구간은 내부 전체 실행구간과 다르므로 두 raw count를 직접 같다고 주장하지 않는다. 평균59.99FPS·1% low59.97·p9516.67ms. 메모리58표본의 private bytes는7.56~7.60GB; 60분 leak 인증은 아니다.
- 최종 전체 MA0T10 D3D12:186개 중 실제통과174/조건부skip11/실패1. 실패는 보호된 운영맵의 기존 LiDAR 높이 fixture다. SlabBroker와 RawReceiptProbe·ContinuousThreeStreamSmoke 등 일부 skip은 별도 opt-in 집중 실행에서 통과했으나 전체 green으로 표시하지 않는다. 마지막 구조체 default 보강 후 공통10개·소유Blueprint16개 메모리 컴파일을 다시 통과했다(저장0).
- 현재 프로젝트/격리 호스트 Editor Development·Shipping 컴파일, 현재 프로젝트 Windows Development Build/Cook/Stage/Archive 통과. 중간 DLL 잠금 및 중단된 재생 시험 로그와 단독 재실행 성공 로그를 함께 보존한다. 반사 구조체의 미초기화7필드도 기본값을 명시했고 저장된 자산은 재저장하지 않았다.
- 재생 실통합은 관찰1회+PCD30초2회, 제출/receipt/내부검증1,200건·invalid context0·외부검증 통과. 평균 engine55.52FPS/p9525.07ms로 **20ms 목표 미달**이며 고정3stream 시험과 별도 조건이다.
- Computer Use 직접 확인: Camera/LiDAR 선택, 조작 시작·경량 preview, Settings drag/resize/hide 후 LiDAR4Hz 복귀. 실제 viewport1606×728. 물리 기즈모 이동·PIE 유지 Esc, Data 메뉴/연결 제어, Main 빈 영역, 전체 목표해상도 검증은 미완료다. 캡처0x80070057·활성화 오류 후 추가 입력을 중단했다. 자동화 UI 통과를 해당 마우스 검증의 대체로 표기하지 않는다.
- 마지막 HTTP/default 보강 포함 재측정(`dtcore_final_*`): PCD-only19.92Hz; 세 스트림 Camera29.96Hz·LiDAR/PCD19.98Hz, 엔진평균59.99FPS·1%low59.99·p9516.67ms, callback wall p9522.04ms. PCD 직렬화p953.39ms(기준선3.44ms), 끝queue0/0/0·invalid/gap/overflow0. 전송 end-to-end p95의 전후5% 비교 전체값은 별도 계측 공백으로 남긴다.

### 센서 안정성 수정 — 2026-09-22 / 59d1027

- UE 5.3 Editor Development 빌드 통과.
- RT-02 수정 전 회귀에서 원래 Camera가 640px·0.2초에 남는 선택/종료 문제 재현. 수정 후 lifecycle·monitor follow·D3D12 PIE runtime **3/3 통과**.
- RT-03 성능 자동화 **5/5**, 실제 성능 판정 스크립트의 정상/0Hz/센서 누락 검사 **3/3 통과**.
- 전체 `MA0T10` NullRHI: **172개 중 실제 통과 156 / 실패 1 / 조건부 skip 15**. 실패는 기존 사용자 변경 `SensorTestMap`의 LiDAR 높이 fixture다. 그중 조작 runtime skip은 별도 D3D12 실행에서 통과했다. 신규 실패는 없으며 전체 green으로 표시하지 않는다.
- Computer Use는 `0x80070057` 화면 캡처 및 `0x80070005` 커서 접근 오류로 실제 마우스·Esc 검증을 수행하지 못했다. RHI 실행 인자는 1280×720이지만 실제 client viewport 크기는 미계측이다.
- 장시간 Artemis·다중 센서 성능, WBP 전수 재컴파일, pinned clean checkout은 이번에 재실행하지 않았다. 기존 개인 UI 저장값은 백업에서 복원했다.
- 증거: `Saved/Reports/SensorStability/report.ko.md`, `reproduction.log`, `rt03-focused`, `full-regression`, `final-rhi`. Saved는 커밋하지 않는다.

### 이전 표시·송신 검증 — f7bdd3b

아래는 기존 실행 증거이며 이번 안정성 수정의 신규 성능 측정 결과가 아니다.

- Development 빌드, 6개 WBP 메모리 컴파일, V2 자산 검사 통과.
- 전체 168개 기록: 실제 통과 157 / 실패 2 / 조건부 skip 9. helper pool의 과거 37개 기대값을 42로 갱신하고 최종 집중 3/3 통과. 남은 운영맵 높이 fixture는 사용자 맵을 수정하지 않았다.
- 실제 RHI 표시 OFF/ON: Camera 변경 0픽셀, GPU depth/semantic 변경 0점. 양성 대조군 Camera는 2,533픽셀 변화.
- ML-X Native 한 대 PCD-only, warmup 10초 후 30초×2, **실제 viewport 753×403**: 제출/receipt/내부/외부 수신 599+600=1,199개 일치, invalid/gap/duplicate/overflow 0, 활성 구간 약20Hz.
- 평균 FPS 59.999/59.799, 1% low 59.940/45.019, frame p95 16.667ms. 후반 1% low 저하를 무시하지 않는다. 다중 센서나 3-stream/1920×1080 보증으로 확대하지 않는다.
- Computer Use 실제 viewport: 내장 988×521, 별도 창 1174×683. 목표 1280×720/1920×1080 전수 검증, 회사 프로젝트, pinned clean checkout, 장시간 soak는 미완료다.
- 증거: `Saved/Reports/SlabVisibility/report.ko.md`, `all.log`, `final_focused.log`, `pcd_external.json`, `pcd_rhi.log`, 실제 밝은 화면 PNG. Saved는 git에 포함되지 않는다.

## IMPROVEMENTS에서 옮긴 이력

### 2026-10-10 단일 작업 폴더·배치 접근성 보완

- 기존 `ma0t10_dt`와 별도 최신 작업본의 버전 차이로 추가/저장 기능을 찾지 못한 상황을 확인했다. 현재 소스와 의존성은 `ma0t10_dt`에서 직접 빌드한다. 별도 worktree를 새로 생성하지 않는다.
- 배치 미등록 상태의 무응답 경로에 소유 Host 생성/실패 안내를 연결했다. 선택 배치가 바뀌면 이전 복원 요청을 취소하고, 북마크 이동 후 이름 편집 대상도 동기화한다.
- 사전 점검은 활성 exact/wildcard 우선순위, 수동 추가와 시나리오 전환, Raw/Engine 연결, GPU fallback echo 상한, 단일 프레임 bucket 예산을 구분한다. 동적 admission 구현으로 확대하지 않으며 RT-08 전체 진행률은 유지한다.
- Runtime `81b56ba`에서 최종 전체217 Success/실패0(실제215·skip2), WBP7개와 V2 자산 검사가 통과했다. 실제1920×1080의2+2 PCD-only는 각19.95/19.9667Hz·평균59.89FPS·low51.13·p9516.67ms, 승인2,395건 내부/UI/외부누락0이다. 두 해상도·네 배율의 렌더 캡처와 Slate/RHI 입력 증거를 남겼다. 직접 조작 도구의 node kernel 초기화(os error3)는 재연결 후에도 실패했으므로 물리 마우스 수용과 첫 사용자 검수는 미완료다. UI/UX 전체 진행률을 완료로 올리지 않는다.
- 일반 실행은 요청대로 31000 접속 정책을 유지한다. 확인 당시 listener와 기본 WebSocket Topic이 없었다. 실제 사용할 업무 서버 실행·`topic.scenario` 구독 설정은 프로젝트 배포/운영 준비 항목이며 DTCore 소스 결함으로 분류하지 않는다. 로컬 Artemis로 TC/DataSync/Slab 수신 경로는 별도 검증했다.

### 2026-10-09 PCD 수요 정책 검증 — 이전 조건

- 새 배치 LiDAR의 WhenRequested를 추가하고 기존 레벨/BP/구형layout은LegacyAlways로 유지했다. actual compatibility routing·BP/native 소비자·recording·수동 요청이 있을 때만 JSON을 생성한다. 정책 변경은 측정/조작/파일 snapshot을 유지하며, 수동 JSON과 전달 metadata도 같은 완료 프레임을 사용한다.
- 같은 표시형1920×1080 PIE의2+2 교차비교3회에서 새 정책 low59.17~59.80, 기존 전체JSON low29.74~36.02다. 새 정책의10회 반복11,995개request 및600초23,997개고정승인집합은내부/UI/외부대조누락0, JSON 작업0으로 통과했다. 반복최소low55.966,600초low59.58이다. PCD 점/주기/제출예산을 낮추지 않았다.
- 센서별 속도·기대 장비 수·실행 수·성능이 누락되거나 실패하면 수용 실패를 반환하도록 시험을 보강했다. 최종 전체211 Success 중 실제208·skip3·실패0, 별도입력RHI와WBP7개/marker987개를 확인했다. 숨긴/offscreen모드와기존Legacy전체JSON의성능제한은유지한다.
- 새 정책 UI의 직접 검수는Windows커서접근0x80070005로미완료다. 이전직접조작은과거이력으로만읽는다. 현재증거는 `Saved/Reports/PcdDemandPlacementAcceptance/FINAL_STATUS.md`다.

### 2026-10-09 배치 작업공간 수용 이력 — a78b443

- 전용 배치 패널·런타임 추가/복제/소유 삭제·북마크·전체 배치 명시적 복원·PCD 사전 점검을 구현했다. 자동화는 최신 전체209 Success 중 실제206·opt-in skip3·실패0, 배치 입력 RHI는 별도 통과다. WBP7개 메모리 컴파일 및 자산 검사 통과, 기존 자산 저장0이다.
- 실제1920×1080에서 새 배치 API의1+1/2+2 PCD-only 60초를 통과했다. 2+2 기본 제출예산2, 각PCD20Hz·Camera acquisition30Hz다. 같은 연결10분은 각PCD19.993Hz·평균59.98FPS·low58.27·p9516.67ms이며두Camera모두실행한최종30초×10회에서11,938개PCD의승인/제출/receipt/내부·UI/외부 request 대조를 통과했다. 정리 후body/events/active0, 복귀 추가제출0이다. 이 조건을4+4·60분·다른공장장면으로 확대하지 않는다.
- **반복 성능은 실패/후속:** 최종10회 반복의1% low38.42~40.25FPS는45FPS 목표에 미달한다. 초기첫Camera정지시험11,982건·low36.84~43.96은이력이다. 연속 성능 fixture의 미송신 호환JSON은stride32/max1024, 반복 신규LiDAR는기본stride1/무제한이므로 동일한 후처리 조건이 아니다. PCD 검출점은 줄이지 않았다. 후보 비용을 분리해 동일 조건으로 재시험해야 하며, 자동화 Success를 전체 성능 통과로 해석하지 않는다.
- 직접1280×720의추가·북마크·Undo/Redo·전체배치저장과 재시작후1920×1080의복원·ID/북마크연결·PCD/결과진입·150%창조작을 확인했다. 전체배율/DPI·첫사용자발견성·UI직접복제/삭제는 남는다. 마지막간결한PCD요약 재확인은Windows캡처0x80070057로미완료다. 자동화를 직접 마우스 검증으로 집계하지 않는다.
- 최초 부하환경에서기존/신규모두약47FPS·2+2 PCD약15.7Hz로실패한원문과사용자GPU작업중지후통과를보존한다. 특정앱원인이나무조건적무지연을주장하지 않는다. 세부증거·환경제한은 `Saved/Reports/SensorPlacementWorkspace/FINAL_STATUS.md`를 따른다.

### "읽는 방법" 절에 있던 날짜별 기록 (IMPROVEMENTS 원문)

**출력 신뢰성 보완 — 2026-10-08:** 느린 LiDAR JSON이 새 측정마다 폐기되던 경로를 수정해 정상 완료를 전달하고 측정 당시 metadata를 보존한다. 50ms 입력에 200/700ms 생성 지연, 전역 4개 작업 상한과 다섯 번째 센서의 진행, Stop/삭제 후 callback 폐기를 집중 검증했다. PCD·Raw telemetry 중복 발행을 막고 선택된 호환 JSON만 drain에 포함한다. 실행 상세와 A/B에 센서·출력별 전달 수를 추가했으며 wildcard의 센서 간 FrameId 간섭·카운터 덮어쓰기·round-robin 순회 오류를 보완했다. `MeasuredOutputRateHz=0`만으로 PCD 실패를 판단하지 않는다. 전송 계약·센서 규격·DTCore·외부 Widget은 그대로이며 UE 학습 데이터 묶음 저장은 추가하지 않았다. 실제 다중 센서 수용 범위와 남은 검증은 RT-05 및 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`를 따른다.

최종 검증: D3D12 전체222 Success/실패0 중6개 opt-in skip(실제216), 별도30초 A/B 실제송수신, WBP6개 메모리컴파일·저장0. 직접1920×1080 Camera기즈모·첫Esc/정지유지와LiDAR정밀이동·10→20Hz복귀, 직접1280×720 PCD재실행·센서별600건A/B·패널resize/scroll을확인했다. PCD전용에서비선택Camera0Hz를측정률실패로오판하던경고를수정했다. 입력fixture는개인의열린Data패널에의존하지않게우리보조패널만숨긴다. 광범위한배치/모든폰트/RMB직접체크리스트와장기지원은별도다. RT-05·RT-06·UI-01 전체완료로확대하지않는다.

**배치·실행 결과 비교 — 2026-10-08:** 맵 값 복원의 실패 안내·Undo, SensorId 변경의 북마크 캐시 및 이전 저장 결과 적용을 보완했다. 새 위치 미리보기는 측정 시작 시각·자세·규격·generation과 실제 완료를 확인하며 Camera 확인에는 JPEG/readback을 추가하지 않는다. 실제 1920×1080 RHI에서 원규격 새 자세 프레임 복귀는 Camera 약31ms·LiDAR 약67ms, 입력→viewport GPU 완료 p95는 약34ms였다. 현재 PIE의 확정 결과 A/B에 배치·설정·기록 시점 Hz와 실행 전체 전송 카운터를 표시한다. 원문 hash·조건 차이·확인 불가를 구분하고 배치 품질 점수로 확대하지 않는다. 실제 30초 A/B의 PCD600/599개 및 모든 acquisition 행렬을 독립 대조했고 비교 UI 표시 전후 frame p95는16.67ms로 같았다. 최종 근거와 직접 조작 범위는 `Saved/Reports/SensorPlacementComparison/FINAL_STATUS.md`를 따른다. UX-01·RT-06의 생성/복제/삭제·다중 구성 완료율은 이번 기능으로 올리지 않는다.

**Slab 초기 배치 복귀 — 2026-10-07: 구현·관련 검증 완료.** 종료·송신 정리 후 진행 패널의 `초기 위치로 복귀` 또는 Actor API로 PIE 시작 당시의 월드 위치·회전에 즉시 복귀한다. 현재 형상/재질/스케일과 보관 목록은 유지하고 진행률·현재 프레임·세 차트·이전 3D 분석 라벨을 초기화한다. 실행·pause·파싱·재생/센서 준비·drain 중 거절, 실패/중단 후 복귀, cm/mm/m와 회전된 Track, 반복 호출·지연 적용 차단을 검사했다. 실제 Artemis 두 실행의 PCD 1,200건 제출/receipt/내부·외부 수신 일치와 복귀 추가 제출 0을 확인했다. 전체 D3D12 자동화 194 Success(실제 185·skip 9), Slab 집중 12 Success(실제 11·skip 1), WBP 6개 메모리 컴파일과 직접 1286×760/100% 조작을 수행했다. 숨김 중 차트 초기화와 동일 시나리오 새 RunUUID 재생도 직접 확인했다. DTCore·운영맵·기존 UI 저장값은 보존했다. `Saved/Reports/SlabResetPlacement/FINAL_STATUS.md`를 따른다. 이는 개별 복귀 기능의 완료이며 RT-05/QA-01 또는 기존 재생 p95 목표를 완료로 올리지 않는다.

**최소 DTCore 적용 검증 — 2026-10-06:** 별도 체크아웃에서 main 기반 최소 수정본을 연동했다. 1차 D3D12 전체161개 Success에는 조건부skip5개가 포함되며 실제156개다. 추가 공통 수명 검증은 13개 Success(실제 수행 12개·Slab Broker opt-in skip 1개), 실제 topic.scenario 수신은 별도 전체 시험에서 통과했다. PCD 전용 및 세 스트림의 1920×1080·10초 준비+60초 시험에서 승인 집합의 내부/외부 누락 0, invalid/gap/duplicate 0을 확인했다. PCD 20Hz, Camera 30Hz, 평균 60FPS·프레임 p95 16.67ms. 단순 누적 카운터가 아니라 고정 측정 request 집합을 대조했다. 최종 소스 전체 자동화는184개 Success(실제179·조건부skip5), 신규 실패0이다. 직접1286×760에서 네 글자 배율·패널·Slab 실행/재생·입력창 Esc 복구를 확인하고 최종 Shipping/Development 패키징을 통과했다. 재생 데이터1200건은 일치하지만 p95는기존24.908ms/최소수정24.925ms로 두 버전 모두20ms 목표가 실패다. Geometry 성능·10회 stream 반복은 미수행으로 유지한다. 기능 회귀와 성능을 포함한 최종 검수 완료를 구분한다. `Saved/Reports/DTCoreMinimalAcceptance/FINAL_STATUS.md`를 따른다. 상세 원인은 이 문서의 부록으로 추가하지 않는다.

**이전 이력 — 2026-09-29 런처 실행:** 격리된 관리형 런처 GUI에서328파일 설치/해시·UE 창·런처 종료 후 동일 UE 프로세스 유지·정상 종료를 확인했다. DTCore CustomLogs가 설치 경로에 쓰이는 점을 발견했으므로 회사 Program Files 권한/로그 위치는 미검증으로 남긴다. 실제 버전 변경/복구·Linux·운영망 검증은 이 결과로 완료되지 않는다.

**이전 이력 — 2026-09-29 패키징:** Runtime의 에디터 전용 테스트11파일을 WITH_EDITOR로 제한하고, 승인된 TestMap의 누락 모니터 참조를 복구했다. 당시 Windows Development 전체 Build/Cook/Stage/Archive와 관련 D3D12 자동화29건을 확인했다. 이후 현재 기준에서는 EnhancedInput 의존 선언, 프로젝트·격리 host의 Editor/Shipping 빌드와 Development 패키징을 검증했다. Linux·회사 운영망 및 실제 소비 프로젝트의 검증은 별도다.

**이전 검수 이력 — 2026-10-03:** `Saved/Reports/DTCoreFinalAcceptance/FINAL_STATUS.md`와 README의 최신 검수 요약을 우선한다. 현재 전체 자동화는190건 Success/실패0이며 실제 수행185건과 opt-in skip5건을 구분한다. 현재 source의10분 연속·10회 반복 송수신은 통과했지만 최종 성능 비교·일부 opt-in·전체 직접 조작 검수는 남아 있다.

## ROADMAP에서 옮긴 이력

같은2026-09-29 후속으로 격리된 Windows 관리형 런처에서 실제 패키지 설치/실행·Manifest328파일 해시·런처 종료 후 UE 수명 유지·정상 종료를 확인했다. 회사 설치 경로 쓰기 권한, 실제 업데이트/복구·Linux/RHEL 인수는 별도 gate로 유지한다.

2026-09-29 배포 준비 증거: 현재 UE5.3 프로젝트의 Windows Development 패키징을 완료하고 외부 런처의 격리된 ZIP/sidecar 접수·승인을 준비했다. 에디터 테스트 가드와 승인된 TestMap의 누락 위젯 참조만 보완했다. 이는 R5 플러그인화·간편 설치 또는 회사 운영 인수 완료가 아니며, 실제 런처 실행·Shipping/Linux·회사 환경 수용은 각각의 증거로 판단한다. 자세한 범위는 [README의 패키징 점검](../README.md)을 따른다.


## 2026-10-12 — UXR-03 전송 탭 보완 CLI 검증

- 소스 커밋 `22d54b9`: 6파일 수정, 이동 0. 쉬운 사전 점검 문장/툴팁, `점검 대상:` 세그먼트, 직접 전송 접힘 헤더와 숨김 표시 비용 차단. 기존 Blueprint API·enum·정책·wire 계약 불변.
- Editor Development, layer 18/18(신규/stale 0), DTCore guard, V2 `-RequireAssets`, WBP 7개 메모리 컴파일 통과. 집중 13 Success/실패·skip 0. 테스트 이름은 `Saved/Reports/UXR/UXR-03/supplement/focused-classified.json`.
- 기존 테스트 기대값 변경 없음. `SensorDataWorkflowTests.cpp`에 기본 문구/접힌 문자열 비용 검증 추가. 초기 텍스트 탐색 실패는 `SNewSensorTool` 선언 이름과 독립 switcher 평가 문제로, 실제 `STextBlock` 클래스 타입과 전송 subtree를 검사하도록 수정했다. SCheckBox는 원래 선언 타입 검사 유지. 초기 실패 3회는 원본 로그에 보관했다.
- 최종 5탭×1280/1920×100/150% 자동 렌더 20장. 요청된 전송 4장은 직접 열어 확인했다. 실제 client 1280×720/1920×1080, window DPI 1.0; PNG에는 6px 테두리 포함. 150%의 아래 점검/상세는 스크롤 접근이며 마우스 접근 검수는 미수행.
- 성능: D3D12, SlabScenarioValidationMap, 실제 1920×1080, 글자 100%, 전송 탭 열림/다른 창 숨김, FullSpec D455 Camera 1+MLX80 Native GPU LiDAR 1 측정, Slab 대기, 송신·파일 저장 없음, 10초+30초. FPS 59.9968→59.9960(-0.0013%), engine frame p95 16.6669→16.6669ms(0%), callback wall p95 20.9410→20.9862ms(+0.2158%), game thread 평균 5.6673→5.7254ms(+1.0237%). 2% gate 통과. engine delta는 FApp::GetDeltaTime(엔진 pacing 포함), callback 지연과 구분. 가상 WinDisc이며 물리 디스플레이/절대 지연 목표 인증 아님.
- 기준선 production은 `3abccdf`, 동일 Editor 계측기만 먼저 빌드했다. 최종 검증은 그 HEAD+변경 소스로 수행 후 `22d54b9` 커밋; 소스/binary hash는 `validation-manifest.json`과 `baseline-manifest.json`. 전체 MA0T10 이름별 비교는 요청의 마지막 통합 단계에서 수행 예정이며 이 보완의 집중 결과로 대체하지 않는다.
- 보호 파일 475/475 불변, 개인 SaveGame 9/9 복원. DTCore `b2504d1` gitlink/worktree 불변. Editor/PIE 정상 종료. 마우스 체크리스트: `Saved/Reports/UXR/UXR-03/supplement/manual/CHECKLIST.md`.


## 2026-10-12 — UXR-04 시나리오 화면 CLI 검증

- 소스 `9477732`: 수정 7파일+추가 3파일, 이동 0. 기존 Progress/Replay 인스턴스와 DTCore 전면/입력 등록을 유지한 나란히 기본 배치, 진행 창 차트 토글, 통합 표시 설정 접힘. 사용자 저장 배치는 우선한다. 소유 native 재생 출력 편집 0개, 기본 정책·기존 API/enum/저장값·wire 계약 불변.
- Editor Development, layer 18/18(신규/stale 0), guard, V2 `-RequireAssets`, WBP 7개 메모리 컴파일 통과. 집중 28 Success/실패·skip 0, 기존 기대값 변경 없음. 신규 `MA0T10.UXR04.ScenarioScreenAndChartToggle`: 차트 토글/실행 불변/접힘 상세 비용/숨김 표시 정지/기본 재생 출력 보존.
- 최종 자동 렌더 8장: 시나리오 진행+보관 목록 및 표시 설정, 실제 client 1280×720/1920×1080×글자 100/150%, window DPI 1.0. 클릭 검수가 아니며 1280×720 150%에는 기존 커서 위치에 따른 일시적인 제목 툴팁이 함께 잡힌다. 주요 버튼·표시/하단 영역은 캡처에서 접근 위치가 보인다. 개인 저장 배치의 직접 복원/마우스 resize/scroll 검수는 보류.
- 성능 fixture: D3D12 SlabScenarioValidationMap, 실제1920×1080, 글자100%, 진행+목록 열림/다른 창 숨김, FullSpec D455 Camera1+MLX80 Native GPU LiDAR1 측정, 기존 synthetic600행을 시간값만3배로 늘린90초 관찰 실행, 10초+30초. 원본 fixture 이름의30s는 유지했으며 명시 duration과 행 elapsed는90초 기준이다.
- 처음 game thread 평균 5.9173→6.0410ms(+2.09%)로 gate 실패. 버튼 상태·텍스트 FText의 표시용 조회를5Hz 캐시로 바꿨고 최종 FPS59.9994→59.8091(-0.3172%), engine frame p95 16.6669→16.6669ms(0%), callback wall p95 20.8877→21.0707ms(+0.8761%), game thread 평균5.9173→5.9611ms(+0.7400%)로2% gate 통과. 0Hz 센서를 평균에서 제외하지 않으며 센서30/20Hz 로그를 별도로 남겼다. callback pacing과engine delta(FApp::GetDeltaTime, pacing포함)는 구분한다. 가상WinDisc이며 물리 디스플레이/절대 지연 인증은 아니다.
- 기준선 production은 `22d54b9`/HEAD`1670b4b`, Editor 계측기만 먼저 빌드(`build-instrument.log`,4actions). 최종 검증은 HEAD+변경으로 수행 후 `9477732` 커밋했다. 소스 hash·캡처/이름별 결과는 `Saved/Reports/UXR/UXR-04/validation-manifest.json`, `captures.json`, `focused-classified.json`. 전체MA0T10는 마지막 통합 단계에서 비교 예정.
- 보호475/475·SaveGame9/9 복원, DTCore `b2504d1` gitlink/worktree 불변, 운영맵·자산·Config/Game.ini·PixelStreaming·Claude전용문서 불변. Editor/PIE정상종료. 직접 체크리스트 `Saved/Reports/UXR/UXR-04/manual/CHECKLIST.md`. 초기 캐시 편집용 inline Python의 quoting 실패1회는 실제 변경 없이 발생했고 UTF-8 파일 스크립트로 수정 후 최종빌드 통과.


## 2026-10-12 — UXR-08 Slab 정보·표면 CLI 및 최종 통합 검증

- 소스`7a63971`: 수정19+추가6파일/이동0. 정보 카드5Hz, 원본 행·누락·단위·기하 계산·현재 Track위치/속도구분, DTCoreClick→weak기존Host→UDxWidget진행인스턴스선택, 기존MID프리셋/opt-in광학값/복원, optional실행결과추가. UObject/Blueprint함수·enum순서·SaveGame슬롯·기본정책·wirebytes불변. 센서플러그인화 없음.
- 읽기 전용Unreal재질조회에서Plain은Hotness/EmissiveStrength/Roughness/Metallic와ColdColor/HotColor만지원. Oxidation은없고legacy재질에는있지만기존경로해석을바꾸지않아산화스케일미지원으로남겼다. 지원4외형×4해상도/배율+정보/표시8장=24장. 150% 표시 설정은자동스크롤로 접근위치확인. 직접마우스결과가아니다.
- EditorDevelopment, layer18/18(신규/stale0,production221파일), DTCoreguard, V2 `-RequireAssets`, WBP7개메모리컴파일 통과. 집중33 Success/실패·skip0. 마지막D3D12전체MA0T10는227 실제Success/20조건부skip/실패0. UXR-03기준선241이름상태유지, 신규6이름(UXR04 1+UXR08 5)은`test-comparison.json`. 기존테스트기대값변경없음.
- 실제RHI: 별도검증용EditorWorld(운영맵로드/저장없음), 결정적BaseColor SceneCapture픽셀, 실제CPU/GPU LiDAR스냅샷, 기존GPU형상분류buffer, 동일FrameId/UTC의envelope로PCD bytes 대조. 기본/link-off 픽셀·intensity·PCD정확히동일, hot/link-off에서LiDAR/PCD동일, link-onCPU평균raw intensity61945.472→24778.194 / GPU62546.359→25018.539, JSONlatest reflectivity940Nm0.08/linktrue확인. 결과metadata는설정provenance이며실측평균을제품JSON에새로집계하지않는다. productionFinalColor/JPEG bitwise동일성은보장하지않는다.
- 초기RHI에서CPU캐시가반사율변경을무시하고GPU가고정기본반사율을쓴것을확인. 기존public캐시무효화를명시변경시1회호출하는L2adapter, GPU기존semantic identity에nativeopt-in광학값을복사하고완료시그snapshot만사용하도록보완. 새flag기본false라기존GPU출력은유지한다. 기존분류buffer가없으면link요청을원자적으로거절하고추가capture/readback을하지않는다. setting/geometry가이후바뀌는경우지원범위재확인필요.
- 초기실패보존: misplaced bounded-id검사/A미정의compile1회; 테스트Intensity/Backend필드명compile1회; PreviewWorld에Results없어accessviolation1회; ScrollBoxenum명compile1회; EditorWorldPrepare에0시각을줘scan미시작timeout1회; FString동적format형식compile1회. 모두수정후최종build/focused/full통과. 초기SceneColorHDR default/no-op카메라는6픽셀최대2채널값renderer변동이있어정확비교실패; 판정허용치를낮추지않고독립BaseColor경로와visiblepositivecontrol로정확성대조했다. GPU분류view를명시한시험조건은report에기록한다.
- 성능같은맵/1Camera+1GPU LiDARFullSpec측정/90초600행관찰시나리오진행/진행+목록열림/기본표면+linkoff/1920×1080/font100/10초+30초. FPS59.998534→59.999763(+0.00205%), engineframep95 16.666900→16.666900ms(0%), callbackwallp95 20.937402→20.841401ms(-0.45851%), gamethread평균5.936682→5.910328ms(-0.44391%). 2%gate통과. 가상WinDisc이고enginepacing와callback은분리; physicalDPI/절대지연인증아님.
- 검증은HEAD`f408338`+변경소스로수행후`7a63971`커밋; baseline은그HEAD의production`9477732` binary. sourcehash/원본로그/skip목록/이름별비교/PNG는`Saved/Reports/UXR/UXR-08/`. 보호475/475불변, 개인SaveGame9/9복원, DTCore`b2504d1` gitlink/worktreeclean, 운영맵/자산/Config/Game.ini/PixelStreaming/Claude전용문서불변. Editor/PIE정상종료(초기테스트crash는실패로분리). push/PR/merge/worktree없음. 직접 마우스 보류와 독립 검토 대기로 진행률 90%.

## 2026-10-12 UXR-08 보완 CLI

검증 기준선0881f0b, DTCore b2504d1. 소스10파일, 라벨/값 그리드·숫자 형식·원본 행 상세 분리·일시정지 속도 명시·query 크기 불변 hover marker. Editor Development/layer18/guard/V2 assets/WBP7 통과. 관련33 및 최종 영향7 이름별 Success(실패/skip0), 렌더24장, scenario+archive+monitor 1920×1080 1Camera+1GPU LiDAR 10초+30초 성능2%gate PASS(FPS+0.004%, enginep95 0%, callbackp95+0.672%, game thread+1.575%). 보호475/개인SaveGame9 불변·복원. 직접 마우스는 미수행. 상세 Saved/Reports/UXR/UXR-08/supplement/RESULT.md.

초기 V2 정적 검사는 rg PATH 부재로 실패 후 기존 rg 경로 지정으로 통과. 첫 성능 호출은 잘못된 환경변수명으로 client1280으로 실행되어 비교에서 제외하고 before-1280-invalid.json으로 보존; 올바른 MA0T10_ACCEPTANCE_RES_X/Y 지정 후 실제1920×1080 기준선을 확보했다. source-only marker 종료 cache 갱신 뒤 incremental build와 영향7을 다시 통과했다. 처음 apply_patch는 동일 파일 delete/add 중복 대상으로 검증 단계에서 거절되어 파일 변경 없이 Update로 재적용했다. inline Python -c의 Windows 인용 실패는 파일 변경 없이 종료하고 Saved의 명시 Python 파일로 대체했다.

## 2026-10-12 UXR-05 CLI

기준선69b9e84 + 동일 Settings/actual-client 계측, DTCore b2504d1. 소스 수정4+추가3, owned Settings 3단·한 문장/ⓘ 도움말·5Hz/숨김·접힘/닫힌 상세 중단. Read/Validate/Apply·잠금·조작 종료·SaveGame v6·wire 불변. build-caption-final Development/layer18 신규0/guard/V2 assets/WBP7 통과. focused-final 실제52 Success/실패0/별도 스트림skip2; Slate 입력·조작 opt-in RHI 수행. 최종 제목/문구/ⓘ 보완 focused-caption-final 17 Success/실패·skip0. 실제 visible Slate 기본/고급 레거시0·금지어16 검사 통과. 2종장비×3화면×1280/1920×100/150% 렌더24장. 동일 Settings+monitor/1Camera+1GPU LiDAR/1920×1080/font100/10초+30초 FPS+0.002%, enginep95 0%, callbackp95−0.146%, game thread−0.317%로2%gate 통과. 보호475/SaveGame9 복원, Editor 종료. 마우스/DPI 미수행. Saved/Reports/UXR/UXR-05/RESULT.md와 이름별 JSON 참조.

초기 test-only compile: const Slate children·TObjectPtr deduction·FEndPlayMapCommand 헤더 누락을 수정. 첫 UI 검사는 native 생성 헬퍼가 모두 SensorToolControl이라는 진단 타입명을 기록해 STextBlock 라벨을 누락하여 실패. metadata visibility 갱신만으로는 해결되지 않았고 실제 타입명을 보존해 통과했다. fresh-tree 실험은 UMG wrapper 교체로 ownership을 해제하여 추가 실패했으므로 같은 wrapper로 되돌렸다. 기존 기대값을 완화하지 않았으며 final 신규 테스트 Success. 초기 render의 자동 hover tooltip 가림은 렌더 프로세스에만 Slate.EnableTooltips=0으로 제거, 원본은 render-initial/ 보존. 150% 제목과 input label·ⓘ 상세 접근을 보완하고 final 빌드/영향17/24장/성능을 다시 검증. 공개 API/저장 값·정책/운영 자산은 변경하지 않음.

## 2026-10-12 UXR-06 및 최종 통합 CLI

Runtime e55fd9a(검증은 9146d6d+동등 worktree), DTCore b2504d1. 소스 수정3+추가2. owned 미리보기1줄/실제 경고색/중지·장비 없음·기존 배치 action·weak 표시 대상 재확인·열린 개발자 정보/범례5Hz·숨김/접힘 중단. native bind/view/texture fallback의 기존 동기 ScanAndSend는 캐시 프레임으로 제한; 명시 호환 export/local capture·legacy/Designer 경로는 그대로 보존. BP/API/enum/SaveGame·정책/wire 불변. production build-layout-final, test-only build-compatibility-test-final Development 성공. layer18→18 신규/stale0/production221, guard/V2 assets/WBP7 통과. 집중 고유29 실제Success(Workspace는 첫 skip 뒤 env1로 별도 재실행, Input/Manipulation 실제RHI 포함), 후속8+명시호환1 Success/실패·skip0. actual1280/1920×100/150% Camera/LiDAR 측정/중지+등록장비0 렌더20장. preview open/1920×1080/font100/1Camera+1GPU LiDAR FullSpec/10초+30초 FPS−0.045%, enginep95 0%, callbackp95−0.520%, game thread−1.373%로2%gate PASS. 최종 전체 D3D12 MA0T10 229 실제Success/20조건부skip/실패0, c428ab6의247개 이름별 상태 유지+신규UXR05/06 2개 Success. 전체skip20는 통과로 세지 않음. 보호475/개인SaveGame9 복원·Editor 종료. 직접 마우스/DPI 전부 미수행. Saved/Reports/UXR/UXR-06/RESULT.md, 각 raw index/이름별 비교/skip/성능 JSON이 근거.

초기 compile 실패: SOverlay include 경로와 존재하지 않는 임시 yaw cache 필드를 정정하고 기존 height/slice 필드만 사용했다. 초기 empty 렌더는 부모 폭이 없어 안내가 과도하게 줄바꿈되어 SBox 폭을 부모 크기로 제한하여 최종 이미지에 보완. 장비0 fixture는 PIE의 sensor 등록만 해제, Actor 이동/삭제·맵 저장 없음. private preview helper의 동기 ScanAndSend 호출을 조사해 새 native 표시에는 추가 획득이 없음을 FrameId로 검사하며 명시 호환 동기 capture도 실제 FrameId 증가로 따로 검사했다. 수정 중 문맥/순서가 맞지 않는 patch들은 검증 단계에서 거절되어 파일 변경 없이 재적용했다. 관련29·후속8/1, 최종 render20/성능/전체249 이름별 검증 후 커밋했다. 원본 실패/초기 렌더/skip은 보존하고 통과/미수행을 구분한다.

## 2026-10-13 — PERF-01 Broker 없는 범위 재개

- Source `fda25e0` vs `2a5f89b`(807f512와 production 동일), DTCoreb2504d1. 동일 test-only 계측기/map SHA, D3D12 actual viewport1920×1080/DPI1/font1,10초+60초. 1+1 Camera Payload encode/미송신,2+2 PreviewOnly/acquisition+렌더·미송신. 정지 Slab/Monitor+Placement/Camera view. 과거 실제PCD 송신 포함2+2 기록과 별도 조건.
- 초기48회 ABAB×3: FPS/enginep95 모두기준안, GT 표시1+1+2.161%,오프스크린1+1+15.928%,표시2+2+2.773% 기준초과,오프스크린2+2−4.560% PASS. 원본FAIL보존. Source62개구간 두중간지점a36179c/a96f638 A/M/A/M GT약+1.50/+1.77%기준안. Insights60초region의GameThread-only events에서Toolbarpaint+0.056ms/frame과Worldtick−0.194ms/frame이함께나타나 단일커밋원인을확정하지않음. trace는gate에서제외.
- 초과3조건 재측정36회: 표시1+1 FPS59.9841→59.9806/GT6.8210→6.9321ms(+1.629%);표시2+2 FPS59.9899→59.9858/GT8.6195→8.7503ms(+1.517%);오프스크린1+1 FPS59.9985→59.9971/GT5.1511→5.2241ms(+1.417%). enginep95모두약16.667ms. 최초오프스크린2+2 FPS59.9931→59.9950/GT8.0268→7.6607ms. 반복에서누적FPS/p95/GT회귀재현안됨;production수정0.
- Editor/Game Development exit0,verifyquick8 gateSuccess/layer0,명시Entry startup+Saved/Reports 커밋맵fixture의MA0T10 D3D12 242Success/skip22/실패·NotRun0. cda6b74 release기존263개이름/상태차이0+PERF opt-in skip1. WBP7메모리compile/저장0. quick+동등회귀이며release14단계로표시하지않음.
- 실제2+2PCD송신 Broker환경보류;시작·설정변경0. 보호파일/DTCore/운영맵/전체자산/개인SaveGame대조원본과새기준선복원은보고서. ComputerUse0. 근거Saved/Reports/PERF/PERF-01/RESULT.md, no-broker-summary.json, repeat-summary.json, cpu-event-comparison.json, validation-final/, preservation-final.json.


## 2026-10-13 — UXR-08 산화 사실화와 UI-02 CLI, 최종 gate 환경 보류

- 시작 `codex/post-program-followups`/221cd2d, DTCore b2504d1 clean. 기존 SensorTestMap 사용자 변경·SlabOxideScaleRenderTests 출력 경로 변경을 확인하고 전자는 보존, 후자는 이번 UXR-08에 사용했다. 전 과정 Entry startup/메모리 장면·기존 SlabScenarioValidationMap PIE이며 운영맵 열기/저장 없음, Computer Use0.
- `28461bc` UXR-08: 수정5파일(기존 asset1+script1+C++3). 기존 oxide만 UE API 재생성, 신규 Content asset0/texture0. 3-octave 작은 비정형 조각/미세 방향/균열/grain/normal/갈색, 냉각·무광 값 구분. display-referred 색을 linear로 변환, BaseColor ROI xyxy425,70,775,830의 조각19.04%/흰색0. pixel379→527(+39.05%), vertex131/sample3 불변. Editor Development/UXR08 D3D12 7 Success/실패·skip0(기본 Camera pixels·CPU/GPU intensity·fixed identity PCD bytes 동일 포함)/quick8/layer0. 5종+근접2+전후+basecolor, `Saved/Reports/UXR/UXR-08/oxide-realism/RESULT.md`·summary·material-statistics·sensor-regression.
- `142a6ff` UI-02: 수정13+추가4파일, 새 asset0. UStatusVisualizer 기반에서 기본2개/세션 상세 토글/priority/거리 대비 fade·크기/시야 밖 숨김/실제 UI 제외/5개 고정 leader pool(42→47). 줄·문자·배경·leader 모두 collision/SceneCapture/GPU 분류/그림자/간접광/반사/ray tracing 제외 유지. UI 사각형은 실제 Slate viewport 변환을 거쳐 L3→L2 값만 전달. 5Hz 연속 갱신, 변화 없는 반올림 text hash 재사용, hidden detail 조립 없음. 개인 SaveGame schema/개별 display 값/API 의미/측정 bytes 유지.
- Editor Development 성공. UI02·SlabAnalysis·UXR08 실제 D3D12 집중18 Success/실패·skip0, SlabAnalysis RHI opt-in1에서 Camera/GPU 격리 수행. quick8 통과(20261013T135150453Z), layer249파일/allowlist0→0/신규·stale0. before/after 두 실제 viewport×6조건(near/far basic/detail·직교 roll20·hot)/font100 각12장. after에서는 실제 배경판 투영 사각형의 viewport 내부·arranged UI 제외·상호 비겹침을 검사. `Saved/Reports/UXR/UI-02/after/automation-1280`·1920 각1Success. 자동 screenshot은 Slate border6px를 포함하며 마우스 PASS가 아님.
- release **호출1회/전체 D3D12 실행0**: `verify.ps1 -Level release -BaseCommit 221cd2d`, manifest `Saved/Reports/verify/20261013T135533554Z/manifest.json`, 시작8 gateSuccess 후 다른 ma0t10_dt_qa의 UnrealEditor-Cmd PID87208/Uxr07Manual 검사 때문에 `Close Editor and Live Coding before feature/release`로 중단. build/feature/full/WBP/rendered vocabulary release 단계 미실행. 이름별 전체 결과 비교·표시형1920×1080/1+1/10초+30초 ABAB×3 성능은 미수행. 사용자 "막히면 멈추고 보고" 적용, 외부 프로세스는 유지.
- 초기 test-only private Status/GetSize 접근 build 실패는 reflected fixture+component bounds로 수정했다. 첫 focused quick은 Git LF→CRLF stderr warning을 PowerShell이 오류로 처리해 실패, 계측기 EOL 정리 뒤quick8Success. 결과 정리 중 Python 기본 cp949가 상세 `[침범]`을 깨뜨린 것을 발견하여 UTF8 source literal로 복구하고 렌더 의미 assert를 추가했다. **이 마지막 2파일은 미커밋/Editor 빌드·실행 대기**이며 기존142a6ff 결과로 덮지 않는다. 재개 시 이 정정 검증을 먼저 수행한다.
- 보호299개 중 우리 쓰기/stage 없이 외부에서 바뀐 CLAUDE.md1개 외298 hash불변. DTCore gitlink/worktree clean, Game.ini/PixelStreaming/Build.cs·uproject/운영맵 기존 상태 보존. Content 변경은 승인oxide1개뿐. 개인 SaveGame9개 원본 hash 복원, 우리Editor/PIE종료. 외부 QA와 병행됐던 functional 검증을 성능 증거로 사용하지 않는다. font125/150/OS DPI·직접 마우스 수용은 보류, manual/CHECKLIST.md 전 행미수행. 상세 `Saved/Reports/UXR/UI-02/RESULT.md`·summary·blocking-process·before-after PNG·focused/·guard/layer JSON. alpha blend 변경 없는 text 대비 fade/거리 숨김, active/bound Slab에 UI 제외 적용이라는 범위를 명시했다.


## 2026-10-15 — DATASET-01 / GPU-CRASH-01

- 기준 f499d57, Runtime d6c41f2, DTCore b2504d1. 일반 GPU+Camera GT 동시 캡처의 stale cached shader binding/CopySrc 전환을337dd10으로 수정하고, e26a60e에서 identity proxy의 불필요한 거리장 참여를 제거했다.
- 일반4경로각100회·동시100회RHI validation 오류0·FullSpec GPU100장면×3·정답/seed/GT-off/취소/상한/복원·Python4단위검사 통과.100장면2013729점, yaw 평균0.115528°/p950.304792°/최대0.563760°.
- Editor/Game Development·release14단계·고유전체278 Success/30명시적skip/실패·NotRun0·실제WireGolden·WBP7 memory compile(저장0)·quick8·layer0. release manifest Saved/Reports/verify/20261015T100220551Z/manifest.json.
- 표시형1920×1080,1 D455+1 Native ML-X,10+30초,ABAB×3=12유효샘플. FPS59.998212→59.998579(+0.000611%), engine p9516.666900→16.666900ms, game-thread5.836946→5.832506ms(−0.076083%);2% gate통과. perf manifest Saved/Reports/verify/20261015T100925865Z/manifest.json.
- 실제1920×1080/DPI1/폰트100% 자동진행화면 확인. 직접Computer Use는0x80070057로초기+재시도2회실패,입력0·보안팝업미조작. 초기cold wall61ms peak와 geometry/PIE내resume 제한을보고했다.
- LLM before/after로distance-field 증가를제거(42.06–42.09MiB유지),readback GT3/LiDAR2·GC후UObject40465유지. 보호553파일hash불변·개인240파일복원,시험2.956GB→7.995MB/8샘플장면. 상세 Saved/Reports/DATASET-01/RESULT.md, Saved/Reports/GPU-CRASH-01/RESULT.md.


## 2026-10-15 SCN-RELEASE-01 - release terminal sensor ownership

General UI reproduction on existing SlabScenarioValidationMap at1920x1080/96DPI/font150%: observation completion and cancellation both left independent PCD sending at0/s while acquisition stayed~20Hz. After finalizing terminal snapshots and cleaning only owned work, release Targets/ControlledIds/preparation bindings; retain Status/CurrentSlab and reject late eligible run frames. Required-data configuration locks remain active through preparation/drain.

New ordinary-UI-path regression failed before the fix and passes after it. Focused D3D12 groups SlabReliability/ScenarioReplay/Automation:12 executed Success,3 explicit opt-in skips,0 failed. Editor Development build, layer0/allowlist0 and DTCore guard passed. Direct Computer Use after the fix confirmed positive independent PCD after completion (20/s) and cancellation (positive,15.7/s in captured sample), followed by normal Alt-F4 teardown. Raw receipt/end-to-end batch acceptance is recorded separately under AUTO-01. No map/asset/DTCore/config change. Detailed evidence: Saved/Reports/SCN-RELEASE-01, including before/after manual checklists and screenshots.


## 2026-10-16 AUTO-01 - automated acceptance and remaining direct UI

Final runtime f6af018 over baseline d45f27e, DTCore b2504d1. Official Python/PowerShell runners combine fresh result schema/step accounting with child status; native Windows graceful exit alone is not authoritative. No forced termination in the new operational job/API/runners. The existing standard UE automation/perf TestExit path uses Force=1 (success0, intentional pre-fix failure255); audit retained, not counted as normal teardown proof. Editor/Game Development and Development BuildCookRun succeeded; package job completed scenario + GPU20 scenes + PCD30s with598submit/receipt, no delivery error/overload. Source/dataset/result SHA agrees. Standard source SensorTransport template remains empty; packaged copy has the same9 empty keys without comments.

Full release manifest Saved/Reports/verify/20261015T153623842Z/manifest.json:14steps,284 unique executed Success,31 explicit skips,0failed/NotRun; actual WireGolden, WBP7 memory compile and rendered vocabulary passed. Python12; native job/API/auth/default-off/timeout and external UI ownership cases passed. SDK status/settings/new Camera capture rerun on final revision. Root SCN fix is separately committed9fa04be; six CPU/socket fault modes passed.

Default-off standard perf against d45f27e: Saved/Reports/verify/20261015T154236642Z, ABABx3,12valid samples, visible verified1920x1080/DPI1,1Camera+1LiDAR,10+30seconds, same map/dependency/instrument, no stream/record/transport. Median callback FPS59.994752->59.979625(-0.025214%), engine p9516.666900ms unchanged, GT6.012903->6.048363ms(+0.589723%),2%PASS.

Direct API checkbox/address/batch-status acceptance remains environment pending: first visible screenshot showed Windows Security public/private network permission for UnrealGame. No dialog input/bypass; bounded session exited normally. User handoff requested, not assumed. Standalone D3D12/offscreen has a startup handled SwapChain ensure22Error lines; generated20-scene existing-map PCA is invalid in all scenes, so no accuracy/training acceptance. Broker receipts are not consumer/business ACK. Protected557files unchanged,44personal files restored, own Broker stopped/config12unchanged, ports closed, private files removed. Details/artifacts/failure histories: Saved/Reports/AUTO-01/RESULT.md.
