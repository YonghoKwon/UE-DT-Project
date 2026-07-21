# 센서 실시간 스트리밍과 캡처/내보내기

`WBP_VirtualSensorCaptureExportPanel`은 오른쪽 아래 모서리를 드래그해 크기를 바꿀 수 있습니다. 기본 크기는 900×620, 최소 크기는 620×360이며 접었다 펼쳐도 이전 크기가 복원됩니다. 패널은 다음 네 탭으로 나뉩니다.

| 탭 | 역할 |
|---|---|
| 캡처 | 선택 센서 1회 캡처, timed capture, 저장 폴더 확인 |
| 내보내기 | 현재 LiDAR 프레임을 CSV/JSONL/PCD/LAS/LAZ 파일로 저장 |
| 실시간 스트림 | 센서 측정 주기에 맞춘 세 Topic 전송 시작·중지 |
| 서버/로그 | STOMP/HTTP 설정, 연결 시험, 최근 전송 상태, JSON/Markdown 진단 보고서 |

## 세 가지 자동 스트림

스트림은 기본적으로 꺼져 있습니다. 서버 설정을 적용한 뒤 필요한 스트림만 시작하십시오.

| 스트림 | 기본 Topic | 메시지 |
|---|---|---|
| LiDAR 값 | `topic.virtual.sensor.lidar.0` | 스캔 완료 시 `virtual-lidar.v1` JSON |
| Camera 이미지 | `topic.virtual.sensor.camera.0` | 캡처 완료 시 Base64 JPEG가 포함된 `virtual-camera.v1` JSON 한 건 |
| Point Cloud | `topic.virtual.sensor.export.0` | 스캔 완료 시 선택한 CSV/JSONL/PCD/LAS/LAZ 바이트를 Base64로 감싼 `virtual-pointcloud.v1` JSON |

Camera 스트림은 JSON과 이미지 바이너리를 중복 전송하지 않습니다. 한 메시지에 메타데이터와 Base64 JPEG가 함께 들어갑니다. Point Cloud 스트림의 LAZ는 LAS 확장자만 바꾸지 않으며, 실제 LAZ 압축 실행 파일이 설정된 경우에만 동작하고 최대 1 Hz로 제한됩니다.

## 부하 및 유실 방지 정책

- 획득과 네트워크 처리를 분리하여 센서 스캔·캡처가 브로커를 기다리지 않습니다.
- 스트림/센서마다 직렬화 중 1개와 교체 가능한 최신 대기 프레임 1개만 유지합니다.
- 오래된 대기 프레임은 최신 프레임으로 교체하며 무제한 큐를 만들지 않습니다.
- 프레임당 전송 제출은 최대 2건, 전체 기본 대역폭은 16 MiB/s입니다.
- 자동 스트림은 기본 10건마다 broker receipt를 요청합니다. 수동 전송은 매번 receipt를 요청합니다.
- receipt는 broker 수락을 뜻합니다. 소비자 처리 완료를 확인하려면 ACK Topic을 설정하고 소비자가 같은 `requestId`를 회신해야 합니다.
- 연결이 끊겼을 때는 매 게임 프레임마다 재전송하지 않고 간격을 두고 재시도합니다.

로그에는 입력/전송 Hz, 마지막 frame ID, 대기 프레임 교체 수, 대역폭 대기, receipt/timeout, request ID, destination과 오류가 표시됩니다. 연속 스트림은 화면/파일 로그를 매 프레임 남기지 않으며, 필요할 때 `진단 보고서 저장`으로 `Saved/Reports`에 JSON과 Markdown을 생성합니다.

## 로컬 Artemis 검증

로컬 Artemis가 실행 중일 때 다음 명령은 Node 구독자를 먼저 연결한 뒤 Unreal 자동화 테스트가 세 Topic을 실제 발행하도록 합니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\run_artemis_stream_smoke.ps1
```

결과는 `Saved/Reports/artemis_sensor_stream_smoke.json`에 저장됩니다. 성공 보고서는 각 Topic의 `sensorId`, `dataKind`, `frameId`, byte 수와 schema를 포함합니다.

실제 `SensorRefactorTestMap`의 Camera와 LiDAR가 측정 주기마다 세 Topic을 연속 발행하는지, 그리고 전송 중 프레임 성능이 유지되는지는 다음 D3D12 테스트로 확인합니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild
```

이 테스트는 PIE 초기화 중 DTCore WebSocket 연결이 안정된 뒤 센서용 STOMP 연결을 시작하고, 세 Topic을 각각 두 건 이상 별도 소비자로 수신합니다. 이후 스트림을 10초 동안 유지하면서 평균 FPS, 1% low와 p95 frame time을 측정합니다. 결과는 `Saved/Reports/sensor_map_stream_rhi_smoke.json`과 `.md`에 저장됩니다.

로컬 기본 통과 기준은 평균 55 FPS 이상, 1% low 45 FPS 이상, p95 20 ms 이하입니다. 이는 비동기·bounded 설계가 게임 스레드 정지를 방지한다는 회귀 기준이며, 실제 부하는 센서 수, 해상도, Payload 크기와 네트워크 대역폭에 따라 달라질 수 있습니다.

UI 상태는 `Saved/SaveGames/MA0T10_VirtualSensorUI_v5.sav`에 저장됩니다. 패널 크기·탭·Topic·전송 간격은 복원하지만 비밀번호, token, 스트림 실행 상태는 저장하지 않습니다.
