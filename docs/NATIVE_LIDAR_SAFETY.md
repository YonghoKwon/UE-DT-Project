# native LiDAR 안전 상태

기준 구현은 [SensorNativeSafety.cpp](../Plugins/DTDevice/Source/DeviceNative/Private/SensorNativeSafety.cpp)와 [표시 문자열](../Plugins/DTDevice/Source/DeviceUI/Public/DeviceUI/LidarNativeSafetyText.h)입니다. native는 DXR inline ray query Tier1.1 및 장치 건강 상태를 요구합니다. 지원 vendor/driver identity가 있어도 미검증 조합은 시작 시 native/RHI byte/point oracle을 제한 시간 안에 통과한 뒤 검증 상태를 저장합니다. 검증된 조합은 재사용하며, 실패는 지원 RHI로 전환합니다. 특정 GPU 한 종만을 영구 whitelist로 쓰는 정책이 아닙니다.

## 상태와 운영

| 표시 | 의미 |
|---|---|
| `native 복구 확인 중(이전 비정상 종료, 이번 실행은 RHI 사용)` | 이전 비정상 세션 뒤 한 번의 안전 RHI 복구 실행. 정상 종료 후 marker를 정리하고 다음 실행에서 native 검증/재시도를 허용 |
| `native 비활성(비정상 종료 2회, 드라이버 변경 또는 안전 상태 초기화 필요)` | 해당 adapter/driver identity에서 비정상 종료2회. 정책을 조용히 우회하지 않음 |

장치가 정상인 일시적 native 제출 실패는 **그 세션만 RHI**로 사용합니다. 검증 캐시는 무효화하여 다음 실행에서 재검증하지만, 정상 종료를 비정상 횟수에 더하지 않습니다. device loss/Faulted는 별도로 unhealthy 상태를 남깁니다. 실제 device removal 뒤 같은 프로세스에서 장치를 복구했다고 보장하지 않습니다.

## 파일과 초기화

`FPaths::ProjectSavedDir()/NativeLidarSafety/` 아래에서 vendor/adapter/driver identity SHA-1에 따라 다음 파일을 사용합니다.

```text
active-<40자리 hash>.guard          실행 소유 marker(pid/생성 시각/recovery)
active-<40자리 hash>.guard.state    failures/validated/revalidate/faulted/reason 상태
```

초기화가 필요한 경우 **관련 게임/Editor/worker를 모두 정상 종료한 뒤 해당 identity의 `.guard.state` 한 파일만** 삭제합니다. 드라이버 변경은 새 identity를 선택합니다. 실행 중 marker나 모든 Saved를 일괄 삭제하지 않습니다. 같은 identity의 살아 있는 PID/생성 시각을 검사해 동시 소유를 구분합니다. 학습 워커는 고유 UserDir/Saved를 사용합니다.

현재 메모리/권한 등 미해결과 수용 경계는 [KNOWN_ISSUES](KNOWN_ISSUES.md) 한 곳에서 관리합니다. 과거 수명/추적 기록은 [26h 원문](archive/LIDAR_NATIVE_LIFETIME_26h.md)에 보존하며 현재 기본값 설명으로 읽지 않습니다.
