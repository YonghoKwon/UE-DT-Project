# MA0T10 Artemis 개발 Broker

`docker compose up -d`로 로컬 개발 Broker를 시작합니다.

- STOMP WebSocket: `ws://127.0.0.1:61616`
- 관리 콘솔: `http://127.0.0.1:8161`
- 개발 계정: `artemis` / `artemis`
- Camera, LiDAR, export 주소는 각각 multicast Topic으로 생성됩니다.

운영 환경에서는 별도의 계정과 TLS WebSocket(`wss://`)을 사용하고 비밀번호는 프로젝트 설정이나 SaveGame에 저장하지 마세요.
