# 빌드·검증과 증거 기준

실행 권한은 [AGENTS](../AGENTS.md), 명령 예시는 [README §3](../README.md#3-빌드와-테스트)를 따릅니다. 문서·링크 검사만 요청한 작업에서는 아래 Unreal/빌드를 실행하지 않습니다.

## 현재 기준

| 항목 | 판정 |
|---|---|
| 빌드 | 해당 변경의 Editor/Development/Shipping 및 필요한 TestHost 조합. 미수행 조합은 명시 |
| release | `verify.ps1 -Level release`; 실제 테스트 Fail0, 기대 목록 누락0. skip을 실제 Success로 세지 않음 |
| 맵/BP | 읽기/메모리 컴파일, 테스트 맵 LogLinker/LogCoreRedirects 경고0. SensorTestMap 포함 기존 맵 resave 금지 |
| 성능 | `verify.ps1 -Level perf -BaseCommit <sha> -PerfRayTracing ProjectDefault`; RT-on 표준 ABAB 중앙값,2% gate·기존 workload 유지 |
| 메모리 | warm-up30분 제외, 30분~종료 OS Commit OLS≤8MiB/h. 평가 구간 자체 최소60분. [계산 정책](MEMORY_ACCEPTANCE_R37.md) |
| 직접 UI | client viewport/DPI/폰트 배율과 클릭·기대·실제·스크린샷 기록. 자동 캡처와 마우스 수용 구분 |
| 현장 수용 | 회사 서버·실장비·운전원 수용은 로컬/합성 통과와 별도 |

기존 #26 memory/digging5쌍 재실행은 사용자 결정으로 장시간 메모리 구간 기준으로 대체됐습니다. 계산 함수·과거 FAIL은 보존하며 QA-02 성능 ABAB2%를 제거한 결정이 아닙니다. current 미해결/진행 결정은 [KNOWN_ISSUES](KNOWN_ISSUES.md)에서 확인합니다.

## 조건·환경

같은 SHA/pin/맵/프로필/센서 수/backend/출력/UI/warm-up/해상도를 기록합니다. engine frame과 wall callback pacing을 구분하고0Hz 센서를 평균에서 빼지 않습니다. acquisition/encode/submit/receipt/검증/외부 수신·gap·invalid·overflow·큐를 따로 봅니다.

측정은 하나씩, 장시간은 detached supervisor와 상태 파일로 실행합니다. 외부 Unreal/빌드·영상 decode 중첩은 감시하고 유효 결과에서 제외합니다. 외부 프로젝트를 종료/중지/활성화하지 않으며 종료될 때까지 대기합니다. 중단·재시도·하네스 실패는 원본을 보존합니다. WS trim이 Commit 기울기를 가리지 않는지 보조 구간을 보고합니다.

## 정적 검사와 완료

- docs 상대 링크/현재 경로·클래스·API 대조, `git diff --check`.
- C++ 변경은 layer 검사(allowlist0), 두 submodule guard(`-RequireConnected`) 및 관련 테스트. source/asset/맵/pin 변경을 승인 범위대로 검토합니다.
- 결과는 `Saved/Reports/<ID>/result.json`에 [schema](../Tools/agent/schema/result.schema.json)대로 작성합니다. 비밀 원문을 쓰지 않습니다.
- `validate_agent_result.ps1 -ResultJson <path>` 후 `merge_check.ps1 -ResultJson <path> -BaseCommit <sha>`를 실행합니다. exit0은 후보,1은 사용자 확인,2는 차단이며 자동 머지 승인이 아닙니다.
- 개인 설정·원본 SaveGame을 백업/복원하고 보호 파일 hash를 확인합니다. raw .utrace/.etl/dump/package는 요약·inventory 후 승인된 소유 경로만 정리합니다.

최근 코드 증거는 [VALIDATION_HISTORY](VALIDATION_HISTORY.md)의 r55f 등 원문입니다. r56은 문서 정리·정적 검사이며 새 release/perf/메모리 통과를 만들지 않습니다.

## 레거시 정책 검사 (STRUCTURE-12F)

이름·폴더 변경과 구현 분할을 따라 실제 소유 파일을 검사하도록 경로를 보정했다. 원래 Pattern assertion은 삭제하지 않았다. 포인트 미리보기는 동일 클래스의 분리 cpp와 공통 Coordinator/Monitor body를 함께 검사하며, 경고 문자열 제거 시 실패하는 음성 테스트도 유지한다.

| 검사 | 결과/처리 |
|---|---|
| validate_point_cloud_preview_policy | 현행 플러그인 에셋·분리 구현·공통 body로 경로 보정, PASS |
| validate_real_sensor_adapter_plan | 현행 Comp cpp·UI 경로 보정, PASS |
| validate_large_content_decision_policy | local_asset_report 문서 부재, 기존 FAIL 유지 |
| validate_laz_placeholder_policy | remaining_work 문서 부재, 기존 FAIL 유지 |
| validate_local_decision_precommit_gate_policy | local_asset_report 문서 부재, 기존 FAIL 유지 |
| validate_monitor_widget_policy | 현행 UI 경로 보정; local_asset_report 문서 부재로 FAIL 유지 |
| validate_payload_schema_review_policy | remaining_work 문서 부재, 기존 FAIL 유지 |
| validate_runtime_config_policy | local_asset_report 문서 부재, 기존 FAIL 유지 |
| validate_server_transport_contract | 현행 Comp cpp 경로 보정; remaining_work 문서 부재로 FAIL 유지 |

관련 로컬 수락/보호 도구의 소비자가 남아 있어 문서 부재만으로 workflow 폐기를 확정하지 않았다. 7개 검사를 은퇴하거나 현재 문서 계약으로 재설계하는 결정은 후속 검토 대상이다. release·실제 수신·회사 수락을 이 정적 검사와 혼동하지 않는다.

플러그인 분리 호환 API의 Coordinator, 공통 패널 base, Settings/Monitor/CaptureExport/Placement 패널, UI host 등 façade 7개는 소스/설정 또는 에셋 참조가 남아 유지한다. 간접 Blueprint·외부 SaveGame 참조 부재를 단순 문자열 검색만으로 단정하지 않으며 맵·에셋을 재저장하지 않는다. 대상 클래스/에셋 참조가 없는 ProfessionalDocument redirect만 제거했다.

perf는 현행 DTDevice gitlink가 있는 기준만 받는다. 이전 구조 include forwarder와 클래스/멤버 alias 주입은 은퇴했으며, 측정 instrument·워크로드·중앙값·2% 판정은 유지한다. 정적 archive 경계 검사는 `Scripts/test_perf_archive_boundary.ps1`이다.

AndroidFileServer 토큰은 host/TestHost DefaultEngine.ini에서 비워 두며 값을 로그나 커밋에 새로 넣지 않는다. `Scripts/validate_repository_tokens.ps1`이 값 출력 없이 검사하고 표준 verify에 포함된다. 음성 fixture는 `Scripts/test_structure12_static_guards.ps1`로 확인한다.
