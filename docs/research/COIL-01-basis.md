# COIL-01 — 제품 부두 코일 선적 시뮬레이션 수치 근거

조사일: 2026-10-22. 대상: 작업 #31의 **구현 전 연구 예비안**. UE 5.3 코드·맵·에셋 변경, Editor 실행, 빌드, 물리 실험 및 성능 측정은 수행하지 않았다.

이 문서는 선적 배치의 사전 해석과 Chaos 확인을 설계하기 위한 자료다. 선박별 승인 Loading Manual(적재 매뉴얼), Cargo Securing Manual(CSM), 복원성 자료, 코일 제조사 적층 허용값을 대체하지 않는다. 자료가 없으면 `미확인`으로 남기며, Chaos에서 움직이지 않았다는 이유로 안전 판정을 내리지 않는다.

## 0. 근거 구분과 단위

| 표기 | 의미 |
|---|---|
| 공개 근거 | 선사·조선소·제조사·IMO·정부·선급·P&I 원문에서 확인한 값/권고. 적용 대상과 판본을 함께 기재 |
| 유도식 | 본 문서에서 명시한 가정 아래 기하·힘 평형·질량 모멘트로 유도. 승인된 코일 설계식이라는 뜻은 아님 |
| 추정/제안 | 시뮬레이션 초기값·경고 구간·시험 입력. 실선 검증값/국제 공통 허용값이 아님 |

길이는 m, 질량은 metric tonne(t), 힘은 kN, 응력은 kPa 또는 MPa, 가속도는 m/s²를 사용한다. `g = 9.81 m/s²`, `1 t/m² = 9.81 kPa`. 선박 좌표는 X 선수 방향, Y 좌현, Z 위쪽으로 제안한다. CSS 위치 표의 원점은 별도로 **선미 수선(AP)** 이므로 변환을 저장한다. DWT는 적재중량이고 배수량 Δ가 아니다. 선체 깊이 D, 흘수 T, 홀드 유효 높이 H도 구분한다.

## 1. 선박 예와 권장 프리셋

### 1.1 공개 제원 4척

모두 1~3만 DWT 범위의 general cargo/multipurpose 예다. 석유제품 운반선(product tanker)을 뜻하는 ‘제품선’은 이 작업 대상에서 제외한다. 치수는 `L × B × H`, 해치는 `L × B`다. 점 구분 `23.317` 등은 선사 원문에서 천 단위 표기다.

| 선박·근거 | DWT / LOA / B / D / 여름 흘수 | 홀드 수·내부 치수(m) | 해치 개구(m) | 탱크톱 / 갑판 화물 크레인 |
|---|---|---|---|---|
| Vikingbank, Wagenborg Vxxl [S1][S2] | 웹 DWAT 11,940 t; 142.65 / 15.87 / 10.78 / 7.729 | 2개; #1 41.00 × 13.20 × 10.78, #2 63.63 × 13.20 × 10.78 | 각 홀드와 같은 L×B로 웹 표기; open hatch/no understow | 등분포 15 t/m² [S2]; gearless [S5] |
| Egbert Wagenborg, EasyMax [S3] | 14,366 t; 149.95 / 15.90 / 12.20 / 8.60 | 2개; #1 47.36 × 13.50 × 12.15, #2 64.38 × 13.50 × 12.15 | 각 47.36 × 13.50, 64.38 × 13.50; open hatch/no understow | 150 kN/m² ≈ 15.29 t/m²; non-uniform 200 kN/m²는 별도 승인 조건. gearless [S5] |
| Reestborg, R-class [S4][S5] | 웹 23,317 t; 169.75 / 20.40 / 13.75 / 9.718 | 2개; 각각 60.59 × 17.00 × 13.55 | 각각 60.59 × 17.00; open hatch/no understow | 등분포 15 t/m²; gearless |
| AAL Fremantle, S-class [S6] | 19,160 t; 149.02 / 23.40 / 13.50 / 9.80; LBP 140.30 | 3개; 내부 L×B×H는 공개 웹 제원에 없음. 용적 #1 660.90 / #2 6,020.40 / #3 13,297.90 m³ | #1 6.30 × 10.50, #2 25.90 × 18.00, #3 52.50 × 18.00 | 20 t/m²; 좌현 350 t급 2기, 각 14.5 m 반경에서 350 t, 32 m에서 140 t |

자료 차이/모델 경계:

- Vikingbank의 2024 Vxxl PDF는 DWAT 11,850 t, 웹은 11,940 t다. 실선 적재 가능 질량을 하나로 확정하지 않고 출처별로 보존한다. Ferus Smit의 같은 12,000 DWT 설계 설명 [S7]은 #1 폭 13.20/9.24, H 10.65 m다. 선수단 협소부·높이 기준 차이를 무시해 완전 직육면체 전역으로 확장하지 않는다.
- [S7]의 non-uniform `20 t/m3`는 원문 단위가 의심되어 채택하지 않았다. [S3]의 200 kN/m²도 임의 받침 면적에 그대로 적용할 수 있는 국부 허용압으로 취급하지 않는다.
- Reestborg 웹의 ballast draught 10.093 m는 같은 페이지 ballast capacity 10,093 m³와 혼동한 것으로 의심된다. 사용하지 않는다. 2024 PDF는 ballast draught 6.00 m, DWAT 23,000 t다 [S4].
- AAL의 해치 치수는 내부 홀드 치수가 아니다. 공개 표만으로 H를 역산하거나 선체 D를 H로 복사하지 않는다. 이 예는 크레인 보유 MPP의 비교 자료이며 정확한 상자 홀드 복제에는 GA가 추가로 필요하다.
- gearless 선박에도 해치 패널용 gantry crane은 있을 수 있다. 이것을 코일 화물 인양용 크레인으로 계산하지 않는다 [S2][S4].

### 1.2 권장 기본값 1세트 — R-class를 참고한 합성 예제

**실선 재현/승인 적재 상태가 아닌 시뮬레이션용 초기 세트**다. 복잡한 협소부는 별도 장애물/금지영역으로 추가할 수 있게 한다.

| 항목 | 권장값 | 근거 수준 |
|---|---|---|
| 선체 외형 | LOA 169.75, B 20.40, D 13.75, 표시용 T 9.718 m | [S4] 공개 제원. 이 흘수에 맞는 정수력 평형은 아직 미구현/미확인 |
| 홀드 | 2개, 각각 60.59 × 17.00 × 13.55 m; 해치 60.59 × 17.00 m | [S4]. 초기 렌더는 직육면체 단순화, 실제 선수/선미 구조물은 별도 입력 |
| 탱크톱 등분포 허용 | 15 t/m² | [S4]. 국부 판/보강재 허용과 홀드 총중량 한도는 `미확인` |
| 선박 크레인 | 없음; 부두 포털 크레인 사용 | [S5] |
| 코일 | 열연, OD 1.600 / ID 0.762 / 폭 1.250 m, 충전율 0.97 | §2의 카탈로그를 참고한 대표값 제안; 질량 약 14.797 t |
| 배치 | eye fore-and-aft; 바닥 1단+필요한 key coil로 시작. 단수 설정에 고정 상한 없음 | [C1]과 예제 선택. 단수 증가 때마다 전 항목 재판정 |
| 받침 초기 형상 | 건조 경목 3줄, 각 폭 0.200 × 두께 0.050 m, 횡방향 연속 배치; 코일 축방향 줄 중심 간격 0.450 m | §3을 참고한 **추정 형상**. 프레임 간격/목재 강도 확인 전 허용하중 근거로 사용 불가 |
| 항해 해석용 LBP / 속력 | 162 m / 14 kn | **추정값**. 공개 LOA를 CSS의 LBP로 대체하지 않기 위한 예제 입력 |
| 복원성 계산 fixture | Δ₀ 30,000 t, KG₀ 7.0 m, KM 9.0 m, 자유표면 보정 0.2 m → GM 1.8 m | **전부 가상 입력**. 표시 흘수와 일치하는 실선 상태를 뜻하지 않음 |
| 부두 크레인 | 훅 아래 총하중 40 t, 반경 11~35 m, 레일 위 28 m/아래 15 m, 스팬 12 m | §5 제조사 표에서 선택한 합성 프리셋; 장구 자중은 총하중에서 차감 |

## 2. 열연·냉연 코일 사양

### 2.1 공개 범위와 조절 범위

| 항목 | 열연(HR) | 냉연(CR) | 적용 주의 |
|---|---|---|---|
| ID | 610 또는 762 mm [P1] | 508 또는 610 mm [P1][P3] | 508/610/762를 UI 선택값으로 제공하되 HR/CR의 모든 조합이 표준이라는 뜻은 아님 |
| OD | [P2] 계산도 예시 1,000~2,100 mm | [P3] 계산도 700~2,500 mm | 계산도 축 범위가 전 규격의 제조/판매 보증 범위는 아님 |
| 폭 | 시뮬레이션 대표 범위 **800~2,000 mm 제안** | 대표 범위 **900~1,800 mm 제안** | [P2][P3] 강종·두께별 가용 도표 안에서 선택. 냉연 다른 제조사 예는 900~1,615 mm [P4] |
| 질량 | [P1] 특수강 코일 표준 포장 5~16 t; 일반 예제 **5~30 t 조절 제안** | [P3] 통상 5~20 t; **5~30 t 조절 제안** | 30 t는 조사에서 확인한 모든 제품의 공통 판매 상한이 아님. 공급자 값 우선 |
| 권장 대표값 | OD 1.600, ID 0.762, 폭 1.250 m, η=0.97 → 14.797 t | OD 1.500, ID 0.508, 폭 1.250 m, η=1.00 → 15.351 t | 모든 값을 독립 변경할 때 질량 일관성 검사 필요 |

`OD > ID > 0`, 폭·질량 양수, 장구/해치/코일 제조 제한을 검사한다. 패킹 외경·총폭·포장 포함 gross mass와 강재 net mass는 별도 필드로 둔다. 슬릿 코일이나 포장된 eye-to-sky 코일은 별도 프로필이며 위 폭 범위 밖이라는 이유만으로 기하적으로 차단하지 않는다.

### 2.2 밀도 7.85 검증식

강재 밀도 `ρ = 7.85 t/m³ = 7,850 kg/m³`를 사용한다. [P2]의 열연 계산도는 밀도 7.85와 권취 면적 점유율 0.97을, [P3] 냉연 계산도는 충전율 100%를 사용한다. **밀도와 충전율을 혼동하지 않는다.**

```text
V_annulus = π/4 × (OD² − ID²) × width                    [m³]
m_net = 7.85 × η × V_annulus                             [t; 모든 길이 m]
m_net = 7.85 × η × π/4 × (OD_mm² − ID_mm²) × width_mm × 10⁻⁹ [t]
OD = sqrt(ID² + 4m_net/(π × 7.85 × η × width))             [m]
```

질량이 명시된 실제 코일은 라벨/계근 gross mass를 선박 하중·크레인 판정에 사용한다. 체적식은 교차검사다. `m_net/(7.85 V_annulus)`가 1을 넘거나 대표 충전율과 크게 다르면 단위·포장·라벨을 경고한다. 불일치 경고 허용폭은 ±5%를 **초기 UI 제안값**으로 두며 계량 허용오차 표준으로 표시하지 않는다.

## 3. 적재 실무와 수치화 경계

### 3.1 방향·받침·쐐기·key coil

- **기본 방향:** CSS Annex 6 §2.1~2.4 [C1]은 원통 옆면(on the round) 바닥 적재, 코일 축 fore-and-aft, 횡방향 dunnage, 이웃 밀착, 마지막 코일이 두 이웃 위에 앉아 row를 잠그는 구성을 제시한다. ‘athwartships rows’는 행을 좌우로 쌓는다는 뜻으로, eye를 athwartships로 향하게 한다는 뜻이 아니다.
- **eye athwartships:** 선박 CSM/제조사 운송 지시가 해당 방향의 cradle·chocking·종방향 구름·횡방향 축 미끄럼을 따로 검증한 경우에만 조건부 프로필로 제공한다. 기본과 동등한 표준 배치라고 자동 승인하지 않는다. 포장된 eye-to-sky는 pallet/별도 지지 조건이며 Annex 6 §1.1의 on-the-round 범위와 분리한다 [C1][C4].
- **목재 규격:** Skuld의 2004 가이드 ‘Steel coils’ [C3]은 15 t 이하에 150 × 25 mm 판재 두 겹의 2줄, 초과에 3줄을 제시한다. American Club §4.3.2 [C4]는 최소 2줄, 무거운 코일(15 t 이상)에는 두께 50 mm 미만을 쓰지 말 것을 권고한다. 보편적인 하나의 규격이 아니므로 단순히 ‘25 mm면 규정 통과’로 코딩하지 않는다. 건조 경목·결함 없음·적절한 처리 여부를 입력한다 [C5].
- **간격:** 받침 줄 간격은 코일 폭·단수·내저판/보강재·목재 휨/압축 강도로 정한다. 공개 자료만으로 공통 최대 간격을 확정하지 못했다. §1.2의 450 mm는 예제다. American Club §5.5.2.1(c)의 100~150 mm는 **코일 row 사이 선수미 간격**이며 dunnage 줄 간격이 아니다. 같은 가이드의 3 m 간격은 판재/파이프 항목이므로 코일에 적용하지 않는다 [C4].
- **chocks/wedges:** 하단 코일의 구름을 막도록 받침 위에 쐐기를 놓고 목재에 고정한다 [C3][C5]. 쐐기 각도·높이·길이, 압축/전단·고정능력, 코일 접촉 위치를 저장한다. ‘못이 있다’는 bool만으로 큰 수평 지지력을 생성하지 않는다. 탱크톱에 직접 놓은 쐐기는 받침과 같은 상태로 인정하지 않는다 [C3].
- **key 위치:** 선수미 각 row의 양쪽에서 중앙으로 쌓고, 남은 간극에 두 하단 코일을 지지점으로 삼는 key를 배치한다 [C2]. 큰 틈에는 복수 key가 필요할 수 있다. American Club §5.5.2.1(g)는 틈이 key OD의 60%를 넘으면 2개 key를 고려하고, 너무 깊이 들어가는 배치를 피하라고 한다 [C4]. 이를 §4.2의 기하 검사와 함께 적용한다. **key 자중은 하단 코일을 바깥으로 미는 힘도 만든다.**

### 3.2 단수 관행과 래싱

| 조건 | 공개 가이드가 말하는 관행 | 시뮬레이션 처리 |
|---|---|---|
| 코일 약 10~12 t 이하 | UK P&I ‘Locking coils’, 인쇄 p.119: 3단 적재 가능 사례 [C2] | 3단 자동 안전 판정 불가; 하중/지지/변형/CSM 검사 필요 |
| 그보다 무거운 코일 | 같은 가이드는 더 올리기보다 새 row 시작을 권고 [C2] | 관행 초과 경고와 해석값을 각각 표시 |
| 15 t 초과를 2단 이상 | American Club §5.5.2.1(e): Loading Manual/선급 확인 [C4] | 미제공이면 ‘강도 확인 필요’. 고정된 단수 상한으로 생성 자체를 금지하지 않음 |
| 1단만 적재 | locking coil/보강 없는 단층의 이동 위험에 주의 [C1][C2] | ‘1단이므로 안전’ 처리 금지 |

CSS Annex 6 §3 [C1]의 목표는 코일을 하나의 움직이지 않는 block으로 만드는 것이다. wire/steel band/동등 수단, 모서리 보호, 재장력 가능성, 상단/끝단/빈 공간의 구속을 검사한다. 완전 바닥 적재·충분한 shoring일 때 locking coil 이외 래싱 생략이라는 제한된 설명을 **부분 적재·젖은 표면·빈틈에 일반화하지 않는다**. 소수 코일은 cradle/wedge/shoring과 선체에 대한 적절한 고박이 별도로 필요하다.

제조 때 감긴 포장 band와 선적용 lashing은 별도다. 밴드 개수만으로 강도를 추정하지 말고 인증 MSL, 연결부/장력기, 방향, 모서리 보호, 실제 하중 전달 경로를 받는다. 코일끼리 묶은 band는 block 내부를 구속하지만 block 전체를 선체에 연결하는 외력을 자동 생성하지 않는다. 임의 선체 부재에 매다는 방식은 CSM/구조 확인 없이 생성하지 않는다.

## 4. 사전 판정 수식 초안

### 4.1 탱크톱: 등분포 하중과 국부 접촉압의 분리

요청의 `코일 무게 / dunnage 접촉 면적`은 **국부 평균 접촉압** 계산이다. 이를 선박 카탈로그의 등분포 t/m²와 직접 비교하는 한 가지 식으로 끝내면 허위 초과/허위 통과가 생긴다. ClassNK TEC-0798 본문 [K1]은 무게·크기·단수·받침 수와 내저판 집중하중을 함께 검증한다. DNV의 Steel Load Planner도 선박별 구조 모델을 사용한다 [K2].

```text
Rj = 받침/바닥 영역 j로 실제 전달되는 수직 반력              [kN]
A_contact,j = b_j × l_contact,j                            [m²]
p_contact,j = Rj/A_contact,j                              [kPa]
q_contact,j = Rj/(g × A_contact,j)                         [t/m²]

q_patch = (해당 구조 영역에 전달된 총 질량 상당량)/A_effective [t/m²]
U_UDL = q_patch/q_allow,UDL
```

`A_contact`는 목재-탱크톱 실지지 면적이며 코일-목재의 좁은 접촉 면적과 다르다. 목재 전 길이, 줄 사이 빈 공간, 홀드 전체 면적을 자동으로 포함하지 않는다. `A_effective`와 허용하중의 적용 면적/스팬은 선박 적재 매뉴얼·보강재 배치가 정한다. 추가로 판재 휨/국부 항복, 보강재·floor/girder 반력, 목재 압축/휨, 홀드 총중량, 선체 종강도(SF/BM)를 검사해야 한다. 이 입력이 없으면 등분포 **예비 스크리닝만** 수행한다.

유도 예: 20 t를 실제 바닥 지지면적 3 × 0.20 × 1.00 = 0.60 m²로 전달하면 국부 평균은 33.33 t/m² = 327 kPa다. 승인된 유효 분산면적이 2.0 m²라면 q_patch는 10 t/m²다. 서로 다른 두 값이며, 15 t/m² UDL과 비교 가능한 것은 적용 조건이 일치하는 후자다. **분산면적 2.0 m² 자체도 예제 가정**이다.

상단 하중은 접촉망을 따라 하단으로 전달한다. 삼각 적층에서 각 코일에 무조건 `자중 × 단수`를 부여하지 않는다. 동적 압력은 `m(g ± a_z)`와 수평 전도 모멘트로 별도 산정하고, 허용값이 이미 동적 계수를 포함했는지 확인해 중복 계수를 피한다.

### 4.2 key/locking coil 기하·접촉각·미끄럼

먼저 축이 평행하고 축방향 폭이 충분히 겹치는 2D 단면을 가정한다. 하단 같은 반지름 R, 상단 r, 하단 **외주 사이 수평 간극** s일 때:

```text
d = 2R + s                           # 하단 중심 거리
h = sqrt((R+r)² − (d/2)²)            # 하단 중심선에서 상단 중심 높이
θ = asin(d/[2(R+r)])                 # 접촉 법선과 수직 사이 각도
z_top = z_bottom_center + h
침하 깊이 δ = 2R − (R+h−r) = R+r−h  # 하단 정상에서 key 최하점까지
```

`s ≥ 0`, 제곱근 실수, `0 < d < 2(R+r)`, 상단 중심이 두 지지점 위, 실제 코일/받침 비관통을 검사한다. `s ≥ 2r`이면 같은 크기 하단 기준 두 접촉으로 받치는 높이가 사라진다. `s=0`도 일반 cantline 접촉이며 특별히 gap을 만들어야만 key가 되는 것은 아니다.

하단 OD가 다르면 같은 높이 중심으로 가정하지 않는다. 하단 중심 C₁,C₂, 거리 d, `s₁=R₁+r`, `s₂=R₂+r`, `e=(C₂−C₁)/d`, 단면 위쪽 단위 수직벡터 n에 대해:

```text
a = (s₁² − s₂² + d²)/(2d)
h = sqrt(s₁² − a²)
C_key = C₁ + a e + h n
|s₁−s₂| < d < s₁+s₂
```

위쪽 교점 선택, 무게중심 투영·축방향 overlap·3D 기울기·지지점 실제 존재를 추가 검사한다. θᵢ는 `(C_key−Cᵢ)/sᵢ`에서 구한다. 납작해진 하단 코일은 원 접촉 해석의 적용 범위 밖이다.

**정적 평형 유도:** 상단 총 유효 하향력 P와 같은 크기 두 하단, 마찰 없는 접촉에서 `N=P/(2cosθ)`, 하단 하나를 벌리는 힘 `H=P tanθ/2`. 수평 관성력 Q가 있으면 `N±=P/(2cosθ) ± Q/(2sinθ)`이고 `N± ≥ 0`가 접촉 유지의 필요조건이다. 즉 `|Q|/P ≤ tanθ`는 이 이상화의 **접촉 이탈 경계**이며 `μ ≥ tanθ` 같은 마찰 규정이 아니다. θ가 90°에 가까우면 반력이 발산한다.

실제 미끄럼/구름 판정은 각 접촉에서 `Nᵢ≥0`, `|Tᵢ|≤μᵢNᵢ`, `ΣF=0`, `ΣM=0`를 동시에 만족하는지 풀어야 한다. 하단 코일-목재 마찰, 쐐기/벽/shoring 강도, 래싱까지 포함한다. 단순 축방향 미끄럼의 참고 상한은 `μΣNᵢ`지만 다른 방향 하중과 같은 마찰 여유를 중복 사용하지 않는다. 상단이 안착해도 하단 H가 지지능력을 넘으면 위험이다.

검산 예: R=r=0.8 m, s=0 → h=1.38564 m, θ=30°, δ/OD=0.133975. 14.797 t key의 자중만으로 접촉 법선은 각 약 83.81 kN, 바깥쪽 힘은 각 약 41.90 kN이다. 여기에 상부 하중과 운동 가속도를 더한다.

**관행 수치의 한계:** [C4] §5.5.2.1(g), 그림 5.3~5.4는 60% gap과 약 1/3 침하의 경험적 설명이다. 본 예비안은 `δ/OD > 1/3`를 검토 경고로 제안하지만 문구/그림 해석을 실선 승인 기준으로 확정하지 않는다. 두 규칙은 수학적으로 동치가 아니다. 같은 반지름에서 s/OD=0.6이면 δ/OD=0.4이므로 별도 검사가 필요하다.

### 4.3 마찰계수

CSS Annex 13 §7.2.3, Table 5의 고박 계산 계수 [C6]다.

| 접촉 | 건조 | 젖음 | 구현 시 처리 |
|---|---:|---:|---|
| 강-목재 | 0.3 | 표에 별도 습윤값 없음 | 젖은 상태를 자동 0.3 확정하지 않고 적용성 확인 경고; 민감도용 0~0.3 스윕은 추정 시험 |
| 강-강 | 0.1 | 0.0 | 젖은 강-강을 강체 기본 마찰값으로 덮어쓰지 않음 |
| 목재-목재 | 0.4 | 0.4 | 적층 dunnage의 각 계면 검사 |
| 강-고무 | 0.3 | 표에 별도 습윤값 없음 | 인증 마찰 증대재는 반복 하중 전단내력/조건까지 확인 |

유분·먼지·서리·압력·패킹 재질 때문에 달라질 수 있다. 특히 방청유가 있는 냉연 포장을 bare steel의 마찰로 확정하지 않는다. 권취 **층간 마찰**은 외부 강-강 마찰과 별도 입력이다. Chaos의 두 Physical Material 결합 규칙도 지정해야 하며 해석 μ와 엔진의 최종 접촉 μ를 각각 기록한다.

### 4.4 CSS 횡/종/수직 가속도

기준: MSC.1/Circ.1623(2020), Annex 13 §7.1, Tables 2~4 [C6]. 아래는 lower hold 발췌다. 적층 높이가 바뀌면 해당 높이 표/정식 모델로 전환한다.

| AP에서 위치 x/L | 0.1 | 0.5 | 0.9 |
|---|---:|---:|---:|
| a_y,0 (m/s²) | 5.5 | 5.0 | 5.9 |
| a_z,0 (m/s²) | 7.6 | 4.3 | 9.2 |

`a_x,0=1.5 m/s²`. 기본: LBP100 m, service speed15 kn, B/GM≥13, 전해역/연중. 도표는 [C7]과 교차 확인했다.

```text
c_Lv = 0.345 v/√L + (58.62 L − 1034.5)/L²    # §7.1.8; L=LBP[m], v[kn]
a_x = a_x,0 × c_Lv
a_z = a_z,0 × c_Lv
a_y = a_y,0 × c_Lv × c_GM
F_x,y,z = m × a_x,y,z + 적용 가능한 풍압/해수 충격력
```

L 범위는 50~300 m. **횡방향만** Table 4 보정: lower hold B/GM=3/7/13 이상에서 c_GM=1.24/1.15/1.00 [C6]. 표 사이 보간은 구현 제안이며 범위 밖 clamp로 통과시키지 않는다.

유도 예: L=140 m, v=15 kn, B/GM≥13, lower hold x/L=0.5 → c_Lv≈0.803301, `(a_x,a_y,a_z)≈(1.20495,4.01651,3.45419) m/s²`. 본 문서 식 검산이며 실선 측정이 아니다. B=23.4, GM=3.0이면 B/GM=7.8로 c_GM 보정이 추가된다. 큰 GM도 횡가속도 증가 경고가 필요하다.

a_y는 횡요 중력을 포함하고 a_z는 정적 중력을 제외한다 [C6]. 구현 시 g 중복/누락을 검사한다. peak 세 축의 동일 위상 사인파를 실제 파랑 재현이라고 표시하지 않는다. 계류 작업과 항해 고박도 별도 load case다.

전체 고박 평형은 §7.2의 횡미끄럼·횡전도·종미끄럼을 구현하고, 종미끄럼의 수직력 보정을 생략하지 않는다. §7.3 대안 방법과 계수를 혼합하지 않는다. 감속에 의한 횡가속도 저감은 §7.2.6.3 Note가 허용하지 않는다 [C6]. 날씨 저감은 승인 절차 확인 전 기본 비활성으로 제안한다.

### 4.5 하단 코일 ovality 위험 근사

**근거 수준: 연구 기반의 미보정 screening 제안.** 공개 자료에서 OD/ID/무게만으로 해상 적층 코일의 허용 ovality를 확정하는 공통식을 찾지 못했다. Anderssen 외, *Analysis of coil slumping* [R1] §2, pp.93~94는 권취 코일을 층간 접촉/슬립을 가진 구조로 다루며, 예제에서 강 자체 탄성계수와 유효 반경방향 강성이 크게 다르다. 이 논문의 제조/보관 조건을 해상 적층 검증으로 옮기지 않는다.

```text
O = (D_max − D_min)/D_nominal                       # 측정 ovality, 정의를 고정
P_diam = 하단 코일에 전달되는 유효 지름방향 압축력   [kN]
S_load = P_diam/width                              [kN/m; 선하중 지표]
ΔD ≈ P_diam/k_diam                                 [m; 실험 보정 k_diam[kN/m]]
O_pred ≈ O_initial + 2ΔD/D_nominal                  # 세로 감소=가로 증가라는 가정
```

k_diam은 해당 코일의 OD/ID·강종·판두께·권취 장력·층간 마찰·밴드·온도·접촉 폭/각도에서 보정한다. 미제공 시 `S_load`와 반력만 보고하고 `O_pred=미확인`으로 둔다. 200 GPa짜리 균질 강재 관으로 계산해 ‘변형 없음’이라 표시하지 않는다. 집중 접촉부 손상·telescoping·국부 좌굴은 위 1자유도 근사로 판정하지 못한다.

UI 예시 경고 구간 O=1%, 위험 O=2%는 **검증 fixture용 추정 임계값**일 뿐 제조사 허용값이 아니다. 실제 모델에 임계값/강성이 없으면 초록색 합격을 내지 않는다. 층수를 늘릴수록 접촉망을 다시 풀어 P_diam을 갱신한다. 실측 초기 ovality가 이미 크면 별도 사전 손상 상태로 남긴다.

### 4.6 KG 상승·횡경사

질량 모멘트의 유도식이다. 모든 z는 동일 keel 기준, y는 중심선 기준으로 한다.

```text
Δ' = Δ₀ + Σm_added − Σm_removed
KG' = (Δ₀KG₀ + Σm_added z_added − Σm_removed z_removed)/Δ'
GM_corrected = KM(해당 흘수/트림) − KG' − ΣFSM/Δ'
화물 w를 선내에서 dz 상승: ΔKG = w dz/Δ  (총 배수량 일정)
TCG = 총 횡방향 질량 모멘트/Δ
tanφ ≈ TCG/GM_corrected = Σ(m y)/(Δ GM_corrected)
```

작은 횡경사·양의 GM·고정 화물 가정이다. FSM은 t·m 단위의 자유표면 모멘트다. 예: Δ=25,000 t, GM=1.5 m에서 20 t 코일을 중심선으로부터 8 m 편재하면 φ≈0.24446°. 1,000 t를 6 m 올리면 같은 Δ에서 KG가 0.24 m 상승한다. 부두 크레인이 매단 코일은 선박에 아직 하중을 넘기기 전 선박 질량에 넣지 않는다. 선박 크레인으로 든 선내 화물은 suspension point로 인한 가상 KG 상승도 별도 계산한다.

실제 승인에는 정수력표, 탱크 적재/자유표면, GZ 곡선·면적, weather criterion, 트림·흘수·SF/BM·계류 조건이 필요하다. 2008 IS Code [R2] Part A §2.2~2.3은 GM 하나보다 많은 요건을 둔다. GM>0만으로 수용하지 않으며, GM≤0은 간이 횡경사 식을 적용하지 않고 위험/해석 불능으로 보고한다. |φ| 2°/5° 경고·위험은 **예제 UI 제안**이며 IMO 공통 항만 작업 한계가 아니다.

### 4.7 해치·크레인·장구 간섭

OD×폭의 코일만 검사하지 말고 hook block·슬링·spreader·C-hook counterweight·tong 다리와 개폐 폭의 swept volume을 검사한다. 해치 coaming, 닫힌 패널, 선체 측벽, 하부 이미 적재된 코일을 포함한다. 로프는 점이 아닌 경로/처짐 범위를 갖게 한다. 해치 크기와 크레인 outreach만 맞아도 심부/wing에 직접 내릴 수 있다는 보장은 없다 [C2] ‘Loading and stowing coils’.

제안 기본 여유 0.20 m는 **시각화/시험용 추정치**다. 부두-선박 상대운동, 조위·적재로 인한 흘수 변화, 훅 최저/최고 높이, 장구 headroom, 사용 반경의 SWL을 함께 계산한다. 명확한 관통은 위험, 양의 여유가 제안 마진보다 작으면 경고, 실제 운용 마진 미확인은 별도 표시한다.

## 5. 제품 부두 포털 크레인과 인양 장구

### 5.1 공개 사양 예

| 예 | 인양 용량·아웃리치 | 양정 | 레일 스팬 | 해석상 주의 |
|---|---|---|---|---|
| Weihua Portal Crane, 제조사 표의 hook 40 t 열 [L1] | 훅 정격 40 t; 반경 11~35 m | 레일 위 28 / 아래 15 m | 선택 10.5/12/14/16 m | 훅 아래 코일+분리 장구 총하중 기준으로 사용할 것을 제안. 현장 인증 load chart/공제 정의 확인 전 예비값 |
| SANY MQG4030S [L2] | 정격 40 t; 9.5~30 m | 해당 공개 페이지 미기재 | 해당 페이지 10.5 m는 ‘Wheel Base’ 표기 | wheelbase를 rail gauge로 확정하지 않는다. GA/브로셔 필요 |
| Liebherr LPS 420 E, 84 t 사양 [L3] | **on the ropes** 84 t(10.5~19 m), 50.6 t(30 m), 35.7 t(40 m), 27.3 t(48 m) | 부두 위 최대 43 / 아래 10 m | 고객별 설계 | rope 용량을 순 코일 SWL로 복사하지 않는다. 훅/rotator/장구 공제 및 실제 구성 확인 |

‘최대 40 t’와 ‘최대 반경 35 m’를 출처의 load chart 없이 모든 반경에서 동시에 보증한다고 일반화하지 않는다. §1.2 프리셋은 L1의 한 열을 예제로 택한 것이며 특정 부두 장비의 수용 완료가 아니다.

```text
m_coil,gross + m_below_hook_gear ≤ SWL_hook(radius, configuration)
# on-the-ropes 표라면 hook block/rotator 등 표의 공제 규칙을 추가 적용
장구 자체 WLL ≥ 실제 장구 하중
대칭 2-leg sling: T_leg = W/(2cosβ)  # β는 수직과 이루는 각
```

예: 코일 30 t+장구 2 t=32 t. 훅 기준 SWL 40 t이면 정적 이용률 0.8. 동적 인양 계수·가속도·편심은 별도이며 정격에 이미 들어간 계수를 중복 적용하지 않는다. 2-leg β=60°에서는 다리 하나 장력이 전체 중량 W와 같아진다.

### 5.2 훅+슬링 / C-hook / coil tong 비교

| 방식 | 입력/장점 | 제약·간섭·손상 위험 |
|---|---|---|
| 훅+허용된 섬유 슬링/spreader | eye·하부 접근 가능, 유연한 형상. 슬링 각도/다리 WLL/모서리 보호/제조사 사용 지시 필요 | 장착·해체 공간, 절단/꺾임·미끄럼, 상부 headroom. bare chain/wire를 코일에 직접 감는 방식은 기본 제외 [C3][C5] |
| C-hook | eye 수평 코일, 내경·폭에 맞는 prong으로 지지. TC-Engineering은 5~40 t 예, prong 3/4 또는 전폭 구성 [L4] | ID와 다리 단면·삽입길이, 편심/CG, counterweight 자중·회전공간 검사. ‘OD만 맞으면 집기’ 불가 |
| 전동 수평 coil tong | 폭 조절·회전·감지/인터록 가능; 제조사 예 15/32/45 t [L4] | 안팎 다리 형상·개폐 폭·압흔·포장 허용·자중·전원/고장 상태. 모든 tong을 외경 마찰 집게로 단순화하지 않음 |

인양 장구의 사용 조건은 제조사/화주별로 다르다. P&I가 허용하는 보호 슬링과 공장 취급 절차가 달라도 하나를 공통 규정으로 만들지 않는다. 장구 질량/접촉판/잠금 상태가 불명확하면 형상 시연과 인양 승인 상태를 구분한다.

## 6. 시뮬레이션 구현 권고

### 6.1 해석을 판정 기준으로 두는 이유

1. **해석 상태:** 코일/받침/선체 지지 접촉망, 하중 경로, 각 위험 지표, 기준 출처·판본·단위·입력 가정·미확인을 저장한다. 정상/경고/위험과 별도로 `판정 가능/입력 부족`을 둔다. 한 위험이 다른 항목의 정상으로 상쇄되지 않게 한다.
2. **사전 preview:** 배치·장구 선택·선박 상태 변경 시 revision 기반으로 해석한다. 저장된 배치를 유지하고 위반 항목과 초과율을 표시한다. 단수 자체로 생성을 막지 않되 실제 해치 높이·하중·간섭의 초과는 경고/위험으로 남긴다.
3. **Chaos 확인:** 강체 이동·접촉 이탈·구름·충돌·래싱 파단 시나리오를 확인하는 별도 실행이다. 결과에 dt/substep/solver 반복/μ 결합/질량/관성/접촉 형상/초기 관통/난수 시드를 기록한다. 조용히 sleeping한 결과는 안정성 증명이 아니다.
4. **추후 보정:** 기하·반력은 수식 fixture, 구조는 선박별 매뉴얼/선급, ovality는 제조사 또는 실험/FEM, 인양은 장구 인증과 비교한다. 본 조사에서는 이 검증을 수행하지 않았다.

Epic UE 5.3 *Simple versus Complex Collision* [E1]은 dynamic simulation에 쓸 simple shape/convex와 trimesh query를 구분한다. 원통 외형 전체를 한 convex hull로 만들면 eye 구멍이 메워진다. 외주 적층만 보는 근사와 eye에 장구가 통과하는 근사를 분리한다. 동적 코일의 구멍은 필요 시 여러 convex 조각으로 표현하되 이음부 contact와 계산 비용을 검증한다. `UseComplexAsSimple`로 동적 코일 문제가 해결됐다고 가정하지 않는다.

원통을 정n각형으로 근사하면 내접 다각형의 최대 반경 오차는 `ε=R(1−cos(π/n))`다(유도식). R=0.8 m일 때 n=24는 약 6.84 mm, n=48은 약 1.71 mm다. 모서리·평면이 인공적인 구름 저항과 key 접촉각을 만든다. 허용 간극보다 큰 오차/접촉 마진으로 판정이 바뀌지 않는지 확인한다. Capsule은 둥근 끝단 때문에 평평한 코일의 축방향 접촉과 다르므로 기본 대체형으로 권하지 않는다.

질량은 convex 체적 자동 추정보다 라벨 gross mass를 우선한다. 균일 중공 원통 초기 관성의 유도값은 `I_axis=m(R²+r²)/2`, `I_transverse=m[3(R²+r²)+width²]/12`다. 포장/장구/권취 불균일을 포함한 실제 관성과 구분한다. Chaos 강체는 코일 층간 슬립/ovality·목재 압궤·내저판 항복을 자동 해석하지 않는다.

기존 DTCore 기반 Actor/데이터 반영/UI 경로를 먼저 검토하고 프로젝트 adapter로 연결한다. 별도 위젯 관리자·센서 플러그인·센서 캡처를 만들 근거는 없다. 해석은 변경 시, 상태 문자열은 최대 5 Hz, 숨긴 상세는 조립 중지, 대규모 코일은 공간 인덱스/국소 접촉망 갱신을 제안한다. 성능 목표/실제 RHI 검증은 후속 구현 작업이며 이번 조사에는 측정값이 없다.

### 6.2 구현 후 테스트 케이스 제안

아래는 **미실행 테스트 설계**다. 정상은 표에 적힌 검사 범위의 정상이며 전체 실선 안전 승인을 뜻하지 않는다. 단위 시험의 구조 허용·k_diam·장구 WLL fixture는 가상값임을 결과에 남긴다.

| ID / 기대 | 입력·상황 | 확인할 결과 |
|---|---|---|
| N1 정상 | HR 대표값 OD1.6/ID0.762/폭1.25/η0.97 | net mass≈14.797 t, m/mm 단위식 일치 |
| N2 정상 | R=r0.8, s0, P=100 kN, 하단 위치 고정 | θ30°, h1.38564 m, 각 N57.735/H28.868 kN; 이 검사만 정상 |
| N3 정상 | 승인됐다고 가정한 유효 patch 4 m², 40 t, UDL15 t/m² | q10, 이용률2/3; contact 압력은 별도 출력 |
| N4 정상 | Δ25000/GM1.5, 코일20 t의 y8 m 편재 | 간이 heel≈0.24446°, 반대 y에서 부호 반전 |
| W1 경고 | UDL15, q13.5 t/m² | 예제 80% 경고 구간 진입; 생성/저장 유지 |
| W2 경고 | 16 t 코일 2단, Loading Manual 없음 | 단수 관행 경고+구조 미확인. 임의 2단 금지/안전 처리 금지 |
| W3 경고 | R=r0.8, s/OD0.6 | δ/OD0.4, 1/3 제안 기준 초과. 60% 규칙과 불일치 탐지 |
| W4 경고 | 해치/장구 최소 여유0.10 m | 제안 마진0.20 미달, 관통과 구별 |
| D1 위험 | 승인 유효 patch2 m²에40 t, UDL15 | q20, 이용률1.333; 넓은 홀드 빈 면적으로 희석하지 않음 |
| D2 위험 | R=r0.8, s1.61 m | 두 하단 위의 두 점 지지 불가; sqrt NaN 대신 원인 보고 |
| D3 위험 | key가 만드는 H100 kN, 실제 chock/shoring 총내력50 kN | 하단 벌어짐 위험. key의 접촉 유지와 별도 판정 |
| D4 위험 | wet steel/steel μ0, 수평50 kN, 다른 구속0 | 마찰 지지0으로 미끄럼 위험 |
| D5 위험 | 훅 SWL40 t, gross coil39+gear3 t | 총42 t, 이용률1.05; 장구 자중 누락 탐지 |
| D6 위험 | GM_corrected≤0 또는 명확한 hatch 관통 | heel 계산 무효/위험 또는 간섭 위험; 초록 상태 금지 |
| U1 미확인 | k_diam/제조사 ovality 허용값 없음 | O_pred 합격 대신 미확인; 반력·선하중 지표는 제공 |
| B1 경계 | L140/v15/x0.5 lower hold, GM 낮음/높음 비교 | 기본 가속도 예 검산, GM 증가 시 c_GM 증가; 중력 중복 없음 |
| B2 경계 | 다른 OD·폭·축방향 overlap 부족, wet/dry 변경 | 원 교점은 있어도 3D 지지가 없으면 부적합; μ 변경 재해석 |
| B3 경계 | n24/n48, dt 변화, 초기 관통0/작은 값, 반복 실행 | 해석과 Chaos 차이·수치 민감도 기록; 시각적 정지로 판정 덮어쓰기 금지 |
| B4 경계 | 1/2/3/4단 이상 연속 추가 후 제거/되돌리기 | 층수 상한 없이 재해석, 반력 합/질량 모멘트 보존, stale 결과 미적용 |

예제 상태 임계 제안은 이용률 U≤0.8 정상, 0.8<U≤1 경고, U>1 위험이다. 확인된 입력/동일한 허용값 정의가 있는 항목에만 적용한다. 제조사/선급 기준이 있으면 그것이 우선한다. 입력 부족은 이용률0으로 취급하지 않는다.

## 7. 공개 자료의 한계와 후속 확보 항목

- 상용 CSS Code 합본·유료 선급 계산 서비스·선박별 승인 매뉴얼/GA는 구매/접근하지 않았다. CSS Annex 6은 네덜란드 정부 공개본, Annex 13은 CR Classification이 공개한 IMO 2020 circular 원문으로 대체했다. 이것이 최신 모든 개정·해당 기국 승인 상태를 검토했다는 뜻은 아니다.
- 공개 P&I 가이드의 규격/단수/경험식은 조건부 권고다. 각 권고를 제조사/선급 허용값과 구분했다. Gard/UK P&I/Britannia는 요청의 예시이며 이번 수치 채택에는 원문 조항이 확인된 UK P&I·Skuld·American Club·Swedish Club을 사용했다.
- 실제 구조 판정에는 내저판 두께·보강재/늑골 위치·받침 강성/강도·구역별 허용반력·hold 총중량/종강도, 안정성에는 정수력/실제 ballast 자료, 변형에는 코일 재질·권취/검사 자료가 더 필요하다. 누락 상태에서도 배치·기하·질량·명백한 과하중 경고의 예비 구현은 가능하다.
- 이 문서의 테스트는 설계 목록이다. 물리/성능/UI 수용 결과, 회사 검수, 실선 안전성 승인으로 보고하지 않는다.

작성 검증: UTF-8·출처 ID 27개/URL 28개·수식 예제의 질량/접촉반력/가속도/횡경사/다각형 오차를 점검했다. 문서만 변경하며 source/build/layer/Editor/Chaos/perf 검증은 수행 대상이 아니다. research worktree의 DTCore gitlink는 b2504d1로 유지되지만 submodule이 초기화되지 않아(`git submodule status` 접두사 `-`) submodule guard 전체는 통과하지 못했다. 미초기화 폴더에서 guard의 내부 `git status`가 상위 research 문서 상태를 읽는 제한도 있다. 초기화/update 및 메인 작업 폴더 확인은 하지 않았다.

## 8. 출처 목록 — URL·문서명·조항

모두 2026-10-22 공개 웹 확인. PDF 쪽수는 원문 인쇄 쪽수를 우선하고, 없는 경우 PDF page를 명시한다. 아래 링크와 본문 ID를 함께 사용한다.

| ID | 문서/발행자·확인 위치 | URL |
|---|---|---|
| S1 | Wagenborg, *Vikingbank*, Main particulars; Holds/hatches | <https://www.wagenborg.com/fleetlist/v-xxl-class/vikingbank> |
| S2 | Wagenborg, *Vxxl-class — Technical Information*, 2024, PDF p.2 General/Main particulars, Cargo capacities, Strengths | <https://www.wagenborg.com/media/51rhnizc/2024-vxxl-class.pdf> |
| S3 | Wagenborg, *Egbert Wagenborg*, Main particulars/Class notations/Holds/hatches | <https://www.wagenborg.com/fleetlist/easymax/egbert-wagenborg> |
| S4 | Wagenborg, *R-class — Technical Information*, 2024, PDF p.2; Reestborg 웹 Main particulars/Holds | <https://www.wagenborg.com/media/ew3jbuak/2024-r-class.pdf>, <https://www.wagenborg.com/fleetlist/r-class/reestborg> |
| S5 | Wagenborg, *Fleetlist 2024*, PDF p.1, Reestborg/Egbert/Vxxl 행, Gear 열 | <https://www.wagenborg.com/media/ofaoqprl/2024-fleetlist.pdf> |
| S6 | AAL, *S-Class HL MPV (19,000 dwt)*, Vessel specifications, Holds/Hatches, Max permissible load, Cargo gear | <https://aalshipping.com/fleet/s-class-hl-mpv-19000-dwt/> |
| S7 | Ferus Smit, *12000 DWT*, Main Particulars; Cargo Holds & Equipment | <https://www.ferussmit.com/portfolio-item/12000-dwt/> |
| P1 | NIPPON STEEL, *Special Steel Sheets and Coils*, U006en 2020, ‘Ordering Information’ §8 Packing weight, §9 Inside/outside diameters, pp.23~24 | <https://www.nipponsteel.com/product/catalog_download/pdf/U006en.pdf> |
| P2 | NIPPON STEEL, *Hot-Rolled Steel Sheets and Coils*, U001en 2020, Reference Tables §2 Coil Width/ID/OD/Mass calculation chart, pp.24~25(PDF p.14) | <https://www.nipponsteel.com/product/catalog_download/pdf/U001en.pdf> |
| P3 | NIPPON STEEL, *Cold-Rolled Steel Sheets and Coils*, U003en 2020, Product Size Range pp.16~19; Coil Width-Mass Curves p.26; Ordering Information p.28 | <https://www.nipponsteel.com/product/catalog_download/pdf/U003en.pdf> |
| P4 | voestalpine, *Cold-rolled carbon steels*, 10/2023, p.6 ‘Dimensions and forms of delivery’ | <https://www.voestalpine.com/stahl/en/content/download/4619/file/Data-sheet-cold-rolled-carbon-steels-voestalpine-EN-311023.pdf?inLanguage=eng-GB> |
| C1 | IMO, *CSS Code*, Resolution A.714(17), Annex 6 §1~3 ‘Safe stowage and securing of coiled sheet steel’; 정부 공개 재현본(이 문서의 Annex 13 판본 기준과는 별도) | <https://puc.overheid.nl/nsi/doc/PUC_2092_14/3/> |
| C2 | UK P&I, *Carefully to Carry — Consolidated Edition 2023*, Chapter 12 Steel, ‘Locking coils’ pp.118~119; ‘Loading and stowing coils’ p.119; §12.5 p.120 | <https://www.ukpandi.com/fileadmin/uploads/ukpandi/Documents/uk-p-i-club/carefully-to-carry/2023/UKPI_Carefully_to_Carry_2023_12.pdf> |
| C3 | Skuld, *Carriage of Steel Cargoes*, 2004-10-02, ‘Stowage and lashing of different types of products / Steel coils’ | <https://www.skuld.com/topics/cargo/steel/carriage-of-steel-cargoes/> |
| C4 | American Club, *Transport Guidance for Steel Cargoes*, §4.3.2 pp.38~39; §5.5.2.1 pp.46~48; figs.5.2~5.4 p.53; §5.7 pp.51~52 | <https://www.american-club.com/files/files/steel_cargo_guide.pdf> |
| C5 | Swedish Club, *Steel cargoes*, ‘2. During loading’, dunnage/wedges/handling | <https://www.swedishclub.com/loss-prevention/loss-prevention-advice/steel-cargoes/> |
| C6 | IMO, *MSC.1/Circ.1623 — Amendments to the CSS Code*, 2020-12-07, Annex 13 §7.1~7.3, Tables 2~5, Appendix 2~3; CR Classification 공개 원문 | <https://www.crclass.org/chinese/download/ti-tc/113/2-4%20MSC.1-Circ.1623%20-%20Amendments%20To%20The%20Code%20OfSafe%20Practice%20For%20Cargo%20Stowage%20And%20Securing%20(Css%20Code)%20(Secretariat).pdf> |
| C7 | Lloyd's Register imorules, *CSS Code Annex 13 §7.1*, Table 2 도표 교차 확인 | <https://www.imorules.com/GUID-3850633D-BFB9-4337-938D-9152C52F6A88.html> |
| K1 | ClassNK, *Steel coil loading program of existing dry cargo ships*, TEC-0798, 2010-01-07, p.1 본문 | <https://www.classnk.or.jp/hp/pdf/tech_info/tech_img/T798e.pdf> |
| K2 | DNV, *Steel Load Planner*, ‘How does … assess load-bearing capacity?’ 및 local strength 범위 설명 | <https://www.dnv.com/services/ship-classification-fleet-in-service/steel-load-planner/> |
| R1 | Anderssen, Fowkes, Hickson, McGuinness, *Analysis of coil slumping*, 2009 MISG proceedings(2010 발행), pp.90~108; §2 pp.93~94/Table 1 | <https://documents.uow.edu.au/content/groups/public/%40web/%40inf/%40math/documents/doc/uow073330.pdf> |
| R2 | IMO, *2008 IS Code*, MSC.267(85), Part A §2.2~2.3; Part B §3.1 자유표면, Annex 1 경사시험 상세 지침 | <https://wwwcdn.imo.org/localresources/en/KnowledgeCentre/IndexofIMOResolutions/MSCResolutions/MSC.267%2885%29.pdf> |
| L1 | Weihua, *Portal Crane*, ‘Main Technical Parameters’, hook 40 t 열/양정/반경/rail gauge | <https://www.weihuascm.com/portal-crane-en.html> |
| L2 | SANY, *Portal Slewing Crane*, MQG4030S 및 Wheel Base 표기 | <https://www.sanyglobal.com/product/port_machinery/portal_slewing_crane/68/> |
| L3 | Liebherr, *LPS 420 E*, technical data ‘Lifting capacities — 84 t’, Main dimensions(above/below quay, individual rail span), PDF pp.2~3 | <https://assets-cdn.liebherr.com/assets/api/c24d32f0-cf2b-4ea7-90ea-1b6726d778c8/Original/> |
| L4 | TC-Engineering, *Coil Hook, Coil Tong*, horizontal eye, rated examples, prong/coil dimensions, interlock | <https://www.tc-engineering.gmbh/our-products/coil-hook/> |
| E1 | Epic Games, UE 5.3 *Simple versus Complex Collision*, Usage/Collision Complexity | <https://dev.epicgames.com/documentation/en-us/unreal-engine/simple-versus-complex-collision-in-unreal-engine?application_version=5.3> |
