# 알려진 미해결 — 현재 단일 목록

기준: #99~#104 완료, master 80e03ed1, 2026-11-05. 이 목록은 관측 사실·미수용 범위를 관리하며, 새 구현 승인이나 기준 완화가 아닙니다. release PASS와 현장/장시간 수용을 구분합니다.

## 1. 해치 메모리 증가 — FAIL 유지, 사용자 결정으로 진행

- r55 해치2시간: warm-up30분 제외 30~120분 OS Commit OLS **+10.53 MiB/h**.
- 승인된 해치3시간 재측정: 30~180분 **+9.78 MiB/h**. 기준≤8 MiB/h를 넘었으므로 두 원본은 **FAIL**입니다.
- 약2시간 시점에 LLM Untracked 약 **+12MiB 계단**이 관측됐습니다. 시점/크기는 실행마다 같다고 확인되지 않았고, 실제 할당 호출자·증가 원인·현재 반복 여부는 **미확인**입니다. 새 채널 객체나 실제 누수로 확정하지 않습니다.
- 사용자는 재측정 없이 다음 단계 진행을 결정했습니다. 해결이나 PASS 전환이 아니며, 재확인은 별도 예정입니다. 과거 r37/r50 통과값을 현재 실패 대신 쓰지 않습니다.
- r55 원본: `Saved/Reports/DATA-IF-10/r55/RESULT.md`, `memory-summary.json`, `hatch-step-analysis*.json`. r55f 최종 회귀: `Saved/Reports/DATA-IF-10/r55f/RESULT.md`(측정 원본 사본: `C:\Unreal Projects\_evidence\wt_dataif`). 원시 이력은 [VALIDATION_HISTORY](VALIDATION_HISTORY.md)에 보존합니다.

## 2. 장애 전환 시 PCD 유실 허용·확인된 유실 표시

정상 연결의 ConnectedNoLoss FIFO/예약 상한은 유지합니다. 브로커 장애·재연결·DROP 부하 상황의 PCD gap은 사용자 승인 범위에서 허용되며, 연결 복구가 무유실/소비자 처리 완료를 뜻하지 않습니다. STRUCTURE-12A는 중단 시작·복구·기간과 센서별 확인된 PCD 유실(로컬 거절·전송 계층 최종 실패)을 집계하고 복구 때 Warning 1회, 기존 연결 상태 UI, 채널 dropped 누적으로 표시합니다. 실제 소비자의 수신 완료나 브로커 내부 DROP까지 정확히 측정하는 기능과는 구분합니다.

소비자 정지 중 Artemis DROP은 새 메시지를 버리고 기존 backlog를 보존합니다. 기존 durable 구독이 회복 즉시 최신 프레임부터 받는다고 보장하지 않습니다. 신규 non-durable 구독으로 확인한 최신 프레임 회복과 구분합니다. 설정·수신 의미는 [Artemis 가이드](ARTEMIS_PCD_SETTINGS.md)를 따릅니다.

## 3. DTCore 다른 섹션의 loose 적용은 범위 밖

DTDevice는 패키지의 DefaultGame.ini와 DTCore 기준 Game.ini/플랫폼 후보에서 **허용된 연결 속성과 장치 토픽 목록만** 읽습니다. DTCore의 API·로그·DataTable·다른 섹션 전체를 loose로 재적용하는 기능은 이번 범위 밖입니다. 직접 인증값도 가져오지 않습니다. 전역 GConfig를 복사/변경/저장하지 않으며 DTCore 소스 수정으로 확대하지 않습니다.

운영에서 쓰는 API·WebSocket 주소 등 DTCore 값은 DTCore의 `TryReadRuntimeOverride`가 패키지 폴더의 설정 파일을 직접 읽으므로 현장 수정이 반영된다(사용자 운영 확인, 2026-11-05). STRUCTURE-12B에서 채널·토픽 읽기도 같은 Game.ini 후보 경로를 지원합니다. 기존 DefaultGame.ini 지원과 허용 속성 범위는 유지하며, 우선순위는 [Artemis 가이드](ARTEMIS_PCD_SETTINGS.md#현장-gameini-경로와-우선순위-structure-12b)를 따릅니다.

## 4. 수용·도구 경계

- native 안전 상태의 ProcessStamp가 ACCESS_DENIED인 경우와 raw 수신4MiB/pass budget은 기존 한계로 남습니다. 기본값을 조용히 바꾸지 않습니다.
- 일부 과거 직접 UI/회사 수용 항목은 자동화 PASS만으로 완료하지 않습니다. [직접 점검표](MANUAL_CHECKLIST.md)와 [이전 백로그 원문](archive/IMPROVEMENTS_BEFORE_R56.md)을 참고하되 현재 작업 승인으로 읽지 않습니다.
- legacy 문서 정책 검사 중에는 baseline부터 없는 local_asset_report/remaining_work 문서나 이동 전 파일·에셋 경로를 요구하는 것이 있습니다. 문서 정리에서 검사 의미를 낮춰 PASS로 만들지 않으며, 결과에 기존 실패와 문서 변경 후 회귀를 구분합니다.
