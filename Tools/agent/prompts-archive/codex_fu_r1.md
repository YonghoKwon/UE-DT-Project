[Claude → Codex 작업 지시 · FOLLOWUP-26-28]

저장소: C:\Unreal Projects\ma0t10_dt (UE 5.3, DX12, r.RayTracing=True 기본 — 사용자 수용 결정). 
브랜치: 현재 체크아웃된 `chore/hatch-corner-captures`(2467059, 코너 캡처 테스트 모드 포함)에서 `feat/followups-26-28` 생성 — 이 PR에 캡처 모드 커밋도 함께 포함된다. perf BaseCommit은 989e7a0. 항목별로 별도 커밋.
보고 폴더: Saved/Reports/FOLLOWUP-26-28/ (progress/, DONE.md, RESULT.md). 진행 순서: #27 → #28 → #26.

## #27 CPU 기준 경로 BVH X=0 경계 광선 오차
- GPU-LIDAR-01에서 X=0 경계 방향 40개 광선이 CPU BVH(ShipHatchRayQuery.cpp 등 snapshot BVH)와 최대 15.714 m 차이, native physics와 GPU는 일치. 증거: Saved/Reports/GPU-LIDAR-01/RESULT.md, progress/.
- 원인 규명(예: 방향 성분 0일 때 역수/slab test, 분할면 경계 포함 조건, 삼각형 edge 판정 epsilon 등) 후 수정.
- 회귀 테스트: 축 정렬/경계 방향 광선(±X,±Y,±Z 성분 0, 분할면 위 원점) 대량 케이스에서 CPU vs physics(vs GPU 가능 시) 일치. 기존 4096 광선 정확도 기준(1 mm) 유지·강화만 허용.
- 수정 후 CPU 측정 Hz 퇴행 없음(해치 기본 3센서 CPU 경로 ~10 Hz 수준).

## #28 OS1-64×3 PCD 송신 예산 초과로 송신 생략
- 현상: 기본 64 MiB/s 예산에서 OS1-64×3 구성 시 한 스트림에 송신율 조절 생략 179~189회, 실제 송신 13.3~19.8 Hz(스트림 간 불균형).
- 할 일: (1) 실제 필요 대역폭 계산·측정(점 수×33 B×Hz×센서). (2) **스트림 간 공정성** — 한 센서만 굶지 않도록 예산을 센서 간 공정 배분(round-robin/토큰 버킷 per stream 등). (3) 예산이 부족하면 UI/상태 텍스트에 "필요 X MiB/s > 예산 Y MiB/s" 경고와 실제 송신 Hz 표시. (4) 예산 값은 설정에서 조정 가능하게(기본 64 MiB/s 유지 — 기본값 변경은 근거와 함께 제안만, 임의 변경 금지).
- PCD 33-byte 포맷·wire 계약·출력 정책(PCD 기본 on) 불변. 비밀번호/토큰 사용 금지, 로컬 loopback 수신기로 검증.
- 기준: OS1-64×3 송신 각 스트림 ≥10 Hz 이고 스트림 간 송신 Hz 편차 축소(수치 기록), 기본 3센서 20 Hz 유지.

## #26 해치 디깅 RSS 8 MiB 게이트 실패
- HATCH-06 결과: 측정 경계 수정(f417e50) 후에도 5쌍 중 3쌍 실패, 차이 중앙값 8.39 MiB, σ 5.62, ~40회 지점 계단식 증가 후 평탄, RHI/UObject 수 고정, allocator 보유량 의심. 증거: Saved/Reports/HATCH-06/v2/…, RESULT.md. GPU 경로 50회 RSS는 −0.047 MiB(기록만).
- 이제 기본이 GPU 경로이므로 **현재 기본 설정(RT on, GPU backend)** 에서 기존 게이트(절대 8 MiB + 50회−0회 대조 8 MiB, 5쌍)를 다시 측정. 통과하면 그 결과로 종료(CPU 경로 결과는 별도 표로 기록).
- 실패가 남으면 실제 증가원 추적(LLM 태그별 diff, memreport, allocator 통계, Chaos cooking/PMC 섹션/동적 버퍼 성장 정책) 후 실제 원인 수정. 게이트 기준·횟수·시간 완화 금지. 조사 시간 상한 약 3시간, 초과 시 근거 정리 후 종료.

## 공통 게이트(완화 금지)
- `Scripts/verify.ps1 -Level release` 실패 0, perf ABAB 2%는 **RT on 기준**(`-PerfRayTracing ProjectDefault -BaseCommit <브랜치 분기 master SHA>`)으로 PASS, layer gate 0→0.
- 금지/보호: DTCore 수정 금지, 플러그인화 금지, `git add -A` 금지. Config/Game.ini, Samples/PixelStreaming, SensorTestMap 저장, CLAUDE.md, docs/IMPROVEMENTS_CLAUDE.md, Saved/Reports/Inspection 원본 손대지 말 것. Config 파일은 필요한 키만 조회(전체 출력 금지). Config/SensorTransport.ini 빈 템플릿 유지. 개인 SaveGame/Editor 설정 백업·복원. Windows 잠금/보안 팝업 조작 금지(잠금이면 Computer Use 단계만 건너뛰고 기록). Docker 금지. CRLF 유지, 자기 헤더 먼저 include.
- 단계마다 짧은 한국어 진행 메시지, 끝나면 DONE.md 요약.
