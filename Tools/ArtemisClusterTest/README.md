# Artemis PCD 로컬 재현

운영 서버를 건드리지 않는 DATA-IF-10 기능 검수 도구다. **Unreal 성능·메모리 측정이 모두 끝난 뒤** 실행한다. Docker Desktop의 Linux 컨테이너 환경과 기존 Python/psutil, 빌드된 현재 worktree Editor가 필요하다. 설치·서비스·보안 설정을 자동 변경하지 않는다.

구성은 Apache 공식 `apache/activemq-artemis:2.42.0-alpine` 4개(2 live + 각각 replication backup), HAProxy TCP VIP 1개다. 호스트 포트는 기존 서버와 충돌하지 않도록 loopback 27616(VIP), 27617~27620(직접 연결), 28161~28164(관리)만 사용한다. 컨테이너의 메시징 포트는 61616이다.

```powershell
python -X utf8 -I -m unittest discover -s Tools/ArtemisClusterTest -p test_local_tools.py
# 아래 명령은 세 장면 메모리 supervisor가 완료됐는지 확인한 뒤에만 Docker를 실행한다.
python -X utf8 Tools/ArtemisClusterTest/run_scenarios.py `
  --memory-state Saved/Reports/DATA-IF-10/r55/memory-final-status.json `
  --report Saved/Reports/DATA-IF-10/r55/broker-r55
```

`configure.py`만 실행하면 네트워크/Docker 호출 없이 설정을 생성한다. 비밀번호·cluster secret은 무작위로 만들고 `.local/test.env` 및 `.local/config/`, `.local/haproxy.cfg`에만 둔다. `git`에서 제외되며 출력하지 않는다. `docker compose config`의 전체 출력이나 원본 env/config를 보고서에 복사하지 않는다. 도구는 알려진 비밀값·인증 헤더·health-check hex를 로그 보존 전에 마스킹한다.

실제 실행은 프로젝트 `dtpcd-if10`만 생성·종료한다. 다른 컨테이너는 종료하지 않는다. 실패해도 이 프로젝트의 컨테이너와 volume을 정리한다. 재현 중 강제로 controller를 중단했다면 아래 **소유 프로젝트만** 정리한다.

```powershell
docker compose --project-name dtpcd-if10 --env-file Tools/ArtemisClusterTest/.local/test.env `
  -f Tools/ArtemisClusterTest/compose.yaml down --volumes --remove-orphans
```

실제 제품의 `sensor` / `sensor-bulk` 채널은 `MA0T10.DataIf10.BrokerRuntime` opt-in 자동화를 통해 사용한다. **NullRHI·합성 33-byte PCD**이며 GPU 획득 정확도·native 성능·메모리 gate를 대신하지 않는다. 3개 스트림을 각 20Hz로 접수하고 일반 JSON의 Broker receipt 지연, 외부 JSON 수신 간격, PCD 전체 바이트/체크섬·순서·gap을 따로 기록한다. 현재 반복점 fixture는 모든 레코드를 독립 Python 코드로 비교한다.

자식 Editor에만 주입하는 환경 변수:

| 변수 | 의미 |
|---|---|
| `DTDEVICE_ARTEMIS_RUN=1` | 실제 loopback 연결 허용. 없으면 명시적 Skip |
| `DTDEVICE_ARTEMIS_TCP_URL` / `DTDEVICE_ARTEMIS_WS_URL` | `127.0.0.1`의 tcp/ws 주소만 허용 |
| `DTDEVICE_ARTEMIS_USER` / `DTDEVICE_ARTEMIS_PASSCODE` | ignored env에서 읽어 전달; 명령행·보고서에 쓰지 않음 |
| `DTDEVICE_ARTEMIS_SECONDS` | 실행 길이 10~600초, 기본 120초 |
| `DTDEVICE_ARTEMIS_POINTS` | 프레임 포인트 수 1~250000, 기본 40000 |
| `DTDEVICE_ARTEMIS_PCD` | `0`은 JSON 대조군, 그 외는 PCD 포함 |

시나리오는 JSON 단독/PCD+JSON, 서로 다른 live 간 전달·연결 내 순서, pair A에 두 연결을 붙인 뒤 live A 강제 종료, 소비자 ACK 정지에 따른 DROP/주소 메모리, 새 비영속 구독 복구, 프레임 크기 및 large-message threshold 비교다. failover 중의 gap/중복은 숨기지 않는다. receipt는 소비자 처리 완료가 아니다.

`DROP`은 새 메시지를 버리고 기존 backlog를 유지한다. 최신 프레임 복구는 **새 비영속 구독**으로 따로 검수하며, 기존 영속 backlog 재개를 최신 프레임 보장으로 표시하지 않는다. 운영 설정·한계·끄기 절차는 [서버 설정안](../../docs/ARTEMIS_PCD_SETTINGS.md)을 따른다.

공식 근거: [Docker 이미지](https://artemis.apache.org/components/artemis/documentation/latest/docker.html), [2.42.0 entrypoint](https://github.com/apache/activemq-artemis/blob/2.42.0/artemis-docker/docker-run.sh), [클러스터](https://artemis.apache.org/components/artemis/documentation/latest/clusters.html), [HA](https://artemis.apache.org/components/artemis/documentation/latest/ha.html), [STOMP large message](https://artemis.apache.org/components/artemis/documentation/latest/stomp.html). 실행 보고서에는 실제 image digest/버전과 관측 결과를 추가한다.
