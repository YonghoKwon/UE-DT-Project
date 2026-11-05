# 현장 메모리·LiDAR 점검 가이드

## 시작 전

배포 버전과 DTCore/DTDevice pin, 맵·센서 수·backend·출력·해상도를 기록합니다. 소스/에셋/맵을 점검 때문에 다시 저장하지 않습니다. 인증값은 로그·문서에 넣지 않습니다. 채널 연결과 토픽은 [Artemis 가이드](ARTEMIS_PCD_SETTINGS.md)를 따릅니다.

## 메모리 관찰

프로세스 시작 후 warm-up30분을 제외하고, 30분~종료 OS Commit OLS≤8MiB/h로 판정합니다. 평가 구간 자체가 최소60분이어야 하므로30분 이하 짧은 실행으로 누수/안정화를 판정하지 않습니다. 마지막1/3·warm-up 증가·OldGate도 보조로 함께 봅니다. WS 급감만으로 Commit 증가를 보정하지 않습니다. [계산 정책](MEMORY_ACCEPTANCE_R37.md)을 따릅니다.

현재 실패와 사용자 진행 결정·재확인 계획은 **[KNOWN_ISSUES](KNOWN_ISSUES.md)**에 있습니다. 예전 r37/r50 수치를 현재 실행 대신 쓰지 않습니다. 해치의 장시간 운용은 별도 지속 관찰 대상으로 남습니다.

## native 안전 상태

startup oracle·한 번의 RHI 복구 실행·조합 비활성 메시지와 정확한 파일 초기화는 [NATIVE_LIDAR_SAFETY](NATIVE_LIDAR_SAFETY.md)를 따릅니다. 실행 중 marker를 지우거나 모든 Saved를 삭제하지 않습니다. 정상 장치의 일시적 native 실패는 이번 세션 RHI와 다음 실행 재검증을 뜻하며, 장치 손실과 구분합니다.

## 이상 시 남길 것

- 실행 SHA/pin·PID/생성 시각·console/RDP 세션·GPU adapter·시작/종료 시각.
- 원본 memory-trend/OS 카운터·관련 시각 로그·native/fallback·센서별 Hz/gap/invalid/overflow.
- 영상 decode/외부 Unreal·빌드 중첩과 WS trim 시각. 다른 프로젝트 프로세스는 종료하지 않고 대기합니다.
- 수행/실패/skip/미수행을 구분하며 원시 trace/dump는 요약·inventory 후 정리합니다.

운전원 직접 화면, 실제 수신 서버 및 회사 수용은 로컬 자동화와 별도입니다. 판정·기본 품질·20Hz·payload 정책을 문제를 숨기기 위해 낮추지 않습니다.
