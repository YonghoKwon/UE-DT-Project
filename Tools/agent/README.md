# 에이전트 작업 하네스

운영 방식 v2(2026-10-22): Claude 계획·예비안·독립 검수·머지, Codex 실행·보고, 사용자 결정·현장 검증. 권한·보호 규칙의 원본은 [AGENTS.md](../../AGENTS.md) §6이며 하네스는 제품 구현 승인이 아니다.

1. [작업 지시서](templates/task-prompt.md)에 목표·기준 SHA·허용 경로·예비안·gate를 채우고 [공통 규칙](templates/base-rules.md)을 참조한다.
2. 측정 없는 병렬 작업은 승인된 worktree에 한정하고 [추가 제한](templates/parallel-worktree.md)을 붙인다.
3. Codex는 증거와 [schema](schema/result.schema.json)에 맞는 `Saved/Reports/<ID>/result.json`을 작성한다. [GPU-LIDAR 실제 예시](examples/result.example.json)는 최신 RT on FAIL을 보존한 이력이며 현재 branch의 통과 증거가 아니다.
4. Windows PowerShell 5.1에서 검증한다. 외부 모듈·Editor·빌드를 쓰지 않는다.

```powershell
powershell -NoProfile -File Scripts/validate_agent_result.ps1 -ResultJson Saved/Reports/<ID>/result.json
powershell -NoProfile -File Scripts/merge_check.ps1 -ResultJson Saved/Reports/<ID>/result.json -BaseCommit <sha>
powershell -NoProfile -File Scripts/test_merge_check.ps1
```

result 및 manifest·artifact 상대 경로는 저장소 기준이다. merge_check는 result 옆 `merge-check.json`을 쓰고 exit 0=머지 가능 후보, 1=사용자 확인 필요, 2=차단을 반환한다. FAIL·보호 경로 변경·잘못된/실패 manifest·비밀 노출은 차단한다. NOT_RUN·SKIPPED(사유 필수)·테스트/게이트 변경·미복원 설정·결정 필요는 확인 대상으로 남긴다. manifest는 현재 verify 형식(Valid/Failure/FailedNames/Steps/Tests) 및 숫자 Failed 집계를 검사한다. 자기 보고만으로 실제 수용을 보장하지 않는다. Claude가 독립 diff·증거 리뷰 후 머지한다.

변경 경로는 `base...head`의 merge-base 기준으로 계산한다. DTCore·지정 보호 경로와 `Content/MA0T10/Maps/SensorTestMap.umap`은 BLOCK, 그 외 `.uasset/.umap`은 REVIEW다. `Source/`, `Config/`, `Content/`, `Shaders/` 변경에 RT ProjectDefault perf PASS가 없으면 별도 REVIEW를 남긴다. 보고 커밋 뒤의 후속 커밋은 개수·제목과 함께 REVIEW, 조상이 아닌 보고 커밋은 BLOCK이다. branch는 로컬 ref, `origin/<branch>` 순으로 확인하며 둘 다 없으면 REVIEW다. manifest의 skip은 gate당 한 줄에 개수를 기록한다.

측정의 PASS/FAIL은 숫자 `value`와 `threshold`가 필수다. NOT_RUN/SKIPPED는 두 필드를 생략할 수 있다(값을 제공하면 숫자여야 한다). 정보용 INFO는 숫자 value만 기록하고 threshold를 두지 않는다. INFO는 gate enum을 확장하지 않으며 성능 gate 통과를 뜻하지 않는다. 예시에 실제 단일 pair 메모리 관찰 INFO와 미수행 현장 검증을 포함했다.

범용 JSON Schema 엔진 대신 schema에서 사용한 필수 필드·타입·enum·배열/객체·숫자·최소 길이·oneOf를 PS 5.1에서 재귀 검증한다. 다른 JSON Schema 키워드를 지원한다고 주장하지 않는다. 테스트/게이트 변경이나 보호 경로 예외를 자동 승인하지 않는다.

## 과거 지시서 목록

29개 파일은 실행 지시가 아닌 이력이다. 현재 규칙·권한을 덮어쓰지 않는다. 원래 파일명을 보존했다. 2026-10-22 토큰/비밀번호/개인키/서비스 키 패턴 검사에서 비밀값 후보 0개(정적 패턴 검사의 한계 있음). 검사 출력에는 원문 값을 싣지 않는다.

| 파일 | 첫 줄 / 제목 |
|---|---|
| [codex_coil31_r1.md](prompts-archive/codex_coil31_r1.md) | [Claude → Codex 작업 지시 · #31 COIL-01 제품 부두 코일 선적 시뮬레이션] |
| [codex_coilres_r1.md](prompts-archive/codex_coilres_r1.md) | [Claude → Codex · 병렬 조사 작업 (측정 없음) · COIL-01 근거 자료] |
| [codex_fu_r1.md](prompts-archive/codex_fu_r1.md) | [Claude → Codex 작업 지시 · FOLLOWUP-26-28] |
| [codex_fu_r2.md](prompts-archive/codex_fu_r2.md) | [Claude → Codex 범위 추가 · 같은 세션 이어서 · 사용자 요청 2026-10-22] |
| [codex_fu_r3.md](prompts-archive/codex_fu_r3.md) | [Claude → Codex 검수 피드백 · 같은 세션 이어서] |
| [codex_gpu25_r1.md](prompts-archive/codex_gpu25_r1.md) | [Claude → Codex 작업 지시 · #25 GPU LiDAR (UE 5.3 하드웨어 레이트레이싱)] |
| [codex_gpu25_r2.md](prompts-archive/codex_gpu25_r2.md) | [Claude → Codex 결정 · 같은 세션 이어서] |
| [codex_gpu25_r3.md](prompts-archive/codex_gpu25_r3.md) | [Claude → Codex 리뷰 후 추가 검증 · 같은 세션 이어서] |
| [codex_gpu25_r4.md](prompts-archive/codex_gpu25_r4.md) | [Claude → Codex 추가 요청 · 같은 세션 이어서 · 사용자 요청 2026-10-22] |
| [codex_gpu25_r5.md](prompts-archive/codex_gpu25_r5.md) | [Claude → Codex · 같은 세션 이어서] |
| [codex_hatch6.md](prompts-archive/codex_hatch6.md) | [Claude → Codex 작업 요청 · CLI 새 세션] HATCH-06: 원료 사실성 · LiDAR 5종 실스펙 · 해치 UX/UI 세련화(설정과 오른쪽 화면 동시 보기) · GTSU 하역기 사실화 — 사용자 요청 2026-10-21 |
| [codex_hatch6_cont.md](prompts-archive/codex_hatch6_cont.md) | [Claude → Codex] 직전 턴이 모델 용량 오류(Selected model is at capacity)로 중단됐다. 중단 시점(분석 작업 임시 할당·렌더 자원 계측으로 일반 3D 50회 RSS 초과 원인 조사, run_stage.ps1 수정 중)부터 그대로 이어서 HATCH-06을 끝까지 완료하라. 작업 트리의 미커밋 변경을 이어서 사용. 기존 요구·원칙·운영·검증·금지는 그대로. |
| [codex_hatch6_r1.md](prompts-archive/codex_hatch6_r1.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] HATCH-06 추가 요구 5 (사용자 2026-10-21) |
| [codex_hatch6_r10.md](prompts-archive/codex_hatch6_r10.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 우선 참고 이미지 지정 (사용자 2026-10-21) |
| [codex_hatch6_r11.md](prompts-archive/codex_hatch6_r11.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 점군 표시 카드가 3D 화면을 가림 (사용자 2026-10-21) |
| [codex_hatch6_r12.md](prompts-archive/codex_hatch6_r12.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 호퍼 위치 정정 (사용자 2026-10-21) |
| [codex_hatch6_r13.md](prompts-archive/codex_hatch6_r13.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 측정 주기 성능 개선 (사용자 2026-10-21) |
| [codex_hatch6_r14.md](prompts-archive/codex_hatch6_r14.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] Claude 검토: 미리보기 점 예산과 RSS 판단 (2026-10-21) |
| [codex_hatch6_r15.md](prompts-archive/codex_hatch6_r15.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] HATCH-06 후속: 메모리 해결 + 선박·하역기 모델 품질 (사용자 2026-10-21) |
| [codex_hatch6_r16.md](prompts-archive/codex_hatch6_r16.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 원료 모델링 품질도 향상 (사용자 2026-10-21) |
| [codex_hatch6_r17.md](prompts-archive/codex_hatch6_r17.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 측정 주기 기준 확정 + 일정 (사용자 2026-10-21) |
| [codex_hatch6_r2.md](prompts-archive/codex_hatch6_r2.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] HATCH-06 추가 요구 6 (사용자 2026-10-21) |
| [codex_hatch6_r3.md](prompts-archive/codex_hatch6_r3.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] HATCH-06 추가 요구 7 — 실제 크기 비례 (사용자 2026-10-21) |
| [codex_hatch6_r4.md](prompts-archive/codex_hatch6_r4.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] HATCH-06 사용자 참고 사진 3장 반영 (2026-10-21) |
| [codex_hatch6_r5.md](prompts-archive/codex_hatch6_r5.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 해치 커버 개폐 방향 정정 (사용자 2026-10-21) |
| [codex_hatch6_r6.md](prompts-archive/codex_hatch6_r6.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] Claude 검토: 20-20261021-1316-coal-full-floor-3d.png 기준 (2026-10-21) |
| [codex_hatch6_r7.md](prompts-archive/codex_hatch6_r7.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 붐 끝 센서 기본 모델 변경 (사용자 2026-10-21) |
| [codex_hatch6_r8.md](prompts-archive/codex_hatch6_r8.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 그랩 표시 토글 + 그랩 모델링 품질 (사용자 2026-10-21) |
| [codex_hatch6_r9.md](prompts-archive/codex_hatch6_r9.md) | [Claude → Codex 추가 지시 · 같은 세션 이어서] 하역기·선박 참고 사진 추가 (사용자 2026-10-21) |
