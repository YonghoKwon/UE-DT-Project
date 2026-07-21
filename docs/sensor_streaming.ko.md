# 센서 실시간 스트리밍과 캡처/내보내기

`WBP_VirtualSensorCaptureExportPanel`은 오른쪽 아래 모서리를 드래그해 크기를 바꿀 수 있습니다. 기본 크기는 900×620, 최소 크기는 620×360이며 접었다 펼쳐도 이전 크기가 복원됩니다. 패널은 다음 네 탭으로 나뉩니다.

| 탭 | 역할 |
|---|---|
| 캡처 | 선택 센서 1회 캡처, timed capture, 저장 폴더 확인 |
| 내보내기 | 현재 LiDAR 프레임을 CSV/JSONL/PCD/LAS/LAZ 파일로 저장 |
| 실시간 스트림 | 센서 측정 주기에 맞춘 세 Topic 전송 시작·중지 |
| 서버/로그 | STOMP/HTTP 설정, 연결 시험, 송신·자체 수신 상태, JSON/Markdown 진단 보고서 |

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

## 에디터 내부 Topic 자체 수신

`SensorRefactorTestMap`의 `SensorTest_ExternalSources`는 맵 시작 시 DTCore의 기존 `UDxWebSocketSubsystem` 연결을 통해 세 Topic을 자동 구독합니다. 송신 Broker에 제출했다는 사실만 확인하는 것이 아니라, 같은 에디터가 실제 메시지를 다시 수신하고 형식까지 검증하는 진단 기능입니다.

| Topic | 수신·검증 클래스 | 검증 내용 |
|---|---|---|
| LiDAR | `UVirtualLidarStreamReceiverTC` | schema, SensorId, FrameId, 측정·검출·Payload 점 수, 해상도와 points 배열 일관성 |
| Camera | `UVirtualCameraStreamReceiverTC` | schema, 해상도, encoding, byteSize, image 존재와 JPEG 바이트 |
| Point Cloud | `UVirtualPointCloudStreamReceiverTC` | schema, format, pointCount, byteCount, data와 CSV/JSONL/PCD/LAS/LAZ 형식 서명 |

기존 송신 Body에는 `MESSAGE_ID`가 없으므로 이 수신기는 `DT_TransactionCode` 자동 분배에 등록하지 않습니다. 구독한 Topic이 Handler를 결정하며, 각 Handler는 `ParseToStruct`에서 백그라운드 파싱하고 `ProcessStructData`에서 게임 스레드 상태와 제한된 로그만 갱신합니다. 수신 프레임을 Sensor Actor에 주입하거나 다시 송신하지 않으므로 자체 수신이 재송신 루프를 만들지 않습니다.

일반 필드 검증은 매 프레임 수행하고, Base64 디코딩과 실제 JPEG·point cloud 바이트 검증은 각 Topic의 매 10번째 프레임에 수행합니다. Topic별로 처리 중 1개와 최신 대기 1개만 유지하고 전체 동시 파싱은 최대 2개로 제한합니다. 밀린 중간 프레임은 큐에 쌓지 않고 `교체` 카운터로 기록하며, 원문 Payload와 Base64 본문은 로그에 출력하지 않습니다.

Capture/Export 패널의 `서버/로그` 탭에서 다음 항목을 확인할 수 있습니다.

- `수신 구독 끊기`: 세 Subscription을 정리하고 자동 재시도를 중단합니다.
- `수신 다시 연결`: 중복 구독 없이 DTCore 연결 상태를 확인한 뒤 세 Topic을 다시 구독합니다.
- Topic별 상태: 구독 상태, 수신·검증 성공·실패·교체 횟수, 최근 SensorId·FrameId·bytes·파싱 시간과 검증 결과를 표시합니다.
- Broker 경고: 송신 Profile의 Broker URL과 DTCore 수신 Broker가 다르면 동일 메시지를 자체 수신할 수 없음을 알립니다.

전송 Topic 설정을 적용하면 활성 수신기도 같은 Topic으로 다시 구독합니다. DTCore 연결이 아직 준비되지 않았다면 제한된 backoff로 재시도하고, PIE 종료 시 모든 Subscription과 대기 작업을 정리합니다. 이 기능의 Broker 접속 정보는 송신 패널 값이 아니라 DTCore 설정을 따릅니다.

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

이 테스트는 PIE 초기화 중 DTCore WebSocket 연결이 안정된 뒤 센서용 STOMP 연결을 시작하고, 세 Topic을 각각 두 건 이상 별도 Node 소비자와 에디터 내부 수신기로 확인합니다. 내부 수신기의 성공·실패 카운터와 FrameId 증가도 함께 검사한 뒤 스트림을 10초 동안 유지하면서 평균 FPS, 1% low와 p95 frame time을 측정합니다. 결과는 `Saved/Reports/sensor_map_stream_rhi_smoke.json`과 `.md`에 저장됩니다.

로컬 기본 통과 기준은 평균 55 FPS 이상, 1% low 45 FPS 이상, p95 20 ms 이하입니다. 이는 비동기·bounded 설계가 게임 스레드 정지를 방지한다는 회귀 기준이며, 실제 부하는 센서 수, 해상도, Payload 크기와 네트워크 대역폭에 따라 달라질 수 있습니다.

UI 상태는 `Saved/SaveGames/MA0T10_VirtualSensorUI_v5.sav`에 저장됩니다. 패널 크기·탭·Topic·전송 간격은 복원하지만 비밀번호, token, 스트림 실행 상태는 저장하지 않습니다.
