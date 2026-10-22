[Claude → Codex · 병렬 조사 작업 (측정 없음) · COIL-01 근거 자료]

작업 폴더: C:\Unreal Projects\ma0t10_dt_research (git worktree, 브랜치 docs/coil-research). **이 폴더에서만 작업.** 메인 폴더 C:\Unreal Projects\ma0t10_dt 는 다른 Codex가 Editor/GPU 측정 중이므로 절대 건드리지 말 것.
금지: Unreal Editor/UnrealEditor-Cmd 실행, Build.bat/UBT 빌드, 성능 측정, GPU 사용 작업, 맵/에셋 수정, push/PR. 문서 작성과 웹 조사만.

목적: 다음 작업 #31(제품 부두 코일 선적 시뮬레이션)의 수치 근거 문서 작성. 지시서 요약은 아래.
- 선박: 제품선/일반 화물선 1~3만 DWT, 상자형 홀드. 코일: 대표 사양 기본값 + 조절. 단수 제한 없이 쌓되 허용 하중·안정성 초과 시 경고.
- 판정: 사전 해석(탱크톱 하중, key/locking coil 기하, 하단 코일 변형, 마찰·선박 운동 가속도, 무게중심/횡경사, 해치·크레인 간섭) + Chaos 물리 확인.

산출물: `docs/research/COIL-01-basis.md` (한국어, 출처 URL·문서명·조항 명시, 추정값은 추정이라고 표시). 포함:
1. 대표 선박 제원 3~4척 예(1~3만 DWT general cargo / multipurpose, box hold): LOA, B, D, 흘수, 홀드 수·치수, 해치 치수, 탱크톱 허용하중(t/m²), 갑판 크레인 유무. 권장 기본값 1세트.
2. 열연/냉연 코일 사양 범위: OD, ID(508/610/762), 폭, 무게, 밀도 7.85 검증식, 권장 기본값.
3. 적재 실무: eye 방향(fore-and-aft vs athwartships 조건), dunnage 규격·간격, chocks/wedges, key coil 배치, 단수 관행(무게별), 래싱 방식. 출처: IMO CSS Code(특히 coil 관련 Annex), P&I 클럽 코일 적재 가이드(Gard, UK P&I, Britannia 등), 선급 자료.
4. 판정 수식 초안: (a) 탱크톱 하중 = 코일 무게/(dunnage 접촉 면적) vs 허용, (b) key coil 안착 조건(상단 OD, 하단 OD, 하단 간격 → 접촉각, 미끄럼 한계), (c) 마찰계수 표(강-목재, 강-강, 건/습), (d) CSS 기준 횡/종/수직 가속도 산정법(선박 길이·속력·GM 의존 표), (e) 하단 코일 ovality 위험 근사(근거 수준 명시), (f) 간이 KG 상승·횡경사 계산.
5. 포털 크레인(제품 부두) 대표 사양: SWL(훅 아래), 아웃리치, 양정, 레일 스팬, 훅+슬링/C-hook/코일 텅 비교.
6. 시뮬레이션 구현 권고: Chaos 원통 충돌 근사 한계, 판정의 기준을 해석으로 두는 이유, 테스트 케이스 목록(정상/경고/위험 각 3개 이상).

완료 시 해당 문서만 커밋(파일 단위 add, `git add -A` 금지), 커밋 메시지 "docs(COIL-01): research basis for coil stowage simulation". 짧게 보고.
막히는 자료(유료 표준 등)는 공개 요약/가이드로 대체하고 그 사실을 표시(예비안 허용, 멈추지 말 것).
