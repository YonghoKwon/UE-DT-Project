# 병렬 worktree 추가 규칙

[공통 규칙](base-rules.md)을 함께 적용한다. 사용자가 승인하고 지정한 worktree·branch에서 측정 없는 작업만 한다. worktree 생성은 별도 승인 범위에서만 가능하다.

- 메인 `C:\Unreal Projects\ma0t10_dt`는 읽기만 허용. 소스·자산·설정·인덱스 변경 금지. 메인 작업의 프로세스·화면·GPU를 건드리지 않는다.
- Unreal Editor, UBT/Build.bat, 성능 측정·GPU 작업 금지. quick과 layer는 스크립트를 읽고 Editor/빌드/측정이 없는 실행 경로일 때만 수행한다.
- DTCore 초기화/update 및 보호 경로 변경 금지. 미초기화 의존성은 한계로 기록하며 실패한 gate를 통과로 표시하지 않는다.
- 원본 증거와 result.json은 worktree 내부 `Saved/Reports/<ID>/`에 둔다. Saved 산출물은 커밋하지 않는다.
- 사용자 확인용 문서 사본은 메인 `Saved/Reports/<ID>/` 한 디렉터리 아래에만 쓴다. 이것이 메인 쓰기의 유일한 예외다. 지시서가 허용한 파일만 복사하고 기존 동명 사본은 덮어쓰기 전에 확인·보존한다. 다른 메인 경로 쓰기 금지.
- 이번 지시서의 허용 사본 파일 / <ID>: <명시>. 명시가 없으면 사본 쓰기도 하지 않는다.
- push/PR/merge 및 메인 branch 전환 금지. 통합·독립 검수·머지는 Claude가 승인 범위에서 수행한다.
