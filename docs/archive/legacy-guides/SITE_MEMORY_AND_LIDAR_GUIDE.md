> **이전 현장 가이드 원문, 현재 기준 아님.** 현재 사용은 상위 문서 색인을 따릅니다.

# 현장 메모리·LiDAR 점검 가이드

기준: 2026-11-02, PR #95 머지 `master 23d143b`, DTCore `bc60f05`.
실행·설정 변경은 담당자와 정한 점검 시간에 한다. 이 문서 갱신에서는 게임·Editor·패키지·빌드·GPU·화면을 실행하지 않았다.

## 1. 시작할 때 확인

1. 배포 버전, GPU·드라이버, 맵, 센서 수·프로필, 실제 backend를 기록한다.
2. 센서별 실제 획득 Hz와 PCD 송신·외부 수신 상태를 확인한다. 요청 Hz만으로 정상 판정하지 않는다.
3. LiDAR 선택 후 **설정의 백엔드 진단**에서 아래 안전 상태 문구를 확인한다.
4. 메모리는 같은 빌드·장면·센서·미리보기·출력 조건으로 점검한다. 영상 재생이나 다른 Unreal/빌드가 겹친 실행은 판정에서 제외한다.

## 2. 메모리 판정

**시작 후 30분을 제외하고, 30분~종료의 OS Commit OLS 기울기가 ≤8MiB/h이면 PASS**다.
준비 구간을 뺀 관찰 시간이 최소 60분이어야 하므로 전체 실행은 **최소 90분**, 권장 3시간이다.
30분 이하 자료는 준비 구간뿐이라 판단할 수 없고, 30~90분 미만도 관찰 길이가 부족하다.

| 확인 항목 | 현장 해석 |
|---|---|
| OS Commit | 프로세스의 private Commit. **MiB/h**는 절대 사용량 MiB와 구분한다. |
| Working Set / RSS | RAM 상주량 참고값. 급감해도 Commit이 유지되면 해제·해결로 단정하지 않는다. |
| 마지막 1/3 기울기·10~30분 증가량 | 준비·후반 추이를 확인하는 보조 값. 현행 판정을 대신하지 않는다. |
| `old gate` | 예전 10~30분/꼬리 구간 값. 과거 실패 이력과 현행 판정을 구분한다. |
| plateau | 후반 증가가 줄어드는 진단 분류. 무한시간 누수 부재나 현장 수용을 보장하지 않는다. |

완주(`summary.complete=true`), 외부 OS 표본·시계, 표본 간격과 환경 감시가 있어야 한다.
0Hz 센서를 평균에서 빼거나 불리한 표본·계단만 제거하지 않는다. 준비된 native 점검 실행 결과는 다음 **사후 분석**으로 확인한다.

```powershell
python Scripts/analyze_memory_acceptance.py '<점검 실행 디렉터리>'
```

`acceptance-r37.json`의 `Status`, `Post30.slope_per_hour`, `OldGate`, `Plateau`, `ActualHz`, `NativeFallbacks`, `Reasons`를 읽는다.
이 분석기는 최종 native 점검 조건도 확인한다. 일반 업무 로그나 다른 backend 자료에 그대로 적용하지 않는다.
해치 runtime 진단은 준비 디깅·센서 구성을 바꾸고 종료 시 게임을 닫으므로 업무 세션과 분리한다.

### 현재 근거

| 기록 | OS Commit MiB/h | 의미 |
|---|---:|---|
| r36 원료 하역(raw-quay) 3시간 | 약 **+0.6** | 30~180분 원시 표본의 사후 OLS 약 +0.582. 귀속 진단 조건의 참고값. |
| r37 해치 3시간 | **+5.14** | 현행 30~180분 기준 PASS. |
| r37 Slab native 3시간 | **+2.36** | 현행 기준 PASS. native 명시 회차이며 맵 기본 DepthProjection과 구분. |
| r37 원료 하역 3시간 | **+2.69** | 현행 기준 PASS. |

r36은 귀속 프로브·memreport/GC가 포함된 진단 자료이고, r37은 해당 무거운 진단을 끈 조건이다. 동일 조건의 수정 전후 A/B로 해석하지 않는다. r37은 fallback 0과 약 20Hz도 확인했다.

해치의 **계단식 약 5MiB/h 증가는 현재 기준을 통과**했다. **24시간 운영을 준비할 때는 별도 장시간 관찰을 권장**한다.
24시간 선형 증가량으로 확정하거나 증가가 영원히 멈춘다고 해석하지 않는다. 장시간 추적은 별도 과제다.
예전 30분·꼬리 기준 FAIL은 당시 이력으로 보존한다. 원래 #26 메모리·디깅 5쌍/ABAB는 사용자 결정으로 새 메모리 기준에 대체됐다. 표준 QA-02 RT 성능/GT 2% 검증은 별도다.

판정 정의: [MEMORY_ACCEPTANCE_R37.md](../../MEMORY_ACCEPTANCE_R37.md). 분석 코드: [analyze_memory_acceptance.py](../../../Scripts/analyze_memory_acceptance.py).

## 3. native LiDAR 안전 상태

native는 센서 소유 BLAS/TLAS·버퍼를 유지하는 GPU 광선 추적 경로다. Windows D3D12·DXR Tier 1.1·단일 렌더링 GPU/노드 조건 등이 필요하다.
코드는 NVIDIA/AMD/Intel의 식별 가능한 장치·드라이버 조합에 안전 정책을 적용한다. 실측 확인 장치는 RTX 4070 Ti / 621.01이며 다른 장치의 현장 성능까지 검증한 목록은 아니다.

- 검증된 조합/캐시가 있으면 native를 선택할 수 있다.
- 미검증 조합은 시작 시 제한된 **native/RHI 결과 일치 자기 검증(startup oracle)**을 한다. 통과 조합은 저장하고, 대기·실패·미지원 조건에서는 지원 RHI를 쓴다.
- 비정상 종료 뒤에는 다음 **1회를 RHI 복구 실행**으로 둔다. 건강하게 끝나면 그다음 실행에서 native를 다시 시도할 수 있다.
- 비정상 종료가 **2회 누적**되면 해당 장치·드라이버 조합의 native를 비활성화한다. 정상 RHI 실행만으로 차단을 지우지 않는다.
- 정상 장치의 일시적 native 제출 실패는 **그 세션만 RHI**로 전환한다. 정상 종료를 비정상 종료 횟수에 더하지 않고, 캐시를 무효화해 다음 기본 실행에서 oracle을 다시 확인한다.
- 실제 장치 손실/Faulted는 별도다. 제거된 GPU가 같은 프로세스에서 복구됐다고 보지 말고 로그를 보존한 뒤 종료·재시작한다.

### 상태 문구와 대응

다음은 코드와 자동 문구 테스트로 확인한 문구다. **이 두 문구의 실제 화면 표시는 미확인**이다.

| 표시 문구 | 의미·대응 |
|---|---|
| `native 복구 확인 중(이전 비정상 종료, 이번 실행은 RHI 사용)` | 이번 1회는 안전 복구 실행이다. 결과를 기록하고 정상 종료한다. |
| `native 비활성(비정상 종료 2회, 드라이버 변경 또는 안전 상태 초기화 필요)` | 해당 조합이 차단됐다. 원인을 확인한 뒤 담당자와 아래 초기화 또는 드라이버 변경을 진행한다. |

지원 RHI도 GPU 광선 추적 조건이 필요하다. GPU 광선 추적 자체가 불가능하면 CPU fallback 여부를 별도로 확인한다.
DepthProjection은 깊이 영상 투영 방식이므로 native와 정확도·FOV·Hz가 같다고 가정하지 않는다.

### 안전 상태 초기화

1. 해당 프로젝트의 게임·Editor 인스턴스를 모두 정상 종료한다.
2. 해당 실행의 실제 Saved 폴더에서 `Saved/NativeLidarSafety/active-<adapter·driver SHA1>.guard.state`를 찾는다.
3. 로그의 adapter/driver와 파일의 `failures`·`reason`을 대조한 뒤 **해당 `.guard.state`만 삭제**한다. 다른 조합·개인 SaveGame은 지우지 않는다.
4. 다시 시작해 안전 상태와 실제 backend를 확인한다. 드라이버 변경은 새 조합으로 판정한다.

SHA1은 vendor·장치명·드라이버를 포함한 조합 식별자다. 파일명을 추측해 만들지 않는다.
별도 `-UserDir`가 있으면 그 UserDir의 Saved 아래를 확인한다. 실행 중 소유자가 쓰는 `.guard`를 강제로 지우지 않는다.
stale `.guard`가 남으면 상태 파일 초기화 후에도 1회 RHI 복구를 거친다. 강제 native 진단 옵션을 복구 수단으로 쓰지 않는다.

상세: [native 안전 상태·초기화](../LIDAR_NATIVE_LIFETIME_26h.md#r38b-native-안전-상태-표시와-초기화).
코드: [SensorNativeSafety.cpp](../../../Plugins/DTDevice/Source/DeviceNative/Private/SensorNativeSafety.cpp), [상태 문구](../../../Plugins/DTDevice/Source/DeviceUI/Public/DeviceUI/LidarNativeSafetyText.h), [하드웨어 조건](../../../Plugins/DTDevice/Source/DeviceNative/Private/SensorNativeRayTracing.cpp).

## 4. 남은 한계와 인수 기록

| 한계 | 운영 시 주의 |
|---|---|
| `ProcessStamp`의 `ACCESS_DENIED` | PID 생성 시각 확인이 제한된다. 동시 실행 소유자 확인 없이 guard를 강제로 지우지 않는다. |
| 수신 루프 4MiB 처리 예산 | 큰 프레임·고부하 수신의 제약은 남아 있다. 전체 메시지/송신 큐 상한으로 해석하지 않는다. |
| native 상태 문구의 실제 화면 | 자동 문구 검증만 완료. 현장 직접 확인은 별도이며 이번 작업에서 재시도하지 않았다. |
| 장시간·현장 수용 | 3시간 로컬 결과가 모든 현장/드라이버·24시간 운영을 보장하지 않는다. |

이상 발생 시 버전·맵·장치/드라이버·센서별 Hz·backend·출력 상태·발생 시각과 로그를 담당자에게 전달한다.
공유 전에 비밀번호·토큰을 제거하고 원본 대용량 PCD/영상 본문을 로그에 붙이지 않는다.

## 검증 원본

원본 기준 폴더는 `C:/Unreal Projects/wt_26final/Saved/Reports/LIDAR-MEMORY-26-FINAL/`이다.
Saved 리포트는 Git 배포물에 포함되지 않으므로 현장 인수 시 요약·JSON을 별도로 확보한다.

| 근거 | 폴더/파일·버전 |
|---|---|
| r36 진단·plateau | `rootcause-commit/r36/RESULT.md`, `rootcause-commit/r36/attr/raw180-a1/attribution-analysis.json`, `rootcause-commit/r36/attr/raw180-a1/os-counters.jsonl`; 진단 코드 `98b3fee` |
| r37 현행 메모리 판정 | `revalidate-r37/RESULT.md`, 회차별 `acceptance-r37.json`; `598c2a3`(판정 분석기), `374f8ba`(180분 검증 허용) |
| r38b/r38c 안전 수정·남은 gate | `gates-r38/RESULT.md`, `result.json`, `merge-check.json`; `7ef51cb`(soft disable), `54ec540`(상태 표시), `94b0a25`(private 진단 기본 컴파일 제외) |
| 최종 통합 | PR #95, `master 23d143b`, DTCore `bc60f05` |
