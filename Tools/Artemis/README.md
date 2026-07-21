# MA0T10 Artemis 개발 Broker

프로젝트 루트에서 다음 명령으로 로컬 개발 Broker를 시작합니다.

```powershell
docker compose -f Tools/Artemis/docker-compose.yml up -d --wait
```

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

- STOMP WebSocket: `ws://127.0.0.1:61616`
- 관리 콘솔: `http://127.0.0.1:8161`
- 개발 계정: `artemis` / `artemis`
- Camera, LiDAR, export 주소는 각각 multicast Topic으로 생성됩니다.
- Broker 인스턴스 전체가 `artemis-instance` 볼륨에 저장되므로 재시작 후에도 주소와 메시지 저장소를 유지합니다.

상태와 로그는 다음 명령으로 확인합니다.

```powershell
docker compose -f Tools/Artemis/docker-compose.yml ps
docker logs ma0t10-artemis --tail 100
```

종료할 때는 `docker compose -f Tools/Artemis/docker-compose.yml down`을 사용합니다. 저장된 Broker 인스턴스까지 초기화하려면 개발 데이터 삭제를 확인한 뒤 `down -v`를 사용합니다.

운영 환경에서는 별도 계정과 TLS WebSocket(`wss://`)을 사용하고, 비밀번호나 Bearer token을 프로젝트 설정 또는 SaveGame에 저장하지 마세요.
