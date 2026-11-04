# DATA-IF-10 — Artemis PCD 서버 설정안

이 문서는 서버 담당자 검토용이다. 운영 적용·무중단 HA·외부 소비자 수용 완료를 뜻하지 않는다. 실제 로컬 검수 결과는 `Saved/Reports/DATA-IF-10/r55/RESULT.md`에 별도로 기록한다. DTCore 채널 브랜치 `dd7bae7`을 사용하며 DTCore main 머지 전에 host를 머지하지 않는다.

## 유지하는 계약과 연결

- 기존 PCD destination은 `topic.virtual.sensor.export.0`, binary full record는 33-byte little endian이다. 기존 destination/헤더/스키마/기본 PCD 정책을 변경하지 않는다.
- `sensor-bulk`는 기존 Raw TCP STOMP worker를 감싸며, JSON/수동 binary는 `sensor`(DTCore STOMP 파생), 9단계 opt-in 공유 JSON은 `Default` 채널이다. PCD를 공유 JSON 연결에 넣지 않는다.
- 설정은 같은 이름의 `DTCoreSettings.DataChannels` 항목이 우선한다. 항목이 없으면 기존 `SensorTransport.ini [MA0T10.SensorTransport]` 및 기존 map/UI 설정을 유지한다. 비밀값은 채널 설정에 넣지 않고 기존 session/Runtime Override 경로로 전달한다.
- bulk reconnect의 양수 채널 설정이 우선이며, 비어 있는 값은 DTDevice 1초/최대 10초/배수 2 기본값을 유지한다. 기존 128MiB 전체·64MiB/스트림 예약과 FIFO, Camera 8/기타 20프레임 한도도 유지한다.
- 접수·제출·Broker receipt·소비자 수신/처리 ACK를 구분한다. **DROP 상황의 receipt 성공은 소비자 수신 성공이 아니다.**

## 서버 설정 예시

아래 제한은 로컬 재현용 시작점이다. 운영 동시 센서 수·실제 프레임 크기·소비자 지연·디스크 예산을 측정한 뒤 서버 담당자가 확정한다. 인증값과 실제 주소는 문서/저장소에 넣지 않는다.

```xml
<address-setting match="topic.virtual.sensor.export.#">
  <max-size-bytes>67108864</max-size-bytes>
  <address-full-policy>DROP</address-full-policy>
  <redistribution-delay>0</redistribution-delay>
  <auto-create-addresses>true</auto-create-addresses>
  <auto-create-queues>true</auto-create-queues>
</address-setting>

<acceptor name="stomp-core">tcp://0.0.0.0:61616?protocols=CORE,STOMP;stompMinLargeMessageSize=16777216</acceptor>
```

PCD 전용 규칙을 넓은 `#` 규칙보다 구체적으로 둔다. 일반 업무 주소에 DROP을 일괄 적용하지 않는다. `max-size-bytes` 외에 broker 전체 `global-max-size`, JVM/컨테이너 메모리와 디스크 한도도 관측한다. 64MiB는 논리 주소 제한이며 전체 프로세스 RSS 제한이 아니다. 큰 프레임·동시 라우팅에 따른 경계 초과량도 보고한다.

`stompMinLargeMessageSize` 이상인 STOMP body는 large message로 디스크에 저장된다. 예시는 보통의 PCD 프레임을 메모리 경로에 두기 위한 **시범값**이며, lower threshold 대조군과 `large-messages`/journal/paging 사용량을 비교한다. 단순히 임계값을 올리면 JVM 메모리 수요가 커지고, 내리면 디스크 I/O·보관량이 커질 수 있다. DROP 주소 제한만으로 전체 디스크 용량을 보장하지 않는다. [STOMP 공식 설명](https://artemis.apache.org/components/artemis/documentation/latest/stomp.html), [주소 메모리/DROP](https://artemis.apache.org/components/artemis/documentation/latest/paging).

## 2 live + 2 replication backup + VIP

live A/B를 active-active cluster로 두고 각각 같은 replication group의 backup A/B를 별도 프로세스로 둔다. 정적 connector 목록과 `message-load-balancing=ON_DEMAND`, PCD 주소의 `redistribution-delay=0`을 사용한다. ON_DEMAND는 실제 소비자가 있는 원격 queue로 전달하며, 연결 전체의 전역 순서나 exactly-once를 만들지 않는다. [클러스터 공식 설명](https://artemis.apache.org/components/artemis/documentation/latest/clusters.html).

HAProxy는 TCP round-robin VIP를 제공한다. 단순 TCP 포트 개방만으로 passive backup을 정상 대상으로 간주하지 않도록 실제 STOMP CONNECTED readiness를 검사한다. 로컬 health check 인증도 ignored 설정에서만 생성한다. 운영은 기존 인증/모니터링 체계로 옮겨 검토한다. replication 동기화·승격·quorum과 네트워크 분할은 별도 운영 검증 대상이다. 저장되지 않은 데이터가 장애 후 보존된다고 가정하지 않는다. [HA 공식 설명](https://artemis.apache.org/components/artemis/documentation/latest/ha.html).

## 소비자 정지·복구 의미

**DROP은 새 메시지를 버리고 이미 쌓인 오래된 메시지를 자동 삭제하지 않는다.** 따라서 영속 queue의 backlog를 그대로 재개하면서 “최신 프레임부터”라고 주장할 수 없다. 로컬 검수는 다음을 분리한다.

1. 연결을 유지하고 ACK/소비를 멈춰 주소 제한·DROP·메모리 상한 및 유실 범위를 관측한다.
2. 비영속 구독을 종료하고 새 구독으로 복구해 이전 구독 backlog가 다시 전달되지 않는지 확인한다.

영속 소비자가 필요하면 TTL/만료, 기존 sensor-id 헤더를 이용한 last-value queue, 소비자의 timestamp/FrameId 기반 stale frame 폐기 중 적절한 정책을 서버 담당자가 별도 결정해야 한다. 이번 통합은 이러한 제품/서버 기본값을 몰래 바꾸지 않는다. 클라이언트 재연결의 receipt 불명확 구간과 Broker DROP의 gap/중복도 각각 보존한다.

## 운영 적용 4단계와 끄기

1. **재현:** [로컬 도구](../Tools/ArtemisClusterTest/README.md)로 공식 이미지 4 broker/VIP를 재현한다. PCD on/off 일반 JSON receipt 지연·수신 간격, 교차 live 순서, live 종료, 소비 정지, 프레임 크기별 디스크/메모리를 표로 남긴다. 합성 데이터 기능 검수와 native GPU 성능/메모리 gate를 구분한다.
2. **서버 설정:** 실제 주소·queue 수명·인증·복제 동기화/승격·주소/JVM/디스크 제한을 서버 담당자가 확정한다. 설정 백업과 rollback 파일을 먼저 준비한다. 기존 업무 주소/구독은 유지한다.
3. **저속 시범:** 별도 시험 주소 또는 승인된 짧은 구간에서 1센서/낮은 Hz로 시작한다. 내부 submitted/receipt와 외부 consumer FrameId/invalid/gap, JSON 지연, 주소/JVM/디스크를 대조한다. 운영 시범만 낮은 Hz를 쓰며 제품 기본 20Hz를 변경하지 않는다.
4. **단계 증가:** 센서/Hz/프레임 크기를 하나씩 늘려 목표 부하를 확인한다. FAIL·유실·일반 업무 지연·예산 초과를 숨기지 않는다. 서버 담당자/현장 수용 후 확대한다.

**끄기:** 신규 송신을 먼저 중지하고 기존 예약/receipt를 확인한다. 소비자/queue 잔여량을 조사한 뒤 서버 설정을 백업본으로 복구한다. queue·메시지를 임의 삭제하지 않는다. 채널 설정 항목을 제거하면 기존 설정 원천을 다시 사용하지만, 데이터 I/F 구현 자체를 이전 독립 연결 코드로 되돌리는 opt-out은 아니다. 로컬 테스트 종료는 `dtpcd-if10` compose 프로젝트만 `down --volumes` 하며 다른 Docker 자원과 운영 서버를 건드리지 않는다.

## 2026-11-05 로컬 재현 관측

환경은 공식 Artemis 2.42.0 4 broker(2 live + 각 replication backup), HAProxy 3.0.30, Windows의 실제 DTCore 파생 채널을 쓰는 NullRHI 검수다. 모든 GPU 성능/메모리 측정을 종료한 뒤 수행했다. PCD는 **반복점 합성 full 33-byte**이며, GPU 획득이나 회사 수용을 뜻하지 않는다. 이미지 digest와 원문은 `Saved/Reports/DATA-IF-10/r55/broker-r55-a5/`에 있다.

| 검수 | 관측 |
|---|---|
| 일반 JSON + PCD 간섭 | Broker receipt 평균 31.48ms(PCD off) →29.73ms(on), p95 31.79→31.49ms. 소비자 업무 ACK 지연은 아님 |
| 20Hz ×3 입력 | 1,320,299-byte 비압축 프레임. 90초에 외부 검증 4,172프레임, invalid/중복/역순 0, 로컬 과부하 거절 1,231건을 별도 기록 |
| 다른 live 간 전달·순서 | 외부 검증 3,483프레임, invalid/중복/역순 0. 같은 연결 내 관측이며 전역 exactly-once 보장 아님 |
| live A 강제 종료 | 두 채널 재연결, JSON 구독 완료 2회·Raw diagnostic Camera 구독의 backup A 관리 지표 확인. gap 1,286 / 로컬 거절 1,265건을 숨기지 않음 |
| 소비자 정지·DROP | 66,297-byte 작은 프레임/동일 live에서 60초간 제출 3,600, 소비 1(ACK 보류), 주소 최대 67,214,306bytes≈64.10MiB. 64MiB 한도와 whole-frame 경계 초과량을 분리 |
| 새 비영속 구독 복구 | 새 구독으로 수신 재개 확인. 영속 backlog의 최신 프레임 보장으로 확장하지 않음 |

합성 비압축 대형 프레임 입력은 약 75.6MiB/s다. 위 90초 수신은 전체 스트림 합계 약 46.4fps(센서당 평균 약 15.5fps)이며, **입력 20Hz를 실제 수신 20Hz로 표시하지 않는다.** 부하 거절은 기존 클라이언트 상한에서 명시적으로 집계된 결과다. 일반 JSON 지연은 유지됐지만 운영의 모든 PCD를 유실 없이 20Hz 수신했다고 주장하지 않는다. 실제 native 기본 출력의 약 20Hz·fallback 0은 별도 메모리 실행에서 관측했다.

1.32MiB 프레임의 ACK 정지에서는 60초 안에 주소 메모리 한도 도달이 재현되지 않았다. 이를 DROP 통과로 바꾸지 않고, 작은 프레임의 주소 한도 시험과 large-message 디스크 시험을 분리했다. 프레임 크기·large-message 경로에 따라 주소 메모리 계수와 디스크 점유가 달라질 수 있으므로 운영에서는 둘 다 감시해야 한다.

실행 중(소비자 종료로 backlog를 지우기 전) 디스크 점유도 따로 비교했다. 수치는 노드당 관측 최대 allocated KiB이며 전체 클러스터 합계가 아니다.

| STOMP 임계값 | body 크기 | large-messages 최대 | journal 최대 | paging 최대 |
|---:|---:|---:|---:|---:|
| 16MiB | 66,297 bytes | 4KiB | 30,732KiB | 4KiB |
| 16MiB | 1,320,299 bytes | 2,588KiB | 30,732KiB | 4KiB |
| 64KiB | 1,320,299 bytes | **972,896KiB(약 950MiB)** | 30,732KiB | 4KiB |

마지막 행은 ACK를 보류한 15초 진단이다. STOMP 임계값을 낮추면 body가 large-message 파일로 이동하며, 주소 메모리가 작아 보여도 디스크 backlog가 커질 수 있음을 확인했다. 클러스터/복제 경로의 내부 large-message 동작까지 단일 STOMP 임계값 하나로 통제했다고 주장하지 않는다. 운영에서는 적절한 disk 한도·경보·queue 수명 정책도 필요하다. 원문은 `broker-large-r55-a1/`에 있다.

재현 하네스에서 보완한 사항은 다음과 같다. 제품·DTCore 동작 변경이 아니다.

- XML은 bootstrap이 읽는 literal `core` 요소와 기본 namespace 형식으로 생성한다.
- 중단된 컨테이너는 Docker DNS 이름이 사라질 수 있으므로 HAProxy에 Docker resolver와 `init-addr last,libc,none`을 둔다. TCP 연결만이 아니라 STOMP CONNECTED readiness를 검사한다.
- 외부 STOMP 소비자는 `client-individual` ACK로 byte window를 반환한다. 소비 정지 시험은 heartbeat만 보내 구독 TTL 만료로 backlog가 삭제되는 혼동을 피한다.
- Raw 재구독 관찰은 Editor 진단에서만 쓰지 않는 Camera 구독을 명시적으로 켰다. 제품의 진단 자기 수신 기본 꺼짐과 PCD wire는 유지한다.

실제 서버 적용 전에는 입력량을 기준으로 장치·프레임 크기·압축·브로커 자원·소비자 처리량을 함께 검토하고, 위 4단계 시범 절차를 따른다. 이 문서의 로컬 설정을 운영에 자동 적용하지 않는다.
# DATA-IF-10 r55b: 현장 채널·토픽 설정

DTCore는 연결·등록·재연결·상태를 담당한다. 토픽 정책은 프로젝트의
`UDeviceDataChannelSettings`(`Config=Game`)가 담당한다. DTCore 소스는 변경하지 않는다.
`sensor`는 JSON/수동 전송, `sensor-bulk`는 기존 raw TCP PCD 경로,
`Default`는 DTCoreWebSocket 저빈도 JSON 모드의 공유 연결이다.

공통 파일은 `Config/DefaultGame.ini`, 플랫폼 파일은
`Config/Windows/WindowsGame.ini` 또는 `Config/Linux/LinuxGame.ini`다.
플랫폼 파일에서 배열을 교체할 때는 `!…=ClearArray` 다음에 **전체 목록**을 쓴다.
일부 항목을 덧붙이면 같은 채널 이름이 중복될 수 있다.

```ini
[/Script/DTCore.DTCoreSettings]
; 연결만 설정한다. Default는 예약 이름이므로 DataChannels에 넣지 않는다.
WebSocketUrl=ws://127.0.0.1:61614
+DataChannels=(ChannelName="sensor",ChannelClass="/Script/DeviceRuntime.DxDeviceSensorStompChannel",Url="ws://127.0.0.1:61614",bAutoConnect=False,bUseDefaultCredentials=False,LoginOverrideKey="SensorLogin",PasscodeOverrideKey="SensorPasscode")
+DataChannels=(ChannelName="sensor-bulk",ChannelClass="/Script/DeviceRuntime.DxDeviceSensorBulkChannel",Url="tcp://127.0.0.1:61616",bAutoConnect=False,bUseDefaultCredentials=False,LoginOverrideKey="BulkLogin",PasscodeOverrideKey="BulkPasscode")

[/Script/DeviceRuntime.DeviceDataChannelSettings]
!Channels=ClearArray
+Channels=(ChannelName="sensor",Destinations=((Alias="camera",Destination="topic.virtual.sensor.camera.0",bSubscribe=False,Purpose="camera JSON"),(Alias="lidar",Destination="topic.virtual.sensor.lidar.0",bSubscribe=False,Purpose="LiDAR JSON"),(Alias="pointcloud",Destination="topic.virtual.sensor.export.0",bSubscribe=False,Purpose="manual export"),(Alias="status-in",Destination="topic.site.status",bSubscribe=True,Purpose="project status"),(Alias="result-in",Destination="topic.site.result",bSubscribe=True,Purpose="project result")))
+Channels=(ChannelName="sensor-bulk",Destinations=((Alias="camera",Destination="topic.virtual.sensor.camera.0",bSubscribe=False,Purpose="JPEG"),(Alias="lidar",Destination="topic.virtual.sensor.lidar.0",bSubscribe=False,Purpose="telemetry"),(Alias="pointcloud",Destination="topic.virtual.sensor.export.0",bSubscribe=False,Purpose="binary PCD")))
+Channels=(ChannelName="Default",Destinations=((Alias="camera",Destination="topic.virtual.sensor.camera.0",bSubscribe=False,Purpose="manual JSON"),(Alias="lidar",Destination="topic.virtual.sensor.lidar.0",bSubscribe=False,Purpose="status JSON"),(Alias="pointcloud",Destination="topic.virtual.sensor.export.0",bSubscribe=False,Purpose="result JSON")))
```

`camera`/`lidar`/`pointcloud`/`imu`/`ack`는 송신 코드가 참조하는 별칭이다.
추가 별칭은 임의로 정할 수 있다. 같은 채널의 같은 destination은 한 번만 구독한다.
서로 다른 채널의 `bSubscribe=True` destination이 겹치면 시작 경고와 모니터 상태 문구를
표시하되 동작을 막지 않는다. 기본값은 기존 이름이며 기본 자동 재수신은 계속 꺼져 있다.
수신 JSON은 registry의 `OnTopicMessage(ChannelName, Alias, Message)`로 프로젝트가 처리한다.
구독 등록만으로 연결을 시작하지 않는다. `sensor`는 DTCore의 재구독 기능을 사용하고,
`Default`는 기존 공유 연결의 연결 상태가 바뀔 때 다시 구독한다.
`sensor-bulk`의 명시 구독은 기존 worker 소켓에서 처리한다. 이 목록은 수신 선택 UI나
PCD 검증을 자동 활성화하지 않는다. 기존 receiver selection이 디코딩·검증을 계속 결정한다.
20Hz PCD body·헤더·FIFO·예산·receipt 처리·서버 destination 계약은 바꾸지 않는다.

배포 시에는 실행 파일을 종료하고 다음 loose 파일을 수정한 뒤 재시작한다.

```text
Windows/ma0t10_dt/Config/DefaultGame.ini
Windows/ma0t10_dt/Config/Windows/WindowsGame.ini
```

자격증명 값은 넣지 않는다. 인증은 기존 Runtime Override 키 방식으로 별도 관리한다.
저장된 사용자 설정이 같은 키를 덮어쓸 수 있으므로 새 설정 적용 검증은 별도 UserDir에서
먼저 한다. 연결 URL과 토픽을 각각 식별 가능한 시험값으로 바꾸고 재시작 후 확인한다.
Development의 `-NullRHI -DeviceChannelConfigProbe`는 읽기 전용 snapshot을
패키지 `Saved/Reports/device-channel-config.json`에 남기고 종료한다.
snapshot은 연결 URL을 loopback 시험값 외에는 가리고 인증값을 기록하지 않는다.
이 진단은 Shipping에서 컴파일하지 않는다. Linux 설정 예시는 제공하지만 Windows에서
Linux 패키지의 실제 로드를 검증했다고 주장하지 않는다.

이전 방법: `SensorTransport.ini`의 BrokerUrl 등 연결값을 `DataChannels`로,
CameraTopic/LidarTopic/ExportTopic/AckTopic을 각각 새 별칭으로 옮긴다.
새 항목이 없거나 destination이 비어 있으면 기존 INI/맵/UI profile을 그대로 사용한다.
연결 항목과 토픽 항목은 독립적으로 이전할 수 있다. 파일을 복사하기 전에 비밀값을 제거한다.
기존 `SensorTransport.ini`는 삭제하거나 자동 덮어쓰지 않는다.
