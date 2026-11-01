# r37 메모리 판정 구간

2026-11-01 사용자 승인으로 메모리 추이 판정 구간을 변경한다. r36 근거는
`Saved/Reports/LIDAR-MEMORY-26-FINAL/rootcause-commit/r36/RESULT.md` 및
`attr/raw180-a1/attribution-analysis.json`이다. 동일 현재 빌드의 Commit 기울기는
10~60분 +4.28, 60~120분 +1.55, 120~180분 -0.79 MiB/h로 평탄화했다.

새 판정은 준비 30분을 제외한 **30분~종료 OS Commit OLS ≤8 MiB/h**이다.
관찰 구간 길이는 최소 60분이며, 프로세스 PID/생성 시각, 완료 마커, 시간 경계,
표본 간격과 환경 감시가 유효해야 한다. 종료 후 메모리 반환은 포함하지 않는다.
WS 감소를 사용해 Commit 증가를 보정하지 않는다.

마지막 1/3 기울기, 10~30분 증가량과 기울기, 구간별 기울기 및 r36 plateau
분류는 보조로 항상 보고한다. 기존 tail-third 및 10~30분 값은 `OldGate`에
병기한다. `analyze_memory_trend.py`, `analyze_lidar_map_memory.py` 등 기존 계산
함수와 원래 #26 절대/차이 8 MiB 5쌍·150ms·ABAB 2% 기준은 변경하지 않는다.

구현은 별도 `Scripts/analyze_memory_acceptance.py`이며 합성 경계·초기 계단·후기
계단·미완료/누락 표본을 검증한다. 새 결과는 과거 실패 결과를 덮어쓰지 않는다.
제품 품질·출력·획득 Hz·기본값 변경은 없다. 검증 프로브에서만 명시적
`HatchMemoryFinalValidation` / `LidarMapMemoryFinalValidation` 플래그로 180분을
허용하며, 무거운 attribution/memreport/trace 진단은 이번 검증에서 비활성이다.
