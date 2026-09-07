# 센서 실시간 스트리밍과 캡처/내보내기

> 2026-08 고성능 3-stream 계약입니다. 기존 Camera와 LiDAR JSON 스트림은 호환 backend에 유지됩니다.

## 60 FPS 고성능 3-stream 모드

`D455 Camera 1대 + ML-X(80) Native LiDAR 1대`의 기본 성능 경로는 `UVirtualSensorHighThroughputTransportSubsystem`이 소유한 전용 Raw TCP STOMP 1.2 worker입니다. worker가 Artemis 61616 acceptor에 연결해 인증, heartbeat, reconnect, SEND/SUBSCRIBE, receipt와 MESSAGE 수신을 처리합니다. 게임 스레드는 immutable shared body를 큐에 넘기고 작은 진단 결과만 받으므로 대용량 STOMP frame 조립·복사·수신 파싱을 하지 않습니다.

| 스트림 | 주기 | 고성능 Body | schema |
|---|---:|---|---|
| Camera | 30Hz | 1280×720 원본 JPEG binary | `virtual-camera.jpeg.v1` |
| LiDAR 값 | 20Hz | 포인트 배열을 제외한 측정 시각·프로필·count·거리/Intensity 통계 JSON | `virtual-lidar.telemetry.v1` |
| Point Cloud | 20Hz | 완전한 PCD v0.7 `DATA binary` | `virtual-pointcloud.pcd.v1` |

Camera queue는 8개, LiDAR/PCD queue는 각각 20개이며 모든 프레임에 receipt를 요청합니다. 정상 연결 중에는 FIFO와 FrameId 순서를 보존하고 상한을 넘으면 조용히 교체하지 않고 해당 스트림을 과부하 오류로 전환합니다. 연결 해제 중 acquisition은 계속되지만 프레임을 디스크에 영구 보존하지 않습니다.

입력 URL은 기존 UI의 `ws://host:61616` 또는 `tcp://host:61616`을 사용할 수 있으며 고성능 worker는 동일 Artemis raw TCP acceptor로 연결합니다. `wss://`는 Engine STOMP compatibility backend로 fallback하고 TLS/WebSocket 비용 때문에 아래 60FPS 보장 범위에서 제외됩니다. 기존 Base64 `virtual-camera.v1`과 포인트 배열이 있는 `virtual-lidar.v1`은 Blueprint/API 호환 모드에 남습니다.

## ML-X(80) 20Hz 실시간 Point Cloud

### 수신 검증과 Slab 연계

기본 **활성 송신만 확인**은 현재 송신 중인 종류와 SensorId를 검사합니다. Raw TCP에서는 실제 Broker MESSAGE의 백그라운드 검증 결과를 수신 TC와 Capture/Export UI가 공유합니다. receipt를 복사해 수신 성공으로 표시하지 않습니다. PCD만 활성화하면 Camera/LiDAR 수신 이벤트를 추가하지 않습니다. **전체 Topic 확인**은 외부 발행자를 검사하는 독립 수신 모드입니다.

PCD 수신은 `sensor-id`/`x-sensor-id`, `frame-id`/`x-frame-id`, `checksum`/`x-checksum-sha1`, `acquisition-profile`/`x-acquisition-profile` 등 기존 두 송신 경로의 별칭을 지원합니다. UTC는 ISO8601 또는 epoch milliseconds를 정규화하고 별칭 값이 충돌하면 오류로 표시합니다. `Binary PCD required STOMP headers are missing` 문제를 해결하기 위해 Broker 한도를 올릴 필요는 없습니다.

Slab 정보는 여전히 STOMP의 `x-run-uuid`, `x-mtl-no`, `x-slab-frame-no`, `x-slab-elapsed-sec`에 있습니다. **PCD 본문에 이 정보를 삽입하는 변경은 포함하지 않습니다.** 일반 비연동 PCD에는 Slab 정보가 없어도 유효합니다. UI 최근 수신 이벤트에서 센서 FrameId와 Slab 프레임을 독립적으로 확인할 수 있습니다.

수신 구독 해제는 송신과 Broker receipt 처리를 중지하지 않습니다. 송신 중지 후 마지막 진단은 보존됩니다. 표시 Hz는 누적 통계이므로 과거 대기·중지 구간이 포함될 수 있습니다. 세션 실패 수와 헤더 검증 오류는 별개이며, 실제 Raw 전달 실패의 마지막 원인을 함께 확인하십시오.

현재 우선 검증 범위는 Raw TCP PCD와 UI 공유 경로입니다. 독립 Camera/JPEG 및 LiDAR telemetry의 형식별 파서 확장은 후속 검증 대상이며, 기존 JSON 호환 경로와 혼동하지 않습니다.

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

STOMP header에는 SensorId, FrameId, UTC Unix epoch milliseconds(`x-utc`), point/source-point count, filter revision, acquisition profile, SHA1 checksum과 멱등 request ID가 포함됩니다. UE 5.3 STOMP header escape와 Artemis의 충돌을 피하기 위해 `x-utc`는 구두점 없는 숫자로 보냅니다. `POINTS 0`도 정상 프레임이므로 대상 없음과 전송 실패를 구분할 수 있습니다. Broker receipt는 Broker 수락이며 `UVirtualPointCloudStreamReceiverTC`의 소비자 수신/검증 카운터가 실제 소비를 나타냅니다.

필터는 `전체 검출점`, `대상 물체만`, `Tag·Semantic`, `센서 로컬 ROI`를 제공합니다. `대상 물체만`은 대상 Mesh Actor에 `PointCloudTarget` Tag를 붙입니다. Tag/Semantic 그룹과 ROI/거리 범위는 AND, 같은 그룹 안의 값은 OR, exclude는 마지막에 우선합니다. Tag와 SemanticLabel은 Digital Twin 전용이고 실제 ML-X 패킷 기능이라는 뜻이 아닙니다. 이 메타데이터 필터는 CPU Trace·Replay·외부 입력 프레임에서 동작합니다. FullSpec `GpuDepthProjection`은 Actor identity를 만들지 않으므로 ML-X Native 20Hz에서 물체 영역을 제한할 때는 센서 로컬 ROI를 사용하십시오.

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
| LiDAR 값 | `topic.virtual.sensor.lidar.0` | 고성능: `virtual-lidar.telemetry.v1`, 호환: `virtual-lidar.v1` JSON |
| Camera 이미지 | `topic.virtual.sensor.camera.0` | 고성능: 원본 JPEG `virtual-camera.jpeg.v1`, 호환: Base64 JPEG `virtual-camera.v1` JSON |
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

호환 JSON 모드에서 `SensorRefactorTestMap`의 `SensorTest_ExternalSources`는 DTCore의 기존 `UDxWebSocketSubsystem`을 사용합니다. 고성능 모드에서는 프로젝트 Raw TCP worker가 세 Topic을 직접 SUBSCRIBE하고 body를 백그라운드 검증합니다. 어느 모드도 수신 데이터를 Sensor Actor에 재주입하거나 재송신하지 않습니다.

| Topic | 수신·검증 클래스 | 검증 내용 |
|---|---|---|
| LiDAR | `UVirtualLidarStreamReceiverTC` 또는 Raw TCP worker | 호환 v1 points 일관성 또는 telemetry schema·count·통계 |
| Camera | `UVirtualCameraStreamReceiverTC` 또는 Raw TCP worker | 호환 Base64 JPEG 또는 raw JPEG 크기·SHA1·SOI/EOI |
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

이 테스트는 D455 1280×720·30Hz와 ML-X(80) Native 576×56·20Hz를 구성한 뒤 Camera JPEG, LiDAR telemetry, Binary PCD를 동시에 발행합니다. Node raw STOMP 소비자와 에디터 내부 worker가 함께 수신하며 입력=직렬화=제출=receipt=내부 소비자 수, FrameId gap/duplicate 0, queue overflow 0, 직렬화·소비 Hz와 game-frame p95를 판정합니다. 기본은 10초 warmup 후 60초 측정이며 결과는 `Saved/Reports/sensor_map_stream_rhi_smoke.json`과 `.md`에 저장됩니다.

2026-08-05 기준 목표 PC의 실제 D3D12 60초 보고서에서는 Camera 외부 수신 1,797건, LiDAR 1,199건, PCD 1,199건, PCD 19.98Hz·20.18MiB/s, 평균 60.00FPS, 1% low 59.99FPS, game-frame p95 16.67ms를 기록했습니다. 내부 검증 실패, FrameId gap, duplicate, invalid, queue overflow와 retry는 모두 0이었습니다. 이 수치는 로컬 Artemis 결과이며 다른 PC·Broker·네트워크에서는 같은 스크립트로 다시 측정해야 합니다.

장시간 검증은 같은 스크립트에서 시간을 늘리고 별도 보고서 이름을 지정합니다. 결과 파일은 커밋하지 않습니다.

```powershell
# 10분 loopback
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -WarmupSeconds 10 -MeasurementSeconds 600 -ReportLabel mlx80_pcd_10min

# 60분 soak
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -WarmupSeconds 10 -MeasurementSeconds 3600 -ReportLabel mlx80_pcd_60min
```

로컬 기본 통과 기준은 평균 55 FPS 이상, 1% low 45 FPS 이상, p95 20 ms 이하입니다. 이는 비동기·bounded 설계가 게임 스레드 정지를 방지한다는 회귀 기준이며, 실제 부하는 센서 수, 해상도, Payload 크기와 네트워크 대역폭에 따라 달라질 수 있습니다.

UI 상태는 `Saved/SaveGames/MA0T10_VirtualSensorUI_v6.sav`에 저장됩니다. 패널 크기·탭·Topic·Point Cloud 필터·로컬 캡처 간격/출력·듀얼 카메라 선택은 복원하지만 비밀번호, token, 스트림 실행 상태는 저장하지 않습니다.
