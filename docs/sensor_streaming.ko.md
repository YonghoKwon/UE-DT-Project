# 센서 실시간 스트리밍과 캡처/내보내기

> 2026-08 Binary PCD 계약입니다. Camera와 LiDAR JSON 호환 스트림은 변경되지 않았습니다.

## ML-X(80) 20Hz 실시간 Point Cloud

실시간 Point Cloud Topic은 PCD v0.7 `DATA binary` 완전한 파일 바이트를 STOMP binary body로 직접 전송합니다. 형식은 `PCD Binary 고정`이며 CSV/JSONL/LAS/LAZ는 로컬 캡처와 수동 내보내기에서만 선택합니다.

| 항목 | 값 |
|---|---|
| Topic | `topic.virtual.sensor.export.0` |
| schema header | `virtual-pointcloud.pcd.v1` |
| content-type | `application/vnd.pcd` |
| 좌표 | 센서 로컬 X/Y/Z, meter |
| 필드 | `x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence` |
| 레코드 | little-endian 33 bytes, 구조체 padding 미사용 |
| 전달 | 완료 프레임 모두, FIFO, 매 프레임 Broker receipt |
| 큐 | 입력 20, 직렬화 완료 body 20, receipt 대기 body/checksum 보관 |
| 실패 | 큐 초과 또는 receipt 3회 재시도 소진 시 명시적 과부하 오류로 스트림 중지 |

STOMP header에는 SensorId, FrameId, UTC, point/source-point count, filter revision, acquisition profile, SHA1 checksum과 멱등 request ID가 포함됩니다. `POINTS 0`도 정상 프레임이므로 대상 없음과 전송 실패를 구분할 수 있습니다. Broker receipt는 Broker 수락이며 `UVirtualPointCloudStreamReceiverTC`의 소비자 수신/검증 카운터가 실제 소비를 나타냅니다.

필터는 `전체 검출점`, `대상 물체만`, `Tag·Semantic`, `센서 로컬 ROI`를 제공합니다. `대상 물체만`은 대상 Mesh Actor에 `PointCloudTarget` Tag를 붙입니다. Tag/Semantic 그룹과 ROI/거리 범위는 AND, 같은 그룹 안의 값은 OR, exclude는 마지막에 우선합니다. Tag와 SemanticLabel은 Digital Twin 전용이고 실제 ML-X 패킷 기능이라는 뜻이 아닙니다.

Native 576×56에서 1 Echo는 약 20~25MB/s, 모든 광선 2 Echo는 약 40~50MB/s입니다. Point Cloud 전용 송신 예산은 64MiB/s이며 보장 대상은 Native 한 대와 정상 로컬 또는 1Gbps 이상 LAN Artemis입니다. 네트워크 단절 중 영구 보존은 하지 않지만 acquisition은 막지 않고 스트림 오류와 원인을 표시합니다.

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
| Point Cloud | `topic.virtual.sensor.export.0` | 스캔 완료 시 raw PCD v0.7 `DATA binary` body와 `virtual-pointcloud.pcd.v1` STOMP header |

Camera 스트림은 JSON과 이미지 바이너리를 중복 전송하지 않습니다. 한 메시지에 메타데이터와 Base64 JPEG가 함께 들어갑니다. LAZ는 실시간 형식이 아니며 수동 내보내기에서 실제 압축 실행 파일이 설정된 경우에만 동작합니다.

Point Cloud 실시간 형식은 `PCD Binary 고정`입니다. UI의 기존 포맷 설정 Blueprint API는 호환을 위해 남아 있지만 실시간 스트림에는 PCD Binary가 강제됩니다. ML-X(80) FullSpec 20Hz에서 `FrameStride=1`, `receipt 간격=1`로 모든 완료 스캔을 FIFO 전송합니다. 직렬화나 Broker가 따라오지 못하면 센서 측정을 막지는 않지만, 프레임을 교체하지도 않습니다. 단계별 큐 20개를 초과하면 과부하 원인을 표시하고 해당 스트림을 중지합니다.

## 로컬 캡처 주기와 출력

- `캡처 간격(초)`: 0.05~3600초, 기본 1.0초
- `센서 주기 사용`: 선택 Camera의 캡처 주기 또는 LiDAR의 스캔 주기를 복사
- Camera 출력: JPEG, Camera Payload JSON
- LiDAR 출력: LiDAR Payload JSON, Point Cloud
- Point Cloud 형식: CSV/JSONL/PCD/LAS/LAZ

로컬 캡처 주기는 Topic의 `전송 간격(프레임)`과 무관합니다. 시간 지정 캡처는 매번 동기 스캔하지 않고 최신 완료 프레임을 사용하며, Point Cloud 직렬화와 파일 저장은 처리 중 한 건으로 제한합니다. 1회 캡처는 새 scheduled frame을 요청하고 해당 FrameId가 완료된 뒤 선택 출력을 저장합니다.

## 부하 및 유실 방지 정책

- 획득과 네트워크 처리를 분리하여 센서 스캔·캡처가 브로커를 기다리지 않습니다.
- Camera/LiDAR JSON은 스트림/센서마다 처리 중 1개와 교체 가능한 최신 대기 1개를 유지합니다.
- Point Cloud Binary PCD는 입력 FIFO 20개, 준비 body FIFO 20개, receipt 대기 body/checksum을 보존합니다.
- 프레임당 전송 제출은 최대 2건입니다. 일반 JSON은 기존 16MiB/s, Point Cloud는 별도 64MiB/s 예산입니다.
- Point Cloud는 매 프레임 broker receipt를 요청하고 연결 상태에서 최대 3회 재시도합니다.
- receipt는 broker 수락을 뜻합니다. 소비자 처리 완료를 확인하려면 ACK Topic을 설정하고 소비자가 같은 `requestId`를 회신해야 합니다.
- 연결이 끊겼을 때는 매 게임 프레임마다 재전송하지 않고 간격을 두고 재시도합니다.

로그에는 입력/전송 Hz, 마지막 frame ID, 대기 프레임 교체 수, 대역폭 대기, receipt/timeout, request ID, destination과 오류가 표시됩니다. 연속 스트림은 화면/파일 로그를 매 프레임 남기지 않으며, 필요할 때 `진단 보고서 저장`으로 `Saved/Reports`에 JSON과 Markdown을 생성합니다.

## 에디터 내부 Topic 자체 수신

`SensorRefactorTestMap`의 `SensorTest_ExternalSources`는 맵 시작 시 DTCore의 기존 `UDxWebSocketSubsystem` 연결을 통해 세 Topic을 자동 구독합니다. 송신 Broker에 제출했다는 사실만 확인하는 것이 아니라, 같은 에디터가 실제 메시지를 다시 수신하고 형식까지 검증하는 진단 기능입니다.

| Topic | 수신·검증 클래스 | 검증 내용 |
|---|---|---|
| LiDAR | `UVirtualLidarStreamReceiverTC` | schema, SensorId, FrameId, 측정·검출·Payload 점 수, 해상도와 points 배열 일관성 |
| Camera | `UVirtualCameraStreamReceiverTC` | schema, 해상도, encoding, byteSize, image 존재와 JPEG 바이트 |
| Point Cloud | `UVirtualPointCloudStreamReceiverTC` | raw PCD header, 33-byte record 크기, point count, SHA1, FrameId 연속성 |

기존 송신 Body에는 `MESSAGE_ID`가 없으므로 이 수신기는 `DT_TransactionCode` 자동 분배에 등록하지 않습니다. 구독한 Topic이 Handler를 결정하며, 각 Handler는 `ParseToStruct`에서 백그라운드 파싱하고 `ProcessStructData`에서 게임 스레드 상태와 제한된 로그만 갱신합니다. 수신 프레임을 Sensor Actor에 주입하거나 다시 송신하지 않으므로 자체 수신이 재송신 루프를 만들지 않습니다.

Camera/LiDAR 일반 필드 검증은 매 프레임 수행하고 Camera JPEG Base64 상세 검증은 표본 수행합니다. Binary PCD는 프로젝트 raw STOMP 구독이 모든 프레임의 전체 body 크기와 SHA1를 검사하며 최대 20개 FIFO를 사용합니다. 전체 동시 파싱은 최대 2개이고 큐 한계를 넘으면 교체하지 않고 검증 오류를 기록합니다. 원문 Payload와 binary body는 로그에 출력하지 않습니다.

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
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -WarmupSeconds 10 -MeasurementSeconds 60
```

이 테스트는 ML-X(80) Native 576×56·20Hz로 전환한 뒤 Node raw STOMP 소비자와 에디터 내부 수신기를 함께 실행합니다. 입력=직렬화=제출=receipt=내부 소비자 수신 수, FrameId gap/duplicate 0, 큐 overflow 0, 직렬화·소비 Hz와 p95 지연, D3D12 FPS를 판정합니다. 기본은 10초 warmup 후 60초 측정이며 결과는 `Saved/Reports/sensor_map_stream_rhi_smoke.json`과 `.md`에 저장됩니다.

장시간 검증은 같은 스크립트에서 시간을 늘리고 별도 보고서 이름을 지정합니다. 결과 파일은 커밋하지 않습니다.

```powershell
# 10분 loopback
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -WarmupSeconds 10 -MeasurementSeconds 600 -ReportLabel mlx80_pcd_10min

# 60분 soak
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -WarmupSeconds 10 -MeasurementSeconds 3600 -ReportLabel mlx80_pcd_60min
```

로컬 기본 통과 기준은 평균 55 FPS 이상, 1% low 45 FPS 이상, p95 20 ms 이하입니다. 이는 비동기·bounded 설계가 게임 스레드 정지를 방지한다는 회귀 기준이며, 실제 부하는 센서 수, 해상도, Payload 크기와 네트워크 대역폭에 따라 달라질 수 있습니다.

UI 상태는 `Saved/SaveGames/MA0T10_VirtualSensorUI_v6.sav`에 저장됩니다. 패널 크기·탭·Topic·Point Cloud 필터·로컬 캡처 간격/출력·듀얼 카메라 선택은 복원하지만 비밀번호, token, 스트림 실행 상태는 저장하지 않습니다.
