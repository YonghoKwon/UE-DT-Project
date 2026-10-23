# MA0T10 Artemis 개발 Broker

## Binary PCD 20Hz 설정

버전 관리되는 `etc-override/broker.xml`은 ML-X(80) 실시간 PCD를 위해 다음 값을 사용합니다.

- WebSocket 단일 frame 상한: 16MiB
- STOMP large-message 전환: `stompMinLargeMessageSize=4194304`
  - ML-X(80) Binary PCD는 1 Echo 약 1.1MiB, 2 Echo 최악 약 2.2MiB입니다. 1MiB 임계값은 모든 정상 프레임을 디스크 large-message 경로로 보내 로컬 실측 처리율을 약 0.7Hz까지 낮췄습니다. 4MiB는 이 계약을 메모리 경로로 유지하며, 그보다 큰 예외 메시지만 large-message로 전환합니다.
- `topic.virtual.sensor.export.0`: `PAGE`, page 10MiB, read-page 64MiB
- journal, paging, large-message 디렉터리를 분리해 Broker 디스크 사용량을 진단 가능하게 유지

실시간 Point Cloud body는 Base64 JSON이 아니라 `application/vnd.pcd` raw binary입니다. Native 2 Echo 최악 조건도 약 2.13MB/frame으로 16MiB WebSocket 상한 아래입니다. 20Hz에서는 약 40~50MB/s가 될 수 있으므로 Broker 데이터 디렉터리의 디스크 처리량과 용량을 함께 확인하십시오.

로컬 설치형 Broker `C:\Project\apache-artemis-2.44.0\bin\myTest`를 사용할 때도 acceptor의 `stompMinLargeMessageSize=4194304`, WebSocket 16MiB, export Topic PAGE/read-page 64MiB가 같아야 합니다. 설정 변경 후 Broker를 재시작해야 적용됩니다.

프로젝트 루트에서 다음 명령으로 로컬 개발 Broker를 시작합니다.

```powershell
docker compose -f Tools/Artemis/docker-compose.yml up -d --wait
```

## Test credentials

All Artemis test scripts (`Scripts/run_*` that talk to the broker,
`stomp_probe.mjs`, `publish_slab_scenario.mjs`) read the broker account from
`MA0T10_ARTEMIS_USER` / `MA0T10_ARTEMIS_PASSWORD` and stop with an error naming
the missing variable when either is unset. Scripts pass them to Unreal and Node
only through the child-process environment, never on the command line, and do
not print them. Set them before running a script:

```powershell
# Once per PowerShell session; values come from the broker you run and are never committed.
$env:MA0T10_ARTEMIS_USER = '<user>'
$env:MA0T10_ARTEMIS_PASSWORD = '<password>'
```

For the Docker broker below, use the account defined by `ARTEMIS_USER` /
`ARTEMIS_PASSWORD` in `docker-compose.yml`; for an installed broker, use that
instance's existing account. The scripts never change the broker configuration.

## Unreal three-stream smoke test

With the broker running, execute the following from the project root. It starts
a real STOMP subscriber and verifies one Unreal-originated message on each
LiDAR, Camera, and Point Cloud topic.

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\run_artemis_stream_smoke.ps1
```

The machine-readable result is written to
`Saved/Reports/artemis_sensor_stream_smoke.json`. Use `--ack-topic` with
`Tools/Artemis/stomp_probe.mjs` when consumer-processing acknowledgement is
also required; a broker receipt alone only proves broker acceptance.

To validate continuous streams produced by the real Camera and LiDAR actors in
`SensorRefactorTestMap`, including D3D12 frame-time thresholds, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild
```

The combined broker and performance evidence is stored in
`Saved/Reports/sensor_map_stream_rhi_smoke.json` and `.md`.

The default run warms up for 10 seconds and measures for 60 seconds. It checks
raw PCD checksum/record layout, FrameId continuity, publisher/receipt/internal
consumer count equality, at least 19 Hz, serialization p95, and D3D12 frame
time. The Node probe negotiates 10-second STOMP heartbeats so Artemis does not
close a long-running subscriber at the connection TTL. Longer loopback/soak
reports use a unique label:

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -MeasurementSeconds 600 -ReportLabel mlx80_pcd_10min
powershell -ExecutionPolicy Bypass -File .\Scripts\run_sensor_map_stream_rhi_smoke.ps1 -SkipBuild -MeasurementSeconds 3600 -ReportLabel mlx80_pcd_60min
```

- STOMP WebSocket: `ws://127.0.0.1:61616`
- 관리 콘솔: `http://127.0.0.1:8161`
- 개발 계정: `docker-compose.yml`의 `ARTEMIS_USER`/`ARTEMIS_PASSWORD`. 스크립트에는 `MA0T10_ARTEMIS_USER`/`MA0T10_ARTEMIS_PASSWORD` 환경변수로 전달합니다(위 Test credentials).
- Camera, LiDAR, export 주소는 각각 multicast Topic으로 생성됩니다.
- Broker 인스턴스 전체가 `artemis-instance` 볼륨에 저장되므로 재시작 후에도 주소와 메시지 저장소를 유지합니다.

상태와 로그는 다음 명령으로 확인합니다.

```powershell
docker compose -f Tools/Artemis/docker-compose.yml ps
docker logs ma0t10-artemis --tail 100
```

종료할 때는 `docker compose -f Tools/Artemis/docker-compose.yml down`을 사용합니다. 저장된 Broker 인스턴스까지 초기화하려면 개발 데이터 삭제를 확인한 뒤 `down -v`를 사용합니다.

운영 환경에서는 별도 계정과 TLS WebSocket(`wss://`)을 사용하고, 비밀번호나 Bearer token을 프로젝트 설정 또는 SaveGame에 저장하지 마세요.


PCD-BANDWIDTH-01: `stomp_probe.mjs`는 헤더에 따른 필드 선택과 `binary`/LZF `binary_compressed`를 검증한다. checksum·point count·Slab metadata 검사는 유지한다. 자격증명 없는 codec 검사는 `node --test Tools/Artemis/pcd_codec.test.mjs`, 3대 loopback은 `Scripts/run_pcd_bandwidth_evidence.ps1`을 사용한다. loopback receipt/echo는 Artemis/회사 소비자 수용이나 업무 ACK가 아니다.
