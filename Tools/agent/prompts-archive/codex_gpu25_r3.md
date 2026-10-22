[Claude → Codex 리뷰 후 추가 검증 · 같은 세션 이어서]

훌륭합니다. 머지 전 두 가지만 보완해 주세요(코드 변경이 필요하면 별도 커밋, 기준 완화 금지).

1. **RT 켜짐(운영 기본값) ABAB 2% 게이트**: 이번 커밋으로 프로젝트 기본이 r.RayTracing=True가 되므로 실제 사용자 조건은 RT on이다.
   `Scripts/verify.ps1 -Level perf -BaseCommit 0db8246 -PerfRayTracing ProjectDefault` (12회, 기존과 같은 맵/조건)를 실행해 2% 판정. 실패 시 원인(GPU time, RT scene 유지 비용 등)을 조사·개선하고, 개선 불가면 수치와 근거를 그대로 기록.
2. **기본 출력 정책 조건 Hz**: 출력 정책은 "PCD 송신 기본 on"이다. 현재 20Hz는 송신/지속 기록 off 조건이다.
   기본 송신 설정(PCD on, transport는 외부 서버 없이 로컬에서 가능한 기본 모드 — LogOnly 또는 로컬 loopback 수신기; 비밀번호/토큰 사용 금지)으로 기본 3센서와 OS1-64×3의 Hz/FPS/150ms 초과를 30초 측정. 필수 ≥10Hz. 미달이면 송신/직렬화 경로 병목을 worker로 옮기는 등 개선(PCD 33-byte 포맷·wire 계약 불변).
3. 결과를 RESULT.md/DONE.md에 표로 추가, 보호 경로·개인 설정 복원 규칙 동일. 완료 후 짧게 보고.
