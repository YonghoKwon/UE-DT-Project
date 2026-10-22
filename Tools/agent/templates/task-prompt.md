# [Claude → Codex] <ID> <작업명>

[공통 규칙](base-rules.md)과 [AGENTS.md](../../../AGENTS.md)를 적용한다. 병렬이면 [추가 제한](parallel-worktree.md)을 적용한다.

## 목표
- 한 항목의 사용자 관찰 가능한 완료 조건:

## 기준
- 작업 폴더 / branch / base SHA / DTCore 고정 SHA:
- 선행 요청 / 현재 증거 / 미해결 제한:

## 범위
- 허용 파일·변경 / 금지 작업 / 보호 경로:
- Editor·빌드·GPU·측정 허용 여부(기본 미허용):
- commit 여부 / push·PR·merge 권한(각각 명시):

## 예비안
| 차단 조건 | 묻지 않고 적용할 예비안 | 검증·보고 |
|---|---|---|
| <조건> | <범위 내 대안> | <선택 근거와 한계> |

예비안으로 해결할 수 없는 계약·보호 경로 변경은 즉시 근거와 선택지를 보고하고 해당 작업을 멈춘다.

## 게이트
| gate | 명령·기준·동일 SHA 조건 | 미적용/미수행 이유 |
|---|---|---|
| quick / feature / release / perf / layer / dtcore_guard / protected_paths | <명령, manifest> | <SKIPPED 사유> |

## 산출물
- 명시 파일 / 의미 단위 커밋 / 증거 경로:
- `Saved/Reports/<ID>/result.json`: [schema](../schema/result.schema.json) 준수.
- `Scripts/validate_agent_result.ps1 -ResultJson <path>`
- `Scripts/merge_check.ps1 -ResultJson <path> -BaseCommit <sha> [-Head <ref>]`
- 사용자 확인용 사본 경로·허용 파일(병렬 지시서에서 명시):

## 보고 규칙
20분마다 진행 보고(변화 없으면 한 줄), 결정·이미지·완료·실패 즉시 보고.
완료: 항목, 순서별 커밋, 변경 요약, gate/보고서, layer allowlist 전후, 보호 경로, 미수행·제한, 질문·결정 필요.
merge_check exit 0은 독립 리뷰를 위한 후보이며 자동 머지 승인이 아니다.
