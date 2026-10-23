# UXR-AUDIT-1023 — 현장 운전원 관점 UX/UI 점검

2026-10-23 · 문서 조사 완료 · 구현·현장 수용 판정은 하지 않음.

우선 검토할 항목은 **센서 도구가 코일 입력을 가리는 배치(T01), 미확인 자료와 위험 0의 분리(C01), 해치의 모의값/측정값 구별(H02), 전송 점검 사유의 잘림(D01)**이다. 총 16건: P1 4건, P2 11건, P3 1건. 모두 제안이며 이번 작업에서는 제품 코드를 수정하지 않았다.

## 1. 기준과 판정 방법

| 항목 | 기록 |
|---|---|
| 작업 위치/브랜치 | `C:\Unreal Projects\wt_ux` / `docs/uxr-audit-1023` |
| 기준 HEAD | `98e2131ed3d7921f1d59e36d4815bb616fa7213f` (요청의 master 98e2131) |
| remote / 시작 상태 | origin `https://github.com/YonghoKwon/UE-DT-Project.git` / staged·worktree 변경 0 |
| DTCore | `b2504d1dfea5659f832ff0d750515edee146a28e`, 초기화됨. 변경·update 없음 |
| 조사 자료 | 메인 `Saved/Reports`의 기존 JPG/PNG·촬영 메타데이터와 worktree의 L3 UI 소스 |
| 실행 제한 | Editor·UBT·빌드·성능 측정·GPU·마우스/키보드 조작 없음. 메인 소스·설정·자산·인덱스 쓰기 없음 |
| 문서/증거 | 커밋 문서 `docs/research/UXR-AUDIT-1023/RESULT.md`; 원본 결과·입력 해시·검사 결과 `Saved/Reports/UXR-AUDIT-1023/` |
| 메인 허용 사본 | `C:\Unreal Projects\ma0t10_dt\Saved\Reports\UXR-AUDIT-1023\RESULT.md` 하나 |

**관찰**은 기존 캡처에서 확인한 사실, **소스**는 기준 SHA 코드에서 확인한 사실, **위험**은 그 근거로 추정한 운전원 오독/조작 부담이다. 색 대비의 계측, 클릭 시간, 글자 픽셀 높이, 실제 DPI 150% 수용은 수행하지 않았다. P1은 오판/핵심 조작 가림을 먼저 검토, P2는 반복 운전의 탐색·읽기 부담, P3는 정리 우선순위다. 심각도는 사고 발생이나 기능 고장을 입증한 등급이 아니다.

예상 소요는 기존 DTCore/Workspace와 소유 위젯 안에서 문구·위치·노출만 고치는 **1인 구현·국소 확인 시간(h)**이다. 정식 빌드·회귀·직접 UI·ABAB gate, 사용자 용어 합의 대기는 별도다. 기능/API/저장값/기본 압축·전송 정책/PCD 필드·wire/기준값/경고 판정/차단 정책은 유지한다. 이 보고서는 구현 승인을 대신하지 않는다.

## 2. 캡처 근거와 소스 시점

아래 링크는 기존 원본이다. 메인에 이미지를 새로 만들거나 보정하지 않았다. share JPG는 1600×900 축소본이므로 글자/잘림 판단은 가능한 1920×1080 PNG도 대조했다. E10은 독립 컨트롤 샘플이며 실제 사이드바 폭·조작 증거로 쓰지 않았다. 선택한 10개 이미지와 9개 메타데이터 파일의 SHA-256은 worktree의 `evidence-index.json`에 남긴다.

| ID | 캡처 / 확인 위치 | 조건과 한계 |
|---|---|---|
| E01 | [코일 목록 UI](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/COIL-CAP-01/cap-09-list-ui.png>) · 하단 분석/미확인 버튼, 좌측 입력 | 1920×1080, font 1, DPI 1, D3D12 offscreen. [share JPG](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/COIL-CAP-01/share/cap-09-list-ui.jpg>)도 대조 |
| E02 | [코일 파일 버튼 150%](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/HATCH07-COIL02/manual/coil-file-buttons-final-150.png>) · 좌측 스크롤 하단 | 이미지 1286×726, 기록된 client 1280×720, window DPI 1, UMG 0.666, 코일 글자 150% |
| E03 | [코일 6단 거절 최종](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/HATCH07-COIL02/manual/coil-6-rejected-final.png>) · 좌상 위험 0/하단 미확인 8 | 150% 세션 자료. 이 이미지의 정확한 순간별 DPI 기록은 없음; E02 세션 조건과 구분 |
| E04 | [코일 센서 도구 최종](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/HATCH07-COIL02/manual/coil-tools-no-slab-final.png>) · 좌측 레일/기존 입력 겹침, 하단 경고 | 코일 글자가 큰 상태이나 센서 도구 상태바는 글자 100% 표시. 동일 배율이라는 뜻이 아님 |
| E05 | [해치 3대 모니터](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/RAW-CAP-01/06-monitor-ouster-three.png>) · 지표 6개/3D 상단 툴바/상면·단면 | 1920×1080, owned font 1, UMG DPI 1, D3D12 offscreen. [share JPG](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/RAW-CAP-01/share/06-monitor-ouster-three.jpg>) 대조 |
| E06 | [해치 측면 최종](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/HATCH07-COIL02/manual/hatch-side-final.png>) · 센서 TF 라벨/상단 연결 오류 문자열 | 이미지 1286×726; 해당 세션 client 1280×720, window DPI 1, UMG 0.666, 글자 100% |
| E07 | [해치 보라 높이 최종](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/HATCH07-COIL02/manual/hatch-presets-violet-final.png>) · 3D 라벨 밀집 | E06과 같은 100% 세션; 현장 스캔/센서 TF 정합 증거가 아님 |
| E08 | [PCD 기본 옵션 100%](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/PCD-BANDWIDTH-01/progress/options-defaults-100.png>) · x88–356 부근 사이드바/하단 경고 | 1920×1080, font 1, window DPI 1. 자동 캡처·production controls fallback, 직접 입력 보류 |
| E09 | [PCD Compact 압축 100%](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/PCD-BANDWIDTH-01/progress/options-compact-compressed-100.png>) · 영문 체크 목록/예산 112.5% | E08과 동일 조건. 전송 정지 상태의 **압축 전 최대 추정**, 실측 초과 아님 |
| E10 | [PCD 컨트롤 Full 100%](<C:/Unreal Projects/ma0t10_dt/Saved/Reports/PCD-BANDWIDTH-01/progress/controls-full-100.png>) | 720×720 독립 컨트롤. 회색 배경·압축 체크 상태를 실제 기본 사이드바 상태로 간주하지 않음 |

촬영 이력과 기준 SHA를 대조했다. **현재 HEAD에서 새로 촬영한 화면이라고 주장하지 않는다.**

| 묶음 | 촬영/보고 기준 | 98e2131과 대조한 UI 차이 |
|---|---|---|
| COIL-CAP-01 | 최종 보고 `1fee3dec7ca9343a4338ea665ba0b488451b6317` | `CoilLoadingMonitor.cpp`, `CoilLoadingMonitorHost.cpp` 동일. 다만 캡처 fixture는 PIE 인스턴스의 plot/footer 배치·layout timer를 임시 조정했다는 원본 한계가 있음. E01 배지 위치만으로 제품 배치 결함을 판정하지 않음 |
| RAW-CAP-01 | 이미지 JSON source `e01a47c9b5e0f4a81da61a8aa4909fe98b1d2148` | `ShipHatchMonitorWidget.cpp`, `ShipHatchSiteVisualization.cpp`, `ShipHatchSensorLabelView.cpp` 동일 |
| HATCH07-COIL02 | 최종 보고 `07f46acd70cd6309adb111631029478e81c2bf69`; manual은 마지막 Editor fixture 커밋 전 | 코일 Monitor/Host·센서 Sidebar 동일. `SensorToolToolbarWidget.Status.cpp`는 기준 SHA에 PCD 필요량/예산 경고 4줄 추가; 레일·상태바의 렌더 폭 판단과 구분 |
| PCD progress | capture-conditions source `f3b59c4c250630b90f894d28aa98edbdaf1f4422` | Bandwidth·Presentation 동일. Owned.cpp는 preflight 호출 1줄 변경(RequireData 관련 인자); E08의 구체적 점검 문구가 현재 신규 실행에서도 동일하다고 단정하지 않음 |

## 3. 화면별 점검표

모든 소스 링크/라인은 **98e2131** 기준 `Source/ma0t10_dt/MA0T10/UI/`다. 캡처의 좌표는 탐색 보조이며 클릭 입력으로 사용하지 않았다.

### 3.1 코일 선적 패널

| ID / 우선순위 / 소요 | 관찰·위험 / 캡처 | 파일:라인 근거 | 제안·완료 확인 |
|---|---|---|---|
| **C01 · P1 · 2–3h** | E03의 `위험 0 · 주의 0`과 하단 `자료 필요 8개 원인`이 떨어져 있음. E01은 10개. **N은 코일/서류 수가 아니라 미확인 사유 그룹 수**여서 운전원이 제출 자료 N개로 오독할 수 있고, 위험 0만 보고 해석이 모두 확정됐다고 읽을 수 있음 | [CoilLoadingMonitor.cpp:138](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L138) code+Known으로 묶음; :148 unknown 그룹당 증가; [:155](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L155) 배지와 :156 미확인 버튼 분리 | `위험 0 · 주의 0 · 미확인 8개 사유`를 같은 요약에 노출. 펼침은 `추가 확인 8개 사유 · 펼치기`. 설명에 `코일 수·문서 수 아님`과 해당 사유/현재 입력 확인 위치 표시. 계산·그룹·기존 배치 허용 정책 유지. unknown 0/다수 및 100/150%에서 요약 확인 |
| **C02 · P2 · 1–2h** | E02의 큰 입력 글자와 하단 분석 밀도 차이. **소스상** 150% 적용 뒤 Refresh가 경고 행을 새로 만들며 기본 12 폰트를 사용함. 실제 경고 문장이 150%에도 같은 비율로 커지는지는 캡처만으로 확정하지 않음 | [CoilLoadingMonitor.cpp:196](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L196) 기존 위젯 배율 적용 후 :207 Refresh; [:145](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L145) ClearChildren, :152 새 행 `Style::Font(12)` | 새 경고 행에도 소유 TextScale 적용 제안. 숫자·미확인 행·상단 배지·본문을 100→150→100, revision 갱신 전후 대조. 폰트만 고쳐도 footer 170–260 높이(:119)의 스크롤 부담을 함께 확인 |
| **C03 · P2 · 2–3h** | E01/E02에 `KG / GM / list`, `UDL`, `TPC`, `eye 선수미`, `hardware_ray_tracing`, `중심편차p95` 혼재. 흘수는 `현재 7.093 / 다음포함 7.098m`처럼 첫 수치의 단위를 뒤에서 추론. 입력 ID는 mm, OD/폭은 m이므로 전체 m 안내만으로는 부족 | [CoilLoadingMonitor.cpp:52](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L52), :55, :58; [:129](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L129), :133 | `횡경사 −10.89°`, `흘수 현재 7.093 m / 다음 포함 7.098 m`처럼 수치마다 단위. KG/GM/TPC는 한글 뜻+약어 병기, backend는 진단 상세에 원문. 코일 식별 ID와 내경 ID를 `코일 번호`, `내경(mm)`로 구별. 기존 값·단위 변환·유효 숫자 유지 |
| **C04 · P2 · 3–5h** | E02는 저장/읽기가 많은 선박 상세 입력 아래에 있고 긴 경로 끝이 잘림. 빈 화면에서 계획 읽기까지 소스상 `설정·상세 → 스크롤 → 계획 읽기`로 최소 2클릭+스크롤+경로 편집. 150%에서 일상 파일 작업과 상세 해석 설정 탐색이 섞임 | [CoilLoadingMonitor.cpp:61](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L61), :64–78 상세; [:79](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L79)–83 파일; :171–181 기존 handler | 파일 관련 기존 버튼/경로를 `계획·목록` 묶음으로 앞쪽 재배치. 경로는 선택/전체 확인 가능하게 노출. 100/150%에서 저장·불러오기 구분과 취소·실패 피드백 확인. 새 파일 선택기·저장 방식·동기 I/O 도입은 이 제안 범위 밖 |

### 3.2 해치 모니터

| ID / 우선순위 / 소요 | 관찰·위험 / 캡처 | 파일:라인 근거 | 제안·완료 확인 |
|---|---|---|---|
| **H01 · P2 · 3–5h** | E05 3분할에서 모델명 3개가 한 줄을 차지하고 `하역기 전…` 이후 시점 버튼이 화면 밖. 스크롤로 접근 가능한 항목이나 가로 스크롤 조작을 알아야 함. E06의 단일 3D는 더 넓어서 같은 버튼이 보임; 삭제된 기능으로 판정하지 않음 | [ShipHatchMonitorWidget.cpp:213](../../../Source/ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.cpp#L213) 수평 ScrollBox+ClipToBounds; :216–223 센서 3개/보기 혼합 | 센서 선택과 시점 프리셋을 별도 행/접기 묶음으로 표시. 가로 넘침이 있으면 다음 항목 존재 표시. 3분할/단일·100/150%에서 주요 시점 이름이 완전하고 최소 1클릭(스크롤 조작 별도)로 식별되는지 확인 |
| **H02 · P1 · 2–3h** | E05 최상단 `64.3% 남은 적재율`, `15796 m³ 남은 부피·모의`, 세 높이, `14.1% 측정 불가`를 같은 카드로 표시. 부피만 모의 표시라 적재율도 실측 산출이라고 읽을 위험. **소스 확인:** 적재율/부피는 모의 surface, 높이/미관측률은 분석 결과 | [ShipHatchMonitorWidget.cpp:124](../../../Source/ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.cpp#L124) 카드 제목; [:443](../../../Source/ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.cpp#L443) `Surface().Fill/Volume`과 `A.Max/Mean/Min/Coverage` | 같은 위치에서 `남은 적재율·모의`, `남은 부피·모의`, `측정 높이`의 출처를 명시. 센서 미수신/새 측정 대기는 기존 상태를 함께 읽게 배치. 모의/실측 수치 계산·시점·유효 판정은 유지. 정상/대기 상태와 100/150%에서 출처 식별 확인 |
| **H03 · P2 · 2–4h** | E05/E06/E07의 `os_lidar_boom`, `os_lidar_cabin_01/02`, `Crane/Trolley/Grab/Mesh_origin`은 운전원에게 역할보다 TF 식별자를 먼저 노출. 3D 투영 라벨이 근접해 겹치며 상단 목표/실제 Hz 3개에는 역할을 툴팁에서 찾아야 함 | [ShipHatchMonitorWidget.cpp:296](../../../Source/ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.cpp#L296) Roles; :117 Hz 순서 툴팁; :307 목표/실제; [ShipHatchSensorLabelView.cpp:34](../../../Source/ma0t10_dt/MA0T10/UI/ShipHatchSensorLabelView.cpp#L34)–40 고정 크기 투영·TF Names | 기본 표시는 `붐 / 운전실 좌 / 운전실 우`, 전체 TF ID는 상세/툴팁에서 유지. Hz에도 역할 연결. 겹치는 라벨은 선택 항목 우선·연결선/위치 조정 검토. TF 원문·센서 ID·좌우 정의·좌표·스캔은 변경하지 않음. 근접/조감·100/150%에서 식별 확인 |
| **H04 · P2 · 2–3h** | E05 `측정 불가 비율 14.1%`, 회색 격자의 원인은 `회색 미관측(툴팁)`과 하단 작은 설명에 분산. 운전원은 장비 장애인지 차폐/FOV인지 구별하려고 hover 탐색 필요. 비율 자체를 고장 경고로 단정할 근거는 없음 | [ShipHatchMonitorWidget.cpp:150](../../../Source/ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.cpp#L150)–151; [:430](../../../Source/ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.cpp#L430) 최근 검출점 없음/현재 격자 진단 구분; :443 비율 | 카드 설명에 `미관측 격자 비율`과 `차폐·거리·시야 범위는 격자 상세에서 확인`을 가까이 배치. 기존 고장 상태와 같은 단어로 묶지 않음. 경고 임계값을 새로 만들지 않고 관측 없음/부분 관측 설명의 가독성 확인 |

### 3.3 데이터/전송 패널

| ID / 우선순위 / 소요 | 관찰·위험 / 캡처 | 파일:라인 근거 | 제안·완료 확인 |
|---|---|---|---|
| **D01 · P1 · 2–4h** | E08/E09에서 `전송 방식이 맞지 않습니…`가 잘리고 해결 버튼은 `연결 설정 열기`만 보임. PCD 옵션 뒤 신규/재생 카드와 점검 대상까지 내려간 다음 원인을 읽게 됨. 150%는 미확인 | [VirtualSensorCaptureExportPanelWidget.Owned.cpp:400](../../../Source/ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.Owned.cpp#L400)–418 순서; :411 summary Ellipsis+툴팁; :413–414 기존 해결 이동 | 현재 점검 대상/짧은 사유/기존 해결 버튼을 전송 화면 상단에서 함께 표시. 상세는 기존 펼침 유지. 문구를 `방식 확인 필요`처럼 줄여도 구체 원문은 상세에 보존. 정책을 통과시킨 것처럼 바꾸지 않고 모든 preflight 사유·100/150% 확인 |
| **D02 · P2 · 2–3h** | E08/E09의 약 268px 카드에서 호환성 설명이 약 6줄이고 제목·원문 `binary_compressed`·추정·주의문까지 연속됨. 기술 설명이 반복 시작/점검 조작 위에 상시 노출. E10의 넓은 샘플만 보면 이 부담을 놓침 | [VirtualSensorCaptureExportPanelWidget.Bandwidth.cpp:21](../../../Source/ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.Bandwidth.cpp#L21), :94–112; :103 툴팁과 :106 동일 설명 상시 표시 | 제목 `PCD 전송 설정`, 압축 `LZF 압축`, 짧은 설명 `서버의 압축 해제 지원을 확인하세요`. PCL/ROS2/자체 parser 호환 내용은 기존 툴팁 또는 소유 상세에 완전 보존. `측정·미리보기·파일 저장 유지`, 변경 전 중지·배수 조건은 짧게 유지. 기본 On/기능/지원 주장을 바꾸지 않음 |
| **D03 · P2 · 2–4h** | E08/E09 `horizontal_index`, `return_index/count`, `time_offset_ns`, `validity`, `confidence`가 체크 목록에 직접 노출. Full/Compact에서 비활성 상태인 이유가 목록 옆에 없음. 사용자가 고장난 체크로 읽을 위험 | [VirtualSensorCaptureExportPanelWidget.Bandwidth.cpp:86](../../../Source/ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.Bandwidth.cpp#L86)–91 `FieldName` 직출력, Custom만 편집/XYZ 고정 | 아래 용어표의 한글 표시명과 raw 이름 툴팁을 적용하는 UI adapter 제안. `필드 편집: 사용자 지정에서 가능 · XYZ 필수` 명시. 프로필별 포함 필드 요약을 우선 노출하고 원문 전체도 접근 가능하게 유지. field mask·필드 순서·타입/단위·wire는 그대로 |
| **D04 · P2 · 3–5h** | E08 정지 상태 `41.25 MiB/s / 346.0 Mbps`, 합계 `123.75 / 1038.1`, 예산 `154.7%`; E09 압축 체크 상태도 `112.5%`. **압축 전 최대 추정**이라는 설명은 있으나 여러 수치 뒤에 있어 실측 과부하/압축 무효로 오독 가능. 수치가 틀렸다는 지적은 아님 | [VirtualPcdBandwidthPresentation.cpp:5](../../../Source/ma0t10_dt/MA0T10/UI/VirtualPcdBandwidthPresentation.cpp#L5)–10 정지 추정/집계 불완전/실측 구분; [VirtualSensorCaptureExportPanelWidget.Bandwidth.cpp:111](../../../Source/ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.Bandwidth.cpp#L111) 단일 문자열 | 첫 줄에 `전송 정지 · 압축 전 최대 추정` 또는 `전송 중 · 최근 1초 실측` 유지, 선택/합계/설정 예산을 짧은 행으로 정렬. MiB/s 주 단위·Mbps 보조로 계층화하되 둘 다 확인 가능하게 유지. 정지 추정 초과를 실제 송신 장애라고 표시하지 않음. 집계 불완전도 0/성공으로 보이지 않게 확인 |
| **D05 · P3 · 1–2h** | E08/E09 `서버 전송 1/1 · 20 Hz 측정 시 20 Hz`는 예시임을 끝까지 읽어야 함. 실제 센서가 다른 Hz여도 고정 20 Hz 문구라 실측 전송률로 오독할 위험. 메뉴를 열면 `전송 1/2`처럼 더 짧아 선택 전/후 표현도 다름 | [VirtualSensorCaptureExportPanelWidget.Bandwidth.cpp:29](../../../Source/ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.Bandwidth.cpp#L29)–32; :108–110 | 선택/메뉴를 `매 프레임(1/1)`, `2프레임마다(1/2)`, `4프레임마다(1/4)`로 맞추고 기존 20→10 예시는 도움말에 표시. 측정Hz와 송신 분주가 별개임을 유지. 분주값·획득·전송 알고리즘은 변경하지 않음 |

### 3.4 센서 툴바/화면 간 이동

| ID / 우선순위 / 소요 | 관찰·위험 / 캡처 | 파일:라인 근거 | 제안·완료 확인 |
|---|---|---|---|
| **T01 · P1 · 4–6h** | **E04에서 센서 레일이 코일 상세 패널의 왼쪽 문구와 버튼을 실제로 가림.** 입력값은 보이나 제목 일부가 사라짐. Slab 카드/시나리오가 안 보이는 것은 기존 분리의 정상 결과이며 결함으로 재등록하지 않음 | [CoilLoadingMonitor.cpp:119](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L119)–124 좌측 고정 배치; [CoilLoadingMonitorHost.cpp:112](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitorHost.cpp#L112)–115 도구 열기; [SensorToolToolbarWidget.Sidebar.cpp:60](../../../Source/ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.Sidebar.cpp#L60) 좌측 레일 | 기존 소유 Workspace/DTCore 열기 경로를 유지하며 Host가 레일 점유 영역에 맞춰 코일 소유 패널 배치를 조정하는 프로젝트 측 방안 검토. 별도 widget manager·DTCore 수정 제안 없음. 열기/숨김/재호스팅·코일 100/150×도구 100/150에서 제목/조작 노출, 동일 위젯 인스턴스 유지 확인 |
| **T02 · P2 · 2–3h** | E04 코일은 큰 글자지만 하단 센서 도구는 `글자 100%`. 코일 버튼 `글자 100/150%`는 현재 상태를 표시하지 않음. 도구 더보기→표시→배율은 3클릭이지만 하단 배율→선택의 2클릭 경로도 있어 전자만 최소 경로라고 세면 안 됨 | [CoilLoadingMonitor.cpp:86](../../../Source/ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.cpp#L86), :195–207; [SensorToolToolbarWidget.cpp:202](../../../Source/ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.cpp#L202)–216; [SensorToolToolbarWidget.Status.cpp:232](../../../Source/ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.Status.cpp#L232)–234 | `코일 글자 150%`, `센서 도구 글자 100%`처럼 현재값/적용 범위를 표시. 하단 바로가기를 알아볼 수 있게 표시하고 기존 별도 저장 책임/배율을 통합하지 않음. 해치 설정 배율도 해치 전용임을 명시 |
| **T03 · P2 · 2–4h** | E04 하단 `전송할 장비를 배치 상단 목록에서 선택…`, E08 `전송 방식이 맞지 않습니다 · 서버 연결 설정에서 실시간 전송을…`의 행동 문구가 잘림. 노란 색과 아이콘은 있으나 무엇을 눌러야 하는지 tooltip/클릭 후 확인하게 됨. E06의 청록 연결 오류 문자열은 당시 화면 로그이며 현 제품 재현/로그 제거 권한으로 확대하지 않음 | [SensorToolToolbarWidget.Status.cpp:219](../../../Source/ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.Status.cpp#L219)–224 warning max 340/일반 240, Ellipsis·툴팁·기존 클릭 이동; [SensorToolToolbarWidget.Sidebar.cpp:64](../../../Source/ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.Sidebar.cpp#L64) 상태바 높이 | 경고 요약을 `장비 선택 필요`, `연결 방식 확인 필요`처럼 줄이고 기존 해결 이동을 `확인`으로 명료화. 원문은 tooltip/기존 상세에 보존. 100/150%·좁은 client에서 원인+행동 식별, 클릭 목적지 불변 확인 |

## 4. 집중 문구 제안

### 4.1 '자료 필요 N개'의 의미

`DataNeededCount`는 입력칸의 빈 수, 증빙 파일 수, 영향받은 코일 수가 아니다. `R.Issues`를 **Code와 bKnown**으로 묶은 뒤 Known=false인 그룹마다 1을 더한다. 따라서 단순 `자료 N개 제출`로 바꾸면 의미가 달라진다.

| 현재 | 제안 | 보존할 의미 |
|---|---|---|
| 자료 필요 10개 원인 · 펼치기 | 추가 확인 10개 사유 · 펼치기 | 미확인 그룹 수 유지, 실제 필요한 자료 종류는 펼친 사유에서 확인 |
| 위험 0 · 주의 0 | 위험 0 · 주의 0 · 미확인 8개 사유 | 미확인 존재를 확정된 정상/위험으로 바꾸지 않음 |
| 중심편차p95 자료 필요(점/방향 부족) | 중심 편차 p95: 비교 불가 · 점/방향 부족 | 센서 비교 불가와 승인 입력 미확인을 같은 요청으로 읽지 않게 설명 |

아래는 **문서 안의 제안 diff**이며 소스에 적용하지 않았다. N 계산은 바꾸지 않는다. 배지 동시 표시/tooltip 등은 별도 후속 구현에 포함해야 한다.

```diff
--- UI/CoilLoadingMonitor.cpp (98e2131:156, proposal only)
- TEXT("자료 필요 %d개 원인 · %s")
+ TEXT("추가 확인 %d개 사유 · %s")
```

### 4.2 PCD 기본 표시와 영문 필드명

첫 화면은 `PCD 전송 설정 → 적용 대상 → 프로필/압축/전송 간격 → 추정 또는 실측 → 현재 점검 사유/해결` 순으로 읽히게 제안한다. 프로필 편집 목록/수신기 호환 상세는 기존 소유 상세 안에서 접근 가능하게 둔다. 상시 주의문은 짧게 하되 **외부 수신기 합의, 변경 전 전송 중지·대기열 배수, 측정·미리보기·파일 저장 불변**이라는 기존 내용을 제거하지 않는다.

| 현재 표시 | 운전원 기본 표시 제안 | 상세/tooltip에 유지 |
|---|---|---|
| Full / Compact / Minimal / Custom | 전체(Full) / 축소(Compact) / 위치만(Minimal) / 사용자 지정(Custom) | 기존 프로필명·B/점·포함 필드·선택값 |
| x / y / z | X / Y / Z 좌표 | raw `x/y/z`, 센서 로컬 m; 축 방향은 기존 계약 설명 |
| intensity | 반사 강도 | `intensity`, 기존 값 의미/스케일 |
| ring | 채널 번호 | `ring`, 기존 인덱스 의미 |
| horizontal_index | 수평 샘플 번호 | `horizontal_index` |
| return_index / return_count | 반사 순번 / 반사 수 | `return_index` / `return_count`, 기존 번호/개수 의미 |
| time_offset_ns | 프레임 내 시간차(ns) | `time_offset_ns`, 기존 기준시각 의미 |
| validity / confidence | 유효 여부 / 신뢰도 | `validity` / `confidence`, 기존 값 범위 |
| LZF 압축 · PCD binary_compressed | LZF 압축 | `PCD binary_compressed`, 수신기별 압축 해제 지원 확인 설명 원문 |

`FVirtualPcdCodec::FieldName`이나 PCD header를 한글로 바꾸는 제안이 아니다. 번역은 L3 표시 adapter에서만 검토한다. 여기서 언급한 단위/뜻은 기존 필드 설명과 다시 맞춰야 하며 새 값 범위·지원 보장을 만들지 않는다.

## 5. 100/150%·클릭 수 점검 범위

앱 소유 글자 150%와 Windows DPI 150%는 별개다. 1280×720/UMG 0.666의 150% 이미지를 1920×1080/UMG 1의 100% 이미지와 같은 조건의 가독성 비교로 사용하지 않았다.

| 화면 | 잘림/밀도 | 영한 용어·단위 | 경고/현재 상태 | 글자 100% | 글자 150% |
|---|---|---|---|---|---|
| 코일 선적 | C04 상세/파일 탐색, T01 레일 겹침 관찰 | C03 수치/ID/약어 관찰 | C01 unknown 병렬 표시 위험, C02 동적 행 폰트는 소스 근거 | E01 관찰; fixture 배치 한계 포함 | E02/E03 관찰; 동일 client의 100%와 교차 검증 아님 |
| 해치 모니터 | H01 가로 스크롤 뒤 보기 기능, H03 라벨 겹침 | H02 모의/실측, H03 TF 영문, H04 미관측 설명 | 지표값·획득Hz 표시 관찰; 낮은Hz/정지/장애 조작 검증 미수행 | E05/E06/E07 관찰, client별 구분 | **보류: 지정 입력의 확인 가능한 150% 캡처 없음** |
| 데이터/전송 | D01 점검 잘림, D02 상시 장문 | D03 필드, D04 단위·출처, D05 예시Hz | 전송 정지·preflight 경고 관찰; 활성 송신/압축 실측은 미검증 | E08/E09 관찰, fallback 한계 포함 | **보류: 지정 progress는 100%만 제공** |
| 센서 툴바 | T01 가림, T03 경고 ellipsis | T02 배율 적용 범위 | warning 클릭 목적지는 소스로 확인 | E04/E08 관찰 | **보류: 도구 150% 직접 캡처 없음; 코일 150%와 혼동 금지** |

클릭 수는 기존 handler/구조로 계산한 **예상 최소 버튼 활성화 횟수**다. 스크롤/hover/경로 타이핑/오입력/소요 시간은 횟수에서 별도이며 실제 마우스로 재현하지 않았다.

| 운전원 작업 / 출발점 | 현재 최소 경로 | 제안 뒤 확인 목표 |
|---|---|---|
| 코일 기본 화면 → 계획 읽기 | 상세 열기 1 + 파일 영역으로 스크롤 + 읽기 1 = **2클릭+스크롤/경로 입력** | 파일 그룹 탐색을 줄이되 handler 동일 |
| 코일 → 미확인 사유 | 하단까지 탐색 후 펼치기 **1클릭**, 해당 사유 강조는 추가 1클릭 | 상단 요약에도 사유 수 표시, 펼침 1클릭 유지 |
| 코일 → 센서 전송 도구 | 상세 1 + 스크롤 + 센서 도구 1 + 전송 레일 1 = **3클릭+스크롤** | 기존 도구 진입 재배치와 겹침 해소; 별도 도구 관리자 생성 없음 |
| 해치 → 센서 도구 | 설정·상세 1 + 스크롤 + 센서 도구 1 = **2클릭+스크롤** | 기존 열기 경로 유지, 반복 이동 탐색 줄이기 |
| 해치 3분할 → 하역기 전체 보기 | 가로 스크롤 + 버튼 **1클릭** | 스크롤 의존 감소; 보기 동작 동일 |
| 센서 레일 → 전송 / 결과 | 해당 레일 **1클릭**; 직접 전송 시작은 접기 열기 1 + 시작 1 추가(사전조건 별도) | 1클릭 탭 이동 유지; 조건 미충족은 기존 점검 이동 사용 |
| 센서 도구 글자 변경 | 하단 배율 메뉴 1 + 배율 1 = **2클릭**. 더보기→표시→배율은 3클릭 | 더 짧은 기존 경로의 발견성/적용 범위 개선 |

**결과 탭·서버 연결 화면·활성 송신 실측 화면은 지정 캡처에서 직접 확인하지 못했다.** 소스 `VirtualSensorCaptureExportPanelWidget.Owned.cpp:490–514`의 결과/연결 진입과 :523의 `snapshot·receipt` 진단 문구는 읽었으나 잘림/글자/클릭 성공 PASS로 판정하지 않는다. 후속에서는 receipt와 소비자 처리 ACK를 구별한 기존 계약이 문구에서도 유지되는지 확인한다.

## 6. 후속 작업과 종료 기록

권장 착수 순서는 P1 **T01 → C01/H02 → D01**, 이후 문구/밀도 묶음 C03·D02–05·T02–03, 150%/행 재생성·3분할 H01·C02·H03–04 순이다. C04의 파일 동선 재배치는 사용 빈도 확인과 함께 진행한다. 16건을 개별 합산하면 **35–59h**이며 화면별 공동 수정은 중복 시간을 줄일 수 있다. 일괄 구현 요청으로 해석하지 않는다.

후속 UI 구현이 별도 승인되면 같은 SHA·client·DPI·폰트 배율로 1920×1080과 1280×720, 100/150%, 좁은 사이드바/3분할, 위험·주의·미확인, 전송 정지·집계 대기·활성·오류를 직접 눌러 확인한다. 개인 설정/창 배치 백업·복원, 숨김/취소/선택 변경/재호스팅, 미등록 위젯 불변도 기존 gate를 따른다. 기능·조작 변경에 필요한 실제 RHI/성능 검증은 이번 문서 조사에서 수행한 것으로 대체하지 않는다.

| 검사/산출물 | 결과 / 범위 |
|---|---|
| 문서 링크·파일:라인·입력 근거 | 선택 캡처 10개 및 해당 메타데이터 존재/해시 기록, 코드 줄과 제안 대조. 기계 검사 결과 `document-validation.json` |
| 문서 diff | `git diff --check` 및 staged diff 검사. 커밋은 이 문서 1개만 |
| DTCore / layer | 읽기 전용 guard·layer 스크립트 실행 결과는 `dtcore-guard.json`, `layer.json`; allowlist 0→0, 새 위반 0 확인 |
| 결과 schema / merge check | `result.json` 생성 뒤 validate/merge_check 실행. 런타임 gate NOT_RUN을 그대로 기록. 최종 판정/검토 사유는 같은 폴더 `merge-check.json`에 기록하며 자동 머지 승인이 아님 |
| Editor/빌드/quick·feature·release/perf/직접 UI/현장 | **미수행**. 요청의 문서 조사와 병렬 실행 금지 범위에 따름. 기존 캡처를 새 통과 증거로 바꾸지 않음 |
| 보호 경로/개인 설정 | 제품 소스·DTCore gitlink/worktree·Config/Game.ini·PixelStreaming·맵/.uasset 변경·stage 없음. 개인 SaveGame·Editor·창 배치 미조작으로 복원 작업 불필요 |
| 비밀 / 외부 작업 | 자격증명 열람·사용·출력·복사 없음. push/PR/merge·submodule update·다른 프로세스 조작 없음 |
| 결정 필요 | 조사 완료를 막는 결정 없음. 제안 구현의 우선순위·현장 용어/수용은 별도 작업에서 결정 |

`Saved`의 JSON·로그·검사 파일은 커밋하지 않는다. 메인에는 허용한 `RESULT.md` 사본 하나만 복사하고 byte hash 동일 여부를 확인한다. 기존 동명 사본은 시작 시 없었다. 검토자는 커밋 diff, `result.json`, `merge-check.json`과 원본 캡처를 독립 확인한다.
