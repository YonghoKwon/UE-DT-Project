[Claude → Codex 작업 요청 · CLI 새 세션] HATCH-06: 원료 사실성 · LiDAR 5종 실스펙 · 해치 UX/UI 세련화(설정과 오른쪽 화면 동시 보기) · GTSU 하역기 사실화 — 사용자 요청 2026-10-21

작업 폴더 `C:\Unreal Projects\ma0t10_dt`, 브랜치 **feat/ship-hatch-v6** (Claude가 master 03ce11f에서 생성). 새 브랜치/worktree 만들지 말 것.
먼저 읽을 것: AGENTS.md, README HATCH 절, `Saved/Reports/HATCH-01~05/`(RESULT·DONE·captures), HATCH-05-LABELS·TESTSET-PERF 커밋(사용자가 Codex로 진행한 최근 작업), 해치 관련 소스 전체, 기존 LiDAR 장비 프로필 체계(`EVirtualLidar*Profile`·DeviceSpec·스캔 패턴), 머티리얼 `M_HatchCargo`.

■ 사용자 요청 (원문 요지)
"원료 선박 해치 시뮬레이션이 너무 잘 구현되었는데,
1) 원료의 형태가 좀 더 사실적으로 표현되면 좋겠고,
2) 센서 종류가 현재 사실상 2개인데, 인터넷 검색을 통해 5개 정도 센서 스펙을 동일하게 세팅해서 적용하고 선택해서 실제 확인할 수 있게,
3) 전체적인 UX/UI를 검토해서 좀 더 세련되게, 세부 센서 설정 위젯/설정을 볼 때 오른쪽 2개 화면을 동시에 볼 수 없는 단점을 감안해서 수정,
4) GTSU(Grab Type Ship Unloader) 하역기 형상을 좀 더 사실적으로."

■ 구현
1. **원료 사실성(철광석·석탄)**: 높이장 해상도/디테일 보강(근거 기반 해상도·LOD), 그랩 하역 현장 같은 표면 — 크고 작은 덩어리(lump)·입도 노이즈, 그랩 자국·사면 붕괴 줄무늬, 안식각 사면, 홀드 벽 쪽 쌓임, 분진/미분 색 차이, 젖은/마른 톤 미세 변화. 철광석(적갈·회갈, 펠렛/분광 혼합 느낌), 석탄(흑색, 약한 광택, 각진 조각). 절차 노이즈·머티리얼 파라미터 우선, 새 자산은 꼭 필요할 때 최소(근거). LiDAR 반사·점군도 재질에 맞게. 성능(해치 맵 60 FPS 근방, 150 ms 초과 0, RSS 기준) 유지.
2. **LiDAR 5종 실스펙**: 웹 검색(가능하면 web search 사용, 제조사 공식 데이터시트 우선)으로 산업/항만에 쓰이는 LiDAR 5종을 선정해 **같은 항목 체계**로 프로필화: 모델명·제조사·채널/빔 수, 수평·수직 FOV, 각해상도, 스캔 패턴(회전식/비반복 등), 최대 거리(10% 반사율 기준 등 조건 명시), 거리 정밀도, 점 속도(pts/s), 프레임 주기, 다중 반사(echo) 수, 파장, 출처 URL·확인 날짜. 후보(검증 후 확정): Livox Mid-360, Livox Avia(또는 HAP), Ouster OS1-64(또는 OS0), Hesai XT32(또는 Pandar), Velodyne VLP-16, SICK(예: MRS1000/LMS) — 기존 IYOBOT ML-X80·Mid-360S와 합쳐 선택 가능. 웹 검색이 불가하면 알려진 공개 스펙을 쓰되 "미확인"으로 표기하고 RESULT에 기록. 각 프로필은 기존 스캔 시뮬레이션(패턴·FOV·노이즈·거리 한계·echo)에 실제로 반영되어 점군 밀도/커버리지 차이가 눈에 보여야 함. 해치 센서 3대 각각 모델 선택·즉시 재측정·커버리지 비교표(센서별 점 수·커버리지·사각 %) 화면 제공. 기존 맵 기본 센서/프로필 기본값 불변.
3. **UX/UI 세련화**: 해치 모니터링 전체 검토 — 시각 위계·간격·타이포·아이콘·색 의미·상태 표시 정리. **설정 패널이 오른쪽 2D/단면 영역을 가리는 문제 해결**: 설정을 (a) 왼쪽 3D 위 접이식 사이드 드로어 / (b) 하단 도크 / (c) 3D 영역 내부 오버레이 중 근거 있게 선택해 **설정을 연 상태에서도 오른쪽 두 화면을 동시에 볼 수 있게**. 센서별 설정은 탭/아코디언으로 압축, 자주 쓰는 조작(원료·센서 모델·퍼내기·조명·보기 전환)은 상단 툴바. 반응형(1920×1080·1280×720·글자 150%). 공통 Presentation Style 토큰 유지·확장.
4. **GTSU 사실화**: 일반적 그랩식 연속 하역기 구조 — 레일 위 갠트리(문형) 다리·바퀴/대차, 육측 백리치·카운터웨이트, 해측 붐(트러스 형상, 붐 끝 센서), 트롤리·로프·그랩 버킷(클램셸), 운전실(해치 위 트롤리 연동 또는 고정 — 현장 배치: 운전실 쪽 센서 2대), 호퍼(육측)·컨베이어. 저폴리 절차 형상이되 실루엣·비례가 사실적이게(치수 근거와 설정값). 센서 장착 위치(붐 끝 1, 운전실 2) 유지. 시야 가림은 실제 구조처럼 반영하고 사각 %에 표시.

■ 원칙
해치 맵 밖 비용 0, 기존 맵·기본 PCD 송신·wire·SaveGame·Blueprint API 불변. 새 .uasset/.umap은 꼭 필요할 때 최소(근거). 퍼내기·되돌리기·단면·좌우 일치(HATCH-05) 회귀 유지.

■ 운영 (사용자 요청)
Claude가 20분마다 진행을 확인해 보고한다. **의미 있는 화면 변화마다 `Saved/Reports/HATCH-06/progress/NN-YYYYMMDD-HHMM-<내용>.png` 1~2장 저장**(성능 측정 중 제외). 끝까지 스스로 완료 후 `Saved/Reports/HATCH-06/DONE.md`(AGENTS.md §6, 미해결 실패 맨 위) + `captures/`(원료 전후·센서 5종 비교·설정 열린 상태에서 3분할·GTSU 전후, 1920×1080·1280×720 150%). push/PR/merge 금지.

■ 검증
- 자동화: 센서 프로필 값 = 문서 표, 프로필별 점 수/FOV/거리 한계 반영, 설정 패널 열린 상태에서 오른쪽 두 영역 가시 영역 ≥ 기준(명시), 퍼내기/되돌리기/단면/좌우 일치 회귀, RSS·150 ms.
- 회귀: release 전체, WBP, 어휘, golden, Python, quick, layer 0, `Scripts/verify.ps1 -Level perf -BaseCommit 03ce11f`(2%, 기본 60초), 해치 맵 성능(3대·모델별).
- 직접 UI(가능하면). 잠금·캡처 오류·보안 팝업이면 입력 없이 기록.
- 산출물: `Saved/Reports/HATCH-06/RESULT.md`(센서 스펙 표·출처 포함), docs/IMPROVEMENTS.md HATCH-06(출처: 사용자 2026-10-21), README HATCH 절, MANUAL_CHECKLIST. 커밋 접두 HATCH-06.

■ 금지
DTCore 수정, 플러그인화, Config/Game.ini, Config/SensorTransport.ini 값 커밋, Samples/PixelStreaming, 운영맵 SensorTestMap 저장·스테이징(사용자 수정분 보존), 마켓 자산, CLAUDE.md / docs/IMPROVEMENTS_CLAUDE.md, `Saved/Reports/Inspection/` 원본 수정, push/PR/merge, git add -A, 비밀번호·토큰 출력, Docker, WSL, Windows 보안 팝업·잠금 화면 조작, 테스트 기대값 완화. 개인 SaveGame·Editor 설정 백업·복원.
