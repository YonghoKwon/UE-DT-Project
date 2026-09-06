# PR #16 기준 센서 기능 PR의 병합 범위

## 기준 이력

- 요청 기준: 2026-08-05 PR #16, `94811a6715a521912e686471f6c70d57490e6b38`.
- 검토 당시 origin/master: PR #17 병합 `a1baacc`.
- `574a317`은 PR #17을 이력 보존 방식으로 revert한 커밋이며, 해당 tree는 PR #16과 동일합니다.
- 센서 기능 브랜치는 이 revert 위에서 개발했습니다. PR은 **기능 추가만이 아니라 PR #16 복원도 포함**합니다.

## 병합 후 유지·변경 범위

| 항목 | 병합 결과 |
|---|---|
| PR #16까지의 Sensor V2, Raw TCP STOMP, Binary PCD, Camera JPEG, LiDAR telemetry | 유지 |
| PR #17 단조 시계 cadence 재설계·추가 timestamp 계약 | revert, PR #16 scheduler 동작 사용 |
| PR #17 전역 글자 배율·전체 WidgetTree 적용 | revert, Host가 소유한 세 센서 패널의 명시 등록 컨트롤만 변경 |
| PR #17 Settings 자유 resize·32px grip 관련 확장 | revert, PR #16 패널 조작 기준 사용 |
| PR #17 SaveGame v7 | 이번 코드는 읽기/수정/삭제하지 않음. 기존 v6 사용, 센서 글자 설정은 별도 appearance 슬롯 |
| Slab 정보 연동 | 새 World subsystem의 Begin/Notify/Pause/End API. 동료 Topic/이동/차트 코드는 변경하지 않음 |
| Map/WBP 자산, DTCore gitlink, Config, Samples | 이번 PR diff에서 변경하지 않음 |

원격 master가 작업 브랜치의 조상이면 Git 충돌 없이 병합할 수 있지만, 이는 삭제된 PR #17 기능까지 유지된다는 뜻은 아닙니다. 미래에 master가 변경되면 병합 가능성을 다시 검사해야 합니다.

## 다른 프로젝트 또는 동료 코드의 주의점

- PR #17의 `SetGlobalSensorUiFontScale`, `GetGlobalSensorUiFontScale`, `ResetGlobalSensorUiFontScale`, `OnSensorUiFontScaleChanged`에 의존하는 외부 C++/Blueprint는 그대로 연결되지 않습니다. 이번 PR은 전역 API를 복원하지 않습니다.
- 센서 소유 패널은 `SetSensorToolFontScale`과 명시적 Text/Input 등록 API를 사용합니다. 동료 차트 위젯을 등록하거나 전체 트리를 순회하지 마십시오.
- 이 저장소의 WBP 컴파일 검증과 동료의 별도 프로젝트 WBP 검증은 다릅니다. 별도 프로젝트에서 구 전역 API를 호출했다면 호출부를 제거하거나 동료 위젯 전용 기능으로 교체한 뒤 컴파일해야 합니다.
- 로컬 AGENTS.md 편집, 사용자 SensorTestMap.umap 변경, DTCore override, Game.ini, PixelStreaming 파일은 stage하지 않았습니다. PR에는 Git에 커밋된 범위만 들어갑니다.

## 병합 직전 재확인

```powershell
git fetch origin
git merge-base --is-ancestor origin/master HEAD
git merge-tree --write-tree origin/master HEAD
git rev-parse 'HEAD^{tree}'
git diff --name-only origin/master HEAD -- Content Plugins Config Samples
```

조상 검사 성공, merge-tree exit 0, 반환 tree와 HEAD tree 동일을 함께 확인합니다. 마지막 diff가 비어 있으면 이 PR은 해당 보호 경로를 수정하지 않습니다. Git 충돌 확인은 빌드·런타임 검증을 대신하지 않습니다.
