# 보완 필요 사항 — 구현 백로그

현재 기준: **`codex/architecture-skeleton` (분기 `fda25e0`, Runtime `81b56ba`) / DTCore `b2504d1`, 2026-10-11**. 작업 폴더는 `C:\Unreal Projects\ma0t10_dt` 하나다. 과거 날짜별 검증 기록은 [검증 이력 보관](VALIDATION_HISTORY.md)으로 원문 그대로 옮겼다. 각 항목의 진행률·완료 조건은 그대로 유지한다.

[작업 지침](../AGENTS.md) · [현재 기능/사용법](../README.md) · [단계별 로드맵](ROADMAP.md) · [검증 이력](VALIDATION_HISTORY.md)

## TESTSET-04 — 회사 환경 대기·파일 확정·실패 재시도

출처: **사용자 2026-10-20**. 브랜치 `fix/testset-company-env`, 기준 `89b45ae`, DTCore `b2504d1` 고정. 회사의 항목 메시지 `dataset.json 확정 실패 또는 생성 취소`를 출발점으로 조사했다.

- 재현: 빌드 SHA 빈 값 주입과 실행 PATH의 Git 제거에서 수정 전 100항목 모두 실패하고 원본 JSON만 남았다. PKG-02의 빌드 SHA 주입 이후에도 남아 있던 빈 SHA 실패 조건/Editor Git fallback이 원인 경로다. 수정은 런타임 Git을 제거하고 `unknown`/`unavailable`을 기록하며, DATASET-01 일반 생성에도 같은 정책을 적용한다.
- 시연 모드의 대상 센서 작업은 일반 시나리오/세션 drain 뒤 자동 임시 중지하고 소유 작업만 복원한다. 실제 수신 모드의 대기/원클릭은 유지한다. 다중 LiDAR는 명시 ID 우선, 미지정 시 수신 Slab ID→해치 기본→검수 기본→ID 정렬 순으로 고정 선택하고 상세/로그에 표시한다. 다른 센서·기본 PCD·자동 재개 정책을 바꾸지 않는다.
- 항목 실패·재시도 거절 사유를 행/상세/상태 줄에 유지하고, 템플릿 재읽기·새 RunUUID 재시도·완료 캡처를 보존하는 목록 저장 재시도를 제공한다. pending 쓰기·최종 이동·취소·파일/ID/33B/5B 검증 실패를 분리하고 `[TESTSET]` 변화 로그로 경로와 대기 작업을 남긴다.
- 같은 날 추가 화면 요구: 상세 옆 **전송** 자리 표시 버튼은 완료 후 안내만 표시한다. 파일/전송 동작은 없다. 트리거마다 기존 창을 재표시·전면 배치하며, 중복 유지/거절 및 DTCore 닫기 뒤 재등록도 포함한다. 기존 배치·크기·세트/RunUUID를 유지한다. 수정 전 두 모드×3회 재열기와 열린 창 전면 배치 실패를 재현했다. 새 자동 회귀는 두 모드 모두 버튼 비활성/활성·실제 Slate 클릭·파일 해시/시각 및 송신 계수 변화0·재수신3회·같은 인스턴스/배치를 확인한다. 직접 입력은 새 창 캡처0x80070057 두 번으로 환경 보류다.
- 최신 Runtime `bd79ddb`: 새 화면 회귀는 두 모드×재수신3회·DTCore 재등록/배치·실제 Slate 클릭/결과파일·전송 변화0을 확인했다. Git 없는 회사 유사200개 및 실제 빈 커밋 Development 패키지100개, 오류5종/재시도·복원·파일 감사, release14단계(고유316Success/40조건부skip/0Fail)·WBP7save0·어휘/golden/Python69/layer0·기본60초ABAB12회2% gate를 통과했다. 첫 선행release의 단일150ms 초과는 원인 미확정으로 보존했다. 직접 입력0·환경 보류, 패키지 Slab정답 품질/회사실환경/Shipping 수용은 별도다. 상세 수치·조건·실패는 검증 이력과 RESULT를 따른다.
- 현재 검증/미수행·최초 실패 및 회사 PC 확인법: [RESULT](../Saved/Reports/TESTSET-04/RESULT.md), [DONE](../Saved/Reports/TESTSET-04/DONE.md), [직접 점검표 T09~T11](MANUAL_CHECKLIST.md). 회사 실제 배포 수용은 로컬 재현과 구분한다.

## HATCH-04 — 그랩식 하역기·현장 3대·단면/조작 피드백

출처: **사용자 2026-10-20**의 피드백 8건, 현장 3대 배치·모델 미확인/변경 가능·그랩식·모든 화면의 해측/육측 표시 답변. 브랜치 `feat/ship-hatch-v4`, 기준 `23133f6`, DTCore `b2504d1` 고정. 기존 해치 맵을 사용하고 새 자산/맵/모듈/플러그인은 만들지 않는다.

- 임의 고소 마스트/갠트리를 제거하고 표시 토글 가능한 경량 붐·트롤리·운전실과 아래 방향 센서 3대를 구성한다. 각 모델/켜기·끄기·X/Y/높이·틸트/요, 기존 2대/현장 3대 프리셋, 3점군·센서 색·광선과 예상 FOV 기여/사각을 연결한다. 기본 Mid-360S 3대는 **현장 모델 확인 필요**다. 치수/좌표/근거와 FOV 수치는 README HATCH-04를 따른다.
- 상면/히트맵은 한 카드에서 전환하고 다른 카드에 현재 모의 높이장의 단면을 표시한다. 방향/위치 슬라이더·노란 선 드래그, 실제 카메라 표시와 이동 모드 2D 클릭, 3D 단면 평면, 모든 뷰의 해측 +Y/육측 −Y와 시점 프리셋을 제공한다.
- 두께 0 원료면은 탱크톱 안쪽으로 내려 바닥/구조 분류를 노출한다. 얇은 잔여층의 picking과 재적재 후 충돌 범위를 보정하며 실제 프레임에 표시 보정을 주입하지 않는다. 타원 그랩·짧은 하강/닫힘/상승·vertex 음영, 좌드래그 카메라 잠금/우드래그·휠 분리, 16 undo/exact 복원을 유지한다.
- 조명 방위각/고도/세기·프리셋/보조광, 열린 Select/선택/hover의 다크 대비와 150% 글자, 소유 표시 버퍼와 기존 5Hz 갱신을 사용한다. PIE 조명 상태는 복원하고 동적 그림자 대신 절차 함몰 음영을 사용한다.
- **구현·자동 검증 완료(Runtime58b333b), 직접 UI12행 환경 보류.** Editor/Game·release14단계(고유317Success/40조건부Skip/실패0)·WBP7/save0·어휘/golden/Python69·layer0,자동90장·3대50회 RSS+2.371/+2.719/+1.293MiB·150ms초과0,기본60초ABAB12회2% PASS(FPS저하-0.019110%,p95증가0.000000%,GT증가-0.451063%)다. 해치3대 표시형59.796679FPS/p9516.667803ms/GT12.088470ms,실제4.800/4.366/4.433Hz다. 직접캡처2회0x80070057·입력0,현장모델확인필요. 개인142파일복원·보호833중 승인된기존머티리얼1개 외832해시 일치. 초기RSS/fixture/collision 실패 원본과 수용 제한을 보존한다. [RESULT](../Saved/Reports/HATCH-04/RESULT.md), [DONE](../Saved/Reports/HATCH-04/DONE.md), [직접 점검표](MANUAL_CHECKLIST.md).

## HATCH-03 — 클릭 퍼내기·복원·센서와 측정 관찰

출처: **사용자 2026-10-19**, 최초 제안 승인 및 같은 날 추가 요구 a–f. 브랜치 `feat/ship-hatch-dig`, 기준 `67b34f7`, DTCore `b2504d1` 고정. 기존 해치 맵을 유지하고 승인된 예외로 원료용 머티리얼1개만 추가했다. 새 맵/모듈/플러그인은 없다.

- 기본 꺼짐의 퍼내기: 일반/점군의 노출 원료 짧은 클릭, 현재 플레이어 투영·높이장·구조 가림, 기본 반경2m/깊이1.5m, 반복·바닥0·삼각면 안식각 제한·부피 보존. 표시용 센서/거치대와 라벨 입력도 원료 클릭과 구분한다.
- 원본+16 undo+scratch는 최대 메시에서도2MiB 이내이며 원래대로/undo는 bit-identical이다. 새 적재는 기록/양 초기화. 최소/최대 적재율·센서 높이·격자 현재값과 drawer 미적용 입력 보존 회귀를 추가했다.
- 측정을 재시작하거나 이전 점군을 숨기지 않는다. 높이장 보정은 표시 copy/2D 미리보기 전용이고 실제 분석은 편집 후 시작한 acquisition만 받는다. 실제 immutable 측정 CRC/PCD/JSON·wire/SaveGame/Blueprint API는 유지한다. 지표는 홀드 전체 모의 적분과 실측/가림 추정을 구분한다.
- 중앙 센서2대+거치대 표시, 이름 선택 강조, 기본OFF 최대96광선/5Hz 애니메이션. ML-X80 좁은 수직FOV를55.351m 높이로 보완하고 Mid-360S8m/−112.5°로40m 내 바닥을 보완한다. 기본 개구부/빈 바닥 FOV100%(80×80+모서리), 실제 관측률은 별도다.
- 절차 입자·덩어리 normal/거칠기의 공유 머티리얼1개, 기존 광학 가정0.25/0.05. 2D/히트맵은 개구부 확대/갑판 mask, 실제 aperture 통과 관측만 분석에 반영한다. 기존 UDxWidget/Subsystem과 InteractableActor 기반, map 밖 추가 Tick/측정/캡처/표시 컴포넌트 생성 경로는 없다.
- Runtime `215eb04`: Editor/Game·release14단계(고유313Success/40조건부Skip/0Fail)·WBP7/save0·어휘/golden/Python69/layer0, 자동66장과 실제 마우스 점검 PASS. 점군50회 두 실행 RSS−1.0625/+1.441406MiB, 일반3D+0.941406MiB·모든commit≤8MiB·150ms초과0. 기존맵 기본60초ABAB12회/invalid0/2% PASS(FPS저하−0.000670%,p95동일,GT−1.367769%). 원본141개인파일 복원·보호832해시 일치. 새머티리얼1개 외 기존자산/맵·DTCore·보호config/PixelStreaming 변경0. 과거 RSS/30초ABAB 실패는 별도 원문으로 보존했다. [RESULT](../Saved/Reports/HATCH-03/RESULT.md), [DONE](../Saved/Reports/HATCH-03/DONE.md), [직접 점검표](MANUAL_CHECKLIST.md)에 근거/조건/수용 제한을 구분한다.

## HATCH-02 — 해치 모니터링 제품 화면·재생성 메모리

출처: **사용자 2026-10-19**(목표 화면 첨부·회사 제품 수준의 UX/UI 및 HATCH-01 RSS 조사 요청). 브랜치 `feat/ship-hatch-ux`, 기준 `25f6617`, DTCore `b2504d1` 고정. 새 자산·맵·플러그인 없이 기존 해치 맵만 대상으로 한다.

- 헤더의 원료/장비·로컬 측정 상태·실측 Hz·갱신 시각, 3분할/최대화, 3D/점군 세그먼트, 점 색/크기/밀도·높이/분류 범례, 6개 지표 카드, 접이식 설정·상세, 회전/확대·시점 프리셋을 구현했다. 공통 고정 다크 Presentation Style을 사용한다.
- 단일 플레이어 뷰의 영역을 맞추고 실제 형상 경계를 담는 구도를 계산한다. 추가 capture/readback/스캔 없이 관측 분석 하나를 최대5Hz로 공유한다. 숨김/접힌 상세 문자열은 조립하지 않는다. 원료 설정·프로필·PCD/wire·SaveGame·Blueprint API는 유지한다.
- 픽셀/texture 재생성을 없애고 캐시한 Slate 격자를 그린다. 관측점·메시 정점/법선/인덱스·worker 높이장 버퍼를 재사용한다. 효과가 입증되지 않은 L1 센서 버퍼 풀 실험은 제거했다.
- **RSS 해석과 조건 변경을 분리한다.** 원래 10초 준비 조건은 간헐적으로 8MiB를 초과했다. 같은 준비 뒤 재생성 **0회**, 측정 화면만 25초 유지한 대조에서도 RSS +11.168MiB / committed −13.934MiB였다. 초기 resident 변화가 섞이므로 최종 검사는 동일 경로 35초 준비 후 측정한다. **50회·원시 RSS 8MiB·committed 8MiB assertion은 유지**, working-set 강제 축소나 대조값 차감은 없다. 기존 10초 조건의 통과로 표시하지 않는다.
- 구현 커밋 `5e5aecd`(대시보드·버퍼 재사용), `a82c6bd`(영역별 카메라 맞춤). Editor/Game·release14단계(고유311 Success/40조건부skip/실패0)·WBP7·어휘/golden·Python69·layer0·자동48장 및 전후24쌍을 확인했다. 최종35초 준비 RSS−3.707MiB/committed+0.648MiB·50회·보유 버퍼 증가0·Actor4다. 같은 최종SHA의 무재생성25초 대조도 RSS+12.602MiB/committed−11.297MiB로 초기 resident 혼입을 재확인했다. 기존 맵25f6617 대비ABAB12회2% PASS(FPS저하−0.00649%,p95동일,GT−0.63715%). **직접8행은 Windows잠금·입력0·환경보류**이며 원래10초 RSS 조건도 통과로 올리지 않는다. 개인141파일복원·보호832 hash일치. 전체 사용자/회사 수용과 분리하며 `Saved/Reports/HATCH-02/RESULT.md`, 직접 행은 `docs/MANUAL_CHECKLIST.md`를 따른다.

## HATCH-01 — 선박 해치·카고 홀드 원료 측정 테스트

출처: **사용자 2026-10-19**, 브랜치 `feat/ship-hatch-sim`, 시작 `8320d70`, DTCore `b2504d1`. 승인한 새 맵은 `/Game/MA0T10/Maps/Tests/ShipHatchTestMap` 하나다.

- 절차 홀드 42.3×25.8×22.5 m·개구부 23×20 m·코밍 2 m, 철광석/석탄 결정적 높이장·적재량, ML-X80 Integration/Native·Livox 단독/동시, 랜덤/직접 시드, 기존 PointCloudOnly·preview 정책을 사용하는 3D·2D·히트맵을 구현한다.
- 관측 격자의 평균 높이·부분 부피/용량 비율과 미관측을 구분한다. 전체 홀드 부피/적재율을 외삽하지 않는다. 치수·재질·밀도·경사 가정, CPU trace·DATASET canonical 라벨 제한은 README HATCH-01 절을 따른다.
- 추가 분석 JSON은 명시적 비동기 저장만 제공하며 기본 꺼짐이다. PCD/wire·기존 Blueprint API·SaveGame·DTCore·기존 맵/자산은 유지한다. 새 모듈/플러그인·마켓 자산·하역은 없다.
- **구현·전체 수용 구분:** Runtime `226cf64`, 새 맵1개·자동24장·Editor/Game·표준release14단계(고유310 Success/40조건부skip/실패0)·WBP7·golden·Python69·layer0을 확인했다.292d03d 대비 기존 맵 ABAB12회2% PASS(FPS 저하0.002216%·p95동일·GT+0.721529%)다. 단독/동시 실제 viewport·재질 반사·거리 색·payload 불변·선택 JSON·숨김 비용·50회 fresh 재측정 검사는 통과했으나 집중 test의 종합 상태는 **FAIL: working set+10.285MiB>8MiB**다. committed+1.758MiB·보유 버퍼 증가0·형상 Actor4와 구분하며 기준을 완화하지 않는다. 직접8행은 Windows잠금·입력0·환경보류다. 개인141파일복원·보호300/Inspection272/source hash 동일을 확인했다. 제한과 다음 gate는 `Saved/Reports/HATCH-01/RESULT.md`와 `DONE.md`를 따른다. 전체 수용 완료로 표시하지 않는다.

## 현재 개발 초점과 착수 순서

### INSP-FIX-4 — 검수 화면의 전환·분할·IMU 해제·입력 안내 (F2·F4·F5·F6·F8·F9)

출처: **사용자 검수 2026-10-17 F2·F4·F5·F6·F8·F9**, 승인 2026-10-17. 기준 `5ef288d` / DTCore `b2504d1`, 브랜치 `fix/inspection-ui`.

- **F9·F8:** 기존 PointCloudOnly API를 카드의 점군만 보기 체크에서 호출한다. 듀얼 Camera 위:아래 비율은 70:30·50:50·30:70을 순환하며 영상 종횡비를 유지한다. 기존 저장 구조에 필드가 없어 같은 인스턴스의 세션 내 기억만 제공한다. Blueprint API·SaveGame 형식·서버 데이터는 유지한다.
- **F4:** 보기 버튼만 사이드바가 펼쳐져 있으면 선택 표시를 끄던 예외를 제거한다. 다른 레일 버튼과 같은 선택·호버·눌림 스타일을 사용한다.
- **F2:** 선택 장비의 IMU 해제 확인·취소를 제공한다. 확인 전 선택 변경은 대상 교체 없이 취소되며, 확인하면 기록·전송을 끄고 기존 FileSave 비동기 CSV 완료 뒤 컴포넌트를 제거한다. 숨김 중에도 진행하며 저장 실패는 IMU·표본을 유지한다. 성공 후 해당 IMU 기록 버퍼를 비워 재부착에 섞이지 않게 하고 AUTO-01 조회의 `imu` 객체도 사라진다. 기본 부착 없음은 유지한다.
- **F5·F6:** 공통 숫자 입력에서 Slate 제한 전 원래 값을 읽어 입력 아래에 최소/최대 조정 안내를 표시한다. LiDAR 사실성 네 옵션과 두 색 모드는 짧은 설명·ⓘ에 화면 변화/학습 데이터 용도/비용/가정 모델 한계를 표시한다. 5Hz·숨긴 상세 조립 중단·기본 꺼짐 정책을 유지한다.
- **검수맵 헬퍼:** 맵 저장 없이 코드의 하단 여백으로 상태바 입력 영역을 피한다. 신규 자산·DTCore·보호 설정 변경은 없다.
- **검증 상태:** Runtime `6b42f84` 불변, INSP-FIX-4b 테스트 `f88af4b`에서 UXR10 event/cursor 불일치를 faux cursor·2프레임 geometry 대기·현재 cursor의 held synthetic move 검사로 격리했다. 최소/최대·저장/복원 기대값은 유지한다. 수정 후 단독10/10, 표준 release2회 연속14단계·전체301 Success/39조건부skip/실패0, 집중11 Success/skip1·WBP7 저장0·어휘/golden·layer0. 기존 자동64쌍/대표 직접6항목과 Runtime ABAB12회2% PASS는 이전 범위를 유지하며 test-only라 perf 재측정/새 직접 수용을 하지 않았다. 개인196/보호653 hash 동일·추가0. 초기 실패·세 revision 비교·원인·제한은 `Saved/Reports/INSP-FIX-4/RESULT.md`의 UXR10 절을 따른다.

### INSP-FIX-3 — 정답 저장 결과·배치 선택 일치 (F7·F10)

출처: **사용자 검수 2026-10-17 F7·F10**, 승인 2026-10-17. 기준 `eeed0e9` / DTCore `b2504d1`, 브랜치 `fix/inspection-behavior`. 성능 비교 기준은 요청한 `a30055f`다.

- **F7:** 저장 버튼의 임시 문구를 미리보기 상태 갱신이 덮던 경로를 수정했다. 기존 FileSave의 bounded 요청 이력·완료 이벤트로 요청/저장 중/완료·경로·폴더 열기/실패 사유를 유지하고, 기존 최근 파일 목록에도 실제 경로·worker가 전달한 크기를 반영한다. 비동기 캡처/worker·4종 파일 내용/완료 marker·Blueprint API·SaveGame 형식은 유지한다. 숨김은 저장 취소가 아니며, 센서 종료/설정 revision 변경의 결과는 실패/취소로 분리한다.
- **F10:** UE5.3 `STableRow`의 기본 mouse-up 알림 전에 목록의5Hz 동기화가 이전 센서 행을 복원하는 경쟁 조건을 재현했다. 알림을 mouse-down으로 앞당기고, 목록 변경 때 같은 Actor의 행 객체를 유지한다. 선택 ID 문자열이 아닌 weak Actor 연결을 유지하며 갱신 주기는 바꾸지 않는다. 원본 검수 로그에는 down/up 이력이 없어 당시 모든 증상이 이 원인인지는 단정하지 않는다.
- **검증 상태:** Runtime `f92c272`, Editor/Game Development·최종 release14단계(고유298 Success/조건부skip39/실패0)·WBP7 저장0·어휘/golden·layer0 통과. 대조 실행은 기존 mouse-up 알림만 복원하면 F10 Fail, 수정 후 집중4건 Success다. 최초 release의 UXR-15 꺼짐 안내 회귀는 완료 캐시와 현재 꺼짐 표시를 분리해 해결했으며 기존 기대값을 유지한다. 기존 UXR-10 합성 최대 resize는2회 실패 뒤 집중/최종 전체에서 통과했고 원문을 보존한다. 최종 Runtime에서1280×720·native DPI1.0/UMG0.666·100% 직접 저장 완료/폴더와 교대 선택20회를 재확인했다(이전 Runtime의150%4회 별도). 성능 gate: a30055f↔f92c272, 표시형1+1 ABAB12 유효·FPS 저하0.00575%·engine p95동일·GT−0.01891%,2% PASS. 최종 수치·skip 이름·보호 복원·제한은 `Saved/Reports/INSP-FIX-3/RESULT.md`에 기록한다. Claude 독립 검토 대기.

### INSP-FIX-2 — 테스트셋 대기·저장 점군 보기·공통 이동 패널 (F13·F14·F1·F3)

출처: **사용자 직접 검수 2026-10-17 F13·F14·F1·F3**, 승인 2026-10-17. 기준 `1de8a98` / DTCore `b2504d1`, 브랜치 `fix/testset-inspection`.

- **원인:** 원본 주입 직전/직후 로그에 `SLAB-LIDAR-001` 측정20Hz가 남아 있었다. DATASET 시작의 `IsSensorRunning()` 첫 차단과 일치한다. 측정·설정 lock·파일/주기 저장·독립 송신의 OR 조건이 같은 오류로 합쳐져 TESTSET의 시작 실패가100개 모두 failed를 만들었다. 동시 독립 송신/주기 저장 존재까지는 원본 증거로 단정하지 않는다.
- **수정:** 사용 중은 센서ID/작업별 이유로 대기하고 해제 뒤 자동 재개한다. 원클릭은 선택 센서의 작업만 잠시 멈추고 종료 뒤 소유한 중지만 복원한다. 일반 시나리오 우선·접수된 송신 drain·사용자 설정/기본 PCD를 유지한다. 실제 오류의 실패/새 RunUUID 재시도는 남긴다.
- **보기:** 완료 목록은 저장 PCD를 읽는 점군 조감도다. 파일 프레임/시간·점 수, 재생/일시정지/중지를 제공한다. 전용 컴포넌트의 immutable snapshot/기존 투영을 사용해 운영 센서·Slab 재생·전송·원본/라벨 파일을 건드리지 않는다.
- **화면:** 공통 Presentation Style/헤더/버튼/글자 배율, Workspace 자유 드래그·크기 조절·v1 위치 저장을 재사용한다. TestSet enum 키만 끝에 추가하며 새 SaveGame 형식/슬롯/필드는 없다. 기본은 진행률·상태·중지/재개·폴더·상태 목록, 나머지는 접힌 상세다. 숨김/접힘 갱신 중지, 표시5Hz·행 상태 변경 감지, 미사용 타이머/파일/미리보기 컴포넌트0을 유지한다.
- **검증 상태:** Runtime `d3df5a2`, 원본100×4조건(각2000PCD·64,512,000점)·파일 감사·실제 원클릭100·9/100 pause/resume·점군3개·드래그/위치/150%/경계 확인. Editor/Game·release14단계 고유297 Success/조건부skip38/실패0·WBP7 저장0·어휘/golden·Python69·quick/layer0 완료. 최초 GT+2.171% 실패 뒤 미사용 hold의 빈 집합 문자열 해시를 생략하고 동일 표준 ABAB12샘플 재측정: FPS 저하−0.00728%·p95동일·GT−1.41824%,2% PASS. 개인196/보호390파일 hash 복원, 기존51 TestSets 보존·세션117폴더만 정리. 최초 PCD enabled assertion 실패는 자동 재개 대기를 무조건 켬으로 기대한 검증 오류였고, retest 전후 enabled=false/false·intent=true/true/Success로 기록했다. 전후2해상도×100/150%8장 완료; 직접은1280×720·DPI1·100/150% 범위이며 다른 직접 조합/선택 센서 조합은 별도다. 검수맵 helper 하단 가림은 기존 테스트용 숨김 API로 분리해 확인했고 기본 맵은 유지한다. 상세·초기 실패·skip 이름은 `Saved/Reports/INSP-FIX-2/RESULT.md`, [직접 T01~T05](MANUAL_CHECKLIST.md). Claude 독립 검토 대기.

### INSP-FIX-1 — 신규 PCD 기본 선택·연결 상태 (F11·F12)

출처: **사용자 직접 검수 2026-10-17 F11·F12**, 승인 2026-10-17. 기준 `872e765` / DTCore `b2504d1`, 브랜치 `fix/pcd-default-connection`.

상태: 구현·Editor/Game·release14단계(Success294/조건부skip38/실패0)·WBP7·어휘·golden·layer0 완료. `48d93d6` 로컬 실제 PCD100프레임/106,567,882bytes·invalid0. 표시형1920×1080·1+1 ABAB12샘플은 FPS +0.0015%·p95 동일·GT −0.4488%로2% gate 통과. 직접1280×720·DPI1·글자150%에서 신규/재생 독립·토글·차단 이동·연결 시험 성공 확인. M13/M15의 나머지 조합·M14 제한 링크·실제 단절 직접 수용은 남는다.

- 검수맵의 관찰 검증 버튼이 신규 출력 선택까지 껐던 경로를 실행별 replay 출력으로 격리한다. AUTO-01 `scenario.start`의 `send_pcd=false`는 기존에도 실행 snapshot에만 적용되며, 비동기 파일/보관 UUID·사용자 켬/끔·저장/복원 회귀로 고정한다. SaveGame 형식·기본 PCD 켬·wire·Blueprint API는 유지한다. 과거 선택의 작성자를 구분할 이력이 없으므로 사용자 끔을 강제로 덮어쓰지 않는다.
- `tcp://` 연결 시험은 Raw TCP worker를 사용한다. 상태바는 설정됨·전송 대기/연결됨/재연결/단절을 구분하고, 연결 전에도 PCD 화면과 경고 칩에 첫 차단 사유를 표시한다. DTCore 업무 서버 `ws://localhost:31000` 오류와 센서 전송을 구분한다.
- 현재 검증·직접 수용·성능 판정과 제한은 `Saved/Reports/INSP-FIX-1/RESULT.md`, M13~M15 직접 기록은 `Saved/Reports/INSP-FIX-1/manual/CHECKLIST.md`를 따른다. 다른 F1~F14 요청은 이번 수정 범위에 포함하지 않는다.

**현재 최우선은 ARCH 단계(아키텍처 뼈대 재정비)다.** 지금까지 기능을 빠르게 쌓으면서 센서 Runtime·Slab 업무·UI가 한 모듈 안에서 서로를 직접 참조하게 됐다. 다음 기능(다중 센서 확대, 배치 UX 수용, 플러그인화)을 안전하게 진행하려면 먼저 **동작을 바꾸지 않고** 레이어 경계를 세우고 gate로 고정해야 한다. 아래 [0절 ARCH 항목](#0-arch--아키텍처-뼈대-재정비-현재-최우선)을 순서대로 진행한다.

- ARCH 진행 중에는 새 사용자 기능을 추가하지 않는다. 기존 항목(UX-01, RT-05 등)의 진행률은 유지하고, ARCH 변경이 일으킨 회귀만 고친다.
- **센서 플러그인 제작(PL-04/R5)은 이번 버전에서 시작하지 않는다.** ARCH는 같은 모듈 안의 정리이며 `.uplugin`·새 모듈·자산 mount 이동을 만들지 않는다.
- DTCore 소스·gitlink는 건드리지 않는다. 공통 수정이 필요해 보이면 프로젝트 쪽 adapter로 우회하고 근거를 남긴다.

**전체 진행 순서 (2026-10-11 사용자 결정, 10-12 보완):** `ARCH-01~04 → UXR-01~08(사용자 흐름 중심 UI 재정비 + Slab 정보·표면) → ARCH-05~08 → 아래 ①~⑤`. 진행률 보고 기준은 [개편 프로그램 진행률](#개편-프로그램-진행률-arch--uxr)이다. UXR은 ARCH-04로 UI 경계가 정리된 직후 시작하고, UI 파일을 건드리지 않는 ARCH-05~08은 UXR 뒤로 미룬다. UXR 상세는 [0-U절](#0-u-uxr--사용자-흐름-중심-ui-재정비-arch-04-직후)이다.

UXR 이후의 사용자 기능 순서(기존 계획 유지):

**① 센서·카메라 추가 배치와 저장 위치 전용 패널 수용 → ② 작업 흐름 중심 UI/UX 재구성(UXR로 대체) → ③ PCD 송신 사전 점검·예산 안내 → ④ 다중 센서 PCD 장기 검증·조작 성능 → ⑤ 플러그인 분리 전 의존성 정리(ARCH가 일부 선행)**

| 순서 | 사용자가 할 수 있게 될 일 | 연결 항목 | 첫 완료 기준 |
|---:|---|---|---|
| 1 | `배치`에서 LiDAR/Camera를 추가하고 목록에서 선택·이동·저장 위치 복원 | UX-01, RT-02/06, UI-01 | Slab 검증맵 PIE에서 추가 장비가 즉시 목록에 보이고, 위치 저장/이동을 도움 없이 찾으며 기존 센서·Slab는 유지됨 |
| 2 | 배치 → 미리보기 → 시나리오 → PCD 전송 → 결과를 일관된 메뉴로 이용 | UI-03, UI-01 | 자주 쓰는 동작과 현재 선택·실행 상태가 보이며, 좁은 창에서도 주요 동작에 접근 가능 |
| 3 | PCD 시작 전에 대상·준비 상태·예상 전송량·처리량 부족과 해결 방법 확인 | RT-08 1차, RT-01/03, DATA-01 | PCD 중심으로 선택된 출력만 계산하고 미리보기 전용 Camera의 JPEG 송신량은 제외; 부족한 설정을 송신 전 안내 |
| 4 | 실제로 추가한 여러 LiDAR의 PCD를 반복 수집하면서 Camera 시점을 빠르게 조절 | RT-05/06, LIFE-01, QA-01 | 지원할 센서 수·미리보기 조건별 장기 송수신/입력 반응/자원 상한 증거 확보 |
| 5 | 다른 프로젝트로 옮길 수 있도록 센서와 Slab·프로젝트 설정의 결합 축소 | PL-01/02/03 → PL-04 | 중립 adapter와 설정 경계를 먼저 검증한 뒤 플러그인·간편 설치로 진행 |

- RT-01/02/03/04, DATA-01, LIFE-01, QA-01의 관련 회귀는 각 변경과 함께 수행한다. 기존 장기 검수 전체 종료를 배치 UX 개선 착수의 일괄 선행 조건으로 삼지 않는다.
- **현재 보류:** CSV/JSONL/LAZ 등 다른 파일 형식의 일반 개선, JPEG·호환 JSON 송신의 추가 최적화, Camera staging 재사용 최적화. **사용자 2026-10-15 결정으로 DATASET-01의 명시적 opt-in 로컬 학습 데이터셋 범위만 보류를 해제한다.** 기본 출력은 PCD 송신이며 화면에서 데이터셋 생성과 조건을 직접 켠 경우만 새 파일을 만든다. 기존 API·기능·계약은 유지한다. IO-01·RT-09의 일반 보류·이력·진행률은 보존한다.
- 소비자 업무 ACK(RT-07), 운영 보안 확장(SEC-01), 실장비 보정(HW-01), 4+4 이상 확대는 실제 필요 조건에 따라 후속으로 착수한다. UE 로컬 학습 데이터 묶음 저장은 DATASET-01의 별도 승인 범위다.
- DTCore `b2504d1`을 유지하고 프로젝트 Actor·Subsystem·소유 UI에서 우선 해결한다. 공통 변경이 필요한 재현 사례는 별도 근거와 최소 변경안으로 다룬다.

### 확인된 접근성 문제와 설계 판단

- 위치 저장의 현재 진입점은 **`장비 추가·위치 저장 → 저장 위치 · 선택 센서`**다. 이름 입력·현재 위치 저장·목록 이동·Undo/Redo를 먼저 배치하고 숫자 Transform은 접힌 영역에 둔다. 첫 사용의 기본 화면은 배치 창+미리보기이며 저장된 사용자 상태는 우선한다. 패널 생성 실패 사유를 표시하고 늦은 준비는 제한적으로 재시도한다. 과거 Settings 하단 경로는 소유 native UI에서 배치 창 안내로 바꿨으며 Blueprint API는 유지한다.
- 이 native 북마크 영역은 우리 Workspace가 소유한 Settings에만 붙는다. 별도 Designer Root가 있는 사용자 WBP는 native 구성을 자동으로 받지 않으므로 모든 WBP에서 보인다고 가정하지 않는다.
- Slab 검증맵 생성 코드와 최근 실제 실행 기록에는 **LiDAR 1대와 Camera 1대**가 있다. 검증 Rig는 LiDAR를 시작하고 Camera는 정지시켜 등록한다. Camera가 정지 상태로 보이는 것과 Camera Actor가 없는 것은 구분한다. 개별 수정 맵의 실제 구성은 해당 맵에서 다시 확인해야 한다.
- **현재 PIE 배치 UI/API가 추가됐다.** 관찰 시점에 Camera/LiDAR를 생성·선택·미리보기하고, 측정 설정만 복제한다. 도구 장비만 삭제하며 수집 잠금과 wildcard 자동 편입을 차단한다. 레벨 Actor를 삭제하거나 맵을 저장하지 않는다.
- 별도 `센서 배치` 패널에서 기존 북마크 서비스와 전체 배치 서비스를 연결한다. Settings는 장비 규격·품질, Data는 PCD 전송·결과를 담당한다. 기존 위치 관련 Blueprint API와 사용자 Designer는 유지한다. 구현 완료와 두 해상도의 전체 직접 수용 검증 완료는 구분한다.

근거: `VirtualSensorSettingsPanelWidget.cpp`의 `RebuildWidget`/`BuildPoseBookmarkControls`, `SensorToolToolbarWidget.cpp`, `VirtualSensorLayoutSubsystem.cpp`, `SlabScenarioValidationRig.cpp`, `Saved/Reports/SensorPlacementWorkspace/FINAL_STATUS.md`. 검증맵은 PIE에서 사용했고 운영맵은 재저장하지 않았다.

### PKG-02 — v1.0 Shipping/Development 패키지 검증 (P0)

- **출처:** 사용자 2026-10-15, 로드맵 체크포인트1(v1.0). 운영 기준은 Shipping. `chore/pkg-02-v1-check`, 시작 `8e2919c`, 검증 패키지 소스 `cf441f0`, DTCore `b2504d1` 고정.
- **진행률: 80% — Shipping 직접 기능 확인·두 구성 자동 PCD 완료, Development 직접 기능은 환경 보류.** 두 구성 BuildCookRun 성공. Shipping의 Camera 정답 모드/4파일·사실성3옵션 on/off·실제 데이터셋3/3(각7파일)을 확인했다. 두 구성 ML-X80 원본0.05초/준비10초+120초는 receipt19.99Hz, 조절 생략/과부하/중단/150ms 초과 hitch0. 직접 점검은1920×1080·96DPI·글자100% 대표 조합이다.
- **수정:** native literal load의 공용 센서 ID 재질을 기존 자산 그대로 AlwaysCook에 포함했다. 데이터셋 manifest의 배포 PC Git 의존은 빌드 SHA 포함으로 제거하고 기존 `project_commit`·wire/기본값을 유지했다. 실제 cooker 자산 누락·패키지 데이터셋 산출물 검증 스크립트2개를 추가했다. Editor Development·관련 실제 D3D12 자동화2 Success/skip1·quick8·layer/allowlist0 통과. 과거 실패와 검증 source hash/provenance는 보고서에서 구분한다.
- **보류/제한:** Development에 Windows Security 방화벽 권한 창이 나타나 사용자 금지 지침에 따라 입력0·GT/사실성/데이터셋 직접 점검 보류. 최종 자동 PCD는 보안 창이 표시된 조건이다. 제한 링크 조절/과부하 UI·서버 PC·원격 PixelStreaming·소비자 업무 ACK는 별도이며 v1.0 전체 gate 완료로 표시하지 않는다.
- **정리/근거:** 개인44파일 hash 복원, 보호300파일 중 승인된 Build.cs 변경 외 예상 밖0, 빈 SensorTransport.ini 유지, 시험 package/산출물 약2.98GB 삭제, 세션 소유 Artemis 정상 종료·설정12파일 불변. 상세·초기 실패·빌드 경고·Shipping 실제 리디렉션 저장 경로는 `Saved/Reports/PKG-02/RESULT.md`, 서버 PC 후속은 [MANUAL_CHECKLIST](MANUAL_CHECKLIST.md#서버-pc에서-패키지로-확인할-항목--pkg-02).

### CAM-GT-01 — 카메라 정답 데이터 1단계 (P1)

- **출처:** 사용자 2026-10-15. `feat/camera-gt`, 기준 master `827bae6`(PCD-OPT 포함), DTCore `b2504d1` 고정. 이번 요청은 카메라 정답 데이터의 최소 검증 저장만 승인하며 다른 파일 형식 개선·학습 데이터셋 대량 생성 보류를 해제하지 않는다.
- **구현:** 기본 꺼짐인 per-camera 옵션, SceneDepth float32 광학축 Z(m), LiDAR와 공통 기본 semantic 규칙/ID/팔레트, 격리 proxy의 Actor ID, visible-mask 2D bbox, acquisition 시각/frame/revision 및 intrinsics/extrinsics. 기존 재질 재사용으로 새 자산·모듈 없이 구현했다. 소유 미리보기 모드·범례·거리 범위와 상세 설정 설명을 연결하고 5Hz 이하 수요/단일 작업/전역 2개 제한을 둔다. FileSave admission·immutable 비동기 검증 bundle 저장을 사용한다.
- **진행률: 90% — 구현·자동 검증 완료, 직접 조작 보류.** `fc214a7`→`5ee818b`→`a473ffa`→`040abbf`→`00e1584`, 기준 `827bae6`, DTCore 고정. 9.9m 앞면 최대 오차0.00007381m, 기본 LiDAR 규칙 일치·동일 클래스 두 Actor ID/box·이동/stop-start ID 유지·disable 취소·EXR float32/PNG mask 무손실 round-trip 통과. 1920×1080/DPI1/글자100% Slab 자동 4모드 렌더·저장 확인. Editor/Game Development·WBP7 메모리 컴파일·golden·quick8·layer 위반/allowlist0 통과. 전체 D3D12는266 Success/skip28/기존 UXR03 실패1/NotRun0였으며, 재개 시 승인된 PCD-OPT 토글 기대값11→12·설명만 수정하여 단독1 Success로 해소했다. 이후 CameraGT+UXR06은3 Success/skip2/실패0. 기본 꺼짐 첫 ABAB의 GT +2.50% 실패를 보존하고 사용하지 않는 RGB GT 갱신과 계측기의 Tick별 옵션 파싱을 줄였다. 최종 ABAB×3(12회)은 FPS −0.0011%·p95 동일·GT +0.7332%로2% gate 통과. 이 변경들의 초기 차이 기여도는 단정하지 않는다. **직접 마우스는 Windows 보안 팝업으로 중단·미조작**했으며 자동 렌더를 직접 수용으로 세지 않는다. 정답 미리보기 opt-in(1280×720·<=5Hz 수요)의 같은 소스 ABAB 비용은 GT 평균 +0.4635ms(+9.14%), GPU frame 평균 +0.3537ms(+5.83%), FPS −0.0094%, p95 동일이며30/20Hz 측정·실패/큐초과0을 유지했다. 마지막 두 GT 프레임의 readback wall latency35.51/36.66ms·decode37.40/76.77ms는 평균/p95가 아닌 관찰값이다. 개인 SaveGame9개 복원·보호308파일 hash 불변. 상세·초기 실패 원본·2단계 제안은 RESULT를 따른다.
- **완료 조건:** 동일 소스/DTCore에서 intrinsics/known-plane depth/분류 일치/instance/bbox/default-off 및 수명 회귀, 저장 파일 무손실 decode, 관련 실제 RHI 회귀·golden·quick·layer0·WBP 메모리 컴파일, 1920×1080 직접 RGB/Depth/Seg/Instance와 저장 확인, `verify.ps1 -Level perf -BaseCommit 827bae6` 기본 꺼짐 2% gate 및 opt-in GPU/GT/readback 비용 보고. 원본은 `Saved/Reports/CAM-GT-01/RESULT.md`.
- **제한/후속:** 기본 원근 카메라·불투명 static geometry 분류, 개별 LiDAR override 자동 공유 없음, ID는 카메라 실행 범위, ISM은 Actor 단위다. 2단계는 새 topic/schemaVersion 서버 송신·ROS2 Image/CameraInfo·3D bbox를 별도 요청으로 설계한다. 이번 작업은 DTCore/센서 플러그인/운영맵/신규 자산을 변경하지 않는다.

## GPU-CRASH-01 — 일반 GPU/Camera GT 동시 캡처 안정화 (11번)

- **출처:** 사용자 2026-10-15 DATASET-01 재개 승인. 데이터셋이 없는 분리 시험에서 LiDAR·Camera GT 단독과 일반 설정 변경은 각각100회 통과했으나 동시 캡처가 충돌했다.
- **원인·수정:** RHI validation이 기존 private semantic scene의 캐시된 mesh shader binding에서 이전 프레임 단기 uniform buffer 사용을 검출했다. 전역 mesh command cache off 진단은 통과하고 readback 전환만 고친 시험은 실패했다. 격리 proxy의 렌더 상태만 갱신해 stale binding을 재사용하지 않고, 모든 sensor texture readback은 SRVMask↔CopySrc 전환을 명시한다. 전역 CVar·원본 Actor/재질·wire는 바꾸지 않았다. 분류 proxy의 거리장/간접광/ray tracing 참여도 제외한다.
- **검증:** `337dd10`의 핵심 수정 후 일반 경로4종 각각100회와 동시 캡처 RHI validation100회(관련 오류0) 통과. DATASET-01과 함께 FullSpec 반복·release278 Success/skip30·기본 꺼짐2% perf까지 통합 통과했다. 원본 덤프/콜스택·분리/진단/수정 후 결과는 DATASET-01 RESULT 및 resume 보고서에 남긴다.

## TESTSET-03 — 소재 트리거만 사용하는 템플릿100개 생성 (임시 기능)

- **출처:** 사용자 2026-10-19 및 같은 날 최종 정정. 시연 모드에서는`SQ23456 077` 메시지의 완전/불완전/없는 시나리오 데이터를 전부 무시한다.
- **트리거 확정(사용자2026-10-19):** 기본 `SQ23456 077`, 문자열 양끝만 trim·가운데 공백/대소문자 정확히 비교. ini 따옴표 유무 모두 보존하며 숫자 별칭은 제거한다. `9999`는 일반 재생·기본PCD 선택 경로다. 새 값의6형태×2경로100개·일반 소재/비일치 회귀·UXR·최종 커밋 release 및 보호/정리는 `Saved/Reports/TESTSET-03/trigger/RESULT.md`를 따른다. 설정/판별만 변경해 사용자 승인으로 성능 재측정 생략. 기존 캡처는 소재가 노출되지 않아 유지하고 c74ea4a 증거로 구분한다.
- **구현:** 기존 TC/DataSync/Slab·SubmitScenarioJson에서 codec 앞의 소재 분기. 첫 행/최상위/메타·문자열/숫자 트리거를 수용하며 일반 codec은 불변. `[AgentTestSet].TemplatePath`의 커밋된 기본 연속 주조 템플릿을 worker에서 명시 단위→기존 Slab 단위로 변환하고 결정적100개 전부 생성한다. 기본 파일은 패키징 runtime dependency로 포함한다.
- **정책:** `AutoFillExamples=True`에서 수신 원본·슬롯 대체 없음. 진행 중/완료/실패 세트의 반복 트리거는 기존 세트를 유지하고 로그/testset.json에 기록한다. 화면의 중복 안내는 진행 문구를 덮지 않는다. 초기화 뒤 새 세트. False에서는 완전한`SQ23456 077`만 각각 수신·기존 SampleStride 유지. 화면의 원본/예시·시연용·대표 프레임 표시는 없음.
- **화면 마무리(사용자2026-10-19 캡처 검토 후 추가 승인):** 중복 안내 화면 제거, complete에서 생성용 일시정지/재개 숨김, 완료 칩 `테스트셋 완료 n/100`, 최초 접수→complete 전체 시간 `총 n.n초` 고정 표시. 기존5Hz 갱신과 native 완료 시각/진단 캐시만 사용한다. 집중12 Success/4조건부skip 및 별도 UXR07 opt-in RHI2 Success/skip0,36개 실제 visible Slate 상태를 통과했다. 최종 커밋 release·수신/진행/완료 새3장·보호 복원은 `Saved/Reports/TESTSET-03/polish/RESULT.md`에 기록한다. 이전 이미지는 captures/before-polish로 보존하며 사용자 승인으로 성능 gate를 재측정하지 않는다.
- **검증/현재 상태 — 구현·자동 검증 완료, 직접 UI 환경 보류:** Runtime718346e에서6형태×2경로·12회 실제100/100을10.071–10.177초(중앙값10.101초)에 생성했다. engine/GT/callback150ms 초과0, 독립 감사1,200 PCD/라벨 쌍·33B/5B·전체 행/변형 결정성·중복/초기화 통과. Editor/Game·release14단계(고유306 Success/40조건부skip/실패0)·WBP7·golden·Python69·layer0 통과. 제공 py는 실제 PIE 자동 실행 성공.292d03d 대비 표시형1+1 ABAB12회2% gate PASS(FPS 저하0.0011%·p95동일·GT−0.8501%). 개인130/보호568파일 복원·불변, 세션77세트 정리/기존100세트 보존. 직접 UI는 Windows 잠금 관찰·입력0·7행 보류이며 자동3장/client1920×1080·DPI1·글자100%와 구분한다. 전체 근거·실패/수정·제한은 `Saved/Reports/TESTSET-03/RESULT.md`를 따른다.
- **완료 기준:**6형태×2경로의 실제100/100·결정성·PCD/라벨100쌍·33B/5B·150ms 초과0, 중복/초기화·완전 데이터 무시, False 및 비트리거 거절·일반 재생/PCD 회귀, Editor/Game·release/WBP/어휘/golden/Python/quick/layer0·2% 성능·가능한 직접 점검·보호/개인 복원.
- **경계:** DTCore·codec·wire·SaveGame·Blueprint API·센서 품질·일반PCD 정책·자산/맵 불변. 회사 Broker/소비자 수신과 전송은 범위 밖이다.

## TESTSET-02 — 예시 자동 채우기·빠른 시연·테스트셋 화면 (임시 기능, 시연 정책은 TESTSET-03이 대체)

- **출처:** 사용자 2026-10-19. 후속 지시로 빠른 생성 목표를 약1분에서 약10초로 변경했다.
- **구현:** 임시 기본 `AutoFillExamples=True`, 원본1+결정적 예시99 자동 등록, DATASET-01 고정 채널 재사용. 추가 결정에 따라 최초 수신·예시·대체 원본 모두 같은1프레임 기본 상한(`ExampleMaxFrames`)을 적용한다. False는 기존 원본 SampleStride 유지. 화면은 원본/예시 배지만 표시하며 시연용/대표 프레임 및 동의어 안내를 제거한다. 추적 목적/정책·출처/시드/변형 값은 파일에 유지한다. 실제 수신 우선 슬롯 대체와 `examples_replaced` 파일 보존, 공통 소유 패널의 속도/남은 시간·간결한 목록·완료 점군 보기.
- **현재 상태:90% — 구현·자동 검증 완료, 직접 UI 환경 보류.** Runtime70f95cf에서100개10.07초·항목별 PCD/라벨1쌍·150ms 초과0, 실제5개 대체 뒤 원본6/예시94·활성100쌍, 원본6,646바이트 보존·반복 변형 결정성·33B/5B 파일 대조를 통과했다. 생성/대기/완료 대체·소규모 전량 원본 전환, False의 기존 SampleStride·대기/원클릭·점군·일반 우선도 통과했다. Editor/Game·release14단계(고유308 Success/44 skip/실패0)·WBP7·golden14·Python69·layer0, 기본 상태19eb3fc 대비 표시형1+1 ABAB12회2% gate(FPS 저하0.0064%·p95동일·GT+1.611%) 통과. 직접은 Windows 잠금 화면 확인·입력0·7행 보류다. 초회 기존 카메라 검사1건 실패와 동일 코드 단독/전체 재실행, 개인142/보호483파일 복원·세션58세트 정리/기존94세트 보존은 `Saved/Reports/TESTSET-02/RESULT.md`에 구분한다. 미수행을 통과로 올리지 않는다.
- **당시 완료 기준(현행 트리거 기본값은 `SQ23456 077`):** 9999 한 개→원본1+예시99→100/100, 같은 원본의 변형 결정성, 이후 실제5개 대체와 최종 실제 목표수에서 예시0개, 파일/라벨·33B 대조, 기존 대기/원클릭·점군·이동 유지,4개 해상도/글자 조합과 가능한 직접 점검, release/quick/layer0/Python/golden/WBP/어휘 및19eb3fc 대비 기본 상태2%.
- **경계:** 예시는 시연용이며 실제 수신 학습 데이터로 표현하지 않는다. 원본·일반 시나리오·PCD 송신·wire·SaveGame·Blueprint API·DTCore·자산은 유지한다.10초 미달 시 실측 최소 시간과 병목을 보고한다.

## TESTSET-01 — AI Agent 시나리오 기반 모델 테스트셋 (임시 기능, 로드맵 #17)

- **출처:** 사용자 2026-10-16. 최종 범위는 트리거/대기열, DATASET-01 기반 생성, 진행 화면, 목표 완료 후 선택→보기 전용 표시, 일반 시나리오 우선이다. 최초 Slab 보기 재생은 INSP-FIX-2에서 저장 PCD 점군 보기로 바뀐다. 전송 전체와 다중 프로세스 구현은 제외했다.
- **구현:** `[AgentTestSet]`의 당시 기본 소재9999(현재 `SQ23456 077`)·목표100, 원본 디스크 보관·별도 RunUUID, 실시간 재생 없이 원본 행 pose 적용→획득, 유한 캡처/worker 파이프라인, 프레임 파일 검증·manifest/시나리오 요약, DTCore 소유 화면/5Hz 표시·실패 재시도·일시 중지/재개·확인 초기화. 기존 일반 보관10개/PCD/wire/ROS2 경로를 보존한다.
- **현재 상태: 90% — 구현·100개 및 release 검증 통과, 직접 UI 환경 보류.** 검증 Runtime4202a91(제품 동작40049ba). 순차32.689초/파이프라인28.524초, 각100개·351프레임·3,243,223점 검증, 일반2건 우선·실제 보기3개·중지/reset/사용자 편집 보존·UUID 생략 시 최초 ID 유지 통과. 최대GT32.8/34.0ms·callback wall34.3/71.0ms, 150ms 초과0. Editor/Game·release14단계·WBP7·golden·어휘·layer0, Python69 Success. Computer Use 캡처0x80070057(최초+재시도2회)로 입력0·직접 보류. 기본 상태9baf759 대비 offscreen1+1 ABAB12회2% gate 통과(FPS 저하−0.00234%·p95동일·GT−1.66243%)이며 `Saved/Reports/TESTSET-01/RESULT.md`에 별도로 기록한다. 초기 오류·reset manifest race와 수정 근거도 보존한다.
- **제한:** 실행 중 목록만 유지하며 프로세스 재시작 resume는 없다. 센서/Slab을 자동 배치하지 않는다. INSP-FIX-2의 명시적 원클릭으로 선택 센서 작업만 잠시 멈추며 일반 시나리오를 강제 중지하지 않는다. 다중 프로세스 N=2/4는 후속 제안이며 실측하지 않은 처리량을 주장하지 않는다.
- **완료 기준:** 혼합·목표100·우선/재개·실패/재시도·초기화·관찰 재생 불변 자동화, 실제100개 산출물·hitch/메모리/디스크·순차/파이프라인 비교, release/golden/WBP/어휘/quick/layer0·기본 상태2%, 가능한 직접 선택/중지/다른 선택 및 보호·개인 설정 복원.

## IMU-01 — 가상 IMU 센서 1단계 (로드맵 #14)

**구현·요청 검증 완료,독립 검토 전.** 출처: **사용자 2026-10-16**. 기준 `feat/virtual-imu` / `f5a55d2`, Runtime `a5de63c`. 기존 장비에 명시적으로 부착하는 L1 컴포넌트,100~400Hz 고정 게임 시간 재구성,FLU 비력/각속도/정답 자세,seed 기반 백색 잡음·bias random walk·scale·정렬 오차를 구현했다. 기존 DTCore 기반 배치 패널의 접힌 IMU 영역과 5Hz 그래프를 사용한다. 부착/설정은 세션 전용이며 기존 배치 저장값을 확장하거나 삭제하지 않는다.

Recorder/비동기CSV, 별도 topic/schema의 StreamPublisher, 선택적 데이터셋 `imu` 라벨, AUTO-01 조회, 외부 `sensor_msgs/Imu` adapter는 각각 opt-in이다. 기존 PCD 기본·wire/golden·DTCore·자산·운영맵은 유지한다. 실제 보정/deskew,레이더,센서 플러그인화,WSL 설치/재설치는 범위 밖이다. [현재 사용법과 시간/좌표 계약](../README.md#imu-01--선택-장비에-imu-부착-기본-없음)을 따른다.

집중 D3D12 6 Success(운동/시계/통계/seed/꺼짐/상한,PIE CSV·설정 변경 취소·AUTO-01,실제 데이터셋 라벨), Python38 Success, 직접1920×1080/DPI1/글자100% 부착·400Hz·노이즈·기록·CSV·전송 선택·접기 확인. 실제 Artemis445메시지/1484표본,invalid/gap/duplicate0,receipt 확인 후 소유 Broker 정상 종료. WSL 실측은 요청대로 단위 테스트로 대체했고 ROS 직렬화/DDS 실측 통과로 표시하지 않는다. release14단계·고유289 Success/skip34/실패0·WBP7·어휘·golden과 기본 꺼짐 ABAB12회2% gate 통과(FPS 저하 −0.008%,p95 0%,GT −0.487%).동일소스off/on/off/on4회에서IMU2개각400Hz·노이즈on(그래프접힘/출력off)의GT중앙값+0.223ms(+3.41%),p95동일. 최종 수치는 `Saved/Reports/IMU-01/RESULT.md`에 기록한다.

한계: 게임 관측 대역폭 이상 진동은 복원하지 않으며 첫 구간/긴 hitch/순간이동은 준비·누락 처리한다. 실제1Hz·20cm 이동의무잡음fallback가속도RMSE0.337m/s²·p950.670m/s²(초기0.3초제외)는게임프레임근사한계로기록했다. MEMS는 공개 수치에 근거한 일반 가정이며 실장비 보정·온도·포화·진동 rectification·AHRS는 미구현이다.2단계는 점별 시각과 IMU pose의 보간/레버암·clock 검증 후 실제 deskew를 별도 요청한다. 관리 문서의 완료 판정은 release/성능 결과와 제한을 함께 따른다.

## EXPORT-01 — 데이터셋 표준 형식 내보내기 1단계 (로드맵 #12)

- **출처:** 사용자 2026-10-15. `feat/dataset-export`, 기준 `3f57d3a`, DTCore `b2504d1`. 기본 PCD·wire를 유지하고 사용자가 선택한 파일만 생성한다.
- **구현:** Python COCO compressed RLE·KITTI XYZI/calib/label_2·선택 SemanticKITTI 점 라벨과 재로딩/투영 검사. canonical class0..6, selected Slab mesh OBB의 additive schema_minor1. 기존 L3 데이터셋 패널/job은 기본 none이며 완료 후 별도 Python 변환, 중단/종료는 cooperative cancel이다. 소스 데이터·기존 필드·저장 슬롯·운영맵을 바꾸지 않는다.
- **계약·한계:** [형식별 매핑·가정·실행법](../Tools/DatasetTools/README.md). COCO는 기존 minor0 지원, KITTI는 3D OBB 없는 과거 데이터 명시 거부. custom class이며 yaw-only 근사는 별도 opt-in, 원본 OBB·근사 부피 비율 기록. selected Slab 외에는 3D 라벨을 만들지 않는다. stereo/IMU·정규 SemanticKITTI 클래스/odometry·모델 학습은 범위 밖이다.
- **완료 조건:** 실제 Slab PCA가 유효한 GPU20장면 → COCO/KITTI 재로딩·점 수/투영 IoU·원본 불변, Python 좌표/calib/bbox/RLE/class/빈 장면/손상 입력, Editor/Game·관련 자동화·release 전체·golden·WBP7·quick/layer0·표시형1+1 기준3f57d3a 대비2% perf, 직접 UI 선택/기본 없음, 보호·개인설정 복원 및 시험 데이터 용량 정리.
- **진행률: 90% — 구현·자동 검증 완료, 직접 UI 환경 보류.** Runtime `eeff0c1`, GPU20/20 Slab PCA 유효·407519점, COCO80마스크 원본 픽셀 일치, KITTI 투영 bbox IoU 평균0.9580/최소0.9196. Python19·Editor/Game·release14단계(전체285 Success/skip32/실패0)·실제 golden·WBP7·quick/layer0 통과. 표시형1+1/10+30초 ABAB×3: FPS +0.01082%·engine p95 동일·GT +1.88175%,2% gate 통과. 원본 hash 불변·실제 child 취소 시 출력/임시폴더0, 보호547 hash 불변·개인268파일 복원. 시험 데이터249239856→9912698bytes,3장면 subset과 대응 표준 형식만 보관. 직접 Computer Use는 Windows Security 창 때문에 입력0·보류이며 팝업을 조작하지 않았다. 초기 어휘/줄바꿈 실패·offscreen startup SwapChain4 handled ensure·KITTI38.66° 근사 부피3.14–4.72배 제한 및 수행/skip 이름은 `Saved/Reports/EXPORT-01/RESULT.md`를 따른다.

## AUTO-01 — 화면 없는 배치 실행과 로컬 제어 API 1단계 (로드맵 #7)

- **출처:** 사용자 2026-10-15. `feat/auto-control`, 기준 `d45f27e`, DTCore `b2504d1` 고정. CLI/job 또는 사용자가 켠 loopback API에서만 동작하며 기본 PCD·wire·SaveGame·출력 정책을 유지한다.
- **구현:** L2 adapter의 version1 job 검증·순차 실행·JSON 결과/종료 코드, UE socket worker의127.0.0.1 고정·선택적 토큰·게임 스레드 직렬 처리. 기존 Actor/Replay/Dataset/FileSave/Publisher API, Python API/job/생성→채점 예제와 단위/실소켓 회귀, 소유 설정 화면의 세션 토글·주소·배치 상태를 추가했다.
- **현재 상태: 90% — 구현·자동 검증 완료, API 직접 UI 환경 보류.** Runtime `f6af018`. Python/PowerShell 실행기가 새 결과+child 상태로 실패/누락/스키마/timeout을 비0 판정하고 UE 정상 teardown/config flush를 유지한다. Editor/Game Development·Development 패키지 job(시나리오→GPU20장면→PCD30초·598submit/receipt·오류/과부하0), Python12, API/인증/실행기, release14단계(전체284실행/skip31/실패0)·실제 WireGolden·WBP7·어휘·quick/layer0 통과. 기본 꺼짐 표시형1+1·1920×1080·10+30초·ABAB12샘플 모두유효, FPS저하0.0252%/p95동일/GT증가0.5897%로2% gate 통과. 직접 API 토글/주소/배치 상태는 Windows Security 허용창으로 입력0·보류다. 초기 handled ensure22줄·PCA20장면 invalid·중지 경계 미제출1·실패 이력/보호557파일/개인44복원 등 제한은 `Saved/Reports/AUTO-01/RESULT.md`에 분리 기록한다. 독립 검토/운영 인수는 별도다.
- **SCN-RELEASE-01 별도 커밋 `9fa04be`:** 일반 UI 관찰 완료/취소 후 독립 PCD0/s를 직접 재현했다. terminal snapshot/소유 작업 정리 후 live 제어를 해제하며 데이터 필수 잠금·완료 결과·늦은 프레임 거부를 유지한다. 새 회귀의 수정 전 실패→수정 후 통과, 집중12실행/skip3, TCP 장애6종, 직접 UI의 양수 송신을 확인했다. `Saved/Reports/SCN-RELEASE-01/RESULT.md`와 검증 이력을 따른다.
- **경계:** DTCore는 HTTP/STOMP 클라이언트여서 listener가 없어 프로젝트 adapter를 사용한다(DTC-AUTO01). DTCore·새 모듈/플러그인/자산·외부 bind·방화벽 변경은 없다. 디스크 resume/checkpoint·외부 노출/TLS·ROS bag·KITTI/COCO는 후속 제안이다.

## DATASET-01 — 학습용 데이터셋 자동 생성 1단계

- **출처:** 사용자 2026-10-15. `feat/dataset-gen`, 기준 `f499d57`, DTCore `b2504d1` 고정. 이 기능의 화면 opt-in 범위에서만 IO-01 보류를 해제하며 기본 PCD 송신·wire·정책은 유지한다.
- **진행률: 90% — 구현·자동 검증 완료, 직접 조작 환경 보류.** 독립 seed/장면 랜덤화, 실제 Actor/immutable acquisition 라벨, 기존 PCD+CAM-GT 재사용, 단일 장면 비동기 writer와 native owned 데이터 영역을 구현했다. 기존 GPU 동시 캡처 버그는 GPU-CRASH-01로 분리 수정했다. CPU 강제 전환은 사용하지 않는다.
- **집중 검증:** D455 FullSpec1280×720 + ML-X Native576×56 GPU100장면 반복, pause/resume, 실제 Actor truth 일치, 같은 seed의 정답 재현, 정확히 장면당 LiDAR1회, GT-off 파일/내부 readback0, 캡처/저장 중 취소·상한·원상 복원 및 Python PCD 파싱/채점. 수행별 SHA·성능/용량·메모리 cache/retained 구분은 RESULT에 기록한다.
- **통합 결과:** Runtime `d6c41f2`, Editor/Game Development·release14단계·고유 전체278 Success/skip30/실패·NotRun0·실제 WireGolden·quick8/layer0·WBP7 memory compile 통과. 표시형1920×1080/1+1/10+30초 ABAB×3(12샘플 모두유효)의 기본 꺼짐 FPS +0.00061%·engine p95 동일·game thread −0.0761%로2% gate 통과. Native100장면2,013,729점·라벨 일치, yaw 평균0.1155°/p950.3048°/최대0.5638°, 약0.44초/장면·1.544MiB/장면. LLM 거리장42.06–42.09MiB·UObject/mesh 유지·staging GT3/LiDAR2로 고정. 초기61ms callback wall peak를 포함해 성능 한계를 기록했다. 직접 Computer Use는0x80070057로 초기+재시도2회 실패하여 입력0·보안 팝업 미조작이며, 자동 진행 화면을 직접 수용으로 세지 않는다. 보호553파일hash 불변·개인240파일 복원, 시험 데이터는샘플8장면/7.99MB만 남겼다. 상세 원본은 RESULT.
- **계약·한계:** README 스키마를 따른다. 선택 센서/시나리오를 중지한 뒤 명시적으로 시작하며 같은 안정화 장면의 두 요청을 같은 engine frame에 시작한다. UTC/FrameId와 RGB bit 동일성을 seed로 고정하지 않는다. Camera GT의 geometry 제한·Actor 단위 ID, 균일 크기 배율, 같은 PIE 안의 resume를 유지한다. 켜짐 비용과 초기 자원 준비 peak는 숨기지 않는다.
- **후속:** KITTI/COCO 변환은 별도 승인 EXPORT-01을 따른다. ROS bag, 대량 headless/디스크 resume는 계속 제안이다. DTCore·플러그인·새 자산·운영맵 저장은 변경하지 않는다.

## LIDAR-FID-01 — LiDAR 사실성 1단계

- **출처:** 사용자 2026-10-15. Isaac Sim과 비슷한 역할의 센서 시뮬레이션을 위한 시간차·움직임 왜곡·재질 반사 1단계. `feat/lidar-fidelity`, 기준 `e0a6901`.
- **감사:** 기존 점별 시간 필드/PCD wire·seed·CPU SurfaceResponse/거리 노이즈·CPU multi-echo·ML-X Native 프로필을 재사용한다. 빈 곳은 장비 순서 가정·움직임 모델·GPU 재질/입사각·센서 상세 옵션과 강도/시각 색이다. 감사표 원본은 `Saved/Reports/LIDAR-FID-01/RESULT.md` 첫 절.
- **구현:** 세 옵션 모두 기본 꺼짐. `[0,T)` 시간차, acquisition snapshot의 rigid tangent-plane motion, 중립 L0 provider를 구현한 Slab 속도, CPU/GPU 공통 반사 모델·deterministic dropout/노이즈, 기존 소유 설정의 사실성/ⓘ 및 5Hz 이하 preview 색 모드. PCD 33-byte와 CAM-GT semantic label/ID, 기존 enum 값·저장값 유지. 발사 순서/semantic 반사율은 가정이며 실장비 보정이 아니다.
- **진행률: 90% — 구현·자동 검증·성능 완료, 직접 조작 환경 보류.** Runtime `eea0268`, 집중5 Success/수동 세션skip1. CPU 실제 추적 `v×Δt` 최대 오차0.00005126cm, 실제 D3D12 GPU0.00917cm. seed 결정성·정지0·센서 이동·옵션 원복PCD byte 동일·비영 time_offset_ns wire 전달·강도/미검출 순서 통과. Editor/Game Development·표준 release14단계/고유전체272 Success·skip29·실패/NotRun0·golden14·WBP7·quick8·layer0 통과. 기준e0a6901 대비 기본 꺼짐 표시형1+1·10+30초·ABAB×3의 FPS +0.0043%, p95 동일, GT −0.1497%로2% gate 통과. 같은 소스의 이동 Slab off/on ABAB에서 GT +2.6578ms(+50.20%)·GPU +0.2011ms(+3.05%)·p95 +0.5105ms(+3.06%)·FPS −0.4670%이며 각30초Camera900/LiDAR600프레임·획득 실패/큐초과0을 유지했다. 전송 없는 측정/미리보기 조건이다. 1920×1080/DPI1/폰트100% 자동2장은 직접 마우스 수용과 구분한다. 직접 캡처는0x80070057로 초기+재시도2회 모두 실패, 입력0·보안 팝업 조작0, 자체Editor 정상 종료. 개인설정243파일(SaveGame9) 복원·보호557파일hash 불변/추가0. 원본/skip이름/한계는 RESULT를 따른다.
- **완료 조건:** Editor/Game Development, 관련 자동화 및 **release 전체 회귀**, wire golden·quick·layer0·WBP 메모리 컴파일, 1920×1080 직접 이동 Slab/강도 색, Editor 종료 후 `verify.ps1 -Level perf -BaseCommit e0a6901` 2% gate·ML-X Native20Hz opt-in 비용. 보호 경로/개인 UI 저장값 복원. 보고서의 실패·skip·미수행은 통과로 합치지 않는다.
- **2단계 제안/미구현:** 빔 발산·혼합 픽셀/가려짐, 물리 multi-echo 연계, 제조사 firing/재질 calibration, ROS2 비영 time_offset/deskew 소비자 검증. 기존 GPU 첫 표면 한계를 유지하며 DTCore·새 자산·플러그인·ROS2 배포는 변경하지 않는다.
### ROS2-01 — Artemis → ROS2 외부 변환 브리지 (P1)

- **출처/범위:** 사용자 2026-10-15. `feat/ros2-bridge`, 기준 `67033d4`(CAM-GT 브랜치, PR #37 미머지), DTCore `b2504d1` 고정. UE C++·기존 wire·맵·자산·Broker 설정을 바꾸지 않는 1단계다. Docker는 사용하지 않는다.
- **구현:** 전용 Ubuntu 24.04 WSL2 설치/실행/정상 종료/완전 삭제, 고정 버전 Jazzy ROS base·Foxglove·STOMP 의존성, PCD PointCloud2·legacy LiDAR world 변환·telemetry 전달·JPEG CompressedImage·가능 범위 CameraInfo·acquisition TF. Image rgb8은 Pillow 선택 의존성 경로가 있으며 추가 설치 결정/실제 WSL 검증은 별도다. PCD 로컬 m/오른손 축을 유지하고 TF만 UE 월드 Y를 변환한다. `/clock`은 simulation-time 계약이 없어 발행하지 않는다.
- **진행률: 80% — 구현·CLI 검증 완료 범위와 미수신/직접 수용을 구분.** Windows25 Success, WSL23 Success/skip2(Pillow 없음), ROS 직렬화3 Success, 실제 Artemis 경유 PCD/legacy LiDAR/telemetry/유효 JPEG4 Success. 최종 고정 설치 스크립트의 빈 WSL 재설치도 통과했다. `9f62a08`의 D3D12 PIE ML-X(80) 576×56·1920×1080·준비10초+측정300초에서 UE 송신 회귀2개 Success. 전체 준비 포함 수는 20Hz UE/브리지6200/ROS6119, 10Hz UE3099/브리지3098/ROS3083이다. 브리지 손상·queue 초과0이며 10Hz의1프레임 미수신과 best-effort ROS 추가누락81/15(발견 포함)은 해소 완료로 표시하지 않는다. 이후 legacy JSON double 반올림을 정확한 STOMP 문자열 FrameId로 복원하는 호환 처리를 추가했으며 최종 Windows/WSL/Artemis 검증을 통과했다. PCD 함수/노드는 실측과 동일함을 AST/diff로 확인했다. 상세 수치·초기 실패 원본·범위는 `Saved/Reports/ROS2-01/RESULT.md`를 따른다.
- **보류/정리:** 화면 캡처 오류 `IGraphicsCaptureItemInterop.CreateForMonitor 0x80070057`와 창/브라우저 inventory 없음으로 직접 UE/Foxglove 조작·3D/축 비교는 수행하지 못했다. Image 디코더 추가 설치는 허용 목록 확인 질문에 응답이 없어 실행하지 않았다. 생성 WSL은 두 설치 모두 unregister, 설치 캐시0·기존3개/기본Ubuntu(v1) 복원, 소유 Artemis 정상 종료, 개인43파일(SaveGame9) 복원·보호860 hash 불변. 환경 다운로드 payload 추정 총량은 약1.091GB이며 HTTP/TLS 등 총 네트워크 bytes는 미계측이다. 자동 수신·프로토콜 검사를 직접 수용으로 세지 않는다.
- **완료 조건:** 두 환경 golden 점 수/좌표/레이아웃·손상/미지원 schema 폐기·유효 JPEG와 ROS 메시지 검증, 동일 SHA/프로필의 실제 Artemis 5분 Hz/bw·UTC 지연·누락/queue 대조, Foxglove 3D와 UE 축 직접 비교, quick·layer0, 개인 SaveGame/보호 경로 복원, 생성 배포판 unregister·설치 캐시 제거·기존 WSL 목록/기본값 보존. 관련 실행법은 [Tools/Ros2Bridge](../Tools/Ros2Bridge/README.md).
- **2단계 제안:** UE UI의 ROS2 상태·버튼, binary Camera calibration/pose와 CAM-GT depth/segmentation의 새 topic/schema 연계. 이번 외부 adapter가 이 후속 계약 변경을 승인하지 않는다.

### ROS2-02 — ROS2 양방향 제어와 acquisition 시간 기준 (P1, 로드맵 #13)

- **출처/범위:** 사용자 2026-10-15. `feat/ros2-control`, 기준 `55b4dce`, DTCore `b2504d1` 고정. 기존 AUTO-01 loopback API를 외부 ROS 노드에서 호출하며 UE C++·API/wire·맵·자산·기본 정책을 바꾸지 않는다. 외부 제어와 `/clock`은 명시적 opt-in이다.
- **구현:** `std_srvs/SetBool` PCD 시작/중지, `Trigger` 캡처/상태, schema가 있는 `std_msgs/String` 명령으로 센서 일부 설정/측정·시나리오·데이터셋 생성/중지를 연결한다. 별도 제어 프로세스와 최대2Hz result/status 피드백을 사용한다(result는 명령별 즉시 발행). WSL NAT→Windows의 실제 WSL vEthernet 주소에만 bind한 임시 사용자 중계가 토큰을 필수 검사하고 기존127.0.0.1 API의 Host/auth 계약을 보존한다. API 응답 대기가 점군 변환을 막지 않는다.
- **시간:** 선택 `/clock`은 유효 acquisition UTC의 단조 최대값이다. PointCloud2/Image/TF stamp를 수신 시각으로 바꾸지 않는다. 역순 측정은 원본 stamp로 유지하고 clock 역행만 막는다. 소비자 `use_sim_time` 사용·초기0·측정 없을 때 정지·단일 clock 발행자·steady timer 규칙을 문서화했다. UE elapsed-time/pause/step/reset이나 OS 시계 조정을 지원한다고 표시하지 않는다.
- **진행률: 100% (이 항목의 외부 제어 범위).** Windows34 Success, WSL32 Success/skip2(Pillow 없음), ROS 메시지/서비스5 Success, 실제 제어13항목·중계 인증3항목 통과. 최종 API input357/submit356/receipt356, 브리지357, ROS PointCloud2342(약19.14Hz)이며342개 수신 point stamp가 clock357개 집합에 모두 포함됐다. clock 역행0·queue0이다. 외부 수신 차이15와 API snapshot/브리지 차이1을 무손실로 표시하지 않는다. 최종 Windows–WSL 오차는 시작−623.877ms/종료−73.732ms로 변했고 OS clock은 조정하지 않았다. quick8·layer0, 보호1025 hash 불변·개인44파일 복원·전용 WSL unregister/기존3개·기본Ubuntu 보존 완료. 선택 사항인 UE 출처 UI는 아래 사유로 추가하지 않았다.
- **검증:** Windows/WSL Python, 실제 ROS 서비스/메시지 직렬화, WSL `ros2 service call`/`ros2 topic pub`→UE Game D3D12에서 PCD 시작→수신→중지, 비동기 캡처,3장면 데이터셋, 시나리오 시작/중지·진행 피드백과 stamp 정합을 검사한다. 실측 수·초기 실패·Windows–WSL 오차·수신 누락·동일 소스/DLL 근거·quick/layer·전용 WSL unregister/목록 대조·보호/개인 설정 복원 결과는 `Saved/Reports/ROS2-02/RESULT.md`를 따른다. UE 코드 변경이 없어 release/WBP/golden/perf 전체는 이 항목에서 반복하지 않는다.
- **UI/제한:** 기존 API 상태에 출처/제어 lease가 없어 UE에 "ROS2에서 제어 중"을 추정 표시하지 않는다. ROS 피드백 `source=ros2_bridge`는 발행 경로만 뜻한다. 센서 변환/직접 Foxglove 3D 등 ROS2-01의 남은 수용은 이 항목의 제어 실측으로 통과 처리하지 않는다. receipt·DDS 수신·소비자 업무 완료는 별개다. 명령·스키마·네트워크/종료 절차는 [Ros2Bridge README](../Tools/Ros2Bridge/README.md)를 따른다.

## 읽는 방법

- **정적 위험:** 실제 소스의 연결/조건을 확인했다. 이 문서만으로 런타임 재현 완료를 뜻하지 않는다.
- **검증 공백:** 구현이 있으나 해당 조건의 근거가 부족하다. 먼저 재현/계측하고 실패가 확인되면 고친다.
- **신규 기능:** 현재 없는 목표 기능이다. 기존 결함으로 취급하지 않는다.
- **P0:** 데이터 정확성·상태 복원·자원 한계의 중요도. **P1:** 기능·사용성의 주요 개선. **P2:** 선택적 제품화. 이 중요도와 위의 현재 착수 순서를 구분한다. 보류 항목의 과거 중요도는 착수 지시가 아니다.
- 구현 상태와 검증 상태를 따로 읽는다. 실제 화면 검증 미수행을 완료로 바꾸지 않는다.
- 미래 요구를 현재 기능이라고 설명하지 않는다. 항목 ID는 커밋/PR/시험 보고서에서 유지한다.

### 진행률 기준

진행률은 **해당 개선항목의 완료조건에 대한 단계 지표**다. 작업시간·남은 사용량·제품 전체 완성도·성능 수치의 비율을 뜻하지 않는다.

| 진행률 | 의미 |
|---:|---|
| 0% | 제안·미착수. 기존 기반 기능만 있는 경우 포함 |
| 10% | 문제와 구현 공백 조사 완료, 해결 구현은 미착수 |
| 40% | 일부 구현 또는 일부 목표 조건의 검증 완료 |
| 70% | 핵심 구현·집중 자동검증 완료, 주요 수용검증이 남음 |
| 90% | 핵심 조건 통과, 제한된 최종 확인·완료 범위 정리가 남음 |
| 100% | 해당 항목의 명시적 완료조건·검증·문서 정리까지 완료 |

- 신규 기능의0%는 현재 센서·카메라 기능 자체가 없다는 뜻이 아니다. 기존 기반 기능을 신규 목표의 구현 진척으로 합산하지 않는다.
- 크기가 다른 항목(ARCH 8개 포함 31개)의 단순 평균으로 프로젝트 전체 진행률을 만들지 않는다. 새 UI/UX 재구성 항목 UI-03은 이번 일부 구현·검증에 맞춰40%다. 보류는 완료가 아니며 요약표와 상세 진행률은 일치해야 한다.
- RT-01·RT-04의 일반적인 장기·복합 장애 시험은 LIFE-01·RT-05와 연결한다. 해당 항목의 최종 증거 대조와 완료 범위가 닫히면100%로 갱신하며, 전체 R1 완료로 확대하지 않는다.

## 항목별 중요도와 진행률

실제 착수 순서는 ARCH-01~08 다음 문서 상단의①~⑤를 따른다. 아래 P0/P1/P2는 문제의 중요도이며, 보류·유지 항목이 현재 작업을 모두 앞선다는 뜻은 아니다.

| ID | 분류 | 우선 | 진행률 | 핵심 | 남은 핵심 조건 | 로드맵 |
|---|---|---:|---:|---|---|---|
| ARCH-01 | 구조 / 기준선·gate 완료 | P0 | **100%** | DTCore guard·38쌍 include allowlist·D3D12 기준선 고정 | 완료; 후속 단계에서 gate 유지, opt-in skip 20개는 별도 명시 | A |
| ARCH-02 | 구조 / 폴더 재배치 완료 | P0 | **100%** | Slab 11파일 → `Scenario/`, Workspace 2파일 → `UI/` | 완료; allowlist 38→31, 기준선 229개 이름별 결과 동일 | A |
| ARCH-03 | 구조 / 편집 타입·기본값 분리 완료 | P0 | **100%** | 편집 타입 → L1 Sensor, 기본값 상수 → L0 Contracts | 완료; ControlTypes 의존 9쌍 해소·UHT 계약·저장 회귀 통과 | A |
| ARCH-04 | 구조 / 호출 역전 완료 | P0 | **100%** | Monitor·배치 입력 interface, Slab UI 약한 존재 registry | 완료; allowlist 22→18, L1/L2→L3 0, 기존 BP/GC 계약 유지 | A |
| ARCH-05 | 구조 / 송신 검증 분리 완료 | P1 | **100%** | Transport/ExternalSource의 WebSocket/TC include 9쌍 제거 | 완료; 전체 249개 이름별 결과 동일·보호 경로 불변 | A |
| ARCH-06 | 구조 / Slab context 중립화 완료 | P1 | **100%** | L0 값 context·provider, L2 Slab adapter·공개 reflected 호환 유지 | 구현·검증 완료; golden14 동일·layer9→0·기존249개 결과 동일, 독립 검토 대기 | A (PL-01 일부) |
| ARCH-07 | 품질 / 검증 진입점 완료 | P1 | **100%** | `verify.ps1` quick/feature/release, 환경변수 67개·Scripts 110파일 분류 | 완료; manifest·skip 원인 집계·전체 기준선 유지 | A (QA-01 일부) |
| ARCH-08 | 구조 / 정리 완료 | P2 | **100%** | test hook guard·대형 3파일 분할 (PixelStreaming 의존 제거는 사용자 결정으로 되돌림 `7316787`) | 완료; 331개 본문 불변·빌드/249개 회귀 동일·성능2%gate | A |
| UXR-01 | UI/UX / 작업 흐름 바 | P0 | **100%** | 소유 흐름 바·5Hz 상태 줄·도구 메뉴·용어 상수, 자동화·4조합 렌더 통과 | Claude 직접 조작 검토는 별도; 세부 화면 재정비는 UXR-02~07 | U |
| UXR-02 | UI/UX / 배치 | P0 | **100%** | 완료(마우스 최종 삭제 1건 보류), 상태 칩·클릭 마커 격리 재검증 | 모니터 복귀 후 최종 삭제 직접 재검수; Saved/Reports/UXR/UXR-02 | U |
| UXR-03 | UI/UX / PCD 전송·결과 | P0 | **90%** | 소유 데이터 5탭·신규/재생 2열·재생 요약·결과/진단 분리, 자동화·20장 렌더 완료 | 보완: 쉬운 점검 문장·대상 세그먼트·직접 전송 헤더, 집중 13 Success/4조합 전송 렌더/전송 탭 성능 통과; 직접 조작 보류(모니터 없음) | U |
| UXR-04 | UI/UX / 시나리오 | P1 | **90%** | 기존 진행·목록 나란히 기본 배치, 차트 토글·표시 설정 접힘, 집중 28 Success/렌더 8장/성능 2% gate 통과 | 직접 조작·독립 검토 보류(모니터 없음), 출력 편집은 전송 화면만 | U |
| UXR-05 | UI/UX / 센서 상세 설정 | P1 | **90%** | owned 3단·ⓘ 도움말·5Hz 캐시, 집중52/최종17 Success·24장·성능2%gate | 기본/고급 레거시0·금지어16 검사 통과, 직접 조작·독립 검토 보류 | U |
| UXR-06 | UI/UX / 미리보기 | P1 | **90%** | 상태1줄·실제 경고색·중지/장비 없음 안내·닫힌 진단 중단, 집중29/최종8+1 Success·20장·성능2%gate | 전체229 Success/skip20/실패0, c428ab6 이름별 유지+신규2; 직접 조작·독립 검토 보류 | U |
| UXR-07 | UI/UX / 문구·수용 | P0 | **70%** | 자동 회귀·주요 직접 점검·7패널 Slate 드래그·ABABAB 상대 성능 통과. 전체 수용은 미완료 | 자유 배치 직접 FAIL 재확인·화면 활성화·남은 조합·사용자 확인 | U |
| UXR-08 | UI/UX·Slab / 정보·표면 | P1 | **90%** | 원본/현재 정보 카드·DTCore Click adapter·MID 5종(승인된 산화 전용 재질)·명시적 광학 연동·optional 결과 | 직접 조작/독립 검토 보류; 산화 전환/복원·CPU/GPU/PCD 회귀 통과, GPU기존분류버퍼 조건·BaseColor회귀 범위 | U |
| UXR-09 | UI/UX / 시각 리프레시 | P0 | **90%** | 카드·더보기·고정 저장 바·간결한 상태바 보완, 최종 상대 성능 2% gate·239 Success·48장 렌더 통과 | 변경 화면 직접 조작 보류: 창 활성화 2회 실패. 절대 성능 목표 및 이전 14.4% 증가 전체 원인은 별도 제한 | U |
| UXR-10 | UI/UX / 사용자 피드백 | P1 | **70%** | 시나리오 resize·기존 Workspace v1 크기 복원, 사이드바 폭 전환, 하위 화면 공통 뒤로 가기 구현·집중 자동검증 | 앱 Codex 직접 마우스·통합 release·Editor 종료 후 QA-02 성능 gate 보류 | U (별도 후속) |
| UXR-11 | UI/UX / 실제 마우스 결함 | P0 | **80%** | 검수본 직접 마우스 FAIL 2건(자유 배치 제목 드래그·모서리 resize) 원인=PIE viewport 캡처 선점, 소유 패널 hit 시에만 해제·포커스·제스처 정리, 프로필 재적용 안내 | 수정본(40aa640) 직접 마우스 재검수·통합 release·성능 gate | U (별도 후속) |
| UXR-13 | UI/UX / 장비 이동 조작 | P0 | **85%** | 사용자 직접 확인(2026-10-14): 이동·회전 시작 후 조작법을 몰라 움직이지 못함, 정지 장비 미리보기가 검게 멈춤. 3D 화면 키 안내·키보드 포커스·보던 화면 유지·정지 장비 안내 | 사용자 직접 확인 | U (별도 후속) |
| UXR-14 | UI/UX / 창 크기·3D 글자 | P1 | **85%** | 사용자 요청(2026-10-14): 오른쪽 아래 미리보기 카드를 모든 변·모서리에서 크기 조절, Slab 3D 글자 크기 56 초과 | 사용자 직접 확인 | U (별도 후속) |
| UXR-16 | UI/UX / v1.1 통합 점검 | P1 | **70%** | API·일괄·정답/데이터셋/변환·IMU 통합 회귀, 전후108장씩·저위험 표시 정리·release290/skip34/실패0·오프스크린2% 통과 | Windows 잠금으로 직접0·표시형 성능 보류.24행/70분·큰 설정 구조 제안·독립 검토 | U / 로드맵 #10 |
| UXR-17 | 미리보기 카드 개선 | P1 | **90%** | 준비/5초 안내·모드 배지/범례·헤더 우선 배치, 전후48장씩·release291/skip37/실패0·WBP/어휘/golden/layer0·2% 성능 통과 | 직접5행은 캡처0x80070057·입력0·환경 보류, 독립 검토 대기 | 사용자 2026-10-16 / 로드맵 #18 |
| UXR-18 | 센서 상세 설정 재배치 | P1 | **90%** | 기본/고급/자동화·기존 IMU/데이터셋/TESTSET 연결·세션 탭 기억·켜짐 개수·전후80장씩·release293/skip37/실패0 | 성능은 반복0xC000013A 종료로2% 미판정. 직접6행은0x80070005/0x80070057·입력0·환경 보류 | 사용자 2026-10-16; 성능 2% gate 통과(2026-10-17 재측정: FPS −0.002%·p95 0%·GT +1.26%) |
| PKG-01 | 배포 / 서버 연결 | P0 | **80%** | Shipping 패키지에서만 "연결 설정 필요": Shipping SaveGame이 사용자 폴더라 UI 저장 서버 설정이 넘어가지 않음 + 프로젝트 빈 `Config/Game.ini`가 pak에 들어가 패키지 옆 파일을 가림 → 프로젝트 `Config/SensorTransport.ini`를 에디터가 읽고 패키징 시 패키지 옆에 일반 파일로 복사(첫 실행 템플릿 생성은 보조), Game.ini·SensorTransport.ini pak 제외. Shipping 패키지에서 적용 로그 확인 | 사용자 실제 브로커 연결 확인 | 배포 |
| PKG-02 | v1.0 패키지 검증 | P0 | **80%** | cf441f0 Shipping/Development BuildCookRun·두 구성 PCD20Hz 2분/hitch0, Shipping GT/사실성/데이터셋3장면 확인. 재질 cook·배포 Git 의존 수정 | Development 직접 기능: Windows Security 창으로 보류, 서버 PC 후속 | 배포 |
| PCD-20HZ | 송신 / 연결 경고 | P0 | **90%** | 원인: 고속 TCP SEND 본문 5초 쓰기 한도 초과 시 연결 해제(브로커 순간 정체 → "연결 설정 필요"). 정체 중 연결 유지·receipt/ERROR 처리·60초 최종 상한·협상 heartbeat·EOF 정확 판정·단절 사유 기록·지연/재연결 문구. 가짜 서버·설치 Artemis 1대 20/10/5Hz·2대 20Hz 5분 모두 경고 0, 성능 2% 이내 | 사용자 환경 실측 확인, PCD 과부하 시 스트림 정지(NoLoss) 자동 재개 정책 결정 | 후속 |
| MAP-01 | 이식 / 새 맵 적용 | P0 | **90%** | L3 Rig 재사용/소유 정리·Slab 없음 안내·DTCore 맵 점검·새 fixture 2개·CLI 검증 | 직접 마우스는 요청에 따라 UXR-07로 이관; DTCore 연결 조회 API 제한·독립 검토 대기 | U |
| RT-02 | 조작 / 구현·자동검증 완료 | P0 | **70%** | 종료 상태 통일·원설정 복귀 | 해상도별 전체 직접 조작 체크리스트 | R1 |
| RT-01 | 성능·전송 / 구현·집중검증 완료 | P0 | **90%** | frame/byte reservation·과부하 진단 | 상한·재시도·종료 증거의 최종 연결 및 완료 범위 정리 | R1 |
| RT-04 | 데이터 / 유지·회귀 | P0 | **90%** | capture 직후 readback·immutable metadata | Camera 배치/미리보기 변경의 정합성 회귀; 송신 고도화 보류 | R1 |
| RT-03 | 진단 / 구현·자동검증 완료 | P1 | **70%** | 0Hz·신선도·공정성 판정 | 실제 정체·일시정지·복귀 화면 확인 | R1 |
| QA-01 | 품질 / 검증 공백 | P0 | **70%** | 고정 의존성·clean checkout·fixture 분리 | 실행 manifest·새 성능 계측·최종 비교 | R0 |
| RT-05 | 성능 / 기본2+2 검증 | P1 | **70%** | 새 배치 PCD 기본값·센서별 수용 | 대표 장면·4+4·숨김/운영 환경·60분 | R2 |
| RT-06 | 사용자 / 집중 구현·검증 | P1 | **40%** | 연속 배치·입력 지연 계측 | 2+2/4+4·직접 조작·지원 매트릭스 | R2 |
| RT-08 | 성능 / 일부 구현 | P1 | **40%** | PCD 사전 예산 안내 → 작업량 기반 접수 | 다양한 echo/backend 실측 대조·동적 접수/공정 배분 | R2 |
| RT-09 | 성능 / 현재 보류 | 보류 | **10%** | Camera staging 재사용 | Camera 배치/미리보기의 확인된 병목일 때 재검토 | R1/R2 |
| UX-01 | 사용자 / 핵심 구현 | P1 | **70%** | 센서 배치 전용 패널·추가·저장 위치 | 첫 사용자 수용·전체 직접 workflow·Editor 영구 설치 | R2 |
| DATA-01 | 사용자·신뢰성 / 구현·집중검증 | P1 | **90%** | 데이터 필수 실행 정책 | Engine/WSS 필수 모드·장시간/운영 환경 검수 | R3 |
| RT-07 | 전송 / 신규 기능 | P2 | **0%** | 소비자 업무 ACK | ACK 계약·멱등 처리·미확인 결과 관리 | R3 |
| LIFE-01 | 안정성 / 구현·집중검증 | P1 | **70%** | 실행별 회수·종료 snapshot·결과 보존 | 전체 fault matrix·자원 인과·장시간 범위 | R1/R3 |
| UI-01 | UI / 검증 공백 | P1 | **40%** | 해상도·DPI·소유권 | 1920×1080 직접 조작·DPI·외부 Widget 입력 | R2/R5 |
| UI-03 | UI/UX / 핵심 구현 | P1 | **70%** | 사이드바·배치부터 PCD 결과까지 주요 직접 workflow 확인 | 자유 배치 입력·상대 성능·전체 조합과 첫 사용자 수용 | R2 |
| UI-02 | UI / 신규 기능 | P1 | **85%** | 3D 진단 라벨 정리 구현(142a6ff, 한글 수정 763ba4d) | 기본 표시 최소화·거리 크기/페이드·겹침 회피·사이드바/카드 영역 회피 | R2 |
| PERF-01 | 성능 / 검증 공백 | P0 | **90%** | Broker 없는 범위 회귀 없음(221cd2d), 실제 PCD 송신 비교만 보류 | Broker 준비 후 표시형 1920×1080 1+1·2+2 ABAB 교차 비교, 누적 회귀 원인 | 후속 |
| OPS-01 | 운영 / 에이전트 환경 | P1 | **50%** | 새 Codex 대화·입력기 포커스·모델 용량 오류 대응 | 전용 계정/VM 또는 점검 시간대 운영 | 후속 |
| QA-02 | 품질 / 자동 검증 | P1 | **70%** | verify 자동 실행·성능 gate 교차 측정 표준화 | 예약/hook 실행, perf 단계 고정 | 후속 |
| FONT-01 | UI / 선택 | P2 | **0%** | 한글 UI 글꼴 에셋 | 사용자 승인 후 소유 패널만 적용 | 후속 |
| IO-01 | 파일 / 현재 보류 | 보류 | **10%** | LAZ 외부 프로세스 제한 | PCD 외 형식의 개선 재개 요청 시 착수 | 후속 |
| PL-01 | 구조 / 신규 기능 | P1 | **0%** | host 의존성 역전 | 중립 인터페이스·Slab 의존 분리 | R4 |
| PL-02 | 이식 / 신규 기능 | P1 | **0%** | 기본값·설정 namespace | 기본값·자산·Topic·저장 경계 중앙화 | R4 |
| PL-03 | 확장 / 신규 기능 | P1 | **0%** | 장비 registry | ProfileId 기반 등록·선택·호환성 | R4/R6 |
| PL-04 | 설치 / 신규 기능 | P1 | **0%** | Bootstrap·배포 | 센서 플러그인·단일 진입점·소비 프로젝트 검증 | R5 |
| SEC-01 | 운영 / 검증 공백·신규 기능 | P2 | **10%** | 보안·연결 운영 | 배포별 인증·권한·TLS·설정 검증 | R3/R6 |
| HW-01 | 충실도 / 검증 공백 | P2 | **0%** | 실장비 충실도 | SDK·실측 데이터·캘리브레이션 비교 | R6 |

## 0. ARCH — 아키텍처 뼈대 재정비 (현재 최우선)

목표 레이어와 현재 위반 목록은 [README §10](../README.md#10-아키텍처-뼈대)이 원본이다. 이 절은 단계별 작업 범위와 완료 조건이다.

**공통 규칙 (모든 ARCH 항목)**

- **동작 불변이 원칙이다.** wire bytes(PCD/JPEG/JSON/telemetry), Blueprint 노출 함수·핀, UCLASS/USTRUCT/UENUM 이름과 enum 값 순서, UPROPERTY, SaveGame 슬롯·버전, Topic 기본값을 바꾸지 않는다. 바꿔야 할 이유가 생기면 멈추고 보고한다.
- 같은 모듈 안 파일 이동은 `git mv`로 하고 include 경로만 고친다. 이동 커밋과 로직 변경 커밋을 섞지 않는다.
- `.umap`/`.uasset`을 저장·재생성하지 않는다. WBP는 메모리 컴파일만 한다. DTCore·`Config/Game.ini`·`Samples/PixelStreaming`·운영맵·개인 SaveGame은 보존한다.
- 레이어 gate의 allowlist는 **줄어들기만** 한다. 파일 이동으로 경로가 바뀐 항목은 같은 커밋에서 경로만 갱신할 수 있지만 개수가 늘면 안 된다.
- 각 항목 완료 = 빌드 + 레이어 gate + 관련 자동화(최소: 기준선에서 통과하던 그룹 전체) + WBP 메모리 컴파일 + 보호 경로 불변 + README §10.2 표 갱신 + 보고서(`Saved/Reports/ArchitectureSkeleton/<ID>/`).
- 새 플러그인·새 UE 모듈·`.uplugin`을 만들지 않는다(이번 버전 범위 밖).

### ARCH-01 — 기준선 고정과 레이어 gate (P0)

**진행률: 100%** · 구현·집중검증·기준선 기록 완료(2026-10-11).

- **변경 전 근거:** `Scripts/validate_dtcore_submodule_guard.ps1`의 `ExpectedCommit`이 `2eec1fee…`인데 실제 gitlink는 `b2504d1`이었다. 문서가 커밋 전 실행을 요구하지만 그대로 실행하면 실패했고, 레이어 경계를 검사하는 도구가 없었다.
- **작업:**
  1. guard의 기대값을 현재 고정값 `b2504d1`(전체 SHA)로 맞추고 현재 worktree에서 통과를 확인한다. gitlink 자체는 바꾸지 않는다.
  2. `Scripts/validate_architecture_layers.ps1`을 추가한다. `Source/ma0t10_dt/MA0T10` 아래 `Tests/`를 제외한 `.h/.cpp`의 `#include`를 읽어 폴더→레이어 표(README §10.1)로 분류하고, 금지 방향을 `Scripts/architecture_layers_allowlist.txt`와 대조한다. 신규 위반 또는 **더 이상 필요 없는 allowlist 항목**이 있으면 실패 종료한다(목록이 줄 때 갱신을 강제).
  3. 초기 allowlist는 현재 위반과 정확히 같게 만든다. README §10.2 표와 개수가 맞아야 한다.
  4. 변경 전 기준선을 기록한다: Editor Development 빌드, `Scripts/validate_sensor_v2_refactor.ps1 -RequireAssets`, D3D12 전체 `MA0T10`. commit/submodule SHA, Success/실패/skip 이름을 `Saved/Reports/ArchitectureSkeleton/A01_baseline/`에 남긴다.
- **완료:** guard·layer gate가 현재 HEAD에서 통과, 위반 1건을 임시로 추가하면 실패, allowlist 항목 1건을 지우면 실패(되돌림 확인). 기준선 보고서 존재.
- **결과:** 기대값을 `b2504d1dfea5659f832ff0d750515edee146a28e`로 수정했고 gitlink·DTCore worktree는 불변이다. `validate_architecture_layers.ps1`은 205개 production `.h/.cpp`의 include를 검사하며 현재 위반 38쌍을 allowlist 38개와 정확히 대조한다. 신규 1건·allowlist 삭제 1건·stale 추가 1건의 exit 1과 원복 exit 0을 `Saved`의 복사 fixture에서 확인했다. production 소스 변경은 없다. README §10.2를 실제 집계(Workspace 14, ControlTypes Camera 1 포함 9, Player→Gizmo 2건 누락 등)에 맞췄다.
- **기준선:** 프로젝트 `1119fbee4c81af3cec14d4f27024ebe952543914`, DTCore 위 고정값. UE 5.3 Editor Development 빌드·V2 `-RequireAssets`·WBP 7개 메모리 컴파일 통과. D3D12 전체 `MA0T10`은 **실행 Success 209 / 실패 0 / 명시적 skip 20 / NotRun 0**이다. UE 원본의 Success state 229개(일반 217 + 경고 포함 12) 중 skip-return-success 20개를 통과에서 제외했다. 사용자 맵 대신 `Saved`에 추출한 커밋본을 composition fixture로 사용했고, 보호 파일 298개 hash 불변·개인 SaveGame 9개 복원을 확인했다.
- **증거·제한:** `Saved/Reports/ArchitectureSkeleton/A01_baseline/summary.json`, `BASELINE.md`, `automation/index.json`, `layer-gate.json`, `gate-self-tests.json`, 빌드·WBP·보호 hash 로그. Success·skip 전체 이름과 사유는 보고서에 있다. opt-in Broker·fault·장기/RHI 입력 시험은 이 기준선에서 미실행이며 수용 통과로 확대하지 않는다. RiderLink `RdEditorModel` hash 불일치 stderr는 남아 있지만 최종 시험 exit 0·보고서 완료를 확인했다. 첫 수집 런처의 stderr 예외 종료 로그도 별도 보존했다. Computer Use·자산 저장·보호 경로 수정은 수행하지 않았다.

### ARCH-02 — 폴더 재배치 (P0)

**진행률: 100%** · 이동·include 경로 수정·기준선 회귀 완료(2026-10-11).

- **작업:** 이동만 한다.
  - `Core/` → 새 `Scenario/`(L2): `SlabExecutionTypes.h`, `SlabRunComparison.*`, `SlabRunDeliveryTypes.h`, `SlabRunResultsSubsystem.*`, `SlabScenarioReplaySubsystem.*`, `VirtualSensorSlabContextSubsystem.*`, `VirtualSlabFrameContext.h`.
  - `Core/VirtualSensorToolWorkspaceSubsystem.*` → `UI/`.
  - 영향받는 include를 고치고 allowlist 경로를 갱신한다. 이동으로 드러나는 L1→L2 위반(센서가 `Scenario/` 헤더를 include)은 allowlist에 남겨 ARCH-06에서 해소한다.
- **완료:** `Core/`에는 센서 공용 서비스만 남는다. 빌드·전체 `MA0T10` 결과가 기준선과 같다(같은 테스트 이름이 같은 결과). `.uasset` 변경 0. allowlist 총 개수는 기준선 이하.
- **결과:** `git mv`로 13개 파일을 이동했다. 이동 파일 중 4개와 기타 57개 파일(Tests/·Editor 포함)의 include 경로 84줄만 수정했으며 클래스·함수·UPROPERTY·로직은 불변이다. rename 감지율은 98~100%다. 기존 위반 16쌍 해소 + 이동으로 드러난 L1→L2 9쌍 보존으로 allowlist는 **38→31**이다. 9쌍은 ARCH-06 대상이며 gate의 신규·stale은 0이다.
- **검증:** UE 5.3 Editor Development 빌드, DTCore guard, layer gate, V2 `-RequireAssets`, WBP 7개 메모리 컴파일 통과. ARCH-01과 동일한 D3D12·커밋본 맵 fixture·opt-in 설정에서 전체 `MA0T10`은 **실행 Success 209 / 실패 0 / skip 20 / NotRun 0**이며, **229개 테스트 이름별 결과 차이 0**이다. 보호 파일 298개 hash 불변, 개인 SaveGame 9개 복원, `.uasset` 변경 0을 확인했다.
- **증거·제한:** `Saved/Reports/ArchitectureSkeleton/A02/summary.json`, `test-comparison.md`/`.json`, `final-source-change-proof.json`, `layer-gate.json`, `dtcore-guard.json`, 빌드·WBP·보호 hash 로그. 첫 빌드는 이전 동일 폴더를 가정한 include로 실패했고 명시적 경로로만 수정해 재빌드 통과했다(실패 로그 보존). ARCH-01의 opt-in skip 20개와 RiderLink stderr 제한은 그대로이며 통과로 확대하지 않는다. Computer Use·자산 저장·DTCore 변경은 수행하지 않았다.

### ARCH-03 — 공통 계약 `Contracts/` 도입 (P0)

**진행률: 100%** · PL-02 1단계를 포함한다. 2026-10-11 구현·UHT 계약 비교·기준선 회귀 완료.

- **작업:**
  1. `UI/VirtualSensorControlTypes.h`에서 Runtime 사용이 확인된 `EVirtualSensorTargetKind`, `EVirtualSensorExportKind`, `FVirtualSensorCaptureSelection`, `FVirtualSensorEditableState`, `FVirtualSensorExportResult`를 `Sensor/VirtualSensorEditableTypes.h`(L1)로 옮긴다. UI 전용 PanelPlacement·GizmoMode·CoordinateSpace·PlacementInputMode·SettingHelpDescriptor, UI/Editor의 MapApplySnapshot과 export delegate는 남긴다. 기존 헤더는 새 헤더를 include하는 호환 shim으로 유지한다.
  2. Runtime 파일의 include를 새 헤더로 바꾼다.
  3. 별도 커밋으로 `Contracts/VirtualSensorDefaults.h`에 흩어진 Topic 기본값·SaveGame 슬롯 이름·`Saved/` 하위 폴더 이름 리터럴을 **같은 값**의 상수로 모은다. 값 변경·설정 객체 도입은 하지 않는다.
- **결정 이유:** 편집 상태가 기존 L1 enum 5종에 의존하므로 편집 타입도 L1에 둔다. 해당 enum은 이동하지 않는다. L0는 L1 타입에 의존하지 않는 계약·리터럴만 소유하며 ARCH-06의 중립 context는 계속 `Contracts/` 대상이다.
- **완료:** L1/L2의 `UI/VirtualSensorControlTypes.h` 직접 include 0(Coordinator→Monitor는 ARCH-04 대상). enum 값·순서·UMETA·USTRUCT 필드·UPROPERTY·기본값 동일, SaveGame v6·Workspace v1 roundtrip·레이아웃/북마크 파일 시험과 WBP 메모리 컴파일 통과.
- **결과:** enum 2·struct 3개 선언을 그대로 이동하고 소비 파일 9개의 include를 바꿨다. allowlist **31→22**, 신규·stale 0이다. `VirtualSensorDefaults.h`의 **22개 리터럴(Topic 3·슬롯 8·폴더 11)**을 production 20개 파일에서 사용하며, 상수를 원래 리터럴로 펼치면 이전 코드와 정확히 같다. 기존 `MA0T10.SensorWorkspace.IsolationAndStorage`에 22개 값 대조와 Workspace v1 메모리 roundtrip을 보강했다. 테스트의 기존 리터럴은 유지했다.
- **검증:** UE 5.3 Editor Development·guard·layer gate·V2 `-RequireAssets`·WBP 7개 메모리 컴파일 통과. 이동/유지된 11개 타입의 원문 선언·기본값과 UHT `.generated.h`/`.gen.cpp` 계약 비교 통과. `ModuleRelativePath`·FID/행 번호·UHT 참조 hash 주석만 위치/생성 식별 정보로 분리했으며 원본도 보존했다. D3D12 전체 `MA0T10`은 **실행 Success 209 / 실패 0 / skip 20 / NotRun 0**, ARCH-01의 229개 테스트 이름별 결과 차이 0이다. `UiPreferencesSerialization`(v6), `IsolationAndStorage`(Workspace v1), `SensorLayout.Persistence`, `Placement.PoseLibraryPersistenceAndHistory` 모두 실행·통과했다.
- **증거·제한:** `Saved/Reports/ArchitectureSkeleton/A03/summary.json`, `test-comparison.md`, `reflection-comparison.json`, `DEFAULTS.md`, `defaults-replacements.json`, 빌드·WBP·보호 hash 로그. 개인 SaveGame 9개 복원, 보호 파일 hash 확인, 자산 저장 0. 최초 L0 의존성 중단은 승인된 L1 목적지로 해소했다. 기준선 opt-in skip 20개와 RiderLink stderr 제한은 유지하며 수용 통과로 확대하지 않는다. DTCore·사용자 맵·로컬 설정·비대상 Widget 변경과 Computer Use는 없다.

### ARCH-04 — Runtime→UI 직접 호출 역전 (P0)

**진행률: 100%** · PL-01/PL-02 일부. 2026-10-12 구현·집중/RHI 회귀·기준선 비교 완료.

- **작업:**
  1. `AVirtualSensorCoordinator`의 Monitor 직접 호출(보기 전환·센서 bind·OwningPlayer 조회)을 Runtime 쪽 interface 또는 delegate로 바꾼다. Monitor가 이를 구현/구독한다.
  2. Blueprint 호환을 위해 `BindMonitorWidget(UVirtualSensorMonitorPanelWidget*)`의 선언·시그니처는 유지한다. 본문은 UI 레이어 번역 단위(예: `UI/VirtualSensorCoordinatorMonitorBinding.cpp`)에 둬서 `Sensor/`가 UI 헤더를 include하지 않게 한다.
  3. `ASlabActor`가 `ASlabSimulationUiHostActor`를 순회하는 부분을 Slab 쪽 등록/조회 API로 바꾸고 UI Host가 자신을 등록한다. 판정 결과는 같아야 한다.
  4. ARCH-01에서 발견한 `Player/DxPlayerControllerTest.cpp`, `Player/DxPlayerTest.cpp` → `UI/VirtualSensorTransformGizmoActor.h`(L2→L3) 2건도 이 항목에서 다룬다. 기존 업무 Player 코드는 최소 변경(interface/delegate 경유)만 하고, 해소가 동작 변경을 요구하면 근거를 보고하고 allowlist에 남긴다.
- **완료:** layer gate의 L1→L3·L2→L3 위반 0. Monitor follow·보기 전환·point-cloud-only·Slab UI 관련 기존 자동화 통과. Blueprint 노드 시그니처 불변(WBP 메모리 컴파일).
- **결과:** L1 native UINTERFACE `IVirtualSensorMonitorView`와 `IVirtualSensorPlacementInputOwner`로 위임했다. `BindMonitorWidget`·Player의 기존 C++ getter/register/unregister는 UI 번역 단위로 이동했고 원문 본문은 동일하다. Caller의 interface 이름을 기존 이름으로 펼치면 Coordinator·Controller·Pawn의 순서/조건도 동일하다. `BoundMonitorWidget`의 concrete 타입·Transient UPROPERTY·GC 보유, Blueprint 핀·Category·파라미터는 그대로다. allowlist는 **22→18**, 신규·stale 0, L1→L3·L2→L3 0이다.
- **Slab 존재 판정:** BeginPlay 등록만으로는 이전 판정을 보존할 수 없어 PostLoad·PostActorCreated·PostDuplicate에서도 약한 존재 정보를 등록한다. 조회는 현재 Slab 참조와 UE 5.3 iterator의 활성 레벨/collection 조건을 사용한다. EndPlay/Destroyed는 활성 등록을 해제하되 아직 유효한 Actor의 약한 존재 정보는 invalidation까지 유지한다(기존 코드도 Widget 활성 상태가 아니라 Actor 존재를 검사했다). 강한 Actor/World/Widget 참조나 새 비동기 작업은 없다.
- **검증:** Editor Development·guard·layer gate·V2 `-RequireAssets`·WBP 7개 메모리 컴파일 통과. D3D12 전체는 **실행 Success 212 / 실패 0 / skip 20 / NotRun 0**(기존 209 + 신규 3)이며 기준선 229개 이름별 결과 차이 0이다. 새 `MA0T10.Architecture.MonitorRouting`, `SlabUiRegistry`, `PlacementInputView`가 호출 순서·소유자·Blueprint/GC 계약·기존 iterator oracle·등록/해제 경계를 확인한다. 보조 자동화 **21개 Success·skip 0**(집중 3, UI/입력 8, 배치 9, Slab portable 1; 전체 시험과 중복 포함)도 통과했다. `InputAcceptanceRhi`, `ManipulationRuntime`, `Placement.InputOwnershipRhi`, `SlabScenario.PortableMapAndLatePlayer`는 별도 opt-in 실행에서 실제로 수행했다. 데스크톱 직접 조작 검증은 아니다.
- **증거·제한:** `Saved/Reports/ArchitectureSkeleton/A04/summary.json`, `test-comparison.md`, `binding-compatibility.json`, `related-tests.json`, `supplemental-tests.json`, 빌드·WBP·보호 hash 로그. 초기 새 fixture의 추상 클래스/중복 World 초기화/미초기화 Actor 종료 시험 오류는 수정하고 실패 기록을 보존했다(기존 기대값 수정 없음). 보호 파일 298개 hash 불변·개인 SaveGame 9개 복원·자산 저장 0. 주 시험의 opt-in skip 20개는 그대로 구분하며, RiderLink stderr와 기준선에도 있는 D3D12 시작 ensure는 환경 제한으로 남긴다. DTCore·로컬 설정·운영맵·비대상 Widget·Computer Use·push/PR/merge 변경/실행은 없다.

### ARCH-05 — 송신 자체 검증의 TC 의존 분리 (P1)

**진행률: 100%** · 2026-10-12 구현·집중/전체 회귀·문서 완료, Claude 독립 검토 대기.

- **근거:** `Core/VirtualSensorHighThroughputTransportSubsystem`·`VirtualSensorPcdReceiveContract`, `Sensor/VirtualSensorExternalSourceHostActor`가 `WebSocket/TC`의 수신 진단 클래스를 include한다.
- **작업:** 순수 타입(`VirtualSensorStreamReceiverTypes.h`)은 `Contracts/`로 옮기고, Transport의 자체 수신 검증은 중립 interface/registry로 호출한다. TC 쪽(L2)이 구현을 등록한다. ExternalSourceHostActor는 센서 입력 hosting과 진단 receiver hosting을 구분해 진단 부분을 L2 adapter로 옮긴다. 구체 설계는 조사 후 보고하고 진행한다.
- **완료:** L1의 `WebSocket/` include 0. Raw receipt·자체 수신 검증·StreamReceiver 관련 자동화 결과 기준선과 동일. 진단 결과를 센서/Transport로 재주입하지 않는 기존 규칙 유지.
- **결과:** 중립 수신 타입은 L0 `Contracts/`로 원문 이동했다. 기존 ExternalSource Actor 클래스·UPROPERTY·BP API를 유지하고 진단 hosting 함수 본문만 L2 adapter 번역 단위로 이동했다. Transport는 module startup에서 등록한 stateless TC codec을 thread-safe shared snapshot으로 호출하며 기존 PCD 파서·결과 타입·worker 실행을 유지한다. 별도 구독 루프·연결·진단 재주입은 추가하지 않았다. allowlist **18→9**, 신규·stale 0, 남은 9쌍은 ARCH-06 대상이다.
- **검증:** Editor Development·V2 `-RequireAssets`·guard·layer gate·WBP 7개 메모리 컴파일 통과. 집중 **35 Success/skip 3**, D3D12 전체 **229 Success/skip 20/실패 0/NotRun 0**, `82dd293` 직전 기준선 249개 이름별 결과 차이 0. 기준선 실행 SHA `e55fd9a`와 `82dd293`의 Source diff 0을 확인했다. 이동 본문 hash 동일, 보호 파일 476개 불변·개인 SaveGame 9개 복원·자산 저장 0.
- **증거·제한:** `Saved/Reports/ArchitectureSkeleton/A05/summary.json`, `test-comparison.md`, `move-proof.json`, `RESULT.md`, `focused-resume/index.json`, `automation/index.json`, 빌드·WBP·guard·layer 로그. 중단된 전체 로그는 별도 보존하고 재실행했다. Broker/opt-in 20 skip과 RiderLink stderr 제한은 실제 통과로 세지 않는다. ARCH-06·Computer Use·DTCore 변경은 수행하지 않았다.

- **ARCH-05 보완:** `90a0758`에서 stream enum→diagnostic receive enum의 ordinal cast 2개를 명시적 switch로 교체했다. 수신/고성능 집중 6 Success/skip1, 최종 전체 D3D12·Editor/Game 빌드 통과. wire·enum 값 순서·DTCore 변경 없음. 증거는 `Saved/Reports/ArchitectureSkeleton/A06/arch05-focused/`, `RESULT.md`.

### ARCH-06 — Slab context 중립화 (P1)

**진행률: 100%** · 2026-10-12 구현·검증 완료, Claude 독립 검토 대기. PL-01의 중립 context 경계까지이며 플러그인 생성은 수행하지 않았다.

- **근거:** 센서 frame/request 타입이 `FVirtualSlabFrameContext`를 포함하고(13개 파일), Camera/LiDAR/ActorBase가 `VirtualSensorSlabContextSubsystem`을 직접 호출한다.
- **작업:**
  1. 먼저 **golden fixture**를 만든다: 같은 입력 snapshot에서 생성한 PCD 헤더·`# MA0T10_META`·호환 JSON·telemetry를 바이트로 저장하는 자동화 테스트. 변경 전 통과를 확인한다.
  2. `Contracts/`에 중립 acquisition context(scenario/run/material/frame 식별자와 elapsed 등 문자열·숫자 필드)와 provider interface를 둔다. 게임 스레드에서 snapshot을 만들고 worker는 immutable 값만 읽는다.
  3. `Scenario/`의 SlabContext가 provider를 구현·등록한다. 센서 쪽은 provider만 안다.
  4. 기존 `FVirtualSlabFrameContext`를 쓰는 Blueprint/공개 API가 있으면 adapter로 유지한다.
- **완료:** L1의 Slab/Scenario 참조 0, golden fixture 바이트 동일, Slab-PCD 연계·replay·per-sensor delivery 자동화 통과. 실제 Artemis 시험은 선택(수행 여부를 보고).

- **결과:** `Contracts/`의 중립 native context·게임 스레드 provider와 weak 월드 registry를 추가했다. SlabContext는 기존 capture/승인/잠금/파생 예약/결과 집계 경로를 adapter로 제공한다. native frame/request와 Camera 대기/Publisher 메시지는 값 snapshot을 사용하며 worker는 UObject/provider를 조회하지 않는다. 기존 `FVirtualSlabFrameContext`·출력 선택의 reflected 속성 12개를 L0 호환 헤더에 원문 분리하고 `/Script/ma0t10_dt`·기존 include 경로·공개 API를 유지했다. 확인한 Blueprint 함수 시그니처 100개는 불변이다. allowlist **9→0**, 신규·stale0.
- **검증:** 변경 전 production 코드의 golden `ba635b8`를 먼저 커밋했다. 고정 sensor/FrameId/UTC/pose와 context 포함·미포함에서 PCD 헤더·MA0T10_META·33-byte 본문, 호환 LiDAR JSON, telemetry, Camera/PCD STOMP 헤더 14개가 변경 후에도 byte 동일이다. 집중 **51 Success/skip9**, 최종 Runtime `90a0758`의 Editor·Game Development·표준 release 12단계·WBP7 통과. 전체 D3D12 **233 Success/skip20/실패·NotRun0**, 기존249개 이름별 상태 차이0 + 신규4개 통과. 성능(1920×1080,1+1,10+30초): callback FPS 47.06→47.42, engine frame p95 29.690→29.607ms, game thread 평균 6.276→6.013ms, 2%gate 통과.
- **증거·제한:** `Saved/Reports/ArchitectureSkeleton/A06/RESULT.md`, `summary.json`, `release-manifest.json`, `test-comparison.md`, `reflected-compat-proof.json`, `blueprint-signature-proof.json`, `performance-comparison.json`. 개인 SaveGame9개 복원·보호 경로 불변. 20개 opt-in skip, 실제 Artemis·Computer Use 미수행은 통과로 세지 않는다. 성능은 실제 D3D12의 offscreen fixture이며 직접 마우스 수용과 구분한다. 전체 개편 진행률은 Claude 검토 통과 전까지 기존84%를 유지한다.

### ARCH-07 — 표준 검증 진입점 (P1)

**진행률: 100%** · QA-01·Claude 제안 C-01/C-02/C-12를 포함한다. 2026-10-12 구현·검증 완료, Claude 독립 검토 대기.

- **작업:**
  1. `Scripts/verify.ps1 -Level quick|feature|release`: 새 검증 로직을 만들지 않고 기존 스크립트만 호출한다. quick=`git diff --check`·DTCore guard·layer gate·보호 경로 불변·계약/fixture 정적 검사, feature=quick+Editor 빌드+지정 테스트 그룹, release=feature+전체 D3D12 `MA0T10`+WBP 컴파일. 결과를 `Saved/Reports/verify/<UTC>/manifest.json`에 commit/submodule SHA·수행/skip/실패 이름과 함께 남긴다.
  2. opt-in 테스트의 `MA0T10_*` 환경변수 표(변수 → 켜지는 테스트 → 필요한 환경)를 README §11에 둔다. 비밀값은 이름만 적는다. skip 로그에 원인 변수를 일관된 접두사로 남긴다.
  3. `Scripts/README.md`에 모든 스크립트를 `현재 사용 / 이력 보존 / 폐기 후보`로 분류한다. 삭제는 하지 않는다.
- **완료:** `verify.ps1 -Level quick`이 1분 안팎으로 끝나고 manifest를 남긴다. 소스의 변수 집합과 문서 표가 일치한다.
- **결과:** 기존 guard/layer/payload 및 smoke runner를 조합하는 `verify.ps1`을 추가했다. 기존 smoke runner의 기본 NullRHI 사용은 유지하고 D3D12·보고서·커밋본 맵 경로를 opt-in 인자로 제공한다. manifest는 SHA·Source hash·단계/이름별 Success/Failed/Skip·사유 변수·환경·시간을 기록하며 환경변수 비밀값은 읽어 기록하지 않는다. README §11의 변수 **67개**를 소스와 자동 대조하고 Scripts **110파일**을 분류했다. 기존 조건/반환값을 유지한 채 24소스 파일의 opt-in skip 문구 31개에 원인 접두사를 추가했다.
- **검증:** quick **6.94초**, feature **4 Success/skip 1**(SensorReceiver+RawReceiptProbe), release 전체 D3D12 **229 Success/skip 20/실패 0/NotRun 0**와 WBP7 통과. ARCH-05 기준선 249개 이름별 결과 차이 0, allowlist **9→9**. 잘못된 BaseCommit 음성 시험은 exit 1·실패 manifest 생성으로 확인했다. 보호 파일 476개 불변·개인 SaveGame 9개 복원. skip 접두사를 제거하면 변경 전 소스와 동일하다.
- **증거·제한:** `Saved/Reports/ArchitectureSkeleton/A07/summary.json`, `RESULT.md`, `skip-only-proof.json`, `test-comparison.json`; 실제 manifest 경로는 summary에 있다. 초기 wrapper 함수명 충돌은 수정 후 재실행했고 실패 기록을 보존했다. opt-in 20개는 자동 활성화하지 않았으며 실제 수용 통과로 세지 않는다. Computer Use·DTCore·자산 저장은 없다.

### ARCH-08 — 정리 (P2)

**진행률: 100%** · 2026-10-12 구현·빌드·집중/전체 회귀·성능·문서 완료, Claude 독립 검토 대기.

- `ma0t10_dt.Build.cs`의 `PixelStreaming`, `PixelStreamingInput`, `PixelStreamingBlueprint` 의존 제거(C++ 사용 0건). `.uproject`의 플러그인 활성화와 `Samples/PixelStreaming`은 건드리지 않는다. Editor 빌드와 Game target(`ma0t10_dt Win64 Development`) 빌드로 확인한다.
- `ForTests` 계열 테스트 hook 멤버를 `WITH_DEV_AUTOMATION_TESTS`로 제한한다(Shipping 노출 제거).
- 초대형 파일(`UI/VirtualSensorMonitorPanelWidget.cpp`, `Sensor/VirtualLidarScanComponent.cpp`, `Core/VirtualSensorStreamPublisherComponent.cpp`)을 **같은 클래스의 여러 `.cpp`로 이동만** 해서 나눈다. 순수 인코딩 함수는 별도 파일로 옮긴다.
- 빈 `Source/m7at10_dt/` 폴더는 **2026-10-12 사용자 승인으로 삭제 완료**했다(Claude 수행, 파일 0·git 추적 0).
- **완료:** 빌드·전체 회귀 동일, 분할 diff가 이동만으로 구성됨을 리뷰에서 확인.
- **2026-10-12 사용자 결정: PixelStreaming은 필요한 기능이다.** 아래 결과의 PixelStreaming 모듈 의존 제거(`65dbcc4`)는 `7316787`로 되돌렸고 Editor·Game Development 빌드를 확인했다. PixelStreaming 모듈 의존·`.uproject` 플러그인·`Samples/PixelStreaming`은 제거 대상이 아니다.
- **결과:** C++ 사용·Blueprint 자산 내 PixelStreaming 문자열 0건을 재확인하고 Public 의존 3개만 제거했다. `.uproject` 활성화·Samples는 불변이며 프로젝트 Blueprint17개와 WBP7개 메모리 컴파일 통과. Runtime hook18선언/참조 줄의 guard를 확인했다. 이미 제한된 hook은 유지하고 유일한 비제한 직렬화 함수의 production 진입점을 `SerializePointCloudFrame`으로 분리해 테스트 이름을 guard했다. UPROPERTY/UFUNCTION hook은 없다. Shipping object에는 테스트 직렬화 이름이 없고 production 이름은 남는다.
- **분할:** Monitor5·Scan4·Publisher3 `.cpp`와 순수 LiDAR JSON helper `.inl` 1개로 나눴다. `.cpp`당 **249~887줄**, 함수·helper **331개**의 원문 hash/문자열 대조 통과. `.inl`은 acquisition 단위 내부의 동일 anonymous namespace에서 한 번만 include해 private 타입과 linkage를 유지하고 mutable helper를 복제하지 않는다. 파일 이동에 따른 기존 Scenario include 3쌍의 allowlist 경로만 해당 이동 커밋에서 바꿨다. 함수·wire bytes·BP·저장 계약 변경은 없다. 빈 `Source/m7at10_dt/`는 2026-10-12 사용자 승인으로 삭제 완료했다.
- **검증:** Runtime `a36179c`, DTCore `b2504d1`. Editor·Game Win64 Development·Game Shipping 빌드, V2·guard·layer gate·WBP7·verify quick 통과. hook 집중 **34 Success/skip5**, 분할 집중 **41 Success/skip3**, D3D12 전체 **229 Success/skip20/실패0/NotRun0**, 기준선249개 이름별 결과 차이0. allowlist **9→9**, 신규·stale0. 보호476파일 불변·개인 SaveGame9개 복원·자산 저장0.
- **성능·제한:** 실제 D3D12 RenderOffscreen·client1920×1080·1 Camera+1 GPU LiDAR·10초 준비+30초·preview·폰트1·송신/로컬 프레임 저장 없음에서 callback FPS **47.20→47.63**, engine frame p95 **29.686→29.612ms**, game thread 평균 **6.250→6.121ms**로2%gate 통과. callback frame p95는33.635→33.298ms이며 engine 시간과 별도다. CLI 요청에 따라 Computer Use는 미수행이고 offscreen 성능을 직접 마우스 수용으로 확대하지 않는다. 기존 opt-in20개는 skip으로 유지한다.
- **증거:** `Saved/Reports/ArchitectureSkeleton/A08/RESULT.md`, `summary.json`, `split-function-proof.json`, `move-proof-final.json`, `hook-guard-proof.json`, `test-comparison.md`, `performance-comparison.json`, target별 빌드·WBP·보호 hash 로그. 최초 상대 보고서 경로 문제는 ARCH-07 runner 수정 후 재실행했다. allowlist partial-index patch의 CRLF 실패는 본인이 만든 두 로컬 커밋만 재구성해 수정했으며 최종 tree/index/worktree 불변 증거를 `commit-scope-repair.json`에 남겼다. ARCH-06·DTCore·보호 파일·push/PR/merge·worktree·submodule update는 없다.

## 0-U. UXR — 사용자 흐름 중심 UI 재정비 (ARCH-04 직후)

### 왜 필요한가 (2026-10-11 코드 조사)

초기에는 **모니터 + JSON Payload + 파일 내보내기** 중심이었고, 이후 **장비 배치 → Slab 시나리오 → Binary PCD 전송 → 실행 결과**로 방향이 바뀌었다. 새 기능을 기존 패널에 덧붙이면서 사용자 입장에서 다음 문제가 생겼다(소유 패널 7개, 화면 문구 약 900개).

| 문제 | 근거 |
|---|---|
| 진입점이 작업과 맞지 않음 | 상단 `PCD 전송`과 `결과`가 같은 데이터 패널의 다른 탭을 연다. 센서 설정 패널은 상단에 없고 배치 창 `측정 설정`으로만 열린다. 센서 선택 콤보가 상단(ID만)과 배치 창(종류·출처 포함)에 따로 있다. 소유/레거시 툴바 구현이 둘이다(`SensorToolToolbarWidget.cpp`). |
| 같은 설정이 여러 곳 | `데이터 필수 실행`, `추가 송신 옵션`, PCD 송신 선택이 데이터 패널(신규 시나리오)과 재생 패널(재생)에 각각 있다. 배치 창 본문 일부는 Settings 패널이 만든다(`BuildWorkspacePlacementControls`). |
| 이전 방향의 기능이 주 화면에 남음 | 기본 설정에 서버 Payload 간격·최대 점·miss 포함, MultiHit, 스캔 시 자동 내보내기, 외부 Source, SensorTestMap 저장 예약이 있다. 데이터 패널에 `현재 JSON Payload 수동 전송`, `Server Payload JSON 저장`이 있다. |
| 개발자 용어 노출 | receipt, coherent, revision, generation, FIFO, Raw TCP, e2e p95, Payload, Topic, wildcard가 기본 화면 문구에 있다. 설정 문구에 옛 이름 `Capture/Export 패널`이 남아 있다. 번호 잔재 `2. 위치·회전 조작`, `3. 기본 센서 설정`. |
| 같은 말이 다른 뜻 | `미리보기 시작/중지` 버튼은 실제로 센서 측정(Start/StopSensor)을 켜고 끈다. 송신·전송·출력·내보내기·캡처·파일 저장·Topic 발행이 섞여 쓰인다. |
| 전역 상태가 없음 | 측정 중 장비 수, PCD 전송 여부, 시나리오 진행, 서버 연결 상태가 각 패널 본문에 흩어져 있다. |

### 사용자 결정 (2026-10-11)

1. **주 사용자는 현장·업무 담당자다.** 기본 화면은 `배치 → 미리보기 → 시나리오 → PCD 전송 → 결과`만 단순하게 보여 준다. receipt·p95·revision 같은 진단 수치는 `도구 → 진단`으로 보낸다.
2. **레거시 기능은 `고급·호환` 메뉴로 옮긴다.** 기능과 C++/Blueprint API는 유지한다.
3. **PCD 출력 설정은 한 화면(`PCD 전송`)에서 신규 시나리오와 재생을 나란히 보여 준다.** 기본값은 지금대로 유지한다(신규=PCD, 재생=관찰 전용).
4. **ARCH-04 직후 시작한다.** ARCH-05~08은 UXR 뒤로 미룬다.

### 목표 정보 구조

```text
상단 작업 흐름 바
[장비 ▾ LiDAR · Lidar_01 ● 측정 중] | 배치 | 미리보기 | 시나리오 | PCD 전송 | 결과 | 도구 ▾
전역 상태 줄 (각 항목 클릭 시 해당 화면)
장비 3대(측정 2) · PCD 전송 중지 · 시나리오 대기 · 서버 연결 안 됨 ⚠ 연결 설정 필요

도구 ▾ : 센서 상세 설정 · 파일 저장 · 서버 연결 설정 · 진단 · 고급·호환 · UI 표시(글자 크기·창 배치 초기화)
```

| 화면 (기존 클래스 유지) | 보여 주는 것 | 옮기거나 숨기는 것 |
|---|---|---|
| **배치** `UVirtualSensorPlacementPanelWidget` | 장비 목록(종류·ID·레벨/추가·상태 점), LiDAR/Camera 추가·복제·삭제, 선택 장비의 측정 시작/중지, 이동·회전(Esc 종료), 저장 위치 목록, 전체 배치 저장/불러오기 | 숫자 입력은 접힘. 측정 규격은 `도구 → 센서 상세 설정` 링크 |
| **미리보기** `UVirtualSensorMonitorPanelWidget` | 영상/점군, 한 줄 상태(장비·상태·Hz·경고) | 투영·색상·범례는 `보기 설정` 접힘, 상세 수치는 `진단` |
| **시나리오** Progress·Replay·Charts | 현재 실행 상태·진행률·일시정지/중단/초기 위치 복귀, 보관 목록과 재생, 차트 열기 | 출력 옵션은 한 줄 요약 + `PCD 전송에서 변경` 버튼 |
| **PCD 전송** `UVirtualSensorCaptureExportPanelWidget` 탭 `전송` | 신규 시나리오 / 보관 목록 재생 출력 2열(PCD, 추가 출력 Camera 이미지·LiDAR 정보, 연결 확인 후 시작), 시나리오 없이 직접 전송, 사전 점검 한 줄(준비됨/이유)+상세 | 필터·간격·receipt 표본은 `고급` 접힘 |
| **결과** 같은 패널 탭 `결과` | 실행 목록·상세·A/B 비교·UUID 복사·폴더 열기 | 단계별 집계·snapshot 상세는 `진단` |
| **도구 → 파일 저장 / 서버 연결 / 진단** 같은 패널 탭 | 파일 저장, 연결 설정·시험, 수신 검증·전송 로그·보고서 | — |
| **도구 → 센서 상세 설정** `UVirtualSensorSettingsPanelWidget` | `기본`(장비 프로필·품질·주기·최대 거리), `고급`(해상도·FOV·채널·미리보기 점 수) | `고급·호환`: 서버 Payload 간격/최대 점/miss, LiDAR 호환 JSON, MultiHit, 자동 내보내기, 외부 입력 Source, SensorId 변경, PIE 시작값 복원, SensorTestMap 저장 예약, 화면 디버그 로그, JSON 수동 전송·저장 |

### 사용자 용어 (화면 문구 기준)

| 화면에 쓰는 말 | 뜻 / 내부 용어 | 기본 화면에서 쓰지 않는 말 |
|---|---|---|
| 장비 | 가상 Camera/LiDAR Actor | 센서 Actor, V2 |
| 측정 시작 / 측정 중지 | `StartSensor`/`StopSensor` | 미리보기 시작/중지 |
| 미리보기 | 모니터 화면 표시(측정에 영향 없음) | — |
| PCD 전송 | Binary PCD를 서버로 보냄 | Topic 발행, 송신 세션, Payload |
| 추가 출력: Camera 이미지 · LiDAR 정보 | JPEG / telemetry Topic | Topic 이름 |
| 파일 저장 | 로컬 디스크 기록 | 내보내기, 캡처(혼용 금지) |
| 서버 / 서버 수락 | Broker / receipt (결과·진단에서만) | receipt, Broker 수락 |
| 시나리오 · 실행 · 보관 목록 | 시나리오 UUID · RunUUID · 메모리 보관 | — |
| 관찰 전용 · 연결 확인 후 시작 | ObservationAllowed · RequireData | 데이터 필수 실행(설명문에서만) |

- 메시지는 **결과 + 다음 행동** 형식이다(예: `PCD 전송을 시작할 수 없습니다 · 서버 연결 설정이 필요합니다 [연결 설정 열기]`).
- coherent·revision·generation·FIFO·Raw TCP·e2e·p95·deadline·wildcard·backend는 `진단`과 `고급·호환`에서만 쓴다.
- 화면 문구는 한 곳(예: `UI/SensorToolUiText.h`)의 상수/LOCTEXT로 모아 같은 개념에 같은 말을 쓴다.

### 공통 규칙 (모든 UXR 항목)

- **기능·API 불변.** Blueprint 노출 함수·핀, `ESensorToolPanelRole` 값 순서(새 값은 끝에만), SaveGame 슬롯(Workspace v1/SensorUI v6/Appearance v1), 기본 정책값(신규=PCD, 재생=관찰 전용), wire 계약을 바꾸지 않는다. 화면 위치·문구·노출 수준만 바꾼다.
- 사용자 Designer WBP(Root가 있는 WBP)와 미등록·동료 Widget은 자동 변경하지 않는다. 레거시 툴바·WBP 경로는 동작을 유지한다(문구 정리만 허용).
- 문구 변경으로 깨지는 자동화는 의도한 변경만 기대값을 고친다. 고친 테스트 목록을 보고한다.
- 각 항목 완료 = 빌드 + layer gate + 관련 UI 자동화 + WBP 메모리 컴파일 + **렌더 캡처**(1280×720·1920×1080 × 글자 100%·150%, `Saved/Reports/UXR/<ID>/`) + 보호 경로 불변 + **Codex 마우스 기능 점검**(2026-10-12부터: Computer Use로 PIE에서 해당 화면의 주요 동작을 직접 눌러 확인하고 단계별 스크린샷과 체크리스트를 `Saved/Reports/UXR/<ID>/manual/`에 남김) + **성능 전후 비교**(아래). Claude는 결과물과 스크린샷을 독립 검토하고 필요하면 Codex 작업이 끝난 뒤 직접 다시 확인한다.
- **성능 전후 비교:** UI 변경 항목은 같은 맵·같은 장비 구성(최소 1 Camera + 1 LiDAR 측정 중)·1920×1080에서 10초 준비 + 30초 측정으로 평균 FPS·frame p95·game thread 시간을 변경 전/후 비교해 보고한다. 평균 FPS·p95가 2%를 넘게 나빠지면 원인을 찾아 고친 뒤 완료한다. UI 갱신은 5Hz 이하, 숨긴 패널은 문자열 조립·갱신을 멈춘다.
- 측정 편차가 큰 경우 같은 조건의 **ABABAB 교차 각 3회 중앙값**으로 FPS·frame p95·game thread의 상대 2% gate를 판정하고 개별값·범위·분산을 함께 남긴다. 측정 중 다른 Editor·빌드·Computer Use·캡처를 겹치지 않는다.

### UXR-01 — 작업 흐름 바·전역 상태 줄·도구 메뉴 (P0)

**진행률: 100%** · 구현·요청된 자동 검증/렌더 완료(2026-10-12). 직접 마우스 검토는 Claude가 별도로 수행한다.

- 소유 툴바를 `[장비 ▾] 배치 · 미리보기 · 시나리오 · PCD 전송 · 결과 · 도구 ▾`로 재구성한다. 장비 콤보는 종류·ID·상태 점을 보여 주고 배치 창 목록과 같은 선택을 공유한다.
- 전역 상태 줄: 장비 수(측정 중 수), PCD 전송 상태, 시나리오 상태/진행률, 서버 연결 상태, 첫 번째 차단 사유. 5Hz 이하로 갱신하고 각 항목 클릭 시 해당 화면을 연다.
- `도구 ▾`: 센서 상세 설정, 파일 저장, 서버 연결 설정, 진단, 고급·호환, UI 표시(글자 크기·창 배치 초기화).
- 용어 상수 파일을 추가하고 툴바·상태 줄부터 적용한다.
- **완료:** 7개 화면 모두 바 또는 도구 메뉴에서 1~2번 클릭으로 열림, 같은 패널을 여는 상단 버튼 중복 0, 레거시 툴바 동작 불변.

**현재 결과:** 소유 바의 장비 선택은 종류·ID·기존 Scheduler 상태를 표시하고 Coordinator/Workspace 선택을 공유한다. 시나리오는 진행+보관 목록을 함께 연다. UXR-03에서 PCD 전송/결과/파일 저장/서버 연결/진단을 독립 소유 탭으로 교체했고 기존 탭 enum·저장값 매핑은 유지했다. 고급·호환은 기존 Settings 고급 영역을 연다. 차트 접근 손실을 막기 위해 UXR-04 전까지 `도구 → 시나리오 차트`를 유지한다. 상태 줄은 읽기 전용·최대 5Hz이며 과거 누적 실패 수를 현재 차단으로 오인하지 않는다.

**검증:** UE 5.3 Editor Development, layer gate **18/18(신규 0·stale 0)**, DTCore guard, V2 `-RequireAssets`, WBP 7개 메모리 컴파일 통과. D3D12 전체 MA0T10은 **실행 215 Success / 조건부 skip 20 / 실패 0**이며 ARCH-04의 기존 232개 이름별 상태와 일치하고 신규 3개(`UXR01.ToolbarRoutes`, `ReadOnlyStatus`, `LegacyToolbar`)가 통과했다. 기존 기대값 수정은 없다. 레거시 툴바 본문 불변과 보호 파일 298개 hash·개인 SaveGame 9개 복원을 확인했다.

**렌더 증거:** `Saved/Reports/UXR/UXR-01/rendered-final-{1280x720,1920x1080}-{100,150}.png` 4개. 실제 client 크기는 RHI 로그로 확인했고 window DPI는 1.0이다. 기본 장비 ID/상태와 작업 버튼이 잘리지 않도록 폭을 보완했다. 긴 경고는 한 줄 말줄임+툴팁으로 유지한다. 자동 캡처는 직접 마우스 검증이 아니다. 상세 매핑/조회 API·skip 이름·기준선 비교는 같은 폴더의 `ROUTES_AND_QUERIES.md`, `skipped-tests.json`, `test-comparison.md`에 있다. DTCore·자산·기본 정책·Blueprint API·저장 계약 변경은 없다.

### UXR-02 — 배치 화면 재구성 (P0)

**진행률: 100% — 완료(마우스 최종 삭제 1건 보류)** · CLI 계속 진행 승인(2026-10-12). UX-01·UI-03 일부. 보류(환경: 모니터 없음) — 확인/취소 직접 검수 + 실제 삭제 자동화 검증 완료, 사용자 복귀 후 재검수.

- 배치 창이 장비 목록·선택 장비 조작·저장 위치·전체 배치 UI를 **직접 만든다.** Settings 패널의 `BuildWorkspacePlacementControls` 의존을 제거하고, Settings의 기존 Blueprint API는 같은 서비스로 위임해 유지한다.
- 장비 목록은 콤보 대신 목록 행(종류·ID·레벨/추가·상태 점, 선택 강조)으로 바꾼다. 장비가 없을 때 안내와 추가 버튼을 보여 준다.
- `미리보기 시작/중지` → `측정 시작/중지`. 수집 잠금·삭제 불가 사유는 해당 버튼 옆 한 줄로 표시한다.
- **완료:** 추가→선택→이동→Esc→위치 저장→전체 배치 저장/불러오기가 배치 창 안에서 끝남, Settings를 열지 않아도 됨, 기존 배치 자동화 통과.

**현재 구현:** 배치 창이 장비 목록/선택 장비/저장 위치/전체 배치를 직접 만든다. Settings의 `BuildWorkspacePlacementControls` 호출은 제거했으며 기존 API·Designer는 유지한다. `SensorPlacementInteraction`은 소유 Workspace 안에서 공유하는 UI action adapter이며 기존 Actor/PlacementSubsystem API로 조작·북마크·Undo/Redo를 처리한다. Esc·선택 변경·숨김·종료는 같은 종료 경로를 사용한다. 목록과 상태는 변경 감지/최대 5Hz로 갱신하고 숨김·접힘 시 표시용 처리를 건너뛴다. 전체 본문 스크롤과 고정 footer로 150% 겹침을 방지했다.

**검수:** 1280×720에서 Camera/LiDAR 추가·선택·정밀 이동·첫 Esc의 PIE 유지·측정 중지·위치 저장·Undo/Redo·전체 배치 저장/미리보기/적용을 실제로 눌렀다. 1920×1080에서는 다음 PIE의 전체 배치/북마크 연결 복원, 150% 스크롤 접근·전체 ID/경고 툴팁·이름 변경·삭제 확인/취소를 확인했다. 최종 삭제 클릭은 보류(환경: 모니터 없음) — 확인/취소 직접 검수 + 실제 삭제 자동화 검증 완료, 사용자 복귀 후 재검수. 증거와 체크리스트는 `Saved/Reports/UXR/UXR-02/manual/`에 있다. 자동 렌더 4개 해상도/배율 조합과 장비 없음·조작 중 2개는 같은 보고서 폴더에 분리한다.

**환경 경계:** 기본 검증맵의 `Engine.PlayerController + DefaultPawn`에는 DTCore 클릭 경로가 없다. 기존 `BP_DxGameMode`/프로젝트 Controller·Pawn을 테스트 PIE에만 임시 적용해 SingleRelease 3D 클릭을 확인했다. 운영 GameMode·맵·설정·DTCore는 바꾸지 않았으며 기본 Pawn의 3D 선택 fallback을 새로 만들지 않았다. 성능은 동일 기본맵/1 Camera+1 LiDAR/1920×1080에서 10초+30초의 동일 계측기로 비교한다(콜백 wall pacing과 engine game-thread cycles를 구분).

**최종 자동 검증:** Editor Development·layer gate 18/18(신규/stale 0)·guard·V2 `-RequireAssets`·WBP 7개 메모리 컴파일 통과. 전체 MA0T10은 **218 실행 Success / 20 조건부 skip / 실패 0**이며 기존 235개 이름별 결과가 유지되고 UXR-02 테스트 3개가 추가됐다. 기존 기대값 수정은 없고 토글/선택기 검증을 기존 UXR-01 테스트에 추가했다. 별도 opt-in `ManipulationRuntime`과 `Placement.InputOwnershipRhi`도 실제 RHI로 통과했다. 성능은 **FPS 59.998→59.999, frame p95 21.065→21.257ms(+0.912%), game thread 평균 6.107→6.129ms(+0.367%)**로 2% 범위 안이다. 이는 기존 절대 p95 20ms 목표 달성을 뜻하지 않는다. 보고서는 `Saved/Reports/UXR/UXR-02/`이며 개인 SaveGame 9개를 복원했다.

### UXR-03 — PCD 전송·결과 화면 (P0)

**진행률: 90% — 구현·자동 검증 완료, 직접 수용 보류** · RT-08·DATA-01 UI 부분(기능·기본 정책 불변).

- 데이터 패널 탭을 `전송 / 결과 / 파일 저장 / 서버 연결 / 진단`으로 바꾸고, 상단 `PCD 전송`=전송 탭, `결과`=결과 탭, 나머지는 `도구`에서 연다.
- 전송 탭에 **신규 시나리오 / 보관 목록 재생** 출력 설정을 2열로 둔다(각 PCD, 추가 출력, 연결 확인 후 시작). 기본값은 바꾸지 않는다. 재생 패널의 같은 설정은 제거하고 요약으로 바꾼다(UXR-04).
- 사전 점검은 한 줄 결과(`준비됨` 또는 첫 차단 사유+해결 버튼)와 접힌 상세로 바꾼다.
- 결과 탭: 실행 목록·상세·A/B 비교. 단계별 집계·terminal snapshot은 진단 탭으로 옮긴다.
- **완료:** 같은 출력 설정이 두 화면에 없음, 기존 실행 결과/A/B/재시도 API·자동화 통과, 기본 정책값 회귀 테스트 통과.

**현재 구현:** 소유 데이터 화면은 전송/결과/파일 저장/서버 연결/진단 5탭이다. 새 native 표시 enum은 저장하지 않고 기존 4값 Blueprint 탭 enum과 슬롯을 유지한다. 신규/재생 출력을 전송 탭에서만 편집하고 기존 Slab Actor·재생 패널 옵션을 그대로 사용한다. 최초 진입은 기존 Workspace/Replay UI Host 경로로 재생 소유 패널을 숨김 상태로 준비한다. 재생 창은 실제 옵션 요약+전송 탭 재생 열 링크만 표시한다(UXR-04 출력 요약 부분 선행). 사전 점검은 대상별 한 줄/해결 링크+접힌 상세, 결과는 실행/선택/UUID/폴더/재시도/A/B, 단계별 집계·종료 snapshot·상세 A/B는 진단이다. 기본 전송/결과 문구는 용어 상수와 사용자 문구 adapter를 사용한다. Designer·기존 API/enum/저장 슬롯/출력 계약은 유지한다.

**검증 요약:** Editor Development, layer gate **18/18(신규·stale 0)**, DTCore guard, V2 `-RequireAssets`, WBP 7개 메모리 컴파일 통과. 집중 자동화 **16 Success/실패·skip 0**. D3D12 전체는 **221 실제 실행 Success / 20 조건부 skip / 실패 0**이며 UXR-02 기준선 238개 이름별 상태를 유지하고 마커 격리 1개와 UXR-03 2개가 추가됐다. 각 탭×1280/1920×글자 100/150% **20장 자동 렌더**를 생성했다. 후속 UXR-03 보완(`22d54b9`)에서 집중 13 Success/실패·skip 0, 전송 탭 4조합 렌더와 열린 전송 탭 전후 성능(2% 이내)을 검증했다. 보류는 직접 마우스 조작이며, WinDisc 1612×816 환경에서 자동 렌더를 직접 수용으로 세지 않는다. 상세 수치·수정 테스트·초기 실패/보완은 [검증 이력](VALIDATION_HISTORY.md)에, 원본 로그/이름별 비교/체크리스트는 `Saved/Reports/UXR/UXR-03/`에 있다.

**보완 결과:** 기본 점검 사유는 쉬운 문장+해결 버튼+전체 문장 툴팁이며 상세 원문은 접힘 상세에 유지한다. `점검 대상:` 세그먼트는 출력 편집과 구분해 하나만 강조한다. 각 열과 직접 전송은 검사 목적·대상이 달라 대상 선택을 유지했다. 직접 전송 헤더를 2열 위에 두어 1280×720의 100/150% 첫 화면에서 보이고, 접힌 동안 해당 문자열 조립은 멈춘다. 기본 정책·검사 결과·API·저장값은 불변이다. 원본 증거는 `Saved/Reports/UXR/UXR-03/supplement/`에 있다.

**표시 비용:** 갱신은 최대 5Hz, 숨긴 탭·접힌 진단 요약 문자열은 갱신하지 않는다. 파일 완료는 숨김 상태에서도 기존 서비스/Blueprint 결과 전달을 유지하지만 UI 문자열 조립과 polling 제한 초기화는 하지 않는다. 명시적 Blueprint 사전 점검/상세 A/B 조회는 기존 상세 정보를 제공한다. UI 때문에 추가 측정·readback·게임 스레드 직렬화를 넣지 않았다.

### UXR-04 — 시나리오 화면 (P1)

**진행률: 90% — 구현·CLI 검증 완료, 직접 수용·독립 검토 보류**

- `시나리오`는 진행(상태·진행률·일시정지/재개/중단/초기 위치 복귀)과 보관 목록(선택·재생·복사·삭제)을 한 흐름으로 보여 주고, 차트는 버튼으로 연다.
- 재생 패널의 출력 옵션을 `이번 재생 출력: 관찰 전용 [PCD 전송에서 변경]` 요약으로 바꾼다.
- 3D 표시·단위·외형 설정은 `표시 설정` 접힘으로 옮긴다.
- **완료:** 시나리오 화면에 출력 정책 편집 컨트롤 0, 재생·복귀·삭제 보호 자동화 통과.

**현재 결과:** 기존 진행·보관 목록을 한 진입/숨김 흐름으로 유지하고 새 기본 배치를 나란히 놓았다. 개인 저장 배치와 Designer·레거시는 유지한다. 진행 창에 기존 차트의 토글을 추가했고 3D 표시·단위·현재 외형은 하나의 `표시 설정` 접힘이다. 출력 편집은 0개이며 재생 창은 실제 출력 요약+전송 화면 이동만 제공한다. 버튼 상태·텍스트는 5Hz 캐시를 사용하고 클릭 시 기존 API가 다시 보호 조건을 검사한다. 숨김·접힘 상세/목록의 표시용 조립은 멈춘다. 새 위젯 관리자·측정·구독 경로는 없다.

**검증 요약:** 소스 `9477732`, Editor Development/layer 18→18(신규/stale 0)/guard/V2 `-RequireAssets`/WBP 7개 메모리 컴파일 통과. 집중 28 Success/실패·skip 0, 시나리오+표시 설정 8장 자동 렌더, 시나리오 진행 중 1 Camera+1 LiDAR의 1920×1080·10초+30초 전후 성능 2% gate 통과. 초기 game thread +2.09%는 매 프레임 상태/문구 조회를 5Hz 캐시로 바꾼 뒤 +0.74%로 보완했다. 직접 조작은 보류하며 상세·체크리스트는 `Saved/Reports/UXR/UXR-04/`에 있다. 최종 UXR-08 통합에서 전체 MA0T10 227 실제 Success/조건부 skip 20/실패 0, UXR-03 기준선 241개 이름별 상태를 유지했다.

### UXR-05 — 센서 상세 설정 (P1)

**진행률: 90%** · owned native 3단/도움말/문구 정리와 CLI gate 완료. 직접 마우스·실제 DPI·독립 검토 대기.

- `기본`(장비 프로필·시뮬레이션 품질·측정 주기·최대 거리), `고급`(해상도·FOV·채널/샘플·미리보기 점 수), `고급·호환`(위 표의 레거시 항목) 3단으로 재구성한다. 번호 잔재를 없앤다.
- 도움말은 한 줄 설명 + `ⓘ` 상세로 줄이고 성능 영향은 `낮음/보통/높음`으로 표시한다.
- **완료:** `기본`·`고급`에 레거시 항목 0, 기존 설정 적용·검증 API·자동화 통과, SensorUI v6 저장값 호환.

**현재 결과:** 기본/고급/고급·호환 3단, 기본에는 프로필·품질·주기·LiDAR 최대 거리, 고급에는 해상도/FOV/채널·샘플/미리보기 점 설정만 둔다. Camera JPEG 품질과 기존 레거시 출력·Source·식별·복원·맵 반영·로그는 호환으로. 새 Camera 거리 기능을 만들지 않았다. 배치 본문·번호 잔재 제거, 한 문장+ⓘ 상세/자동 스크롤·성능 영향3단. 기존 BP/Read·Validate·Apply/수집 잠금/조작 종료/저장 슬롯·값/정책/wire 불변. 동적 문구5Hz·숨김/접힘 정지, 닫힌 상세 조립 중단. 동일 UDxWidget/소유 Workspace를 사용하며 신규 DTCore 우회 후보 없음.

**검증 요약:** Editor Development/layer18→18 신규0/guard/V2 assets/WBP7 통과. 집중52 실제Success/실패0/별도 스트림skip2, 최종 문구·도움말 보완17 Success/실패·skip0. 실제 visible Slate의 기본/고급 레거시 controls0·금지어16개 검사, v6 저장 호환·수집/조작·Slate 입력 RHI 자동화 통과(마우스 검수와 구분). Camera/LiDAR×3화면×1280/1920×100/150% 렌더24장. Settings 기본+모니터 열림/1Camera+1GPU LiDAR/1920×1080/10초+30초 성능 FPS+0.002%, enginep95 0%, callbackp95−0.146%, game thread평균−0.317%로2%gate 통과. 보호475/개인SaveGame9 복원. `Saved/Reports/UXR/UXR-05/RESULT.md`와`manual/CHECKLIST.md` 참조. 개편 프로그램 검토 통과 가중치는 Claude 독립 검토 전에는 올리지 않는다.

### UXR-06 — 미리보기 단순화 (P1)

**진행률: 90%** · owned 미리보기/빈 상태/문구 비용 정리와 CLI gate 완료. 직접 마우스·실제 DPI·독립 검토 대기.

- 기본 화면은 영상/점군 + 한 줄 상태(장비·상태·측정 Hz·경고)다. 상세 진단 문자열(프레임·Payload·공정성·렌더러 등)은 `진단`으로 옮기거나 `개발자 정보` 접힘으로 숨긴다.
- `보기 설정`(투영·색상·범례·포인트 전용)은 접힌 영역으로 둔다. 듀얼 카메라 등 기존 기능은 유지한다.
- **완료:** 기본 화면의 수치 줄 3줄 이하, 기존 보기 모드·point-cloud-only·monitor follow 자동화 통과.

**현재 결과:** 영상/점군+장비·상태·측정Hz·실제 경고1줄, 정상/없음은 경고색0. 프레임·상세/렌더러는 보기 설정의 개발자 정보로. 중지 안내/측정 시작은 기존 Placement 잠금·조작 종료 경로, 장비 없음은 기존 배치 열기만 한다. 선택/삭제 뒤 weak 표시 대상 재확인. 보기·point-cloud-only·follow·듀얼/저장 값/BP API 유지. 기본·열린 메뉴5Hz, 숨김/접힘 갱신 정지, 닫힌 상세·범례 조립 중단. native 보기/빈 상태는 기존 프레임만 사용하며 숨은 동기scan을 부르지 않는다. 명시적인 호환 캡처/저장과 레거시/Designer의 기존 경로는 유지한다. 신규 DTCore 우회·관리자·capture/readback/게임스레드 직렬화 없음.

**검증 요약:** Development/layer18→18 신규0/guard/V2 assets/WBP7 통과. 집중 고유29 실제Success(Workspace/Input/Manipulation opt-in RHI 포함), 후속8+명시호환1 Success/실패·skip0. 기본 금지어16·빈 상태·NoSyncScan·명시 legacy capture·수명 검사, 실제1280/1920×100/150% 측정/중지/장비0 렌더20장. 모니터 열림/동일1Camera+1GPU LiDAR/1920×1080/10초+30초 FPS−0.045%, enginep95 0%, callbackp95−0.520%, game thread−1.373%로2%gate 통과. 마지막 D3D12 전체229 실제Success/조건부skip20/실패0, c428ab6 기준선247개 이름별 상태 유지+신규UXR05/06 2개 Success. 보호475/개인SaveGame9 복원·Editor 종료. 마우스 보류. `Saved/Reports/UXR/UXR-06/RESULT.md`와 이름별 비교/skip JSON 참조. 개편 가중치는 독립 검토 전에는 올리지 않는다.

### UXR-07 — 문구 전수 점검과 수용 (P0)

**진행률: 70% — 자동 회귀·주요 직접 점검 완료, 전체 수용 미완료** · UI-01·UI-03 수용 부분. 2026-10-13 선행 세션에는 Computer Use로 실제 PIE를 조작했다. 이번 보완에서 상대 성능과 7패널 Slate 드래그는 통과했지만, 자유 배치의 기존 직접 FAIL 재확인·남은 직접 조합·사용자 무안내 확인이 남아 100%가 아니다.

- 소유 패널 문구를 용어표 기준으로 전수 점검한다. 기본 화면에 금지 용어가 남지 않았는지 스크립트로 검사한다(진단·고급·호환 영역 allowlist).
- 이번 준비: 1280×720·1920×1080 × 글자 100/150% 자동 렌더. 사용자 복귀 후 Codex가 UXR-02~08의 보류 마우스 점검과 첫 사용자 시나리오(PIE 시작 → 장비 추가 → 배치·저장 → 시나리오 → PCD 전송 → 결과), 100/125/150% 접근을 한 검수 세션으로 수행한다. Claude 독립 검토와 사용자 확인으로 종료한다.
- **완료:** 금지 용어 검사 통과, 첫 사용자 시나리오가 안내 없이 끝남, 사용자 확인.
- UXR-01 검토 메모(이 단계에서 함께 처리): 상태 줄 칸이 비활성 입력창처럼 보여 클릭 가능함이 드러나지 않음, 150%에서 경고 문구 말줄임 → 전체 문장 툴팁.

**현재 결과:** `SensorToolUiText.h` 표시 상수 87개와 실제 native Slate 라벨·입력 힌트·툴팁을 기본 29영역/허용 6영역으로 검사한다. 용어 정책은 `Scripts/fixtures/sensor_ui_vocabulary.json`, 검사는 `Scripts/validate_sensor_ui_vocabulary.ps1` 및 `MA0T10.UXR07.OwnedScreenVocabulary`다. 공통 숨김 툴팁·결과 재시도·서버 연결 라벨의 18개 영역별 위반을 정리하고, 동적 메시지·파일 저장 설명·옛 화면 이름을 통일했다. 편집 값(장비 ID·주소·경로), Designer/레거시 화면 및 실행하지 않은 모든 오류 조합까지 전수 검증했다는 뜻은 아니다. API·기능·기본 정책·저장값·wire 계약은 유지한다.

**검증 요약:** UE 5.3 Editor Development, layer 0→0, DTCore guard, 전체 D3D12 **234 실제 Success/조건부 skip 20/실패 0**, WBP7 메모리 컴파일, 문구 검사 통과. 이전 release 기준선 253개 이름의 상태 변화/누락 0, 신규 위 문구 검사 1개 Success. UXR 집중 18 Success/실패·skip 0, 자동 렌더 94장과 동일 조건 전후 성능 2% gate 통과. 상세는 [검증 이력](VALIDATION_HISTORY.md)과 `Saved/Reports/UXR/UXR-07/RESULT.md`에 있다. 보호 대상 코드·설정·맵·자산은 유지하고 개인 SaveGame9개를 복원했다. 별도 수정된 `CLAUDE.md`는 건드리지 않고 해시 차이를 보고서에 남겼다.

**현재 직접 결과:** `Saved/Reports/UXR/UXR-07/manual/session-20261013T0106/CHECKLIST.md`·`manual-results.json`에 55행의 수행/부분 수행/미수행과 스크린샷을 분리했다. 장비 추가·정밀 이동/회전·첫 Esc·선택/숨김 종료·북마크/Undo/Redo·다음 PIE 전체 배치 복원·시나리오/3중 점진 차트·중단/복귀·A/B·새/현재/주기 파일 저장·상태 칩을 직접 확인했다. 실제 두 client 크기×글자100/125/150%는 주요 화면 접근 근거이며 모든 패널/조작의 6조합 전수는 아니다. Portable 두 맵도 직접 시작/종료·Rig/Slab 없음 안내와 주요 경로를 확인했으나 MAP-01 체크리스트 전체 완료는 아니다. `USER_TRIAL.md`의 무안내 수용은 사용자만 판정한다.

**이번 보완:** 미리보기 경고를 제목과 분리, 긴 제목/북마크 말줄임·전체 툴팁, 장비0 조작 영역 숨김, 미리보기 상태의 고정 높이, 추가 출력 경고의 사용자 용어, 관찰자 근처의 기즈모 크기를 보완했다. 10cm/5° 조작과 첫 Esc를 재검사했다. 공개 Blueprint·저장값·DTCore·측정/송신 규격은 유지한다. 숨긴 차트/진행을 열자마자 찍은 옛 상태 문구는 다음 5Hz 갱신에서 일치해 제품 결함으로 확정하지 않았다.

**남은 실패/보류:** 자유 배치 제목 드래그의 선행 직접 FAIL은 유지한다. 새 시험의 입력 상태 집합 수명 문제를 수정한 뒤에는 변경 없는 Runtime에서 실제 Slate hit-test→누름→소유 패널 capture→이동→release가 7개 패널×100/150%×1280×720·1920×1080 모두 통과했다. 임시 제목 전달 후보·진단 로그는 폐기했고 근거 없는 Runtime/DTCore 수정은 남기지 않았다. 직접 재검수는 최초 활성화와 재시도 2회 모두 실패했고 읽기 전용 fixture의 전경은 `TextInputHost.exe`였다. 화면 캡처만 가능했으며 클릭은 수행하지 않았다. 시험 항목 최종 삭제의 사전 승인은 확보됐지만 실제 삭제·듀얼/포인트 전용·표면 반사율·전체 조합/재호스팅은 환경 보류다. 서버 수신/원격 PixelStreaming은 서버 환경 보류, 무안내 수용은 사용자 확인 대기다. 자동 통과를 마우스 PASS로 옮기지 않는다.

**보완 자동/성능:** Runtime은 `9823f01`과 동일하고 `cda6b74`는 Editor 회귀 시험만 추가했다. Editor Development·release14단계·고유242 실제 Success/조건부 skip21/실패0·WBP7·금지어 통과. 기존262개 이름/상태 변화0, 새 `MA0T10.UXR07.FloatingTitleDragRhi`는 기본 전체 실행에서 명시 opt-in skip이며 두 해상도의 별도 실제 Slate 실행에서 통과했다. 같은 저장값을 매회 복원한 1920×1080 1+1/10+30초 **ABABAB 각3회 중앙값**은 FPS −0.192%, callback frame p95 +0.148%, game thread 평균 −0.900%, engine frame p95 +0.192%로 **상대2% gate 통과**다. A의 game thread 6.815~6.881ms와 B의 6.686~6.863ms가 겹쳐 기존 단일 기준선5.443ms 대비 +24%는 재현되지 않았다. 차이가 남지 않아 조건부 이분 탐색/Insights 및 추정 최적화는 하지 않았다. 과거 측정 차이의 외부 원인을 확정한 것은 아니며, 양쪽 약47FPS이므로 절대55FPS 목표 통과로 표현하지 않는다. 기존 실패 기록은 보존한다. 교차 표·분산·소스 hash·판정 근거는 `Saved/Reports/UXR/UXR-07/supplement-20261013T0310/performance/`, 최종 release는 `Saved/Reports/verify/20261012T183335001Z/manifest.json`이다. 직접 수용의 최신 집계/환경 사유는 `Saved/Reports/UXR/UXR-07/CURRENT_ACCEPTANCE.md`와 `current-acceptance-summary.json`을 따른다. 전체 개편 합계94%를 부분 수행만으로 올리지 않는다.

**보완 수용 집계:** 총55행 중 **25 PASS / 1 FAIL / 29 보류**(환경27·사용자 확인2)다. 출력 설정 확인(06)은 기존 직접 점검23·24·25의 증거로 재분류했고 실제 송수신은27에서 별도 보류한다. 이번 복원(54)은 새 백업 기준52/52 hash 일치·SaveGame9개/Editor 설정 복원·검수 프로세스 정상 종료로 통과했다. 별도 실제 입력 시험 `MA0T10.Placement.InputOwnershipRhi`, `MA0T10.SensorV2.UI.InputAcceptanceRhi`, `MA0T10.UXR07.FloatingTitleDragRhi` 3개는 Success다. 기존 직접 FAIL(51)은 자동 통과로 해결 처리하지 않았다. 화면 활성화 문제를 해결한 뒤 남은 마우스 행과 사용자 체험을 이어가야 한다. DTCore·Game.ini·운영맵·PixelStreaming·자산·CLAUDE 파일은 수정/stage하지 않았다.

### UXR-01 보완 — 토글 동작과 장비 선택기 (2026-10-12 사용자 요청, UXR-02와 함께)

**보완 결과:** 토글/다른 화면 전환·활성 강조, 종류별 선택 목록/색 상태 점/추가 진입, DTCore `Click()` 기반 센서 선택, hover 칩·전체 경고 툴팁 구현 및 주요 마우스 경로 통과. 장비 선택만으로 창/측정을 시작하지 않는다. 소유 패널은 전면 처리 순간에만 DTCore에 등록해 일반 닫기 소유권을 넘기지 않는다. 기존 Widget/SaveGame/기본 출력 정책은 유지한다.

- **상단 버튼은 토글이다.** 이미 열려 있으면 다시 눌러 숨기고, 닫혀 있으면 열어 앞으로 가져온다. `PCD 전송`/`결과`처럼 같은 패널의 다른 탭은 다른 탭이 보이는 중이면 탭만 바꾸고, 같은 탭이 보이는 중이면 숨긴다. 열린 화면의 버튼은 강조 표시한다. 숨김은 기존 규칙대로 측정·송신·저장·재생을 멈추지 않는다.
- **장비 선택기**: `장비 · Camera · VCAM-001 ○ 정지`의 중복 접두사를 없애고 `Camera VCAM-001 ● 정지`처럼 종류·ID·색 상태 점(측정 중/정지/정체/오류)만 둔다. 펼친 목록은 LiDAR/Camera로 묶고 각 행에 상태를 보이며, 맨 아래 `+ 장비 추가 (배치)`로 배치 화면을 연다. 선택만으로 패널을 열거나 측정 상태를 바꾸지 않는다.
- **DTCore 기반 선택**: 3D 화면에서 센서·Slab(`AInteractableActor` 계열)을 클릭하면 DTCore의 상호작용 경로(`DxWidgetSubsystem`/InteractableActor)와 같은 방식으로 선택·정보 화면을 연다. 프로젝트 Controller의 SingleRelease 클릭 정책과 기존 기즈모 입력 우선순위를 유지한다.

### UXR-08 — Slab 정보와 표면 표현 (P1, 2026-10-12 사용자 요청)

**진행률: 90% — 구현·CLI 검증 완료, 직접 수용·독립 검토 보류**

- **근거:** 시나리오 행에는 `MtlNo`·`Length/Width/Thickness`·`Weight`·`CenterX/CenterY`·`LeftAngle/RightAngle`·`ElapsedSec`·`bMeandering`(사행)이 있지만 화면은 진행 패널의 긴 문장과 차트에 흩어져 있고 중량·사행은 보이지 않는다. `ASlabActor`에는 `SurfaceMaterial`·`bHotAppearance`·`Oxidation`·`Roughness`·`EmissiveStrength`가 있으나 화면에는 `고온 외형` 토글 하나만 있다. LiDAR 반사율은 `UVirtualLidarSurfaceResponseComponent::Reflectivity940Nm`로 따로 설정되어 외형과 연결되지 않는다.
- **Slab 정보 카드** (시나리오 화면 상단 + 3D에서 Slab 클릭 시): 소재번호, 치수 L×W×T(단위 표시), 중량, 진행(행 n/N·경과/전체 시간·진행률), 현재 위치(Track 기준)·속도, 좌/우 기울기, 중심 이탈, 좌/우 여유, 사행 여부. 값이 없거나 계산 불가면 `—`. 정상/주의 판정 색은 **사용자가 임계값을 설정했을 때만** 표시한다(기본은 판정 없음, AI 판정처럼 보이지 않게).
- **표면 표현 프리셋** (Slab 표시 설정): `기본(현재 값)` · `고온 적열` · `산화 스케일` · `냉각 강판` · `분석용 무광` — 기존 `bHotAppearance/Oxidation/Roughness/EmissiveStrength`와 `SurfaceMaterial`을 조합한다. 새 머티리얼 자산이 필요하면 생성 스크립트로만 만들고 운영맵·기존 자산을 재저장하지 않는다.
- **센서 결과 연동은 명시 선택이다.** 표면 프리셋은 Camera 영상과(연동을 켜면) LiDAR intensity를 바꾼다. `LiDAR 반사율도 함께 적용` 옵션을 켰을 때만 프리셋별 `Reflectivity940Nm` 등 SurfaceResponse 값을 적용하고, 실행 결과 JSON에 사용한 프리셋을 **선택적(optional) 필드**로 기록한다(기존 `ma0t10.slab-run.v1` 필드·의미 불변, 새 필드는 추가만). 기본값은 지금 외형·반사율 그대로다.
- **성능:** 프리셋 적용은 머티리얼 파라미터 변경 1회이며 매 프레임 갱신하지 않는다. 정보 카드는 5Hz 이하, 숨김 시 문자열 조립 중단.
- **완료:** 기본 상태에서 Camera 픽셀·LiDAR intensity·PCD bytes가 변경 전과 같음(회귀), 프리셋+연동 시 의도한 값 변화가 실행 결과에 기록됨, 정보 카드 값이 시나리오 원본과 일치, 렌더 캡처와 Codex 마우스 점검 통과.

**현재 결과:** 원본 행 카드와 현재 Track 위치/실제 속도를 구분하고 누락 Y/사행은`—`, 중량은원본단위로 표시한다. 기본 판정색/임계값은없다. DTCore`Click()`을 유지한 기존 약한 Host registry adapter로 기존 진행 인스턴스만 선택·전면 처리한다. MID는변경시1회, 광학연동은명시opt-in이며CPU캐시를1회무효화/GPU기존ID-depth스냅샷에중립광학값을함께복사한다. 추가capture/readback/동기scan/설정자동변경은없다. GPU버퍼미요청은거절한다. 2026-10-12 승인된 새 `M_SlabSurfaceOxideScale`로 산화 스케일을 지원한다. 나머지 프리셋/해제는 원래 SurfaceMaterial·MID와 외형 값으로 복원하며 기본값을 바꾸지 않는다. 기존 자산·맵 재저장 없음, 기존API/정책/저장값/PCD불변. 사용한경우에만결과JSON의optional surfaceAppearance를추가한다.

**현재 검증 요약:** 산화 보완은 Editor·Game Development, `verify.ps1 -Level quick`, layer0→0(신규/stale0), guard/V2 `-RequireAssets`/WBP7 메모리 컴파일 통과. 관련 D3D12는 **40 실제 Success/6 조건부 skip/실패0**. 산화 외형만 적용하면 CPU/GPU intensity·PCD가 유지되고, opt-in `0.12` 적용 및 기본 복원에서 결정적 BaseColor 픽셀·intensity·고정 envelope PCD bytes 회귀가 통과했다. 5종 동시 렌더·근접2장·contact sheet를 저장했다. shader pixel instructions253→379, vertex131/texture samples3/samplers3은 동일(외부 텍스처0). 동일 1920×1080·1Camera+1GPU LiDAR·10초+30초에서 기본 전후 및 최종 바이너리 기본↔산화의 FPS/frame p95/game thread 2% gate 통과. `DirectoriesToAlwaysCook`에 따른 최소 Windows cook에서 새 자산 포함 확인(전체 package는 미수행). 현재 근거는 `Saved/Reports/UXR/UXR-08/oxide/RESULT.md`. 이전 전체227 Success/skip20은 이전 검증 결과이며 이번 전체 회귀로 소급하지 않는다. production FinalColor/JPEG bitwise·실측 보정·직접 마우스는 미검증이다. 상세 날짜별 이력은 `VALIDATION_HISTORY.md`에 보관한다.

### UXR-09 — 시각 디자인 리프레시: A안 작업 사이드바 (P0, 2026-10-12 사용자 선택)

**진행률: 90%** · 구현·자동 검증·최종 상대 성능 gate 통과, **변경 화면 마우스 검수는 환경상 보류로 전체 완료 아님**(2026-10-12). 시안: https://claude.ai/artifact/SHhmPcxxrBuSCLkE4xoZ3s (Main.dc.html = A안)

- **배경:** UXR-01~08로 기능 위치·문구는 정리됐지만 사용자는 "아직 세련되지 않고 복잡하다"고 판단했다. 여러 떠 있는 창이 겹치고, 텍스트 버튼·테두리 상자가 많고, 위계가 약하다.
- **레이아웃(A안):** 왼쪽 고정 사이드바(약 372px) = 아이콘 레일(배치·미리보기·시나리오·PCD·결과, 아래 도구) + 선택한 작업 **하나만** 보이는 본문. 3D 화면은 넓게 둔다. 미리보기는 오른쪽 아래 작은 카드(×로 닫기, ⋯ 보기 설정), Slab 요약은 오른쪽 위 카드(진행률 막대 + 기울기·중심 이탈·여유 3줄), 전역 상태는 **하단 한 줄 상태바**(경고만 노란 칩). 기존 상단 흐름 바·상태 줄은 이 구조로 대체한다.
- **디자인 토큰:** 바탕 #14171B / 면 #1A1E23 / 강조 면 #1F3350 / 테두리 #262B32 / 본문 #E6E9ED / 보조 #8C96A1 / 강조 #3D7FE0(채움)·#8DBBFF(글자) / 정상 #3FB97F / 경고 #F2C26B on #3A2E17 / 위험 #E5534B. 모서리 8~12px, 간격 8px 단위, 제목 16 굵게 / 섹션 라벨 12 보조색 / 본문 13. 버튼 3단계: 주요(강조 채움)·보조(테두리)·위험. 패널 머리 버튼은 아이콘(–, ×, ⋯)+tooltip.
- **DTCore 기반:** `UDxWidget`/`UDxWidgetSubsystem`(열기·닫기·BringToFront·입력 차단)을 그대로 쓰고, DTCore `DxWidgetThemeData`가 쓸 수 있으면 토큰 연결을 검토한다(없거나 맞지 않으면 프로젝트 스타일 + "DTCore 변경 후보" 기록).
- **호환:** 기존 7개 패널 클래스·Blueprint API·`ESensorToolPanelRole` 순서·SaveGame(Workspace v1)을 유지한다. 사이드바 모드가 기본이며, 기존 플로팅 배치는 `도구 → UI 표시`의 "창 자유 배치(이전 방식)"로 남긴다(사용자 저장 배치 보존). 레거시 툴바·사용자 Designer WBP 불변.
- **글꼴:** 이번에는 기존 글꼴 유지(새 폰트 자산 없음). Noto Sans KR 적용은 별도 승인 시.
- **산화 스케일 외형 개선(UXR-08 후속):** 현재 `M_SlabSurfaceOxideScale`은 밝은 회색 바탕에 흰 윤곽 얼룩이라 물결/반사처럼 보인다(`Saved/Reports/UXR/UXR-08/oxide/`). 짙은 회흑~청흑색 바탕, 불규칙하게 벗겨진 밝은 강판 패치, 윤곽 하이라이트 없음, 거친 비금속으로 다듬는다. 기본·냉각 강판·분석용 무광도 서로 구분되게 조정한다. 기존 자산 수정은 이 머티리얼에 한해 승인된 범위다.
- **완료:** 1280×720·1920×1080 × 100/125/150% 렌더가 시안과 같은 구조, 한 번에 작업 패널 1개만 열림, 금지 용어 검사·전체 회귀·WBP·성능 2% gate 통과, Codex 마우스 점검.

**현재 구현/검증:** 기존 7개 패널 인스턴스를 재사용하는 고정 작업 영역·레일, 우하단 미리보기·우상단 Slab 요약·하단 상태바를 구현했다. 사용자 Designer/레거시 툴바·role 순서·Blueprint 시그니처·저장 schema는 유지한다. 자유 배치의 위치/크기/접힘/열림 상태는 dock 사각형으로 덮어쓰지 않으며, 표시 모드·사이드바 활성 작업은 현재 PIE의 임시 상태다. 새 PIE는 기존 저장 상태를 존중하는 사이드바로 시작한다. 레거시 native 글꼴의 즉시 적용과 명시 저장된 닫힘 우선순위 회귀도 수정했다. 픽셀 배율은 소유 UI에서만 5Hz로 계산하며 엔진 DPI/글꼴 자산은 바꾸지 않았다.

보완에서는 장비를 ID/종류·출처/상태 카드로 표시하고 복제·삭제를 더보기로 옮겼다. 이동·회전 주요 버튼, 한 줄 모드·좌표 세그먼트, 접힌 입력 방식·숫자 입력, 저장 위치 링크, 하단 고정 전체 배치 저장·불러오기를 적용했다. 상시 설명은 툴팁과 한 줄 상태로 정리했다. 미리보기는 도킹 시 상태·보기 설정·닫기 한 줄이며, 상태 칩은 왼쪽에 모으고 글자 배율은 오른쪽에 둔다. 전송의 추가 출력은 접고 기본 탭은 전송·결과와 현재 열린 도구 탭만 노출한다. 설정/API/정책 값은 바꾸지 않는다. 미리보기의 명시적 저장 닫힘은 보존하고, 기본 화면 렌더 fixture에서만 초기 배치를 명시적으로 복원했다.

Editor·Game Development, `verify.ps1 -Level release -BaseCommit 20848ea`, layer **0→0**, guard/V2 assets/WBP7 메모리 컴파일/금지 용어 검사 통과. 최종 전체 D3D12는 고유 이름 기준 **239 Success / 20 기존 조건부 skip / 실패·NotRun 0**이다. 이전 238 Success의 결과를 유지하며 카드 대비·기본 조작 노출 검사 1개를 추가했다. 1280×720·1920×1080 × 100/125/150% × 8종 렌더 48장을 재생성했다. 근거는 `Saved/Reports/UXR/UXR-09/supplement/`, release manifest는 `Saved/Reports/verify/20261012T142301373Z/manifest.json`이다. 기존 버전의 38개 마우스 기록은 이번 변경 화면의 검수로 소급하지 않는다.

**최종 성능·남은 제한:** `20848ea`와 최종 소스를 같은 폴더에서 각각 빌드하고 실제 1920×1080·1 Camera+1 Native LiDAR·정격 프로필·10초+30초로 3회씩 재측정했다. 추적을 끈 중앙값은 FPS **47.477→47.536(+0.125%)**, callback frame p95 **33.773→33.555ms(-0.646%)**, game-thread 평균 **6.606→6.394ms(-3.206%)**로 상대 2% gate 통과다. Insights 동일 30초 구간에서 배치 패널 paint **0.154→0.116ms/프레임**, Slate prepass **1.223→1.184ms/프레임**을 확인했다. 툴바 paint는 **0.047→0.071ms**로 작은 추가 비용이 남는다. 설명·컨트롤 간소화와 숨김·접힘을 유지하며 표시 문구는 5Hz 이하로 갱신한다. SInvalidationPanel 실험은 표시 값 정체가 재현되어 철회했고 해당 성능 결과는 폐기했다. 최종 코드에 그 캐시·갱신 adapter는 없다. 원시 trace·함수/위젯별 CSV·전후 표는 `performance/final-ui-costs.csv`, `final-performance-table.csv`, `FINAL.md`에 있다. 이번 기준선 자체가 기존 55FPS·20ms 절대 목표에 못 미치며, 과거 5.594→6.398ms 증가 전체를 한 함수나 외부 부하로 확정하지 않았다. 센서 규격·주기·출력·DTCore는 변경하지 않았다.

**남은 핵심 조건 — 직접 조작:** 1920×1080 테스트맵 PIE 창은 발견했으나 Computer Use 활성화가 재바인딩 포함 2회 모두 `failed to activate captured window`로 실패했다. 클릭·드래그·입력은 0건이며 변경된 더보기/세그먼트/저장 바/미리보기/상태바와 1280×720 직접 검수는 미수행이다. 요청한 재시도 상한에 따라 보류했고 PIE/Editor를 정상 종료·개인 SaveGame 9개를 복원했다. 자동 렌더를 마우스 통과로 대체하지 않는다. `supplement/manual/CHECKLIST.md` 참조. 이 항목은 전체 완료로 집계하지 않는다.

**산화 스케일 후속:** 승인된 `M_SlabSurfaceOxideScale` 한 자산과 재현 스크립트만 갱신했다. 청흑색 base, 밝은 벗겨진 강판 패치, 거친 저금속 표면으로 조정했으며 별도 윤곽/normal/WPO 효과는 없다. 냉각 강판·분석용 무광의 파라미터는 구분하되 기본과 기존 MID override 복원은 유지한다. 기본 Camera/intensity/PCD·명시 광학 연동 회귀와 5종 비교/근접/base-color 캡처 통과, shader 비용은 기존 산화 버전과 동일한 pixel 379 / vertex 131 / samples 3 / 외부 텍스처 0이다. 실제 맵의 노출/조명에 따른 최종 외형의 독립 시각 검토는 남으며 조명·전역 노출은 바꾸지 않았다.

### UXR-10 — 시나리오 크기 조절과 하위 화면 뒤로 가기 (P1, 2026-10-13 사용자 피드백)

- **출처:** 사용자 요청 “시나리오 위젯 크기 조절”, “왼쪽 사이드바 세부 위젯에서 이전 화면으로 돌아가기”. Claude의 UXR-10 CLI 요청으로 범위를 고정했으며 개편 프로그램 가중치(106)에 합산하지 않는 후속 항목이다.
- **구현:** 자유 배치 진행 창의 우하단 `↘` 드래그와 실제 geometry 좌표계·글자 배율 최소 크기를 사용한다. 기본 최소 360×280 logical px, 150% 최소폭 540, 최대는 설정 상한과 viewport/상단 작업 바의 가용 범위다. 크기는 기존 Workspace v1 `Layout.ExpandedSize`에 저장하며 슬롯·버전·UPROPERTY·Blueprint API를 바꾸지 않는다. 사이드바는 기존 공통 머리에서 기본 372 / 넓게 560 design px를 전환하고 화면 폭 48% 이내로 제한한다. 폭 선택은 세션 전용이다.
- **뒤로 경로:** 배치→장비 상세 설정→배치, 설정 고급/고급·호환→기본, 진행→보관 목록/차트→진행, 데이터 탭→직전 탭, 도구에서 연 화면→도구 메뉴. 등록된 기존 인스턴스를 재사용하며 화면 간 부모 경로 최대 8단·데이터 탭 경로 최대 5개만 보관한다. 레일 직접 진입은 하위 경로를 초기화하며 같은 레일 재클릭의 숨김 토글을 유지한다. 화면 복귀는 센서 재바인딩/설정 적용/측정·전송·파일 작업 시작·중지를 하지 않는다. 기존 숨김 계약으로 조작을 종료하지 않도록 화면 간 뒤로는 조작 중 비활성화하고, 설정 내부 탭 복귀는 조작·미적용 입력값을 유지한다. Backspace/마우스 뒤로 버튼은 새로 연결하지 않았다.
- **비용·기반:** 새 Tick·timer·invalidation root·센서 순회·캡처/readback 경로 없음. 크기/폭/경로 변경 이벤트에서만 배치를 갱신하며 기존 5Hz 갱신을 유지한다. 기존 `UDxWidget`/Workspace의 `UDxWidgetSubsystem` 입력 차단·전면 처리를 사용한다. Designer WBP와 미등록 Widget은 새 머리/resize 적용에서 제외한다. DTCore 소스/gitlink 불변이며 공통 후보는 아래 DTC-UXR10이다.
- **현재 상태: 70%.** Editor Development·집중 D3D12 **2 Success/실패·skip0**, 기존 UXR/UI 관련 회귀 **49 Success/3 조건부 skip/실패·NotRun0**, WBP7 메모리 컴파일·quick8·용어 검사·layer0 통과다. 1920×1080/window DPI1.0의 기준/변경 각24장(글자100/125/150%)과 변경 경로9장을 보관했다. 직접 마우스·통합 release·성능 gate는 요청에 따라 보류다. 자동 RHI 렌더/Slate 이벤트는 직접 마우스 수용을 대신하지 않는다. 최종 명령·성공/실패/skip 이름·전후 렌더·개인 SaveGame 복원 근거는 `Saved/Reports/UXR/UXR-10/RESULT.md`와 `manual/CHECKLIST.md`를 따른다. 이번 CLI 세션의 상세 기록은 [검증 이력](VALIDATION_HISTORY.md)에 보관한다.

### UXR-11 — 실제 마우스 드래그·resize 결함 (P0, 2026-10-14 검수 마우스 점검)

- **출처:** 검수 복사본(`ma0t10_dt_qa`, 221cd2d) 직접 마우스 점검에서 자유 배치 창 제목 드래그와 시나리오 자유 창 우하단 resize가 무반응(확정 FAIL 2). 기존 Slate 자동 테스트는 위젯에 이벤트를 직접 주입해 통과했다. 같은 점검에서 첫 Esc·숫자 입력·출력 왕복·포인트 전용·듀얼·삭제 취소는 PASS, 프로필 선택 시 값이 기본 규격으로 재적용되는 것은 의도된 계약이나 안내가 없어 '초기화'처럼 보이는 UX 지점(P2)으로 기록됐다.
- **원인·수정:** PIE 창 활성화/배경 클릭 뒤 game viewport(`SViewport`)가 이미 마우스 캡처를 쥐고 있어 패널 press가 viewport로 갔다. L3 Workspace에 입력 pre-processor를 등록해, press가 **열린 소유 패널의 실제 hit path**일 때만 viewport 캡처·lock을 해제하고 Slate가 원래 이벤트를 정상 라우팅하게 했다. viewport 캡처 정책·input mode·DTCore 입력 차단은 바꾸지 않는다. 패널은 명시적 user focus와 함께 캡처하고, delta를 local 좌표로 계산하며, 숨김·도킹·재바인딩·파괴 시 제스처를 끝내고 캡처를 해제한다. 프로필 선택은 기존 재적용 계약을 유지하고 "프로필 기본값 적용: 품질 …, 주기 … (이전 사용자 값 …)"을 다음 조작까지 표시한다(Tick·timer 없음).
- **검증(40aa640):** 신규 `MA0T10.UXR11.NativeCursorCaptureRhi`(viewport 캡처를 실제로 심은 뒤 native cursor down/move/up을 7개 패널·글자 100/150%로 수행, PIE 카메라·폰·선택 불변, 캡처 해제, 기존 v1 저장 복원)와 `ProfileReapplyNotice` 2/2, UXR10 2/2, `MA0T10.UXR`+`UI02`+`SensorMonitor`+`Workspace` **37 Success/실패0**(D3D12 표시 창), quick 8/8, layer 0. Codex CLI가 구현 도중 서버 오류로 중단되어 Claude가 미커밋 변경을 검토·완성·검증했다.
- **현재 상태: 80%.** 남은 것: 검수본(40aa640) 직접 마우스 재검수(앱 Codex 진행 중), 통합 release, `verify -Level perf`.

### UXR-12 — 시각 다듬기: 더 심플하고 세련되게 (P1, 2026-10-14 사용자 요청)

**진행률: 70%** · 표현 구현·집중 회귀 완료. 변경 화면 직접 마우스·통합 release·상대 성능은 별도 인계이며 전체 수용 완료로 표시하지 않는다. 개편 프로그램 가중치에는 합산하지 않는 후속 항목이다.

- **출처·문제:** 사용자 “전체적인 UI 디자인 지금 좋은데, 좀 더 심플하지만 세련되고 보기 좋게”. Claude의 UXR-10 렌더 검토에서 레일 라벨 잘림, 머리의 접기/닫기/폭/더보기와 별도 뒤로 줄, 중복 부제, 기본 Slate 콤보와 숫자 입력의 스타일/높이 차이, 반복 도움말, 비활성 Undo/Redo, 빽빽한 PCD 체크박스/점검 대상, 미리보기 경고 잘림과 작은 Slab 진행 막대를 확인했다.
- **토큰·머리:** 제목16 Bold / 본문13 / 보조11, Text·Muted 두 회색, 간격8·섹션16·패널 여백16·폼 컨트롤32(글자 배율 반영). 레일72와 `배치/보기/시나리오/전송/결과`, 선 아이콘, Accent15% 면·왼쪽3px 막대. 머리는 `[←] 제목 [더보기] [×]` 한 줄이다. 뒤로 부모는 툴팁으로 유지하고 접기·폭·초기화는 더보기로 이동했다. 기존 폰트의 `⋯` 누락 글리프는 직접 그린 세 점으로 처리한다. 제목 드래그 hit 범위와 UXR-10 복귀·UXR-11 입력 라우팅은 유지한다.
- **폼·행동·카드:** 센서 설정은 라벨 위/입력 아래, 소유 ComboBox/Input/SpinBox 스타일, 섹션 한 줄 도움말·작은 성능 배지/정보 버튼이다. 품질은 `전체 사양(고부하)`로 표시하고 레거시 문구/enum은 유지한다. 배치의 중복 `장비` 부제 제거·위치 저장 링크 왼쪽 정렬·기록 있는 Undo/Redo만 노출. 저장·불러오기와 상태를 저장 위치 바로 아래로 모아 큰 중간 빈칸을 없애고 같은 본문 스크롤로 접근한다. PCD 신규/재생은 켬/끔 pill 토글, `연결 확인 후 시작` 정책 이름 유지, 점검 대상 콜론 제거와 신규/재생/직접 한 줄 세그먼트다. 미리보기 경고는 한 줄 칩과 원문 툴팁, Slab 진행은 전체 폭4px·퍼센트와 오른쪽 값 정렬이다. 주요 행동만 Primary, 보조는 테두리, 도구/링크는 텍스트 버튼이며 결과의 사용 불가 행동은 숨긴다.
- **경계:** A안 구조·기능·DTCore·Designer WBP·미등록 Widget·Blueprint API·enum 순서·저장 schema·출력 기본값 불변. 새 자산/글꼴/Tick/timer/invalidation root/센서 캡처·readback·worker 경로 없음. 기존 숨김/접힘 갱신 중단과 5Hz 제한을 유지한다. DTCore 변경 후보를 새로 추가할 필요는 없으며 기존 UXR-09 adapter 범위다.
- **검증·완료 gate:** Editor Development, UXR/UI02/SensorMonitor/SensorWorkspace/SensorUi/SensorControl/Slab.UI 집중 회귀 **58 Success / skip3 / 실패·NotRun0**, WBP7 메모리 컴파일·quick8·layer/allowlist **0/0**·기본 화면 용어 검사(103 상수와 실제 Slate inventory) 통과. 1920×1080·1280×720 ×100/125/150% ×9화면 전후54장씩, 레일 할당 폭과 폼 높이32/40/48 검사 통과. 레일 문구 기대값2개와 세그먼트 수 검사1개만 의미를 보존해 갱신했다. 보호 파일484·개인 SaveGame9 불변이다.
- **근거·남은 조건:** Runtime `399ea46`, `Saved/Reports/UXR/UXR-12/RESULT.md`, `summary.json`, `before/`, `after-spacing/`, `comparison/`, `manual/CHECKLIST.md`. 실패했던 폼 높이 검사·이전 문구 기대값·줄바꿈 경고 기록은 보존한다. QA 마우스 검수와 충돌하는 `UXR11.NativeCursorCaptureRhi`는 미수행이며 다른 SHA의 결과로 대체하지 않는다. 직접 마우스는 앱 Codex, release와 `verify.ps1 -Level perf -BaseCommit fe20bcb`(상대2%)는 모든 화면 작업 종료 후 Claude가 수행한다. 이 CLI에서는 release·성능 측정을 하지 않았다.

### UXR-13 — 장비 이동 조작을 지금 보는 화면에서 (P0, 2026-10-14 사용자 직접 확인)

- **출처:** PR #31 머지 후 사용자 직접 확인: "위치 이동 누르면 센서·카메라 쪽으로 가고 거기서 움직일 수 없다", "검은 화면에서 변하지 않는다", "기존 보는 창에서 이동이 되면 좋겠다".
- **원인:** ① 키 조작(W/A/S/D·Q/E·방향키·Z/C·오른쪽 버튼 시점·Esc)은 배치 창의 작은 회색 한 줄에만 있어 알 수 없었다. ② 이동·회전 시작 시 미리보기가 조작 장비의 시점으로 바뀌는데, 정지 장비는 기존 계약("조작 중에도 새 측정을 시작하지 않음", `SensorSettingsReliabilityTests`)대로 촬영하지 않아 미리보기가 검게 남았다. ③ 시작 직후 키보드 포커스가 버튼에 남아 첫 키 입력이 3D 화면으로 가지 않을 수 있었다.
- **수정:** 조작 중에만 3D 화면 아래쪽에 키 안내 카드(hit-test 없음, Tick 없음)를 표시하고, 정지 장비이면 "미리보기는 바뀌지 않으니 3D 화면에서 장비를 보며 옮기라"는 안내를 함께 보인다. 소유 조작 시작 시 미리보기 시점을 바꾸지 않고(보던 화면 유지) 키보드 포커스를 3D 화면으로 보낸다. 정지 장비 무측정 계약은 유지한다.
- **검사 공백 보완:** 입력 opt-in 테스트(`Placement.InputOwnershipRhi`)가 개인 저장값(글자 150%로 넓어진 사이드바)이 덮는 고정 좌표를 써서 사용자 PC에서만 실패했다. 실제 3D 화면 hit 지점을 찾도록 고쳤다. 신규 `MA0T10.UXR13.KeepViewAndKeyGuideRhi`(보던 화면 유지·포커스·안내 표시/해제·렌더).
- **검증:** opt-in 입력 테스트 포함 SensorV2·UXR·UI02·Workspace·Monitor·Ui·Control·Placement·Settings **102 Success/실패0**, quick 8/8. 렌더 `Saved/Reports/UXR/UXR-13/`.
- **현재 상태: 85%.** 남은 것: 사용자 직접 확인.

### UXR-14 — 미리보기 카드 사방 크기 조절 · Slab 3D 글자 크기 확대 (P1, 2026-10-14 사용자 요청)

- **미리보기 카드:** 사이드바 모드의 오른쪽 아래 카드(기존 고정 400×288)와 자유 배치 창을 **네 변·네 모서리**에서 실제 마우스로 조절한다. 가장자리 7px·모서리 16px 영역에서 커서가 ↔/↕/⤡로 바뀐다. 왼쪽·위쪽 조절은 반대편 변을 고정하고, 끄는 변만 사이드바·상태바·화면 끝에서 멈춘다(카드가 밀리지 않음). 최소 260×180(배율 반영). 카드 크기는 세션 동안 유지되고 `⋯ → 이 창 위치·크기 초기화`로 기본값으로 돌아간다(저장 schema 불변). 조작 중 PIE 입력으로 새지 않는 UXR-11 경로를 그대로 쓴다.
- **Slab 3D 글자:** 표시 설정 글자 크기 상한 56 → **120**. UI-02가 실제 3D 라벨을 28px로 한 번 더 제한해 41 이상은 화면에 반영되지 않았다. 비율(×0.68)은 유지하고 상한만 설정값에 맞췄다. 글자가 커져 소유 UI·다른 라벨과 겹치면 UI-02 우선순위대로 낮은 순위 라벨이 숨는다.
- **검증:** 신규 `MA0T10.UXR14.CardEdgeResizeAndSlabTextRhi`(OS 커서 경로로 왼쪽·위·왼위 모서리·오른아래 모서리·한계 밖 끌기·최소 크기·초기화), `UI02.LabelPresentationRhi`에 글자 112 단계 추가(3D 라벨 world size 28→29.1, 112→83.3). 입력 opt-in 포함 SensorV2·UXR·UI02·Workspace·Monitor·Ui·Control·Placement·Settings·Slab **140 Success/실패0**, quick 8/8. 렌더 `Saved/Reports/UXR/UXR-14/`.

### UXR-15 — 기능 추가 후 UX/UI 전체 점검·정리 (P1, 로드맵 #10 1차)

- **출처:** 사용자 2026-10-15: "기능들 개선하면서 UX/UI 개선이 꼭 있어야 한다." 최근 PCD 과부하·자동 재개·전송률 조절, Camera 정답 미리보기, LiDAR 사실성, 데이터셋 생성의 사용 흐름을 함께 점검한다.
- **범위·점검:** `feat/uxr-15`, 기준 `b00a91f`, DTCore `b2504d1`. 생산 UI 수정 전에 소유7패널(배치 포함)의20화면을 actual client1920×1080/1280×720·기본/최소 사이드바·글자100/150%·window DPI1.0에서80장 캡처했다. 고정 다크이며 라이트는 해당 없음. 심각도·문구/상태/찾기/잘림/꺼짐/첫 사용 근거와 같은 조합 최종80장은 `Saved/Reports/UXR/UXR-15/RESULT.md`에 있다.
- **정리:** `파일 저장 · 데이터셋` 진입명, 일반 저장 뒤 접힌 데이터셋 그룹, 현재 켜짐/꺼짐·다음 행동 안내, 한국어 회전/시드/조명 문구, 대상명 줄바꿈·전체 툴팁, 항목명 아래 최솟값/최댓값 두 칸, 줄바꿈 동작 버튼. 상태바는 연결 설정 필요→재연결→연결된 사전 점검 차단→과부하/재개 대기→전송률 조절→지연→필수 출력 오류의 첫 경고만 표시한다. PCD 칩의 중복 문장을 줄이고 실제 우선순위를 README 표로 정리했다. 갱신5Hz·숨긴 상세 중단·기존 DTCore 소유/적용 경로를 유지한다.
- **승인된 CAM-GT-FIX:** 사용자가 전달한 Claude 범위 추가 승인으로 Camera 정답 체크의 누락된 `ApplyPendingState`를 수정했다. 다른 설정과 같은 Actor 검증·적용 transaction으로 즉시 runtime에 반영한다. 정답을 끄면 기존 RGB로 복귀한다. 최근 체크27개(UI→runtime→출력→기존 저장/복원 정책)를 전수 검사했고 다른 미적용 결함은 없었다. 기존 layout v1·프로필/SaveGame v6·데이터셋 세션 draft·Publisher 인스턴스 설정을 구분하며 새 영구 저장 정책을 만들지 않았다. 신규 자동 회귀3개와 상세 `TOGGLE_AUDIT.md`를 추가했다.
- **직접 확인:** 주요8개+범위15개+파일 출력4개의 on/off, 실제 정답4모드·4파일 저장, 정답/사실성 포함 데이터셋3장면 완료·끄기 후 RGB 복귀를 확인했다. client1920×1080·DPI1.0·글자100%의 대표 조합이다. 초기 Camera FAIL과 timeout 종료 이력은 보존하고 수정 후 재검수로 구분했다. 보안 팝업/캡처0x80070057은 관찰되지 않았다.
- **통합 직접 점검:** [MANUAL_CHECKLIST](MANUAL_CHECKLIST.md)에 PCD-OPT·CAM-GT-01·LIDAR-FID-01·DATASET-01·UXR의 중복 보류를20행/57분 순서·통과 기준·원본 매핑으로 묶었다. 전체57분 실행,125/150%·좁은 창 직접 전수, 자유 배치 과거 직접 드래그 FAIL, 제한 링크/소비자/원격·사용자 무안내 수용은 이번 옵션 재검수로 닫지 않는다.
- **최종 gate:** Editor/Game Development, release14단계·D3D12 고유280 Success/skip31/실패0(신규3개 포함), WBP7 메모리 컴파일·실제 Slate 어휘114/기본30영역/허용7영역·위반0, layer 위반/allowlist0/0. 기본 꺼짐 표시형1+1·1920×1080·10초 준비+30초·ABAB×3, `verify.ps1 -Level perf -BaseCommit b00a91f -PerfMeasurementSeconds 30`: FPS 저하 -0.0162%·p95 +0.0000%·GT -1.9570%, 2% gate 통과. quick8/8·문서 링크/시간/diff check도 통과했다. 기능 기본값·wire·저장 형식·Blueprint API·DTCore·자산 변경은 없다. Camera 미적용 수정만 승인된 동작 예외다.
- **현재 상태: 90%.** 요청 구현·자동 검증·대표 조합 직접 옵션 확인 완료, Claude 독립 검토와 전체 사용자 수용 대기. 큰 구조 변경(데이터셋 전용 화면/상태 상세/라이트)은 제안으로만 남긴다.

### UXR-16 — v1.1 체크포인트 UX/UI 2차 점검·통합 검증 (P1, 로드맵 #10)

- **출처/범위:** 사용자 2026-10-15 "UX/UI 지속 개선". `feat/uxr-16`, 통합 기준 `5dd9327`, UI `b6f2dc8`, DTCore `b2504d1` 고정. AUTO-01·EXPORT-01·ROS2-02·IMU-01이 합쳐진 상태를 먼저 검증했다.
- **통합:** 실제 API 캡처 중 IMU 설정, batch 점유 중 변경409/조회200·IMU revision 독립, 실제 Slate 정답+IMU+COCO 선택→생성·변환/파일·복원 회귀를 추가했다. 새 제품 결함은 재현되지 않아 INTEG-01 동작 수정 없음. 초기 release의 UXR10 resize1 FAIL을 보존하며 단독/집중/최종 전체에서 재통과했지만 원인 수정 완료로 표시하지 않는다.
- **점검·정리:** 소유7패널의 기존20+새7화면, client1920×1080/1280×720·글자100/150%·window DPI1.0 전후108장씩. 자동화 그룹·API/일괄/IMU 활동과 기존 첫 경고 공존, 요청 대기/접수 작업 구분·한국어 문구·IMU 숫자 배율·기록/전송 현재 상태·변환 선행 안내/줄바꿈을 정리했다. 기본값·wire·저장 형식·BP·DTCore·자산 불변. 기본/고급/자동화 큰 재배치는 캡처 기반 목업 제안만 남겼다.
- **현재 검증:** Editor/Game Development·release14단계, 고유290 Success/skip34/실패·NotRun0, 집중9·Python69 Success, golden14 byte 동일·WBP7 메모리 compile/저장0·실제 Slate114상수/기본34/허용7영역·위반0·layer/allowlist0/0·quick8/8. 실제 D3D12 오프스크린1+1·10+30초·ABAB×3: FPS 저하 +0.0022%·engine p95 +0.0000%·GT -0.6700%,2% gate 통과. 선행a55872d의GT+2.141% 실패는 보존하고 비활성 활동 위젯 제거·IMU 문구5Hz 캐시 뒤 재검증했다. 이는 표시형 결과가 아니며 잠금 확인 이력과 절대값/센서별0Hz 포함 결과는 RESULT를 따른다.
- **직접/남은 gate:** 첫 sky 캡처가 Windows 잠금 화면이어서 즉시 중단, 마우스·키보드0·보안 팝업 조작0. CAM-GT/DATASET/AUTO API/IMU/EXPORT/동시 상태6행은 환경 보류다. [통합 점검표](MANUAL_CHECKLIST.md)를24행/70분으로 확장했다. 잠금 해제 후 대표 직접 재시도와 표시형2% 성능, 사용자 무안내 수용·과거 자유 배치 FAIL·서버/ROS2/원격 수용은 남는다. 이전 직접 PASS/FAIL·보안 팝업 기록을 덮어쓰지 않는다.
- **진행률:70%.** 핵심 구현·집중/전체 자동 회귀·문서 정리 완료, 주요 직접/표시형 수용·독립 검토 대기. 보호 소스/자산558·개인286파일(SaveGame9) hash 일치·추가0, DTCore gitlink/worktree clean. ignored UHT Timestamp2 재생성은 별도 기록. 전체 이름/skip·초기 실패·캡처·목업·제한: `Saved/Reports/UXR/UXR-16/RESULT.md`.

### UXR-17 — 미리보기 카드 개선 (P1, 로드맵 #18)

- **출처/범위:** 사용자 2026-10-16, PreviewSwitch의 첫 Camera 프레임 대기·정답 모드 구분·긴 헤더 문제. `feat/preview-polish`, 기준 `e93d44f`(TESTSET-01 포함), DTCore `b2504d1` 고정. 기존 소유 미리보기 카드(L3)만 변경한다.
- **표시:** 새 Camera의 유효 영상이 없으면 `영상 준비 중…`과 기존5Hz 캐시의 진행 표시, 5초 뒤 센서 상태 확인 안내. 기존 acquisition 캐시/정답 texture의 유효성을 읽으며 첫 유효 영상에서 즉시 해제한다. 이미 측정 중인 센서는 자기 캐시 영상을 즉시 연결한다. 새 캡처/readback/타이머·송신/저장 정책은 없다.
- **모드/헤더:** 카드에 일반/깊이/종류/개체를 표시한다. 기존 범례가 보기 설정 메뉴 안에만 있던 점을 보완해 깊이0–50m 색 막대·공통 클래스 팔레트·개체별 색 설명을 카드에 노출했다. LiDAR 점군만 보기에도 배지를 둔다. 이름만 말줄임하고 전체 상태/이름 툴팁과 별도 상태·Hz 영역을 유지한다. 기존 메뉴 용어·Blueprint API·기본값·wire·저장 형식·DTCore·자산은 불변이다.
- **현재 검증:** 구현 `d519ac0`, client1920×1080·DPI1·글자100/150%·기본/최소 카드의12상태×4조합 전후48장씩. 실제RHI 준비/시간 초과/해제·모드/공통 팔레트/0–50m·150% 긴 이름·별도 렌더 프레임20회 전환 회귀 통과. Editor/Game Development·release14단계·고유291 Success/skip37/실패·NotRun0, WBP7 메모리compile/저장0·어휘114/기본34/허용7·위반0·golden·quick8/8·layer/allowlist0→0. `verify.ps1 -Level perf -BaseCommit e93d44f -PerfDisplay Visible -PerfMeasurementSeconds 30`의1+1 ABAB×3(12유효 실행): FPS 저하 -0.0053%·engine p95 +0.0000%·GT -1.1632%,2% 수치gate 통과. client1920×1080은 실행기가 확인했지만 Computer Use 화면 캡처 수용은 별도다. 상세는 `Saved/Reports/UXR/UXR-17/RESULT.md`.
- **진행률:90%.** 구현·집중/전체 자동 검증·성능 수치·문서 완료. 직접5행은 창 재선택 포함2회 모두0x80070057로 캡처 실패·입력0·환경 보류이며 잠금/보안 팝업이 관찰됐다고 추정하지 않는다. 보호558/개인330파일 hash 복원·추가0, DTCore clean. [추가 직접 점검표](MANUAL_CHECKLIST.md#uxr-17-미리보기-추가-직접-점검)를 사용하며 자동 렌더를 마우스 수용으로 표시하지 않는다. Claude 독립 검토 대기.

### UXR-18 — 센서 상세 설정 재배치 (P1, 사용자 2026-10-16)

- **출처/범위:** 사용자 2026-10-16 승인, UXR-16의 설정 구조 목업 후속. `feat/settings-tabs`, 기준 `ff1ec9a`(v1.1-rc2), 구현 `9a5fc7b`, DTCore `b2504d1` 고정. 기존 소유 Settings의 화면 위치·노출 수준과 기존 소유 화면 진입만 바꾼다.
- **구성:** 기본=프로필·품질·주기·거리·미리보기 점·같은 배치 소유 측정 동작. 고급=Camera 정답·LiDAR 사실성/다중 검출·측정 규격·기존 배치 IMU/노이즈 진입·접힌 호환 출력/관리. 자동화=API/주소·일괄 상태를 맨 위·기존 데이터셋/변환 형식·TESTSET 화면 진입·기존 스캔 자동 파일 저장. 링크만으로 부착/생성을 시작하지 않는다. Blueprint·콜백·기본값·적용/저장/복원·wire·SaveGame 형식·DTCore·자산 변경0이다.
- **탭/비용:** 기존 분할 탭을 스크롤 바깥에 유지한다. 새 인스턴스는 기본, 같은 세션 재열기/선택 변경/재호스팅은 마지막 탭 기억. 기존 저장 구조에 필드가 없어 영구 슬롯/버전 확장0이다. 켜진 고급/자동화 개수는 센서 revision·기존 툴바5Hz 활동 캐시로 표시하며 0은 숨김, 숨김/접힘 갱신·비활성 API 상세 조립을 멈춘다. 새 Tick/timer·Actor 전체 순회·capture/readback/scan0이다.
- **현재 검증:** 두 client1920×1080/1280×720·글자100/150%·좁은/넓은 사이드바 ×10화면의 전후80장씩, metadata80쌍의 해상도/배율/DPI/폭/선택 장비 일치·누락0. 최종150% 켜짐 한 줄 geometry 회귀 통과. 집중8 Success/skip0/실패0이며27개 토글의 기존 UI→runtime→출력·저장/복원 회귀를 새 위치에서 실행했다. 최종 검증 소스4c14a82의Editor/Game Development·release14단계·고유293 Success/skip37/실패·NotRun0·WBP7 메모리compile/저장0·어휘114/기본36기록(고유33)/허용7·위반0·golden·layer0 통과. 표시형 perf는 표준 verify의11번째 기준 실행과 별도 프로세스의대체 ABAB 후보 실행이 모두0xC000013A 제어 종료로 중단됐다. 원본10회와 재시도 기준1회는 유효하지만 불완전 블록·서로 다른 기준 GT 상태를 합치지 않았으며2% gate는 미판정이다. 종료 원인은 확정하지 못했고 제품 성능 회귀로 단정하지 않는다.
- **직접/진행률:70%.** 핵심 구현·전체 자동 검증·문서 완료, 직접/성능·독립 검토 보류. 첫 sky 활성화/캡처는0x80070005, 최신 PIE 재선택 후 캡처0x80070057로 직접 PNG 없음·입력0·환경 보류6이다. 잠금/보안 팝업 관찰로 추정하지 않는다. [추가 직접 점검표](MANUAL_CHECKLIST.md#uxr-18-센서-상세-설정-추가-직접-점검), 캡처·탐색 클릭 수·검증 원문·보존/제한은 `Saved/Reports/UXR/UXR-18/RESULT.md`를 따른다. 자동 렌더/성능은 직접 수용이나 독립 검토를 대체하지 않는다.

### MAP-01 — 새 맵에 넣어도 바로 동작 (P0, 2026-10-12 사용자 요구)

**진행률: 90%** · CLI 구현/검증 완료, 직접 마우스는 요청된 예외에 따라 UXR-07에서 수행하며 독립 검토 대기다.

**현재 결과:** L3 `AVirtualSensorToolRig` 하나가 유일한 기존 Coordinator/센서 Host/Slab Host/Replay Host를 재사용하거나 빠진 구성만 transient로 생성한다. Slab·배경·장비·Player/공유 설정은 생성·교체하지 않는다. 중복 Rig/구성은 거절하고 복수 Slab은 `TargetSlab`을 요구한다. Slab 없음·늦은 배치·기존 3D 대상 선택·PCD 설정 연결을 다룬다. EndPlay는 생성한 helper만 정리하며 재사용한 연결을 복원한다.

**맵 점검:** `USensorMapCheckWidget : UDxWidget`를 기존 `UDxWidgetSubsystem`에 등록·닫기·전면·입력 차단 등록한다. 기존 Workspace에 새 저장 role이나 병렬 위젯 관리자를 만들지 않는다. 11개 읽기 전용 항목, 표시 중 최대 1Hz, 변경 없는 문구의 FText 재사용, 닫힌 화면 갱신 중단이다. `ReceiverId`, `IFactory-agent`, `WebSocketTopics/topic.scenario`, `DT_DxLevel`은 설정을 찾기 위한 정확한 이름이므로 금지어 검사의 **map-check 진단 allowlist**로 기록하고 실제 Slate 문구를 전수 수집한다. 기본 시나리오/Slab 없음 안내는 금지어 없이 유지한다.

**저장/맵:** 다른 맵은 원본 맵 예약 저장을 버튼과 직접 API 양쪽에서 거절한다. SensorTestMap 고정 경로·태그·기존 EndPIE 저장 계약은 유지한다. `setup_portable_sensor_maps.py`로 승인된 `PortableSetupMap`/`PortableNoSlabMap`만 생성했으며 재실행은 기존 대상 덮어쓰기 전에 중단한다. 하드코딩 조사 212개 위치에서 기본/cook 맵·공통 자산·테스트 fixture·호환 API는 근거 있는 예외로 유지한다. 범위 밖 외부 입력의 첫 장비 fallback은 변경하지 않았다.

**검증/제한:** 최종 소스 `efcd353`, Editor/Game Development·`verify.ps1 -Level release -BaseCommit b16f35c` 14단계 통과. 고유 242 Success/skip20/실패·NotRun0, 기존 259개 이름별 결과 유지+신규 MAP-01 3개. 두 맵×두 해상도×100/150% 기본/점검 렌더 16장, Rig 포함 성능 FPS −0.006%/callback p95 −0.789%/engine p95 0%/game thread −10.741%로 2%gate 통과. layer0/0·WBP7·map-check 진단 allowlist 통과, 보호297/297·개인 SaveGame9/9 복원. 상세는 `Saved/Reports/MAP/MAP-01/RESULT.md`와 `summary.json`, 날짜별 원문은 VALIDATION_HISTORY를 따른다. DTCore `b2504d1`에는 현재 업무 서버 상태 조회 API가 없어 해당 행은 확인 불가 경고이며 출력 서버의 실제 연결 조회와 구분한다. 직접 마우스 체크리스트와 UXR-07 런북에 10단계를 추가했다. MAP-01 독립 검토 전에는 개편 프로그램 **87%**를 올리지 않는다.

- **요구:** 실제 적용은 신규 맵에 이 도구들을 넣고, Slab·배경은 그 맵에서 따로 다시 세팅한다. 그런 맵에서도 잘 동작해야 한다.
- **현재 걸림돌(조사 대상):** Coordinator·UiHost·Slab UI Host 등을 수동 배치해야 함, 맵 저장 예약이 `SensorTestMap` 전용 경로, 자산·맵 경로 하드코딩 가능성, Slab TrackReference/ReceiverId·TC DataTable·DTCore `WebSocketTopics` 설정 누락 시 조용히 실패할 수 있음.
- **작업:**
  1. 맵 독립성 조사: `SensorTestMap`·`/Game/MA0T10/Maps/...` 하드코딩, 특정 Actor 이름/태그 의존, 테스트 Rig 의존을 전수 확인하고 목록화한다.
  2. **"센서 도구 설치" 단일 진입점(프로젝트 모듈 안, 플러그인 아님):** 맵에 Actor 하나(예: `AVirtualSensorToolRig`)만 놓으면 PIE 시작 시 Coordinator·UI Host·Slab UI Host를 **있으면 재사용, 없으면 자기 소유로 생성**한다. GameMode/Main/Player를 교체하지 않고, 자기가 만든 것만 EndPlay에서 정리한다. 맵을 저장하지 않는다.
  3. Slab·배경은 사용자가 따로 배치한다는 전제로: Slab이 없으면 시나리오·Slab 정보 영역은 "Slab이 없습니다 · 배치 방법" 안내로 동작하고 센서 기능은 그대로 쓸 수 있다. Slab이 여러 개면 대상 선택을 안내한다.
  4. **맵 점검 화면(도구 → 맵 점검):** 필요한 구성(센서 도구 Rig, Slab·TrackReference, ReceiverId, TC 등록, DTCore Topic, 서버 연결)을 ✓/⚠로 보여 주고 해결 방법을 한 줄로 안내한다. 읽기 전용, 자동 수정 없음.
  5. `SensorTestMap` 전용 맵 저장 예약은 현재 맵 기준으로 일반화하거나, 일반화가 위험하면 다른 맵에서는 비활성+이유 표시.
  6. 검증용 **새 테스트맵**(스크립트로 생성, 예: `/Game/MA0T10/Maps/Tests/PortableSetupMap`): 빈 레벨 + Rig 1개 + 간단한 바닥/Slab만 두고 자동화로 장비 추가→배치→시나리오→PCD→결과 흐름을 확인. Slab 없는 변형도 확인.
  7. README에 "새 맵에 적용하기" 절(필요 구성·순서·점검 화면 사용법)을 추가한다.
- **완료:** 새 테스트맵과 Slab 없는 맵에서 전체 흐름 자동화 통과, 기존 맵(SensorTestMap·테스트맵) 동작 불변, 하드코딩 목록 0 또는 근거 있는 예외만, 맵 점검 화면 렌더·마우스 점검.

### 순서·진행률 갱신
- 순서: (산화 스케일 마무리) → UXR-09 → MAP-01 → UXR-07(리프레시된 UI + 새 맵 기준 최종 마우스 점검·사용자 확인).
- 가중치: UXR-09 = 10, MAP-01 = 8 추가 → 총 106. 현재 통과 82 → 82/106 = **77%**(범위 추가로 분모가 커짐).

### 개편 프로그램 진행률 (ARCH + UXR + MAP)

**UXR-08 보완 CLI 결과(2026-10-12):** 정보 카드를 소재/진행/자세·위치 2열 그리드로 정리했다. 현재 표시 위치·속도는 기본, 원본 행 위치·속도는 상세 접힘이며 일시정지 0.0 cm/s를 명시한다. 숫자 형식·단위 표시를 통일했다. 마커는 기존 충돌 크기·DTCore Click/hover를 유지하며 main/depth pass만 끈다(새 자산 없음). Editor Development, layer18→18(신규/stale0), guard, V2 assets, WBP7, 관련33 Success 및 최종 마커/UXR08 7 Success(실패/skip0), 1280/1920×100/150% 렌더24장 통과. 모니터+진행+보관 목록을 연 동일 1Camera+1LiDAR·1920×1080·10초+30초에서 FPS +0.004%, engine p95 0%, callback p95 +0.672%, game thread 평균 +1.575%로 2%gate 통과. 보호475/475·개인SaveGame9/9 복원. 직접 마우스는 계속 보류하며 진행률90%를 유지한다. `Saved/Reports/UXR/UXR-08/supplement/RESULT.md` 참조.

Codex가 한 항목을 끝낼 때마다 Claude가 이 표 기준으로 **개편 프로그램 진행률**을 보고한다. 제품 전체 완성도나 위의 개별 항목 진행률 평균이 아니라, 이번 ARCH+UXR 개편 계획 중 **검토까지 통과한 항목의 작업량 비중**이다. 가중치는 예상 작업량이며 범위가 바뀌면 이 표를 함께 고친다.

| 항목 | 가중치 | 항목 | 가중치 |
|---|---:|---|---:|
| ARCH-01 기준선·gate | 3 | UXR-01 흐름 바·상태 줄 | 5 |
| ARCH-02 폴더 재배치 | 3 | UXR-02 배치 + UXR-01 보완 | 8 |
| ARCH-03 계약·상수 | 4 | UXR-03 PCD 전송·결과 | 8 |
| ARCH-04 호출 역전 | 5 | UXR-04 시나리오 | 5 |
| ARCH-05 TC 분리 | 5 | UXR-05 센서 상세 설정 | 6 |
| ARCH-06 Slab context 중립화 | 8 | UXR-06 미리보기 | 5 |
| ARCH-07 검증 진입점 | 4 | UXR-07 문구·수용 | 6 |
| ARCH-08 정리 | 5 | UXR-08 Slab 정보·표면 | 8 |
| MAP-01 새 맵 적용 | 8 | UXR-09 A안 리프레시 | 10 |
| **ARCH+MAP 합계** | **45** | **UXR 합계** | **61** |

총 106(2026-10-12 UXR-09·MAP-01 추가로 88→106). 진행률 = 검토 통과 항목 가중치 합 ÷ 106. 2026-10-12 기준 통과: ARCH-01~04, UXR-01, UXR-02(마우스 최종 삭제 1건 보류), UXR-03(직접 조작 보류), UXR-04, UXR-08(산화 스케일 지원·직접 조작 보류), UXR-05, UXR-06(직접 조작 보류), ARCH-05, ARCH-06, ARCH-07, ARCH-08 → 82/106 = 77%(범위 추가로 분모 증가). 2026-10-12 UXR-09 검토 통과(시안 충실도·상대 성능 gate 통과, 마우스 점검은 TextInputHost 포커스 고착으로 보류) → 92/106 = 87%. 2026-10-13 MAP-01 검토 통과(새 테스트맵 2개 PIE 흐름·맵 점검 화면, 마우스 점검은 UXR-07로 이관) → 100/106 = **94%**. 남은 것은 UXR-07(최종 마우스 점검·사용자 확인)이다. 남은 UXR-07은 사용자 마우스 점검·확인이 필요하다. 보류된 마우스 점검은 사용자 복귀 후 일괄 수행하며, 그 결과로 통과가 뒤집히면 이 값을 다시 낮춘다. UXR-02는 CLI 마무리 완료(최종 삭제 마우스 1건 보류)이며 독립 검토 반영 전에는 이 합계에 더하지 않는다.

### 개편 후속 개선 (2026-10-13 Claude 검토, 사용자 요청으로 문서화)

개편 프로그램(ARCH+UXR+MAP)은 **UXR-07 사용자 체험·확인**이 끝나면 닫는다. 아래 항목은 진행률 표(106)에 넣지 않는 별도 후속 항목이며, 권장 순서는 **UXR-07 사용자 체험 → PERF-01·UXR-08 외형 확인 → UI-02 → OPS-01·QA-02 → DTCore 수정 단계 → 센서/카메라 플러그인화(R5)**다. FONT-01은 선택이다.

#### PCD-20HZ — 원본 사양 20Hz PCD 송신 중 연결 경고 (P0)

- **출처:** 사용자 2026-10-14. 아이요봇 ML-X(80) 원본 사양, 측정 주기 0.05초, 고속 TCP PCD 전송 중 연결 뒤 하단에 "연결 설정 필요 · 서버 연결 설정을 확인하세요"가 나타남. Editor/패키지 발생 범위는 아직 확인하지 못했다.
- **완료 조건:** 로컬 Broker에서 0.05/0.1/0.2초 각 최소 5분, LiDAR 1대(가능하면 2대)의 연결·heartbeat·STOMP 오류·프레임 bytes/Hz·예산/드롭/큐·Broker 로그를 시간축으로 대조한다. 실제 단절과 표시 오류를 구분한 뒤 원인에 맞춰 수정하고 PCD wire bytes를 유지한다. SensorTransport/SensorStream/SlabReliability·golden fixture·quick·layer0, 송신 경로 변경 시 상대 성능2%, 직접 UI 확인을 별도로 기록한다.
- **현재 결과(2026-10-14): 진행률 60%, 수신 정체 경로 재현·수정.** `fix/pcd-20hz-connection`, 기준 `54cd68a`, DTCore `b2504d1`. Node 22 가짜 STOMP 서버에서 1.06MB × 20Hz 송신 도중 TCP reads를 8초 멈추면 기존 worker가 2회 단절했다. 부분 STOMP SEND를 버리지 않고 기존 frame/byte reservation을 유지하면서 incoming receipt/ERROR/heartbeat를 처리하도록 수정했다. 큐 상한의 신규 프레임 생략은 유지하며 지연을 별도로 표시한다. 송수신 heartbeat 시계를 분리·협상하고, ERROR 사유와 socket/write 진단은 제한된 비밀 제거 텍스트로 남긴다. 설정이 있으면서 5초 미만 단절이면 재연결 안내, 연결된 과부하는 전송 지연 안내다. 서버 자체 7시험, Editor Development, SensorTransport/SensorStream/SlabReliability·상태 판정 **31 Success / skip6 / 실패0**, quick8·golden fixture·layer 위반/allowlist0 통과. 정상 로컬 Artemis의 수정 전 Native 20Hz 1대는 5분 **6000프레임 모두 submit/receipt/consumer 완료**, Raw·상태바 연결 유지(Engine 비연결)였으므로 **사용자 환경의 동일 원인으로 단정하지 않는다**.
- **현재 결과(2026-10-14): 90%.** 최종 HEAD 기준 가짜 STOMP 서버·설치된 Artemis 2.44(설정 변경 없음)에서 1대 20/10/5Hz와 2대 20Hz 각 5분: 6,000/3,000/1,500/11,994 전부 제출·receipt·수신, 단절·경고 0(2대 20Hz 42.35MB/s). 가짜 서버 8초 정지는 수정 전 단절 2회 → 수정 후 0회 "전송 지연" 후 회복, 20초 정지도 연결 유지. 실제 worker 장애 7종·관련 회귀·quick·layer0, perf(54cd68a 대비) FPS·p95 동일·GT +0.29%. Claude 검토로 프레임 쓰기 60초 최종 상한 추가. 남은 것: 사용자 환경 확인. 과부하 자동 재개는 사용자 2026-10-14 결정에 따라 아래 PCD-RESUME에서 구현·검증했다. 근거 `Saved/Reports/PCD-20HZ/RESULT.md`.

#### PCD-RESUME — PCD 과부하 중단 후 자동 재개 (P0)

- **출처:** 사용자 2026-10-14, "PCD 자동 재개는 켜는 쪽으로". PCD-20HZ의 영구 과부하 중지 정책 후속이며 다른 스트림의 자동 재개·Broker 설정 변경은 범위 밖이다.
- **완료 조건:** 과부하 전용 예약과 사용자 중지/재설정 취소, 기본 2초·지수 백오프·최대 30초·정상 receipt 30초 후 초기화, 실제 연결 및 예약 배수 확인, 옵션 off 기존 동작, NoLoss 실패·폐기 기록·시퀀스/PCD golden bytes 유지. 상태바와 진단 문구를 연결 경고와 구분하고 SensorTransport/SensorStream/SlabReliability·golden·장애 전체·quick·layer0·`a26c2d5` 대비 perf ABAB·직접 UI를 따로 보고한다.
- **현재 결과(2026-10-15): 진행률 100%, 요청 범위 구현·검증 완료.** `feat/pcd-auto-resume`, Runtime `849e4e4`→`00b4ec7`, 기준 `a26c2d5`, DTCore `b2504d1`. component 공개 설정 기본 on, 일시 중단/revision/예약 취소, 실제 worker의 해당 PCD 예약 0 + 전역 bytes 절반 이하 + 연결·지연 해소 gate를 구현했다. 과부하 폐기/Slab failure를 기록하고 새 프레임부터 재개한다. 관련 D3D12 회귀 38 Success/skip7/실패0, 상태 전이 7 Success, 실제 worker 장애 8종·최신 복구 재시험·Node 서버 7/7 통과. 직접 PIE(1920×1080, DPI1.0, 글자150%)에서 과부하→정상 표시·진단 재개1회/폐기41프레임·PCD 중지 후 유지 확인. Editor/Game Development·WBP7 메모리 컴파일·quick8·golden14 bytes·layer0 통과. 첫 perf ABAB의 GT +3.27% 실패를 보존하고, 복구가 없으면 순회를 생략하도록 보완했다. UI 끄기 `ConfigureStream(false)`의 unsent 큐 정리도 보완·직접 재검증했다. 최종 `00b4ec7`의 같은 ABAB×3은 FPS +0.0025%, engine p95 동일, GT +0.45%로 2% gate 통과다. 근거 `Saved/Reports/PCD-RESUME/RESULT.md`.

#### PCD-OPT — 운영 20Hz PCD 송신 최적화·시나리오 주기 변경 멈춤 (P0)

- **출처:** 사용자 2026-10-15. 운영 서버 PC의 **Unreal Editor PIE**에서 ML-X(80) 원본 1대 .05초만 과부하·자동 재개가 발생했고, 시나리오 중 .1↔.05 반복 변경 후에는 Slab과 화면 전체가 멈췄다. 서버 사양/로그/시나리오는 제공 불가이며, 사용자는 근거 있는 추정 수정을 승인했다. 패키지였다는 앞선 가정은 철회했다.
- **완료 조건:** 18.5MB/s·RTT10ms .05초 5분 과부하0·receipt 공백1초 이하, .06/.1 및 Artemis 20Hz 생략0, 시나리오 주기 변경12회와 소켓 포화·affinity 2~4 논리 CPU PIE에서 150ms 초과 hitch0. default-on/off 조절·명시 생략 집계·Slab 실패 계약·wire/golden 불변, 관련 회귀·장애 전체·quick·layer0·291b4f9 대비 perf를 확인한다. 실제 운영 멈춤 원인 미확정은 열린 항목으로 보고하고 구현을 막지 않는다. 크기 축소는 분석만 한다.
- **현재 결과(2026-10-15): 구현100%·요청 CLI gate100%, 직접 UI 보류.** `7275b3d`에 AIMD admission·명시 생략/Slab 실패·UI 옵션·소켓/이벤트 대기·로그 제한, `2d18939`에 .05→.06 즉시 여유 회복, `2b1388a`에 실제 viewport 검증을 구현했다. 실제1080p .05초5분 과부하/hitch0·gap95ms·17.35Hz/18.40MB/s/생략818, 4논리 CPU·시나리오 변경12회5분도0·gap218ms·16.96Hz/17.98MB/s/생략802. .06/.1과 Artemis20Hz는 생략0. 1080p·포화·변경12회2/4논리 CPU hitch0(max124.70/93.88ms). 기존 폴링의326ms(GC313ms) hitch는 재시험에서 재현되지 않아 운영 원인으로 단정하지 않는다. 관련45 Success/skip8·Node11·장애9·Editor/Game Development/Shipping·quick8·layer0 통과.291b4f9 대비 ABAB12회 중앙값 FPS −0.01%·p95 동일·GT +1.77%로 perf 통과. 개인SaveGame9복원·추가0·소유Editor/Broker 종료. 직접 UI는 Windows 보안 권한 팝업 때문에 입력0/보류이며 자동 API 변경과 구분한다. 상세 동일1280 전후표·1080p 수용표·제한·크기 축소 분석은 `Saved/Reports/PCD-OPT/RESULT.md`.

#### PERF-01 — 절대 성능 기준선 재확인 (P0)

- **근거:** UXR-09·MAP-01·UXR-07의 성능 판정은 모두 "같은 측정 방식의 변경 전/후 ±2%" 상대 비교였고 통과했다(UXR-07 교차 측정: FPS −0.19%, p95 +0.15%, game thread −0.90%). 그러나 그 측정의 절대값은 1 Camera+1 LiDAR에서 **평균 약 47 FPS**(오프스크린 D3D12)이고, 2026-10-10 기록은 **실제 표시 1920×1080 2+2 PCD-only 평균 59.89 FPS**였다. 측정 방식 차이인지 개편 중 누적 회귀인지 확인되지 않았다.
- **작업:** 개편 시작 `fda25e0`과 현재 HEAD를 **같은 방식**(실제 표시 PIE 1920×1080, 1+1과 2+2, 10초 준비+60초, ABAB 교차 3회)으로 비교한다. 같은 조건에서 오프스크린 측정과 표시 측정의 차이도 기록한다. 누적 회귀가 확인되면 커밋 구간을 이분 탐색하고 Unreal Insights로 원인을 찾아 고친다. 마우스 점검·다른 Editor 세션이 도는 동안에는 측정하지 않는다.
- **완료:** 개편 전 대비 FPS·frame p95·game thread가 각각 2% 이내, 또는 원인과 선택지 보고. 표시형 2+2 PCD-only 평균 ≥55 FPS(ROADMAP 제안 gate) 유지 여부를 기록한다.

**현재 결과(2026-10-13): 진행률 90% — Broker 없이 승인된 범위 완료, 실제 PCD 송신 환경 보류.** `fda25e0` vs `2a5f89b`(production은 `807f512`와 동일), DTCore `b2504d1`, 같은 맵 바이트·계측기·1920×1080/D3D12·10초+60초로 표시/오프스크린 1+1·2+2 각 ABAB×3(48회)를 수행했다. 모든 FPS·engine p95는 기준 안이었고 초기 GT +2.16/+15.93/+2.77% 초과는 원본에 남겼다. 두 중간 지점 `a36179c`/`a96f638`와 Insights를 조사하고 초과 세 조건을 ABAB×3 재측정(36회)한 결과 GT +1.63/+1.52/+1.42%로 모두 2% 안이어서 특정 커밋의 회귀로 확정하지 않았다. production 수정 없음. Editor·Game Development, quick8, 전체 D3D12 **242 Success / skip22 / 실패·NotRun0**, 기존263개 이름/상태 동일+신규 PERF opt-in1, WBP7 메모리 컴파일 통과; layer/allowlist0→0. 1+1은 Camera encode 포함 미송신, 2+2는 acquisition+미리보기만이며 과거 2+2 PCD-only 송신 포함 기록과 다르다. 표시/오프스크린 모두 약60 FPS여서 오프스크린만으로 과거47 FPS를 재현하지 않았으나 장면·화면·동작 조건도 달라 원인을 단정하지 않는다. **실제 2+2 PCD 송신은 Broker 환경 보류**이며 Broker 시작·설정 변경은 하지 않았다. 원본·실행 조건·Broker 준비 뒤 명령·QA-02 절차는 `Saved/Reports/PERF/PERF-01/RESULT.md`, 상세 이력은 `VALIDATION_HISTORY.md`에 있다.

#### UXR-08 후속 — 산화 스케일 외형 확인 (P1)

- UXR-09에서 `M_SlabSurfaceOxideScale` 외형을 다듬었다는 보고만 있고 Claude·사용자의 화면 확인 전이다. 5종 비교 이미지를 다시 만들어 확인하고, 밀 스케일처럼 보이지 않으면 파라미터만 조정한다(새 자산 금지, 기본 프리셋 센서 결과 불변).

**현재 CLI 결과(2026-10-13, `28461bc`):** 기존 oxide 재질만 재생성하여 작은 3-octave ALU 조각·가는 균열·grain/미세 normal·소량 갈색을 적용했다. 색상은 display-referred 값을 linear로 변환했으며 노출 강판은 흰색 대신 금속 회색이다. 중앙 BaseColor ROI의 조각 19.04%/흰색 0%, shader pixel 379→527(+148)/vertex 131 불변/외부 texture 0. 냉각 강판·무광 값도 구분하되 기본 MID/광학 복원 유지. Editor Development·실제 D3D12 UXR08 7 Success/실패·skip0·quick8·layer0 통과, 기본 Camera/CPU·GPU intensity/PCD bytes 불변 회귀 포함. 5종·근접2·전후 비교는 `Saved/Reports/UXR/UXR-08/oxide-realism/`에 있다. **Claude 판단(2026-10-13, 사용자 기준 "정말 사실적이지 않으면 현재 열연 표면으로 충분"):** 흰 얼룩 문제는 해소됐으나 조각이 고르게 퍼져 사진급 밀 스케일은 아니다. 이 버전은 선택 프리셋으로 유지하고 추가 반복은 하지 않는다. 기본 표면은 기존 열연 표현을 쓴다. 최종 통합 release/성능은 아래 UI-02와 함께 남는다.


#### OPS-01 — 에이전트 작업 환경 (P1)

- **Codex 대화 분리:** 앱 대화(thread `019f6014-…`)의 기록이 1.1GB를 넘었다. 느려지고 요약 과정에서 맥락을 잃을 수 있으므로, 다음 단계(PERF-01 이후)는 **새 Codex 대화**에서 시작한다. 첫 요청에 AGENTS.md·README §10·IMPROVEMENTS 요약표를 읽게 하고 이전 thread ID를 남긴다.
- **입력기 포커스 고착:** 사용자가 한글을 입력하면 `TextInputHost`가 전경을 잡고, 실제 클릭 전까지 Computer Use(Claude·Codex 모두)가 막힌다(UXR-02·03·07에서 반복). 마우스 점검은 사용자가 PC를 쓰지 않는 시간에 몰아서 하고, 근본 해결은 **에이전트 점검 전용 Windows 계정 또는 VM**이다.
- **현재 결과(2026-10-13): 진행률 50%.** 후속 작업은 새 Codex CLI 대화에서 진행 중이고, 개발 폴더와 검수 전용 복사본(`ma0t10_dt_qa`)을 나눠 개발·마우스 점검을 병행한다(AGENTS §6). 입력기 포커스 고착은 여전히 재발(사용자 실제 클릭으로 해소)하며 전용 계정/VM은 미착수다.
- **모델 용량 오류:** `server_overloaded`가 반복된다. 앱은 자동 재시도 후에도 완료 판정 전 90초 대기, CLI는 10분 간격 재시도 규칙을 유지한다(AGENTS §6).

#### QA-02 — 자동 검증 실행과 성능 측정 표준화 (P1)

- `Scripts/verify.ps1 -Level quick`이 자동으로 실행되는 곳이 없다. 커밋 전 hook(설치는 사용자 승인 후) 또는 예약 실행으로 돌리고, feature/release는 야간 예약으로 돌린다.
- 성능 gate를 **ABAB 교차 측정 중앙값**으로 표준화해 `verify.ps1`의 별도 단계(예: `-Level perf`)로 고정한다. 오프스크린/표시 방식, 해상도, 장비 수를 manifest에 기록한다.
- **완료:** quick이 자동 실행되어 결과 manifest가 남고, 성능 단계가 같은 입력에서 재현 가능한 판정을 낸다.
- **현재 결과(2026-10-13, Claude 작업): 진행률 70%.** `Scripts/run_perf_abab.ps1` + `verify.ps1 -Level perf` 추가: 두 revision을 별도 archive checkout(`Saved/PerfAbab-<시각>-owned`, 소유 표시 확인 후 정리)에 빌드하고 같은 PERF-01 계측기를 넣어 ABAB×N 실행, revision별 중앙값으로 FPS·engine p95·game thread를 각각 2% 판정, manifest에 revision/DTCore/계측기·맵 hash/GPU·driver/표시 방식을 기록한다. `-SummarizeOnly`로 PERF-01 원본 48회를 재판정해 보고서의 초기 값(GT +15.93/+2.16/+2.77/−4.56%)을 그대로 재현했다. `Scripts/install_verify_automation.ps1`(opt-in: pre-commit quick hook / 야간 feature 예약)은 작업 복사본에서 설치→실행→제거를 확인했다. quick 8단계 통과. **남은 것:** Editor가 모두 닫힌 시점에 `-Level perf` 실제 1회 실행, 사용자 승인 후 hook·야간 예약 설치.

#### FONT-01 — 한글 UI 글꼴 (P2, 선택)

- A안 시안은 Noto Sans KR 기준이었으나 실제 UI는 기존 글꼴이다. 라이선스가 확인된 한글 글꼴(일반·굵게 2종) 에셋을 추가해 소유 패널에만 적용한다. 새 자산이므로 사용자 승인 후 진행한다.

### DTCore 변경 후보 (후속 DTCore 수정 단계 입력)

사용자 예고(2026-10-12): 이번 개편 뒤 **DTCore 수정 단계**와 **센서/카메라 플러그인화**를 진행한다. 지금은 DTCore를 참고·활용만 하고 수정하지 않는다. 대신 작업 중 DTCore로 해결되지 않아 프로젝트 쪽에서 우회한 지점을 아래에 쌓아 두고, DTCore 수정 단계의 출발점으로 쓴다. 항목 추가는 Codex/Claude 모두 가능하며 실제 DTCore 변경은 그 단계에서 별도 승인 후 한다.

| ID | 발견 항목 | 프로젝트 쪽 현재 우회 | DTCore에서 바뀌면 좋은 점 | 근거 |
|---|---|---|---|---|
| DTC-HATCH04 | 소유 Widget 수명 외 해치 단면/FOV 평면·센서별 CPU 표시색 계약 없음 | 기존 UDxWidget/Subsystem과 같은 플레이어 뷰를 유지하고 L3 표시 component/캐시, L1 기본 꺼짐 tint를 사용. 실제 frame/wire/전용 manager 변경 없음 | 여러 host가 필요로 하면 중립 소유 뷰 표시 helper 검토 | `ShipHatchMonitorWidget/Overlay`, `VirtualLidarVisualizationComponent`; DTCore 소스/gitlink 불변 |
| DTC-HATCH03 | Widget 수명 계약 외에 소유 플레이어 뷰의 프레임 교체 동안 임시 표시 보류 계약은 없음 | 기존 센서 점 버퍼를 유지하고 L3가 해당 PC의 자신이 추가한 hidden-component 항목만 관리. 새 프레임 강제 갱신 뒤 해제하며 종료 시 복원. 별도 구독/위젯 관리자 없음 | 여러 host가 필요로 하면 캡처/측정에 영향을 주지 않는 소유 뷰 임시 가림의 중립 수명 helper 검토 | b2504d1 `Core/DxWidgetSubsystem.h`, `ShipHatchMonitorWidget::ApplyPreview/RestorePointVisibility`; DTCore 소스/gitlink 불변 |
| DTC-HATCH02 | Widget 등록/전면/입력 차단 외 플레이어 3D 뷰의 패널 영역 합성 계약은 없음 | 기존 UDxWidget/UDxWidgetSubsystem을 유지하고 해당 World에서만 SceneViewExtension으로 기존 플레이어 뷰 사각형/투영을 조정. 추가 캡처·위젯 관리자 없음 | 여러 소비 프로젝트에 필요해지면 저장/입력 정책과 분리한 중립 뷰 영역 adapter 검토 | b2504d1 `Core/DxWidgetSubsystem.h`, `ShipHatchMonitorHost/Widget`; DTCore 소스/gitlink 불변 |
| DTC-HATCH01 | 정적 원료 높이장/점군 분석 뷰와 자산 없는 분석 화면 구성 계약은 없음 | L2 절차 형상·기존 센서 semantic/반사 응답, L3 `UDxWidget` 등록·입력 차단, 기존 FileSave의 opt-in 분석 JSON을 사용. 전용 위젯 관리자/구독 루프 없음 | 여러 소비 프로젝트에 필요할 때 분석 뷰/명시적 분석 파일 요청의 중립 확장점 검토. 선박/원료 정책은 host 유지 | `ShipHatchSimulation`, `ShipHatchMonitorHost/Widget`, `Core/DxWidgetSubsystem.h`; DTCore 소스/gitlink 불변 |
| DTC-EXPORT01 | 선택적 오프라인 변환 프로세스·완료/취소 계약이 없음 | 기존 L2 Dataset이 파일 확정·원상 복원 후 외부 Python을 실행하고 worker에서 대기·cooperative cancel. 기존 UDxWidget 패널 재사용 | 여러 소비 프로젝트에 실제 필요할 때 opt-in 외부 작업 수명 helper 공용화 검토; dataset schema/변환기는 host 도구 유지 | b2504d1 Source에서 CreateProc/Python/ExternalProcess/DatasetExport 검색0; VirtualDatasetSubsystem::StartExport, DTCore 소스/gitlink 불변 |
| DTC-AUTO01 | DTCore API/WebSocket은 외부 HTTP/STOMP 클라이언트이며 loopback 수신 계약이 없음 | 프로젝트 L2 UE Sockets adapter·게임 스레드 명령 처리·기존 서비스 재사용 | 공통 필요 시 opt-in listener의 수명·인증·GT admission 검토; 업무 command는 host 유지 | b2504d1 Core/DxApiSubsystem.h·DxWebSocketSubsystem.h; DTCore 소스/gitlink 불변 |
| DTC-LIDAR-FID01 | `UMechDriverCompBase`에 획득 시점의 kinematic linear/angular velocity 조회 계약 없음. SetActorTransform 기반 Slab은 일반 GetVelocity만으로 시간차를 복원할 수 없음 | 기존 MechDriver 상속을 유지하고 L0 motion provider를 SlabMotion에서 구현, L1이 immutable sample로 사용 | 여러 소비 프로젝트에 필요할 때 world rigid velocity/pivot의 읽기 전용 중립 interface 검토. Slab 보간·업무 정책은 host 유지 | DTCore b2504d1 `ActorComponent/MechDriverCompBase.h`는 constructor/BeginPlay만 제공; `Contracts/VirtualSensorMotionProvider.h`, `SlabMotionComponent::CaptureSensorMotion`; DTCore 소스/gitlink 불변 |
| DTC-UXR08 | Actor Click에 기존 소유 정보 패널/선택 대상으로 이동하는 중립 활성화 확장점 없음 | 기존 SlabUiRegistry에 weak 활성화 callback을 추가해 현재 Host/UDxWidget 진행 인스턴스만 재바인딩·전면 처리 | WidgetFlag와 분리된 선택/기존 인스턴스 navigation delegate 검토 | AInteractableActor::Click의WidgetFlag 기반OpenWidget, 프로젝트SlabActor::Click/SlabUiRegistry::Activate; DTCore 소스/gitlink 불변 |
| DTC-TESTSET01 | 비실시간 샘플 계획·획득/파일 완료 큐는 DTCore 일반 Widget/DataSync 역할 범위 밖 | 기존 TC/DataSync와 UDxWidget 관리를 유지하고 프로젝트 L2에서 DATASET-01·원본 보관 조립 | 여러 host가 요구할 때만 중립 job 상태 계약 검토 | `Scenario/AgentTestSetSubsystem.*`, `VirtualDatasetSubsystem.*`; DTCore 소스/gitlink 불변 |
| DTC-MAP01 | 업무 WebSocket의 현재 연결/구독 성공 상태를 읽는 공개 조회 API 없음 | 맵 점검은 확인 불가 경고와 로그 확인 방법을 표시하며 OnConnected 이력을 현재 상태로 추정하지 않음. 설정의 TC/Topic은 읽기 전용 조회 | 연결/재연결/구독 readiness의 읽기 전용 snapshot API 검토 | `Core/DxWebSocketSubsystem.h`의 private 상태·protected StompClient, 공개 OnConnected만 존재; DTCore 소스/gitlink 불변 |
| DTC-ARCH05 | TC 구현을 참조하지 않고 binary 수신 결과를 worker에서 검증하는 중립 codec 등록 계약 없음 | 프로젝트 L0 codec registry에 L2 TC 파서를 등록; 기존 DTCore 업무 분배·공유 구독과 Raw binary worker 유지 | DTCore 공용화가 필요할 때 stateless binary codec 등록 계약 검토 | `Contracts/VirtualSensorStreamReceiverRegistry.*`, `WebSocket/TC/VirtualSensorStreamReceiverRegistration.*`; 기존 PCD 파서 사용, DTCore 소스/gitlink 불변 |
| DTC-ARCH06 | 센서 acquisition snapshot용 host provider 계약이 확인되지 않음 | L0 값 context·weak 월드 registry에 L2 SlabContext를 등록; 기존 세션/결과 집계를 adapter로 호출 | 다른 소비 프로젝트에도 필요할 때 중립 값/provider 공용화를 검토; Slab 정책은 host에 유지 | DTCore `b2504d1`의 `AcquisitionContext/FrameContext/SensorContext` 검색 0건; `Contracts/VirtualSensorAcquisitionContext.*`, `Scenario/VirtualSensorSlabAcquisitionProvider.cpp`; DTCore 소스/gitlink 불변 |
| DTC-UXR03 | 기존 위젯 안의 탭/편집 위치로 이동하는 중립 navigation 계약 없음 | 기존 Workspace가 등록된 Data/Replay를 연결하고 native 탭·포커스 adapter로 이동. 기존 `UDxWidget`·입력 차단·DTCore 전면 처리 유지 | 공통 navigation 대상에 선택적 subtarget을 전달하는 확장점 검토 | `Core/DxWidgetSubsystem.h`의 Actor/Widget 열기 API와 `OpenOwnedTab`/`FocusOwnedOutputSettings`; DTCore 소스·gitlink 불변 |
| DTC-UXR10 | 기존 동일 인스턴스의 하위 화면 복귀·native resize/layout 저장 계약 없음 | 기존 소유 Workspace/UDxWidget base에 세션 부모 경로·공통 머리와 모서리 표시만 추가, 기존 v1 크기 저장·DTCore 입력 차단·전면 처리 재사용 | 여러 소비 프로젝트에 필요할 때 동일 인스턴스 subtarget 복귀와 선택적 resize adapter 확장점 검토 | `UI/DxWidget.h`의 WidgetFlag 기반 OpenChildWidget/CloseChildWidget, `Core/DxWidgetSubsystem.h`, UXR-10 집중 회귀; DTCore 소스/gitlink 불변 |
| DTC-UXR09 | `DxWidgetThemeData`는 색상 토큰을 제공하지만 native Slate의 간격·모서리·글자 역할·버튼 단계·dock 표시 계약은 없음 | 기존 `UDxWidget` 패널과 Workspace의 Main/Viewport·입력 차단·전면 표시 경로를 유지하고 명시 소유 native UI의 시각 토큰·표시 사각형만 프로젝트 adapter로 적용 | 공통 필요가 확인되면 색상 외 타이포그래피/간격 토큰과 기존 인스턴스 dock 표시 확장점을 검토. 프로젝트 레일·업무 화면 구성은 host에 유지 | `UI/DxWidgetThemeData.h`, `UI/SensorOwnedPresentationStyle.h`, `VirtualSensorToolWorkspaceSubsystem.Docking.cpp`; DTCore·공용 테마 자산 불변 |

## 1. 성능·측정·전송

### RT-01 — Raw TCP receipt 대기 상한 (구현·집중검증 완료, P0)

**진행률: 90%** · 구현: 완료 · 검증: 상한/receipt 누락 집중시험 및 실제 FIFO 송수신 통과.

- **남은 조건:** 상한·동일 body 재시도·종료 후 반환·UI 보관량 증거를 최종 검수 목록에 연결하고 이 항목의 완료 범위를 닫는다. 복합 장애는 LIFE-01, 구성별 장기 지원은 RT-05에서 관리한다.

**수정 전 근거:** Raw body는 PendingReceipts에 보관됐지만 호환 WaitingReceipts와 UI admission 기준이 분리돼 있었다.

- 구현 `a120ba6`: worker가 접수부터 receipt/최종 실패/종료까지 request reservation을 소유한다. 전역128MiB/스트림64MiB, Camera8/기타20프레임. 같은 body retry는 중복 집계하지 않는다. UI는 worker 실제 보관량을 조회한다.
- 집중 검증 3/3: cap/terminal ledger 및 실제 TCP fake Broker receipt 누락(20 SEND/60 bytes 후 21번째 거부, 종료 후 reservation0). 실제 Artemis PCD/세 스트림 정상 FIFO·checksum·외부 수신도 검사했다. 전체 네트워크 장애 조합과60분 soak는 후속이다.
- 증거: `Saved/Reports/DTCoreSync/raw-fault`, `receipt-probe.json`, `dtcore_after_pcd.json`, `dtcore_after_three.json`. 전역상한은 특정 구성의 지원 보장이 아니다.

### RT-02 — Esc 조작 종료 연결 (구현·자동검증 완료, 수동 검증 대기)

**진행률: 70%** · 구현: 완료 · 검증: lifecycle/Slate 자동시험 및 일부 실제 조작 통과.

- **남은 조건:** 포커스 이탈·선택 변경·종료 등 해상도별 전체 직접 조작 목록을 완료한다. Camera 회전·LiDAR 숫자 변경의 최신 직접 증거는 아래 기록을 따른다.

2026-10-08 추가 직접 검수: 실제1920×1080 client에서 Camera 기즈모 이동·회전으로 월드 Transform 변경과 플레이어 view rotation 불변, 정지 Camera 첫 Esc·Settings 숨김 종료를 확인했다. LiDAR Z1800→1850 입력 후 첫 Esc로4Hz→20Hz 복귀와 PIE 유지도 확인했다. bounded capture1180×771 때문에 하단 일부와 전체 경계 checklist는 여전히 미완료다.

2026-10-03 검수: 소유 패널/PIE viewport에 한정된 Slate 입력 중재기, 첫 Esc·repeat/release, 원설정 복귀 및 Main/외부 Widget 격리 시험을 통과했다. 직접 마우스로 Camera/LiDAR 이동·LiDAR 회전·첫 Esc의 PIE 유지도 확인했다. 숫자 Transform 편집이 전체 측정 설정을 재시작하던 회귀와 빠른 pointer drag 누락을 수정했다. Camera 회전·LiDAR 숫자 변경 등 해상도별 전체 직접 조작과1920×1080 수동 gate는 아직 미완료다. Camera 숨김 복귀의 추가 증거는 아래 최신 기록을 따른다.

추가 회귀 `2eb30f0`: 이전 커서 위치의 global UI hover가 실제 viewport 클릭을 거부했다. 우리 기즈모의 event-position hit path만 사용하도록 수정하고, 이전 패널 hover를 강제로 남긴 시험에서 viewport drag 수락·실제 패널 클릭 거부를 확인했다. 공통 DTCore hover·다른 Widget 정책은 변경하지 않았다.

현재 source의 직접 조작 추가 증거: Camera Z170→180 숫자 변경 후5Hz 경량 조작 유지, Settings 숨김으로 종료 후 원래640×360/10Hz 미리보기 복귀, 실제 client1280×720의150% 제목/버튼 가독성. 전체 두 해상도/모든 경계의 수동 완료로 확대하지 않는다.

**수정 전 근거:** Gizmo Esc가 Actor 종료와 분리되어 있었고 Settings가 종료할 때 현재 선택 Actor를 조회했다. 새 회귀 테스트에서 선택 변경 후 원래 Camera의 640px·0.2초·미리보기 모드가 복원되지 않는 것을 재현했다.

- 구현 `d2024bf`: 종료 요청 이벤트 → Settings 단일 정리 → 시작 때 고정한 약한 Actor 참조. 일반 drag commit과 종료를 분리하고 직접 Gizmo 비활성도 정리한다. 선택 변경은 새 센서 조작을 자동 시작하거나 이전 monitor 선택을 덮어쓰지 않는다.
- 검증: 단위 lifecycle/monitor follow 및 D3D12 PIE runtime 3/3. Camera/LiDAR FullSpec 복원·후속 프레임·숨김·Gizmo EndPlay·반복 종료, 정지 상태·삭제된 대상·Widget 파괴를 검사했다. 동기 측정을 추가하지 않았다.
- 이전 검증 당시 미수행: Computer Use 오류 `0x80070057`/`0x80070005` 때문에 물리 Esc와 실제 viewport 측정이 막혔다. 이후2026-10-03에 첫 Esc·이동 및 실제1280×720 일부 직접 검증을 수행했다. 이 과거 제한을 현재 모든 Esc 검증이 미수행이라는 뜻으로 읽지 않는다.
- 증거: `Saved/Reports/SensorStability/reproduction.log`, `final-rhi/index.json`, `report.ko.md`.

이전 이력 — 2026-10-02: 실제1606×728에서 조작 시작/preview·Settings drag/resize·LiDAR 설정 숨김 후4Hz 복귀를 확인했다. 당시 Esc는 Editor Stop PIE를 실행했다. 이후 수정·검증은 위2026-10-03 기록을 우선한다. 당시 증거 `Saved/Reports/DTCoreSync/manual-camera.png`, `manual-lidar.png`, `gui-final.log`는 실패/제한 이력으로 보존한다.

### RT-03 — 0Hz 최소율/공정성 (구현·자동검증 완료, 화면 확인 대기)

**진행률: 70%** · 구현: 완료 · 검증: 정체·유예·복귀·pause·공정성 자동시험 통과.

- **남은 조건:** 두 센서·두 해상도의 전체 표시/입력 경계 확인을 완료한다. 실제1280×720 LiDAR 정체·pause·재개는 아래 최신 증거로 확인했다.

2026-10-08 직접 정체 시험에서 Scheduler는0Hz였지만 소유 모니터가 마지막20Hz를 표시하는 불일치를 재현했다. `1d17016`은 소유 모니터만 현재 SensorId·종류에 맞는 Scheduler 상태를 표시한다. 실제1280×720의0Hz·정체 경고→20Hz 복귀, PIE pause의일시정지→재개 화면을 확인했다. 준비·경량 조작·정지·정상 복귀 formatter 자동시험도 통과했다. 측정 이력·주기·전송·미등록 Widget은 변경하지 않는다.

**수정 전 근거:** Scheduler min/max가 양수 Hz만 포함해 완전히 멈춘 센서를 숨길 수 있었다. 마지막 acquisition-rate 값도 자동 만료되지 않았다.

- 구현 `59d1027`: World-time 진행 시각, 시작 유예 `max(1초, 실제 주기×3)`, 오래된 양수 Hz→0Hz, 정체 ID 및 공정성 유효성 flag. pause는 측정 없이 진단만 갱신하고 조작용 임시 주기는 정격 공정성 통과에서 제외한다. 센서 주기는 변경하지 않았다.
- 검증: SensorPerformance 5/5, 보고서 판정식 정상/0Hz/센서 누락 3/3. 두 센서 중 하나/모두 정체, 복구·시작/재등록 유예·pause·저주기·조작·실패 이력·빈 그룹을 검사했다. 기존 전체 회귀에서 신규 실패 0.
- 주의: 기존 fairness 수치 0은 판정 불가 sentinel이다. 외부 Blueprint는 추가된 `bCameraFairnessEvaluable`/`bLidarFairnessEvaluable`을 확인해야 한다. Camera는 기존 acquisition 정의이며 GPU 픽셀 완료 검증은 RT-04다.
- 이전 시험 당시에는 직접 정체/일시정지 확인이 미수행이었으며, 이후2026-10-08 실제1280×720 LiDAR 확인을 추가했다. Camera·두 해상도의 전체 경계 조합은 남아 있다. 이전 근거: `Saved/Reports/SensorStability/rt03-focused`, `full-regression`, `report.ko.md`.

### RT-04 — Camera FrameId와 실제 픽셀 (구현·집중검증 완료, P0)

**진행률: 90%** · 구현: 완료 · 검증: 고유 token/CRC990개 완료 JPEG·metadata 및 음성 대조군 통과.

**현재 착수 상태:** Camera 배치·미리보기·공통 자원 변경에 필요한 정합성 회귀를 유지한다. JPEG 송신률/encode 고도화와 신규 파일 형식 개선을 이번 PCD 우선 작업의 선행 조건으로 두지 않는다.

- **남은 조건:** 현재 source의 증거를 Camera 수용 목록과 최종 대조해 항목을 닫는다. 전체 복합 수명 fault는 LIFE-01, 전후 성능·다중 구성은 RT-05/QA-01에 연결한다.

2026-10-03 검수: 완료 이벤트의 immutable JPEG/metadata를 bounded decoder에 전달해 고유 acquisition token+CRC16을 990개 모두 대조했다. 포화·encode 역순·pose/revision/Slab context·Stop/재시작·삭제와 이전 이미지 음성 대조군을 포함한다. 현재 source의 전체 자동화와 독립 host 빌드는 통과했다. LiDAR 측정 시작 UTC 보정 후 성능 비교에서 간헐적인 기준선 실패가 있어 재분석 중이며, 예전 12회 결과를 최종 source 통과로 사용하지 않는다.

**수정 전 근거:** 스트리밍 readback slot 부족 시 metadata를 보관하고 나중의 현재 RenderTarget을 읽을 수 있었다.

- 구현 `017d20f`: 자연 capture hook에서 pose/UTC/context를 고정하고 해당 capture 직후 readback copy를 예약한다. coalesced frame/slot 부족은 명시적 파생 실패이며 나중 픽셀을 옛 ID에 붙이지 않는다. Actor의 프레임 UTC도 encode 완료 시각이 아니라 acquisition 시각을 사용한다.
- 이전 안정화 단계 기록: CameraAcquisitionSnapshot 및 D3D12의 red/blue marker·encode 지연·포화 복귀를 JPEG25개 폴링으로 확인했다. 이 제한된 시험은 위의 최종 고유 token/CRC990개 완료 이벤트 검증과 구분한다. 초기 harness의 ShowOnly/cold shader 문제와 최종 통과 로그를 모두 보존한다.
- 증거: `Saved/Reports/DTCoreSync/camera-unit`, `camera-rhi4`, marker JPEG. 같은 장면의 출력 성능은 10+60초 전후 비교했다. 모든 이동/context/삭제/설정 변경 조합의 stress 인증은 후속이다.

### RT-05 — 다중 센서 실측 지원 범위 (검증 공백, P1)

**진행률: 70%** · 구현: 새 배치 PCD 중심 JSON 수요 정책·센서별 수용 gate · 검증: 기본생성1+1/2+2 PCD60초교차비교, 2+2 PCD10분·30초×10회 정격/정합성/회수 통과. 대표장면·4+4·숨김/운영환경·60분은 남는다.

**현재 검증 순서:** 새 배치 장비의 WhenRequested와 LegacyAlways를 같은 전체 JSON 설정·표시된 PIE·맵/pose에서 비교한다. Camera는 배치·미리보기 부하를 포함하고 이미지 송신은 기본 꺼짐이다. 센서별 실제 속도·request 집합·성능·회수를 모두 판정하며, 특정 센서 누락이나 성능 미달은 합산 수가 충분해도 실패다. 4대는 수요·환경을 정한 후속이다.

- **남은 조건:** 대표 장면·4+4·숨김/운영 환경·60분 지원 매트릭스다. 기본생성WhenRequested의표시형2+2 반복low미달은이번조건에서해결됐으며LegacyAlways전체JSON과offscreen의실패를지우지않는다. 전체출력이나다른PC의무조건적지원으로확대하지않는다.
- 2026-10-08 추가 범위: 실제1920×1080 D3D12·10초 준비+60초, Camera1280×720/30Hz·ML-X Native576×56/20Hz를 유지했다. 2+2 PCD 전용은 평균59.85FPS·1% low51.8·p9516.69ms, 각 PCD 약19.9Hz와 선택하지 않은 Topic0건이다. 2+2 전체 출력은 기존 제출 예산2건/프레임에서 Camera 큐 과부하로 실패했다. 명시적4건/프레임에서는 각 Camera29.883Hz·각 LiDAR/PCD19.917Hz, 평균59.76FPS·1% low51.81·p9517.04ms, 측정 구간 승인 request의 receipt·내부·UI·외부 누락0으로 통과했다. **기본2건 설정의2+2 전체 출력 정격 지원은 선언하지 않는다.** 운영맵·Actor 기본값을 자동 변경하지 않았다.
- 2026-10-08 당시1% low는 가장 느린1% 엔진 프레임 평균의 역수다. 이전p99역수와직접비교하지않는다. 당시보고서는 `Saved/Reports/SensorOutputReliability/FINAL_STATUS.md`이고진행률40%였다. 현재70%의근거는이번기본생성PCD교차/반복/600초검증이며4+4/60분은남는다.
- 이전 장기 근거: Runtime `2eb30f0`의 실제1920×1080 세 스트림10분은 Camera29.998Hz·LiDAR/PCD20Hz, 승인 요청/receipt/내부·외부 수신 대조 오류0으로 통과했다.10회 반복과 일부1280×720 직접 조작도 확인했다. 최신2+2 장기 지원을 완료한 결과는 아니다.

- 이전 근거는 ML-X Native **한 대 PCD-only, 753×403, 30초×2**다. 1,199건 송수신 일치, 후반1% low45.019FPS. 이후 실제1280×720/1920×1080 및1+1 정격 시험은 README의 source별 기록을 따른다. 옛2+2/4+4 저출력률 시험을 현재 정격 지원 근거로 쓰지 않는다.
- 이전 이력 — 2026-10-02: 실제1274×680 PCD-only/1+1 시험과 재생 p9525.07ms 미달을 기록했다. 이후 재생 시험의 scoped background throttle 보정으로 통과한 기록과 현재 source에서 갱신해야 할 opt-in을 README에서 구분한다. 과거 해상도·지연값을 최신 판정으로 사용하지 않는다.
- 1+1 → 2+2 → 4+4를 동일 SHA/고정 fixture에서 검증한다. 8+8은 탐색용 best effort이며 자동 지원으로 분류하지 않는다.
- 각 단계에서 실제 client 해상도, profile/backend/echo, preview/출력 on/off, FPS·1% low·p95/p99, 센서별 acquisition/encode/submit/receipt/내외부 receive, queue/bytes/memory를 기록한다.
- Slab 검증맵의 현재 `one_percent_low`는 가장 느린1% 프레임 시간의 평균으로 계산하며 원시 `engine_frame_ms`를 보존한다. 기존 scheduler/UI의 p99 역수 기반 최근 통계와 구분한다. engine delta·실제 wall pacing·trace의 대기를 합쳐 하나의 FPS로 표시하지 않는다.
- 완료: 60초 회귀를 통과한 구성만 10분 안정성, release 후보는 60분 soak로 확장한다. 요청값을 낮춰놓고 원래 FullSpec 통과로 발표하지 않는다. 목표 수치는 ROADMAP의 제안 gate를 승인·고정한 뒤 적용한다.

### RT-09 — Camera staging texture 재사용 및 지연 원인 분리 (구현 공백, P1)

**진행률: 10%** · 구현: Camera 재사용 미착수 · 검증: 반복 생성 경로와 trace 한계 조사 완료.

**현재 보류:** JPEG 출력 전용 최적화는 진행하지 않는다. 다중 Camera 배치·미리보기 또는 PCD 공통 자원을 실제로 방해하는 원인이 확인될 때 범위를 정해 재검토한다.

- 2026-10-08 자원 진단: readback 생성 횟수, 슬롯·render consume·callback·encode·완료 순서 버퍼와 알려진 CPU 용량을 분리했다. 실제 지연 시험에서 확인한 누적은 staging 누수로 단정하지 않고 완료 순서 창8개로 제한했다(`89bb327`). 픽셀/metadata987개와 포화·복귀·Stop/삭제를 검사했다. readback 재사용은 구현하지 않았으며, 정상 반복의 메모리 증가 인과는 별도 확인한다.

- **남은 조건:** Camera 자원별 지연 인과를 확인하고 성능 실패의 관련 원인으로 확인될 때 재사용 구현·정합성·전후 시험을 진행한다. 현재 최종 검수 범위에서는 조건부 수정이다.

- 2026-10-03 trace에서 game-thread render 대기와 D3D12 texture 생성 지연을 관측했다. LiDAR의 매 프레임 readback 생성은 `0c502af/e6afe4f`에서 재사용·현재 제출 fence 검사로 수정했다. 개별 trace 자원 이름이 없어 모든 생성 지연이 LiDAR라고 단정하지 않는다.
- Camera `QueueScheduledGpuReadback`은 현재도 슬롯을3개로 제한하지만 프레임마다 `FRHIGPUTextureReadback`을 생성하고 완료 후 해제한다. **슬롯 수 제한과 staging GPU 자원 재사용은 다른 기능**이다. Camera 재사용 최적화는 이번에 구현하지 않았다.
- 후속 변경은 동일 크기/format 슬롯 자원 보존, 새로운 copy의 fence 확인, resize/Stop/삭제 시 render-thread 해제, 고유 token/CRC 회귀로 구성한다. 이미지·metadata 정합성이나 출력 주기를 희생하지 않는다.
- 완료: 동일 조건의 전후3회 trace에서 해당 readback allocation 감소를 확인하고, 실제 제출/receipt/수신Hz·지연·FPS·990개 identity·장시간 반환을 재검증한다. 외부 CPU 부하와 엔진 자원 지연은 인과 증거 없이 하나의 원인으로 합치지 않는다.

### RT-06 — 조작 중 입력 지연 (검증 공백, P1)

**진행률: 40%** · 구현: 소유 PIE의 연속/정밀 이동·RMB 관찰자 분리·선택 센서 15/10Hz preview · 검증: 1+1 실제 Slate/PIE와 프레임률 독립성 집중 통과.

후속 새 자세 미리보기 시험: Camera PreviewOnly의 render 완료 fence와 LiDAR 완료 snapshot을 사용해 이전 자세 프레임을 배제했다. 실제 두 해상도의 원규격 복귀는 약31/67ms, viewport GPU end-frame p95는 약34~43ms로 1+1 목표를 통과했다. 전체 회귀210개 수행 통과·skip6·실패0이며, 이 버전의 직접 마우스는0x80070005로 환경상 미완료다. 다중 구성 및 물리 RMB/전체 drag 검수는 계속 남는다.

- **남은 조건:** 2+2/4+4 input-to-visible 계측과30초 연속 조작·최종 프레임 복귀 시험을 수행한다.

- 기본 Camera 640×360/5Hz, LiDAR 120×24/4Hz의 경량 조작 API는 이미 있다. 이는 interaction request 기본값이지 API의 고정 최대치가 아니다. 새로 만드는 작업이 아니라 다중 실행 경계 검증이다.
- 2026-10-08 소유 배치 도구는 기본 Camera 640×360/15Hz·LiDAR 120×24/10Hz를 선택 실행 센서에만 사용한다. 기존 request 기본값·미등록 Widget·정지 센서 정책은 유지했다. 1280×720 및 1920×1080 실제 Slate/PIE의 입력→Transform 1프레임·viewport end-frame GPU p95 약 31~34ms(센서별 각 32개)를 확인했다. 30/60/120FPS 이동·회전과 hitch/대각선/Shift/Ctrl, 소유 입력/외부 입력창/정밀 반복 차단을 검사했다. 두 해상도 직접 조작에서 정밀 이동·Undo/Redo·Esc와 북마크 재실행 보존을 확인했으나, 물리 RMB 연속 탐색·전체 drag·1.5초 최종 프레임 복귀 수용시험은 남아 있다. 물리 입력·실제 모니터 및 다중 정격 지원 수치로 확대하지 않는다.
- 입력 timestamp→Transform 적용→화면 표시를 측정한다. 평균 FPS만으로 조작감을 평가하지 않는다. 기존 world/local, Shift/Ctrl, 최종 품질 복원 유지.
- 완료: 2+2/4+4에서 30초 연속 drag/rotate, 키보드, 선택 전환, Esc/hide/end를 실행. 제안 목표는 input-to-visible p95 ≤50ms, 종료 후 coherent 최종 프레임 ≤1.5초. 불가능한 구성은 지원 밖/제한 원인을 공개한다.

### RT-08 — 작업량 기반 admission (신규 기능, P1)

**진행률: 40%** · 구현: 읽기 전용 PCD 사전 점검·예산 안내 · 검증: 40/140건/s·wildcard 중복·미리보기 Camera 제외·원규격/부작용 불변 집중 시험. 동적 작업량 기반 admission은 미착수다.

**1차 범위 — PCD 사전 점검:** 선택된 LiDAR 수·요청 주기·실제 body 크기 추정·송신 예산·연결 준비를 시작 전에 보여준다. 미리보기 전용 Camera를 JPEG 송신량에 포함하지 않는다. 현재2+2 전체 출력의140건/s과 기본 제출 예산2건×60FPS=120건/s 사례는 원인 설명용이며 PCD-only의40건/s도 부족하다고 일반화하지 않는다. 관측 FPS에 따른 계산은 추정임을 표시하고 실제 수신률로 확인한다. 한도 부족 시 이유·대상·명시적 설정 방법을 안내하며 자동 주기 저하·숨은 설정 변경을 하지 않는다. 전체 동적 작업 배분 정책은 이후 구현한다.

- **UXR-03 UI:** 전송 탭의 점검 대상 선택·한 줄 상태/해결 링크·접힌 상세로 이동했다. 같은 검사/예산 판단을 공유하고 동적 admission·기본값은 변경하지 않았다. 직접 해결 버튼 검수와 열린 데이터 화면 성능은 보류다.

- **남은 조건:** 작업량·EWMA·자원/대역폭 예산 기반 접수와 공정 배분을 구현하고 혼합 부하를 검증한다.

- 현재 FPS tier는 센서 **개수** 중심이다. 같은 4대라도 해상도·rays·echo·GPU/CPU·JPEG·PCD·semantic proxy 비용은 다르다.
- pixels/s, rays/s, 측정/파생 처리 EWMA, GPU/readback/encode 슬롯, queued bytes·네트워크 예산으로 사전 capacity 표시와 공정 배분을 보완한다.
- Requested/Admitted/Acquired/Published Hz를 분리한다. preview 품질과 물리 측정 규격도 분리하며 사용자 모르게 resolution/주기를 낮추지 않는다.
- 완료: 동일 센서 수의 경량/고부하 혼합에서 starvation 0, bounded 자원, 경고와 실측 결과 일치. 추가 센서가 기존 센서의 완료율을 독점하지 않음.

## 2. 사용자 workflow·데이터 신뢰성

### UX-01 — 센서 추가·배치와 저장 위치 전용 패널 (현재 최우선, P1)

**진행률: 70%** · 구현: 개별 북마크·Undo/Redo에 장비 수명·전체 배치 저장/복원·전용 패널 추가 · 검증: lifecycle/저장/보호 자동화·WBP7개, 직접 두 해상도의 핵심 저장/복원 및2+2 PCD10분/10회. 첫 사용자·전체직접목록은 남는다.

- UXR-07 현재 직접 점검에서 Camera/LiDAR 추가·10cm/5°·Esc·북마크 이동/이름 변경/덮어쓰기/취소·Undo/Redo·다음 PIE 동일 ID/자세/북마크 연결을 확인했다. 최종 삭제·복제 전수·자유 배치 드래그와 첫 사용자 수용은 남아 진행률70%를 유지한다.

- **남은 조건:** 첫 사용자 발견성, UI 직접 복제/삭제·모든 배율과 입력 경계, Editor 영구 설치를 검증한다. 현재 PIE의 추가→선택→북마크/전체 배치 복원→PCD/결과 연결은 구현·핵심 검증됐다.

- 현재 native/WBP `센서 배치` 패널이 장비 목록과 저장 위치를 함께 제공하고 툴바에서 한 번에 열린다. 기존 Settings의 위치 관련 API는 같은 구현을 재사용한다. 최초 사용자 발견성 시험은 아직 별도다.
- 패널의 기본 화면: `LiDAR 추가`·`Camera 추가`, 장비 목록(종류·이름·SensorId·측정/정지·수집 잠금), 선택 장비의 위치·회전/조작, `현재 위치 저장`과 저장 위치 목록이다. 이름 입력·저장 상태·`이동` 버튼을 고정된 위치에 두고 속도·단위·고급 설정 뒤에 숨기지 않는다. 장비 규격은 Settings로 연결한다.
- 기존 Actor 클래스/API·Coordinator 등록·공유 서비스 연결을 재사용하는 생성 서비스를 프로젝트에 추가한다. UI가 Scan/Capture 필드를 직접 수정하지 않는다. 고유 ID 생성과 중복 검사를 하고, 생성 위치·방향은 명시적으로 선택·미리 확인하게 한다. 배치/미리보기 상태와 PCD 수집 상태를 구분하며 추가만으로 Topic 송신을 시작하지 않는다.
- 1차는 PIE/runtime 추가·복제·우리 도구가 생성한 장비 삭제다. 기존 레벨 Actor는 출처를 표시하고 삭제 대상으로 자동 편입하지 않는다. 대상 센서의 수집/준비/drain/미완료 출력 중에는 추가된 장비라도 이동·삭제를 거절하고 이유를 표시한다. 새 장비가 진행 중 세션의 고정 대상에 자동 합류하지 않게 한다.
- 저장은 두 종류를 명확히 구분한다. **저장 위치**는 선택 센서 하나의 pose(기존20개)다. **전체 배치 저장**은 별도 버전 JSON에 장비 종류·stable ID·프로필·pose·측정 설정·미리보기 상태를 보관하고 명시적으로 복원한다. 송신 활성 상태·인증·프레임 본문은 제외한다. 기존 pose 파일만으로 새 Actor가 생성되지는 않는다. 전체 배치는 충돌/잠금/맵/정확한 대응을 먼저 검사하고 변경 미리보기 이후 적용하며 운영맵을 묵시적으로 수정하지 않는다.
- 기존 북마크·Undo/Redo 서비스와 저장 파일을 재사용한다. 새 패널은 별도 소유 Role을 추가 등록하고 기존 enum 값·저장 키·Blueprint 전달 API를 보존한다. Settings에는 배치 패널로 이동하는 안내를 두어 중복 편집 화면을 줄인다. 사용자 WBP Designer와 미등록 Widget을 일괄 변경하지 않는다.
- 첫 사용자 수용시험: 기본 Slab 검증맵에서 도움 없이 LiDAR/Camera 추가, 목록/3D 대상 일치, pose 저장·즉시 이동·Undo, 잠금 사유 확인을 수행한다. 재실행 후 북마크 조회는 우선 stable ID가 유지되는 기존 레벨 센서로 검사한다. 새 런타임 장비의 재생성/북마크 연결은 전체 배치 저장과 함께 명시적 ID 복원·중복 검사로 검증하며 이름으로 추측하지 않는다. 운영맵에 검증 Rig를 옮기거나 기존 맵 배치를 자동 덮어쓰지 않는다.

- PIE runtime 생성·복제·소유 삭제·개별 위치·전체 배치 저장/복원은 연결했다. Editor 영구 설치 도구나 모든 장비 수의 정격 지원으로 확대하지 않는다.
- 2026-10-08 개별 센서의 이름 있는 20개 위치·회전과 현재 PIE의 50개 gesture 이력을 추가했다. 별도 `Saved/SensorPlacements` 파일이며 목록 수동 이동·쓰기 실패 보존·ID/맵 검증과 잠금을 적용한다. 기존 설치 Actor를 새로 만들거나 맵을 저장하지 않는다. 같은 30초 시나리오의 배치 A/B는 각 600/599개 외부 PCD의 좌표 행렬·RunUUID별 승인/receipt/수신 ID를 대조했고 시작 snapshot과 북마크 정보를 실행 요약에 보존한다. 전체 배치 세트·생성/복제/삭제·Editor Undo는 남는다.
- Editor 설치와 PIE 임시 설치를 명확히 구분한다. 고유 ID 생성/중복 검사, 이름/종류별 목록, transform 입력·snap·복제, 배치 preset 저장/불러오기, Editor Undo/Redo를 제공한다.
- Actor API와 setup descriptor를 사용하고 측정 Component 필드 직접 변경은 금지한다. Editor 배치의 영구 저장은 명시적 동작으로만 수행한다.
- 완료: 10개 생성·복제·이름 변경·삭제·Undo/Redo·재로드, 소유하지 않은 Actor 불변, duplicate ID 0, 송신/비동기 작업 중 삭제 안전. 센서 없는 레벨도 빈 상태 안내로 시작.

### DATA-01 — 관찰 가능한 실행과 데이터 필수 실행 (구현·집중검증, P1)

**진행률: 90%** · 구현: 선택형 준비 gate·오류 중단·RunUUID별 receipt 집계/요약 저장 · 검증: 실제 PCD/세 출력, TCP 장애6종, 최종 자동화198 Success(실제188·skip10), WBP6개 및 직접1286×760.

- **남은 조건:** Engine STOMP/WSS 필수 모드의 별도 실사용 시험, 회사 운영 환경과 장시간 수집 범위를 검수한다. Broker 수락을 외부 소비자의 저장/처리 완료로 세지 않는다. `Saved/Reports/ScenarioDataReliability/FINAL_STATUS.md`의 수행/미수행을 따른다.

- **UXR-03 UI:** 신규/재생 `연결 확인 후 시작`을 전송 탭에 모으고 재생 패널은 요약만 표시한다. 결과·진단을 나눴으며 RequireData/관찰 fallback·A/B·실제 저장 재시도 계약은 유지한다. CLI 자동화 통과는 Engine/WSS·회사 운영 수용을 대신하지 않는다.

- 현재 live 설정 부족은 관찰 fallback으로 이동한다. 좋은 개발 편의지만 학습 데이터 수집 작업에서는 “움직였으니 데이터도 수집됐다”는 오해를 막아야 한다.
- 기본 fallback을 유지하고 opt-in **데이터 필수 실행**을 구현했다. preflight→실제 STOMP 연결 준비→실행→최대10초 drain→실행별 결과를 구분한다. worker 생성만으로 준비 완료라 하지 않는다.
- 필수 실행의 연결 끊김·최종 receipt 실패·과부하·대상 삭제/정체에서 움직임과 신규 접수를 중단한다. 재실행은 새 RunUUID이며 자동 fallback/중간 재개는 하지 않는다.
- 실행 요약은 원본 시나리오/실행 UUID·선택 센서·설정 revision·승인/제출/receipt/수신 검증·실패/미완료를 기록한다. 메모리20개·소유 JSON100개, 실행 중 식별자65,536건 상한이다. 준비 실패·취소·중복/재시도 집계·보관·초기 복귀 후 결과 보존을 검사한다.

### RT-07 — 소비자 업무 ACK (신규 기능, P2)

**진행률: 0%** · 구현: 업무 ACK 계약 미착수 · 검증: 전용 수용시험 없음.

- **남은 조건:** ConsumerProcessed·멱등 처리·재전송·미확인 결과와 보존 정책을 버전 계약으로 정의하고 구현한다.

- 현재 drain의 Raw 대기는 Broker receipt 중심이다. 자체 parser의 성공도 최종 AI/업무 소비자의 저장·처리 완료는 아니다.
- 필요 시 `(RunUUID, Segment, SensorId, SensorFrameId, checksum)` 기반 application ACK/누락 대조/멱등 재전송을 별도 버전 계약으로 추가한다. 기존 PCD body를 묵시적으로 변경하지 않는다.
- 완료: 소비자가 중단되어도 BrokerAccepted와 ConsumerProcessed를 혼동하지 않음. 중복 메시지 재처리 방지, 종료 결과에 미확인 목록, timeout/재시도/보존 정책이 명시됨.

### LIFE-01 — 중도 종료와 재연결 (검증 공백, P1)

**진행률: 70%** · 구현: 같은 worker의 실행 소유권/회수·종료 전 식별자 대조·확정 결과/저장 재시도 · 검증: 집중 자동화와 동일 연결의 30초×10회 PCD/세 출력 기능·요청 ID 대조 통과.

- `be2b418`은 시험용 실제 수신 ID 집합을 실행별 대조·저장 후 회수하고, 기록 상한 초과를 명시적 실패로 처리한다. 요청 시 자원별 snapshot과 종료 후 weak owner·Camera callback/encode 회수를 검사한다. `89bb327`에서 지연된 앞 JPEG 때문에 완료물이 계속 쌓이는 경로를 실제 재현하여8개 접수 창으로 제한하고 실패한 앞 항목의 후속 결과를 해제했다. `cd28373`의 SceneCapture 격리도 실제 Camera/GPU 깊이 양·음성 대조군으로 검사했다. 전체 프로세스 메모리 누수 검수 완료로 확대하지 않는다.
- 최신30회 근거: PCD17,989/세 출력62,979 request ID집합의 제출·receipt·내외부 수신 대조, 동일 worker/connection,20/21/25/30회 결과20개·body/events 반환 및 Stop/PIE weak owner12개를 확인했다. 이번 최소FPS59.82/59.94,low47.80/55.32,p95≤16.67ms, 변경 전후 각3회5% 이내. 실행1→30 private memory 증가15/44MiB의 전체 owner는 미확정이므로 진행률70%와 남은 인과/fault 범위를 유지한다. 증거는 `Saved/Reports/SensorResourceLifecycle/FINAL_STATUS.md`.

- **남은 조건:** 전체 파이프라인 fault matrix와 thread/handle·잔존 객체·메모리 복귀, 장시간/Engine STOMP/WSS를 조사한다. 과거 private bytes 약32MB 증가를 이번 실행 상태 회수만으로 해결했다고 표시하지 않는다. 반복 후반 성능과 실제 마우스 검수는 `Saved/Reports/SlabRepeatRunStability/FINAL_STATUS.md`의 최종 판정을 따른다.
- `b9b4369/73c71e1/a0f24ba`: 실행 상세 진단과 수신 연속성 상태를 회수하고 종료 집계20개를 유지한다. 늦은 프레임을 막는 최소 UUID/generation 기록은 별도다. PIE 즉시 종료도 정리 전 단계별 대기와 고유 미완료 수를 보존한다. 결과 목록 선택·요약 저장 재시도는 현재 PIE20개에 한정한다.
- 실제 1920×1080에서 변경 전후 PCD/세 스트림 각3회는 FPS·p95 변화5% 이내였지만, 동일 연결10회 반복 후반에 별도의 p95/1% low 실패를 재현했다. trace에서 CPU 미리보기 ISM 갱신 비용 증가를 확인해 `21f7b32`에서 진단 인스턴스의 collision/physics/navigation을 제외했다. 점 수·측정 주기는 유지하며 전후 반복 결과를 별도로 판정한다.
- 최종 physics 제외 소스의1920×1080 동일 연결30초×10회는 PCD/세 출력 모두 성능·identity·회수 통과. 외부 PCD6,000, 세 출력 Camera8,999/LiDAR6,000/PCD6,000; 종료 bytes/sequence0. 최종 전후3회 비교도5% 이내. fault6종×2와 실제198개 자동화 수행, WBP6개 및 직접1174×683 결과 조회/재시도/중단/복귀를 확인했다. Source·보고서별 원본 실패와 opt-in skip5를 보존한다. 모든 pipeline·회사 환경·장시간 또는 목표 두 크기의 전체 mouse gate 완료로 확대하지 않는다.

- weak pointer/revision·세션 drain·file writer 취소는 이미 있으므로 전체 재작성하지 않는다.
- acquisition/readback/encode/직렬화/socket/receipt/파일 저장 각 단계에서 Actor 삭제, Stop, 품질 변경, PIE 종료, world 이동, 동일 시나리오 재실행을 주입한다.
- 완료: 늦은 결과 적용·double complete·old generation 수신 0, 종료 시간 상한, thread/handle/메모리 복귀, 다른 owner 연결 유지. 각 경로 테스트가 어느 수명 경계를 증명하는지 기록한다.

### IO-01 — LAZ 프로세스 제한 (알려진 제한, P1)

**진행률: 10%** · 구현: timeout/cancel 보강 미착수 · 검증: 외부 실행 공백 조사 완료.

**현재 보류:** PCD 중심 정책에 따라 일반 LAZ 및 다른 확장자 개선을 현재 착수 목록에서 제외한다. 사용자 2026-10-15의 DATASET-01 opt-in 데이터셋 예외만 별도로 허용한다. 아래 제한과 이력은 보존하며 기존 LAZ가 개선 완료된 것으로 표시하지 않는다.

- **남은 조건:** timeout·cancel·hang 회수·부분 파일 정리와 fake compressor 시험을 구현한다.

- 외부 compressor 미설정 시 성공을 가장하지 않으며 실제 timeout/cancel 및 장시간 hang 정리는 아직 보완 대상이다.
- 자신이 시작한 자식 프로세스만 추적해 timeout/cancel, 제한된 stdout/stderr, 임시 파일 cleanup, 오류 코드를 제공한다. 무관한 프로세스를 이름으로 종료하지 않는다.
- 완료: fake compressor 성공/오류/hang/취소 시험, worker slot 복귀, 부분 파일을 완료로 표시하지 않음. CSV/PCD/JPEG 작업이 함께 굶지 않음.

## 3. UI·가독성

### UI-03 — 작업 흐름 중심의 소유 UI/UX 재구성 (신규 기능, P1)

**진행률: 70%** · 구현: 사이드바/카드와 배치→미리보기→시나리오→PCD→결과 진입점 · 검증: 소유권·저장 호환 자동화와 UXR-07 주요 직접 workflow. 자유 배치 입력·상대 성능·전체 수용/사용자 확인이 남는다. **남은 수용은 UXR-07**(0-U절)로 관리하며 전체 완료로 올리지 않는다.

- **남은 조건:** 새 배치 진입점과 기존 소유6개 패널을 `배치 → 미리보기 → 시나리오 → PCD 전송 → 결과` 흐름으로 정돈한다. UI-01의 DPI/격리 검증과 구분해 관리한다.
- 툴바에 `배치` 직접 진입점을 제공하고 현재 선택 장비를 분명히 표시한다. 목록 선택·3D 강조·설정·미리보기의 대상이 항상 일치해야 한다. 장비가 없거나 Camera가 정지됐을 때 추가/미리보기 시작 방법을 안내한다.
- 센서 배치 패널은 UX-01, Settings는 장비 규격, Monitor는 큰 미리보기, Data는 PCD 준비/송신/결과, Slab 패널은 시나리오·차트·진행을 담당한다. 반복된 조작과 전문 진단은 정리하고 주 동작·선택 대상·잠금 이유는 즉시 보이게 한다.
- `위치 저장`, `전체 배치 저장`, `PCD 파일 저장`, `PCD 송신`, `창 배치 저장`을 같은 '저장'으로 섞지 않는다. 저장 요청 접수/완료/실패와 Broker 수락/실제 수신을 분명히 표시한다. CSV/LAZ/JPEG 등 기존 선택 기능은 고급/호환 영역으로 두고 신규 포맷 개선은 추가하지 않는다.
- 기존 다크 스타일을 바탕으로 글자 역할·대비·여백·버튼 위치·상태 표시를 통일한다. 기본 동작은 스크롤 깊은 곳에 숨기지 않고 상세 내용만 접기/스크롤을 사용한다. 위험 조작은 인접한 실행 버튼과 구분한다. 키보드 focus·한글 줄바꿈·큰 글꼴·빈 목록·실패 복구도 설계에 포함한다.
- 소유 등록 인스턴스에만 적용한다. 새 배치 패널 추가 시 전용 Role/저장/입력/폰트 등록을 검증한다. 기존 소유6개 패널은 화면을 개편하되 저장값·동작·API 호환을 유지하고, 동료·미등록 Widget의 배치·스타일·저장값은 보존한다. UI 변경으로 측정·전송을 추가 실행하지 않는다.
- 완료: 1280×720·1920×1080에서 처음 사용하는 사람이 추가→배치→위치 저장/불러오기→PCD 실행→결과 확인을 수행한다. 도움 없이 찾을 수 있는지, 조작 횟수·되돌아가기·오류 복구를 기록한다. 숨김/resize/접기/네 글자 배율과 미등록 Widget 불변도 확인한다.

### UI-01 — 실제 해상도/DPI/소유권 (검증 공백, P1)

**진행률: 40%** · 구현: 소유 패널 격리·배율·창 조작 존재 · 검증: 일부 실제1280×720 및 Main/외부 Widget 자동시험 통과.

- UXR-07에서 실제1280×720·1920×1080×글자100/125/150%의 주요 화면을 직접 확인했다(window DPI1, UMG DPI0.666/1). OS DPI를 바꾼 시험이나 모든 창/외부 Widget의 전수 확인은 아니다. 자유 배치 제목 드래그 실패와 전체 조합이 남아40%를 유지한다. 자동 입력 RHI 통과가 이 마우스 실패를 무효화하지 않는다.

- **남은 조건:** 1920×1080 전체 직접 조작·DPI 대표 조합·외부 Widget 입력·전체 checklist를 완료한다.
- 이전 직접 근거: 실제client1280×720,150% 제목/버튼 가독성, 숫자 Transform·Settings 숨김 복귀를 확인했다. 당시 일반 PIE의 최대client는1920×998였다. 이후의1920×1080 직접 검수와 아래 최신 기록을 함께 읽는다.
- 2026-10-08 직접 client1280×720에 이어 세션 복구 후1920×1080에서 Camera gizmo 이동·회전·정지 첫Esc·Settings 숨김,LiDAR 숫자Z1800→1850·4Hz→20Hz 복귀,데이터 패널 열기·85/100/125/150%·resize/scroll/collapse를 확인했다.125/150%에서 좁은 Settings의 SensorId가 잘렸지만 resize 후 전체가 표시됐다.1280×720의 정체/pause/복귀는 수정 후 실제 확인했다. bounded capture1180×771로 전체1920 checklist·직접 준비 취소·Main/외부 Widget 입력 전수는 남긴다. 앞선0x80070005 실패는 원본 증거로 보존한다.
- 이전 이력: 988×521/1174×683은 과거 수동 시험 크기다. 실제 동료 프로젝트는 미제공이다.
- 두 목표 client 크기, 100/125/150% OS DPI 및 네 폰트 배율의 대표 조합을 검사한다. 창 경계/메뉴 overflow/입력 clipping/접기 후 hit-test/Tab focus/숨김 중 작업/재호스팅을 확인한다.
- 완료: 실제 크기 증거, 기존 저장값 roundtrip, 미등록/중첩 Widget의 font·geometry·입력·파일 hash 불변. 확보 못한 조합은 명시적으로 미완료.

### UI-02 — 3D 라벨 배치 (신규 기능, P1)

**진행률: 80% — 구현·집중 자동/렌더 검사 완료, 통합 검증·수용 미완료** · 2026-10-13 P1. `142a6ff`에서 기본 소재번호·진행/기울기 2개, 카드·표시 설정의 세션 전용 "3D 상세 라벨", 거리 크기/대비 페이드·시야 밖 숨김, 우선순위/leader line·실제 소유 UI 영역 회피를 구현했다. 기존 표시 선택·저장 schema·숫자/단위·L/R Track 의미·측정 제외 유지. 기존 DTCore StatusVisualizer 기반 L2 utility에 L3가 실제 viewport-pixel UI 사각형을 전달하며 역방향 include 없음. 연속 표시 5Hz, 숨긴 상세 문자열 조립 중단·값 hash 재사용이다.

**검증:** Editor Development, layer/allowlist0→0, guard, quick8, UI02·SlabAnalysis·UXR08 집중 **18 Success/실패·skip0**(실제 Camera/GPU 측정 격리 포함). 1280×720·1920×1080/font100의 근접/원거리/상세/직교 roll/고온 before·after 각12장, after 실제 투영 라벨의 viewport/UI/라벨끼리 겹침 검사 통과. 근거 `Saved/Reports/UXR/UI-02/RESULT.md`·`summary.json`·`after/`·`focused/`.

**남은 검증:** 마지막 release 호출은 시작8 gate 뒤 별도 QA checkout의 Editor PID87208(`UXR07.Manual`) 때문에 종료 조건에서 중단했다. full D3D12/이름별 비교/WBP release와 표시형1+1/10초+30초 ABAB×3 성능은 미수행이다. 사용자 요청대로 중단했으며 다른 세션을 종료하지 않았다. 결과 정리 중 상세 침범 문자열의 UTF8 오류를 발견해 소스 문구와 렌더 의미 검사를 정정했으나, 이 마지막 2파일은 미커밋·빌드/렌더 대기다. 기존 `142a6ff` 렌더를 정정 검증으로 세지 않는다. QA 종료 뒤 이 정정 검증→release→성능을 이어간다. Computer Use는 이번 요청에서 제외하여 `manual/CHECKLIST.md` 전 행 보류, font125/150·OS DPI·직접 수용도 미완료다.

- **남은 조건:** QA Editor 정상 종료 후 마지막 문구 정정 빌드/렌더, 통합 release·이름별 비교, 표시형 ABAB×3의 2% 성능 gate와 후속 직접 수용을 완료한다.

- 현재 큰 문자 배경판이나 도구 창이 다른 라벨을 가릴 수 있다. 선/글자 크기, 대비, 한글 표시 자체는 구현·수동 확인했다.
- 시야 밖/뒤쪽 점을 숨기거나 안전 위치로 안내하고, 라벨별 priority·최대 겹침·leader line/거리 fading을 추가할 수 있다. 숫자/단위/좌우 의미는 유지한다.
- 완료: 원근/직교, 근접/원거리, roll, 태양광/고온 Slab 조건의 읽기 시험. 진단이 Camera/LiDAR를 오염시키지 않고 5Hz 표시 budget 유지. 전역 occlusion/visibility를 바꾸지 않는다.

## 4. 플러그인·프로필·운영

### PL-01 — host 의존성 역전 (신규 기능, P1)

**진행률: 0%** · 구현: 중립 경계 전환 미착수 · 검증: 무Slab/adapter fixture 게이트 없음. 프로젝트 안 선행 부분은 ARCH-04·05·06이 담당한다. ARCH 완료 후 남은 범위만 이 항목으로 진행한다.

- **남은 조건:** 중립 provider/observer·출력 정책·UI extension으로 Slab/host 직접 의존을 분리한다.

- 현재 센서 frame/request가 `FVirtualSlabFrameContext`를 포함하며 Camera/Scan이 SlabContext Subsystem을 직접 호출한다. 데이터 UI는 SlabActor를, Workspace는 Slab panel/Host를 직접 안다.
- 중립적 frame context provider/completion observer, output-session policy, host UI extension으로 역전한다. Slab codec/업무 원본/차트/재생은 host adapter에 남긴다.
- 완료: 센서 Runtime이 host 모듈을 include/dependency하지 않는 static/build gate. Slab 없는 fixture + Slab adapter fixture에서 기존 wire bytes/metadata 의미 회귀 통과. provider UObject를 worker에서 직접 읽지 않음.

### PL-02 — 기본값·설정 namespace (신규 기능, P1)

**진행률: 0%** · 구현: 제안 중앙 설정/bridge 미착수 · 검증: 전용 이식 게이트 없음. 상수 모음(1단계)과 편집 타입 이동은 ARCH-03이 담당한다.

- **남은 조건:** 기본값·자산·Topic·저장 namespace와 Runtime/UI 경계를 중앙화하고 기존 값을 보존한다.

- `UVirtualSensorIntegrationSettings`와 `UVirtualSensorDtCoreBridgeSubsystem`은 현재 소스에 없다. 경로·맵 저장 대상·슬롯·Topic이 여전히 여러 곳에 있다.
- soft references/설정 객체를 도입하되 명시적 Actor/Transport 설정을 우선하고 없을 때만 기본값을 사용한다. 새 값으로 기존 Map/WBP를 자동 재저장하지 않는다.
- Frame/편집 상태 타입은 Runtime public 계약에 두고 UI→Runtime 단방향을 만든다. 저장 namespace는 plugin/rig/user 기준으로 설계하되 기존 v6/Workspace v1은 opt-in migration으로 보존한다.
- Coordinator의 Monitor 타입/직접 bind 호출도 Runtime delegate/interface와 UI adapter로 분리한다. 편집 타입 이동만으로 Runtime→UI 역의존이 해소되지는 않는다.
- 완료: host 이름 일괄 치환 없이 자산/저장/Topic 변경 가능, 없는 자산의 명확한 진단, 다른 rig/Widget 저장값 불변, Editor 저장 allowlist 유지.

### PL-03 — 장비 registry (신규 기능, P1)

**진행률: 0%** · 구현: ProfileId registry 미착수 · 검증: switch 수정 없는 신규 등록 시험 없음.

- **남은 조건:** DataAsset/registry 등록·선택·capability·기존 enum 호환성을 구현한다.

- 현재 장비 enum·resolver는 존재한다. 프로필을 또 다른 switch에 계속 추가하는 방식 대신 안정적인 ProfileId+DataAsset/registry를 제안한다.
- 물리 규격·quality preset·capability·calibration provenance와 acquisition backend/실장비 SDK adapter를 구분한다. 프로필 데이터 추가만으로 SDK가 구현되는 것은 아니다.
- 완료: 테스트 장비 하나를 핵심 switch 수정 없이 등록/선택/검증. 기존 enum→built-in ProfileId 호환, 원본 ML-X/Integration200 구분, 단일 설정 transaction·한 번의 재예약 유지.

### PL-04 — 한 클래스 진입점·배포 (신규 기능, P1)

**진행률: 0%** · 구현: 센서 플러그인/단일 설치 진입점 미착수 · 검증: 빈 소비 프로젝트 설치 게이트 없음.

- **남은 조건:** 센서 플러그인·Bootstrap을 구성하고 빈 프로젝트·다른 GameMode/Main·패키징을 검증한다. DTCore 최소 host 빌드와 이 항목의 센서 설치 시험은 다르다.

- Bootstrap façade가 기존 Coordinator/UI Host/선택 SourceHost를 조합한다. 기존 인스턴스를 우선 사용하고 자신이 만든 것만 정리한다. 새 거대 manager로 통합하지 않는다.
- host GameMode/GameInstance/Main를 강제 교체하지 않는다. 기본은 로컬 preview, 외부 송신은 명시적 설정 또는 host 시나리오 정책이다.
- 완료: 빈 UE5.3 프로젝트에 DTCore+센서 plugin만 넣고 Bootstrap 하나로 preview/UI 확인. 다른 GameMode/Main 유무, 지연 PlayerController, 다중 PIE, packaged Development, cook/asset migration 통과.

### SEC-01 — 보안/연결 운영 (검증 공백·신규 기능, P2)

**진행률: 10%** · 구현: 배포별 보안 보강 미착수 · 검증: 평문/전역 설정 등 정적 위험 조사 완료.

- **남은 조건:** 배포별 인증·권한·TLS·secret·연결 상태 및 프로젝트 전역 설정을 검증한다.

- Raw TCP는 평문이다. wss 호환 경로·HTTP·자체 진단·실장비 입력은 동일 기능이 아니며 연결을 임의 통합하면 안 된다.
- Broker 인증/권한·주소 유형·message limit·receipt timeout·재연결·TLS 성능을 배포별 설정으로 관리한다. 개발 계정/endpoint를 release 기본값으로 복사하지 않는다.
- 현재 조기 Bootstrap의 WebSocket 1ms 설정은 프로세스 전체 영향이 있다. 레벨 BeginPlay로 옮기면 초기화 시점이 늦다. host 정책으로 유지하거나 명시적 opt-in 조기 설정으로 별도 검증한다.
- 완료: 암묵적 전역 설정/다른 Subscription 종료 0, secret 미저장, wss가 평문으로 바뀌지 않음, 오류/인증/연결 상태가 UI에 정확히 표시됨.

### HW-01 — 실장비 충실도 (검증 공백, P2)

**진행률: 0%** · 구현: 실장비 근거에 맞춘 보정/adapter 미착수 · 검증: golden dataset 없음.

- **남은 조건:** SDK·실측 캡처·캘리브레이션을 확보하고 센서 관측/패킷을 대조한다. 공개 사양 프로필은 실장비 동일성 검증 진척으로 세지 않는다.

- ML-X Native는 공개 사양 기반, ROS2/Livox/RealSense는 stub/확장 지점이다. 실제 장비 동일성을 주장할 자료가 없다.
- SDK/패킷·calibration·raw capture golden dataset을 확보한 뒤 좌표·거리·intensity·echo·timestamp·miss를 대조한다. HW Ray Tracing/GPU physical 모델은 capability별 별도 승인 작업이다.
- 완료: profile마다 구현/미구현·단위·오차·지원 backend·근거 dataset을 공개. synthetic truth와 보정된 센서 관측, Digital Twin semantic을 구분한다.

## 5. QA-01 — 재현 가능한 기준선 (검증 공백, P0)

**진행률: 70%** · 구현: 의존성 고정·격리 host/맵 fixture·검수 도구 일부 완료 · 검증: 현재 빌드·패키징·자산·회귀 통과, 최종 비교 미완료.

- **남은 조건:** 실행 manifest·원시 프레임/새1% low 계측·요약 자동 대조·최종 기준선 비교를 완성한다.
- 현재 상태: 프로젝트 `2ec67a2`는 최소 수정 DTCore `b2504d1`을 고정한다. `067195b`는 이전 동기화 안정화 이력이다. 현재 프로젝트와 별도 plugin 복사본의 최소 host 검증은 실제 다른 소비 프로젝트의 호환성을 대신 보장하지 않는다. 현재 작업에서는 DTCore를 수정하지 않는다.
- 그럼에도 개인 Game.ini/SaveGame/수정 운영맵에 의존하지 않는 clean checkout build/asset load 기준선이 필요하다.
- 운영맵 Z=150cm fixture를 통과시키기 위해 사용자 맵을 수정하지 않는다. 커밋된 스냅샷 검사 또는 임시 검증 scene으로 fixture와 실사용 편집을 분리한다.
- 보고서에 commit/submodule SHA, Engine/toolchain/GPU/driver, 실제 viewport/DPI, 설정 hash, 실행한 tests/skip, 원본 count를 남긴다. 문서 요약과 원본 JSON을 자동 대조한다.
- 완료: clean setup 명령 한 묶음으로 빌드·WBP·계약·핵심 RHI 재현, 운영 asset hash 불변, 실패·skip이 집계에서 숨겨지지 않음.

## 다음 착수 순서

0. **개편 상태(2026-10-13): ARCH-01~08, UXR-01~06·08·09, MAP-01 완료. UXR-07은 Codex 점검 완료(PASS 25/FAIL 1/보류 29), 사용자 15분 체험·확인만 남음. 그 뒤 순서: PERF-01·UXR-08 외형 확인 → UI-02 → OPS-01·QA-02 → DTCore 수정 단계 → 센서/카메라 플러그인화(R5).** 상세는 "개편 후속 개선" 절. UXR-08(Slab 정보·표면)은 시나리오 화면(UXR-04) 직후에 하고, UXR-07 최종 수용은 모든 UXR 뒤에 한다. 각 단계는 독립 커밋 묶음이며 이전 단계의 gate가 통과해야 다음으로 간다. 아래 1~5는 이 순서가 끝난 뒤 재개한다.
1. **UX-01:** 구현된 배치 패널의 첫 사용자 수용·UI 직접 복제/삭제·남은 배율/입력 경계를 닫는다. Editor 영구 설치는 별도 요구로 확정한다.
2. **UI-03:** 기존 작업 흐름을 유지하며 해상도/DPI·모든 소유 패널의 주요 동작과 설명을 검증한다. 마지막 PCD 요약 화면 확인도 포함한다.
3. **RT-08 + RT-01/03 + DATA-01:** 구현된 PCD 사전 점검을 다양한 echo/backend 실측과 대조한다. 동적 작업량 admission·공정 배분은 확인된 병목을 기준으로 별도 설계한다.
4. **RT-05/06 + LIFE-01 + QA-01:** 통과한2+2 PCD10분/10회 조건을 기준으로 대표 장면·부하 환경·조작 응답을 확장한다. 4+4/60분은 별도 지원 gate다.
5. **PL-01/02/03 → PL-04:** 위 사용 흐름과 데이터 계약을 유지하면서 프로젝트 의존성·설정·장비 등록을 정리한 뒤 플러그인/간편 설치를 진행한다.

PCD 외 포맷의 신규 개선(IO-01 포함)은 보류다. 관련 정확성·정합성·종료 회귀는 각 단계에 포함하지만 전체 미완료 목록을 매번 재실행하거나 배치 작업의 착수 조건으로 삼지 않는다.

완료 이력은 각 항목에 검증 SHA·보고서 경로·남은 제한을 붙인다. 이 문서 작성은 해당 기능의 구현 승인이나 실행 완료가 아니다.
