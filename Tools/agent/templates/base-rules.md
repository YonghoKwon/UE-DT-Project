# 공통 작업 규칙

저장소 AGENTS.md와 함께 적용한다. 템플릿은 권한을 확대하지 않는다. 충돌하면 기존 보호 규칙을 우선하고 보고한다. 사용자가 명시한 경로·범위·승인은 우선한다.

- 시작 시 작업 경로·branch/HEAD/remote/status를 기록하고 기존 변경을 보존한다. 한 요청의 한 항목만 작업한다.
- 보호 경로: `Config/Game.ini`, `Samples/PixelStreaming`, 사용자 운영맵, `Content/MA0T10/Maps/SensorTestMap.umap` 저장, `CLAUDE.md`, `docs/IMPROVEMENTS_CLAUDE.md`, `Saved/Reports/Inspection` 원본 수정·stage 금지.
- DTCore 소스·parent gitlink 수정, 자동 submodule update/reset, 센서 플러그인화 금지. DTCore 기반과 프로젝트 adapter 경계를 유지한다.
- 비밀번호·토큰·개인키 사용·출력·복사·커밋 금지. Config는 필요한 키만 조회하고 전체 출력하지 않는다. 로컬 검증은 자격증명 없는 loopback을 사용한다.
- 개인 SaveGame·Editor 설정·창 배치는 조작 전에 백업하고 종료 시 원본 집합·해시를 확인해 복원한다. 미조작이면 결과 note에 명시한다.
- Windows 잠금 화면·보안 팝업 조작 금지. 잠금 시 허용된 오프스크린 검증만 수행하고 직접 마우스 검증은 미수행으로 기록한다. Docker 금지.
- CRLF 유지, C++는 자기 헤더 먼저 include. 새 모듈·자산은 지시 범위 안에서만 만들고 목록을 기록한다.
- 테스트 기대값·정확도·Hz·2%·150ms 등 기준 완화 금지. 실패/skip/다른 SHA 증거를 PASS로 표시하지 않는다.
- RT on 성능 기준: `Scripts/verify.ps1 -Level perf -BaseCommit <sha> -PerfRayTracing ProjectDefault`. RT off·진단 실행은 별도 기록하며 정식 gate를 대신하지 않는다. 다른 Editor/GPU 작업 중 측정 금지.
- PCD 송신 기본 on, binary 33-byte 레코드·wire 계약·FIFO·기존 예산/예약 상한 보존. acquisition/serialize/submit/receipt/검증 수신과 생략/gap을 구분한다. receipt는 업무 ACK가 아니다.
- Editor/빌드/측정은 지시서가 허용한 경우만 수행한다. `git add -A` 금지, 명시 파일만 stage. push/PR/merge는 별도 권한 없이 하지 않는다.
- 20분마다 진행 보고(변화 없으면 한 줄). 결정 필요·이미지 산출·완료·실패는 즉시 보고. 예비안은 재질문 없이 적용하고 선택·근거를 기록한다. 보호 경로/계약 변경 등 진짜 차단만 멈춘다.
- 완료 시 `Saved/Reports/<ID>/result.json`을 schema에 맞게 작성한다. 수행/실패/skip/미수행·설정 복원·한계를 사실대로 기록하고 merge_check 결과를 첨부한다.
