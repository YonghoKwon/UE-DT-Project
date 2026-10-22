[Claude → Codex 작업 지시 · #25 GPU LiDAR (UE 5.3 하드웨어 레이트레이싱)]

저장소: C:\Unreal Projects\ma0t10_dt (UE 5.3, DX12). 브랜치: `feat/gpu-lidar-rt` (Claude가 master 0db8246에서 이미 생성·체크아웃함, HATCH-06 #66 머지 포함).
참고: HATCH-06 RSS 게이트 실패는 별도 후속 #26으로 등록됨 — 이번 범위 아님. 단 GPU 경로의 RSS(50회 디깅)는 측정해 기록만 할 것.
작업 ID: GPU-LIDAR-01. 보고 폴더: Saved/Reports/GPU-LIDAR-01/ (progress/, captures/, DONE.md, RESULT.md).

## 목표 (사용자 확정)
- Isaac Sim RTX LiDAR 수준: 기본 해치 3센서(붐 XT32 + 캐빈 2대) **필수 ≥10 Hz, 목표 20 Hz**, 60 FPS 근처 유지.
- 고사양 구성(OS1-64 ×3, 현재 CPU 3.3–4.3 Hz)도 ≥10 Hz 목표.
- 현장/서버 PC는 모두 RTX. 개발 PC RTX 4070 Ti.

## 방향
1. 먼저 조사(웹 검색 허용): Isaac Sim RTX LiDAR 구조(OmniLidar/RTX sensor, 프로필, 출력), UE 5.3 HW RT에서 커스텀 레이 쏘기 방법
   - 후보 A: RDG Global shader + `RayTracingPipeline` ray-gen(.usf, `TraceRay`) — scene TLAS(`FRayTracingScene`) 재사용
   - 후보 B: inline ray query(`RayQuery`, SM6.5) compute shader
   - 후보 C: Lumen/HWRT 경로 재사용 가능성
   UE 5.3 엔진 소스(C:\Program Files\Epic Games\UE_5.3\Engine)에서 실제 API 확인 후 선택, 근거를 RESULT.md에 기록.
2. 구현: 기존 LiDAR 광선 패턴(프로필 5종+기존 것)을 GPU 버퍼로 올리고, 결과(거리/hit/법선/semantic label 또는 actor id/강도)를 비동기 readback(`FRHIGPUBufferReadback`, 프레임 지연 허용, 게임 스레드 블로킹 금지).
   - 결과는 기존 normalized frame 주입 경로(`InjectPointCloudFrame` 등)와 payload/PCD/recorder/preview/Slab 분석을 그대로 통과.
   - semantic label(Slab 등)·재질 강도는 CPU 경로와 동일 의미 유지(primitive id → actor 매핑 등).
3. 설정
   - 센서/매니저 단위 `Backend = Auto | GpuRayTracing | Cpu`. Auto: RT 지원 시 GPU, 아니면 CPU fallback.
   - 프로젝트 레이트레이싱 활성화(`r.RayTracing=1`, `r.SkinCache.CompileShaders=1` 등)는 필요한 최소만. 다른 렌더 기능(Lumen HWRT, RT 그림자 등)은 **기본 off 유지**해 화면/성능 영향 최소화. 영향(쉐이더 컴파일 시간, 메모리, 기존 맵 FPS)을 측정해 기록.
   - CPU 경로는 fallback + 정확도 기준으로 그대로 유지.
4. 정확도: 동일 장면·동일 광선에서 GPU vs CPU 거리 오차(중앙값/95%/최대), hit 일치율, label 일치율 측정. 기준: hit 일치 ≥99%, 거리 95% ≤1 cm(정적 지오메트리). 동적 메시(원료 표면 갱신, 그랩)가 TLAS에 반영되는지 확인 — 원료 디깅 후 다음 스캔에서 GPU 결과가 새 표면을 보는지 테스트.
5. 성능: 기본 3센서, OS1-64×3, 기존 운영 맵(SensorTestMap은 열기만, 저장 금지) 각각 Hz/FPS/150 ms 초과 프레임/GPU 시간. 기존 perf 게이트(ABAB 2%)는 RT off 상태 기준으로 회귀 없음 확인, RT on 영향은 별도 표.

## 게이트 (완화 금지)
- `Scripts/verify.ps1 -Level release` 실패 0, perf 2% PASS(RT 기본 off 경로), 레이어 게이트 0→0, 새 자동화 테스트(GPU 정확도/동적 반영/fallback) 추가.
- 테스트 기대값을 낮추지 말 것. 실패는 그대로 기록.

## 금지/보호 (기존 규칙 동일)
- DTCore 수정 금지, 플러그인화 금지, `git add -A` 금지, 파일 단위 add.
- 손대지 말 것: Config/Game.ini, Samples/PixelStreaming, SensorTestMap 저장, CLAUDE.md, docs/IMPROVEMENTS_CLAUDE.md, Saved/Reports/Inspection 원본.
- 비밀번호/토큰 출력·커밋 금지, Config/SensorTransport.ini는 빈 템플릿.
- 개인 SaveGame/Editor 설정은 백업 후 복원. Windows 보안 팝업/잠금화면 조작 금지. Docker 금지.
- CRLF 유지(autocrlf=true). 자기 헤더 먼저 include.
- DefaultEngine.ini 변경 시 정확한 diff와 이유를 RESULT.md에 기록.

## 진행 보고
- 단계마다 Saved/Reports/GPU-LIDAR-01/progress/ 에 캡처(점군 GPU vs CPU 비교 등) 저장, 짧은 한국어 진행 메시지.
- 완료 시 DONE.md: 선택 방식·근거, Hz/FPS 표, 정확도 표, RT 활성화 영향, 남은 한계.
