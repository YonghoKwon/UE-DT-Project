# Artemis ↔ ROS2 bridge (ROS2-01 / ROS2-02)

외부 Python 노드가 기존 STOMP wire를 읽는다. UE C++·DTCore·맵·저장값·Broker 설정은 바꾸지 않는다. Docker를 사용하지 않는다. 현재 단계는 PCD/LiDAR/JPEG adapter이며 CAM-GT depth/segmentation 서버 송신은 포함하지 않는다.

```mermaid
flowchart LR
  UE[UE 5.3: acquisition snapshot] -->|기존 TCP STOMP| A[Windows Artemis :61616]
  A -->|WSL2 NAT gateway| B[전용 Ubuntu 24.04 / rclpy]
  B --> PC[PointCloud2 / Image / CameraInfo / TF]
  PC --> F[foxglove_bridge :8765]
  F --> V[Windows Foxglove / ws://localhost:8765]
  R[WSL ROS2 services / JSON request topic] --> C[별도 control_node.py]
  C -->|Bearer · loopback Host 유지| P[Windows WSL vEthernet :18099 · 임시 인증 중계]
  P --> L[기존 AUTO-01 · 127.0.0.1:8099]
  L -->|기존 public API| UE
  C --> S[ROS2 result / status]
  B -->|명시적 옵션 · acquisition UTC| K[/clock]
```

## 설치·실행·종료·삭제

Windows Python 3.9+·WSL2가 이미 있는 PC에서 저장소 루트 기준으로 실행한다. `python` 실행 경로를 먼저 확인한다. 기본 WSL 배포판은 사용하지 않는다. 이 스크립트가 소유한 배포판 이름은 **`ma0t10-ros2`** 하나다. 이미 같은 이름이 있으면 중단한다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Ros2Bridge/setup-wsl.ps1 install
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Ros2Bridge/setup-wsl.ps1 status
```

공식 Ubuntu `.wsl` 이미지를 `Saved/Reports/ROS2-01/wsl`에 내려받아 고정 SHA256을 검사하고 `wsl --import ... --version 2`로 설치한다. OOBE·사용자/비밀번호 생성 없이 root를 사용한다. 원본 이미지는 성공/실패 시 모두 삭제한다. 공식 ROS apt-source와 버전을 고정한 `ros-jazzy-ros-base`, `ros-jazzy-foxglove-bridge`, Python venv 안 STOMP 및 그 의존성만 설치한다. 별도 ROS desktop/Docker/실장비 SDK는 설치하지 않는다. 설치 시 추가/갱신된 Ubuntu·ROS 의존성 349개는 `apt-packages.lock`으로 고정하고, rootfs에 원래 있던 패키지는 rootfs SHA256으로 고정한다. 설치된 전체 버전은 보고서의 `apt-installed-lock.txt`, 직접 설치 버전은 `environment-lock.json`에도 기록한다. 오래된 apt 버전이 사라졌으면 설치는 실패하며 최신 버전으로 조용히 대체하지 않는다.

`sensor_msgs/Image(rgb8)` 디코딩은 **Pillow 선택 의존성**이다. 추가 설치를 허용한 경우에만 `install -WithImageDecoder`를 사용한다. 기본 설치는 JPEG `CompressedImage`와 `CameraInfo`를 발행하며 `image_decoder_unavailable`을 센다. `Image` 수용은 디코더 설치·유효 JPEG 검증까지 별도 확인해야 한다. 기존 JPEG golden/JSON fixture는 의도적으로 짧은 가짜 JPEG이므로 이미지 디코딩 통과 근거가 아니다.

기존 Artemis를 원래 설정으로 실행한다. 자격 증명은 세션의 `MA0T10_STOMP_USER`, `MA0T10_STOMP_PASSWORD` 환경변수로만 제공한다. 값을 명령 인자·스크립트·스크린샷·보고서에 적지 않는다. `setup-wsl.ps1 up`은 해당 프로세스의 `WSLENV`로 두 값을 전달하며 파일에는 저장하지 않는다.

```powershell
# 환경변수는 값이 화면/기록에 남지 않는 승인된 경로에서 미리 설정한다.
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Ros2Bridge/setup-wsl.ps1 up
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Ros2Bridge/setup-wsl.ps1 down
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Ros2Bridge/setup-wsl.ps1 remove
```

`run`/`stop`은 `up`/`down`의 별칭이다. `down`은 소유 ROS 프로세스를 정상 종료한다. `remove`는 ownership 기록을 확인한 뒤 **해당 테스트 배포판만 `wsl --unregister ma0t10-ros2`**로 삭제한다. 삭제 후 배포판 이름 목록·설치 파일 잔존을 검사한다. `Saved/Reports/ROS2-01/wsl`의 작은 검증 로그/버전 기록은 남는다. 재설치는 새 `-ReportRoot Saved/Reports/ROS2-01/wsl-next`를 지정해 소유 기록을 분리한다. 기존 배포판·기본 배포판·`.wslconfig`·방화벽·netsh·보안 프로그램을 바꾸지 않는다. 자동 system-wide WSL shutdown/restart도 없다.

## 네트워크

먼저 `C:\Project\apache-artemis-2.44.0\bin\myTest\etc\broker.xml`의 acceptor만 읽어서 61616 바인드를 확인한다. 현재는 `0.0.0.0:61616`이다. WSL의 `ip -4 route show default` 게이트웨이를 Windows 호스트로 사용한다. `check_network.py`는 인증 없이 TCP 접근만 확인한다.

localhost 전용 Broker일 때만 `tcp_relay.py`를 테스트 동안 실행할 수 있다. `Get-NetIPAddress`로 실제 `vEthernet (WSL...)` IPv4와 prefix를 읽어 `--bind <그 IP> --wsl-subnet <해당 CIDR> --port 61617`을 지정하고, 브리지 환경의 `MA0T10_STOMP_HOST`·`MA0T10_STOMP_PORT`를 그 주소/포트로 설정한다. 중계는 그 인터페이스·WSL subnet만 받고 127.0.0.1:61616으로 연결한다. 최대 8연결·64KiB 전달 청크이며 Ctrl+C로 끝낸다. 현재 구성에서는 실행할 필요가 없다. Windows 보안/방화벽 팝업이 보이면 조작하지 않고 중단한다.

ROS DDS는 WSL 내부 `ROS_DOMAIN_ID=71`, `ROS_LOCALHOST_ONLY=1`로 제한한다. Foxglove WebSocket은 WSL 8765에서 받아 WSL 기본 localhost 전달로 Windows `ws://localhost:8765`에 노출된다. NAT 전달이 막히면 시스템 설정을 자동 수정하지 않는다. Raw STOMP는 평문 TCP다. 암호화 지원을 주장하거나 기존 WSS 설정을 평문으로 덮어쓰지 않는다.

## ROS2로 보기

1. 위 설치·환경변수·Artemis 확인 후 `up`을 실행한다.
2. UE PIE의 **배치 → LiDAR 선택 → 센서 상세 설정**에서 ML-X(80) Native·FullSpec(576×56)와 주기 0.05 또는 0.1초를 확인한다. **측정 시작**, **PCD 전송 → 전송**으로 기존 PCD 출력을 켠다. 맵이나 개인 세션 저장은 필요하지 않다.
3. Windows 브라우저의 [Foxglove](https://app.foxglove.dev/)에서 **Open connection → Foxglove WebSocket → `ws://localhost:8765`**를 선택한다. 3D 패널의 frame은 `ma0t10_world`, point cloud는 아래 `/points` 토픽으로 지정한다. 센서 선택은 원본 SensorId의 UTF-8 hex를 사용하는 토픽 이름과 `bridge-metrics.json`의 sensor_id를 대조한다.
4. 축 표시를 켠다. UE X전방은 ROS +X, UE Y우측은 ROS −Y, Z위쪽은 +Z다. 점군의 **센서 로컬** Y는 이미 좌측이므로 다시 반전하지 않는다. 센서가 움직이면 acquisition timestamp의 TF를 사용한다.
5. 끝나면 PIE·이 작업이 켠 Artemis를 정상 종료하고 `down`, `remove`를 실행한다. 개인 SaveGame을 시험했다면 사전 백업을 복원한다.

UE의 ROS2 연결 상태 표시·전용 버튼은 현재 UE 화면에 추가하지 않았다. AUTO-01 상태에는 호출 출처/제어 lease 필드가 없어 API 활성화만으로 "ROS2에서 제어 중"이라고 표시할 수 없다. ROS2-02 피드백의 `source=ros2_bridge`는 **피드백 발행 경로**이며 배타적 제어권을 뜻하지 않는다. 직접 마우스·UE/Foxglove 비교 수용과 CLI 수신은 별개다.

## ROS2-02 양방향 제어 — 기본 꺼짐

출처: 사용자 2026-10-15, 로드맵 #13. UE에 새 서버를 만들지 않고 기존 [AUTO-01 계약](../../README.md)의 로컬 HTTP API만 호출한다. 별도 `control_node.py` 프로세스가 HTTP를 처리하므로 API timeout이 PointCloud2 변환 executor를 막지 않는다. API 수신 서버는 계속 **127.0.0.1**이다. ROS DDS 범위는 기존 domain71/localhost이며 이 전용 테스트 환경의 로컬 ROS 참가자를 신뢰한다.

전용 배포판을 새 보고서 경로로 설치한다. 두 방향 모두 끄면 ROS2-01과 같은 단방향 동작이다. `-PublishClock`은 제어와 독립적인 opt-in이다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Ros2Bridge/setup-wsl.ps1 install -ReportRoot Saved/Reports/ROS2-02/wsl
# 같은 비어 있지 않은 MA0T10_AUTOMATION_TOKEN을 UE와 중계/WSL 자식에게만 상속한다.
# 값은 인자·화면·로그에 쓰지 않는다. UE는 -MA0T10Control로 명시적으로 API를 켠다.
# ControlConfig의 token 키가 있으면 환경변수보다 우선하므로 실제 API 토큰과 일치시킨다.
$ros2If = Get-NetIPAddress -AddressFamily IPv4 | Where-Object InterfaceAlias -Like 'vEthernet (WSL*'
# 한 개의 실제 WSL IPv4 항목을 확인한 뒤, 별도 터미널에서 테스트 동안만 실행한다.
python Tools/Ros2Bridge/control_relay.py --bind $ros2If.IPAddress --wsl-subnet "$($ros2If.IPAddress)/$($ros2If.PrefixLength)" --port 18099 --api-port 8099
# 위 중계가 실행 중인 상태에서:
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Ros2Bridge/setup-wsl.ps1 up -ReportRoot Saved/Reports/ROS2-02/wsl -EnableControl -PublishClock -Lidar SLAB-LIDAR-001 -CaptureSensor VCAM-001
```

`control_relay.py`는 Windows의 실제 WSL vEthernet 주소/prefix를 조회·대조하고 해당 주소만 bind한다. LAN/0.0.0.0/loopback bind는 거절한다. WSL subnet만 받고, 모든 요청에 토큰을 검사한 뒤 127.0.0.1로 전달한다. 중계와 제어 노드는 빈 토큰을 거절한다. Host는 `127.0.0.1:<api-port>`로 보존한다. Origin/중복·불필요한 헤더/파이프라이닝/비허용 경로는 거절한다. 중계는 8연결, 헤더8KiB·본문256KiB·요청수신 총2초로 제한하며 body/header/예외 내용을 로그에 쓰지 않는다. 프록시·redirect·자동 재시도는 없다. `--stop-file <자신이 소유한 marker>` 또는 Ctrl+C로 종료하고 listener 해제를 확인한다. 보안/방화벽 팝업이 나오면 입력하지 않고 중단한다. netsh·방화벽 규칙·`.wslconfig`를 바꾸지 않는다.

아래 명령은 Windows에서 **이름을 지정한** WSL에 실행한다. `<ROS>`는 `wsl -d ma0t10-ros2 -u root -- bash '/mnt/c/Unreal Projects/ma0t10_dt/Tools/Ros2Bridge/ros-cli.sh'`이다. 경로가 다른 checkout에서는 실제 WSL 경로를 사용한다.

| ROS 이름 (`/ma0t10/control/` 접두사) | 타입 | AUTO-01 매핑 / 의미 |
|---|---|---|
| `pcd` service | `std_srvs/srv/SetBool` | true: `POST /v1/pcd/start`, `-Lidar` 대상. false: `/v1/pcd/stop`, 소유 스트림만 정지 |
| `capture` service | `std_srvs/srv/Trigger` | `POST /v1/capture`, `-CaptureSensor` 대상. 먼저 측정을 시작해야 함 |
| `get_status` service | `std_srvs/srv/Trigger` | `GET /v1/status` |
| `request` topic | `std_msgs/msg/String`, reliable/volatile/depth8 | 아래 JSON 명령 |
| `result` topic | `std_msgs/msg/String`, reliable/volatile/depth16 | `ma0t10.ros2.result.v1`: request_id·op·ok·api 또는 고정 error 코드 |
| `status` topic | `std_msgs/msg/String`, reliable/transient-local/depth1 | 최대2Hz `ma0t10.ros2.status.v1`: ok·api 또는 고정 error 코드, 진행/terminal/dataset 상태 |

```bash
# 위 <ROS> 접두사 뒤에 붙일 인자 예:
service call /ma0t10/control/pcd std_srvs/srv/SetBool '{data: true}'
service call /ma0t10/control/pcd std_srvs/srv/SetBool '{data: false}'
service call /ma0t10/control/capture std_srvs/srv/Trigger '{}'
service call /ma0t10/control/get_status std_srvs/srv/Trigger '{}'
topic pub --once /ma0t10/control/request std_msgs/msg/String '{data: "{\"schema\":\"ma0t10.ros2.control.v1\",\"request_id\":\"start-camera-1\",\"op\":\"sensor.start\",\"params\":{\"sensor_id\":\"VCAM-001\"}}"}'
topic echo /ma0t10/control/status --qos-durability transient_local
```

JSON 최상위 키는 정확히 `schema`, `request_id`, `op`, `params`다. schema=`ma0t10.ros2.control.v1`, request_id는 `[A-Za-z0-9_.-]{1,64}`, params는 객체다. 중복 키·NaN/무한값·256KiB 초과·추가 키를 거절한다. JSON에 토큰을 넣지 않는다.

| `op` | `params` (AUTO-01와 같은 이름/단위/범위) |
|---|---|
| `status`, `sensors` | `{}`; GET `/v1/status`, `/v1/sensors` |
| `sensor.start`, `sensor.stop`, `capture`, `pcd.start` | `sensor_id` |
| `sensor.set` | `sensor_id` + `interval_sec` 또는 `profile`/`quality`. enum·admission은 UE가 검증 |
| `pcd.stop`, `scenario.stop`, `dataset.stop` | `{}` |
| `scenario.start` | `json_path`(**Windows 절대 경로**) 또는 `uuid` 중1개. 선택 `send_pcd`, `sensor_id` |
| `dataset.start` | `name`, `scenes`, `slab`, `lidar_id`, `camera_id`. 선택 `seed`, `settle_frames`, `camera_truth`, `lidar_fidelity`, `light`, `ranges` |

예: `{"schema":"ma0t10.ros2.control.v1","request_id":"dataset-3","op":"dataset.start","params":{"name":"ROS202","scenes":3,"slab":"SlabActor_0","lidar_id":"SLAB-LIDAR-001","camera_id":"VCAM-001"}}`.

서비스 `success=true`/topic result `ok=true`는 HTTP 접수 성공이다. 비동기 작업의 실제 성공은 **같은 `api.operation_id`**의 `terminal=true && success=true`를 status에서 확인한다. 취소의 terminal 실패는 정상적인 취소 표현이다. dataset 상태는 status의 `dataset_active`, `dataset_completed`, `dataset_requested`에도 있다. 기존 API의 긴 작업1개 제한/409/소유 작업만 정리하는 규칙을 그대로 따른다. 브리지 종료는 UE의 진행 중 작업을 임의 취소하지 않는다. 명시적으로 stop하고 terminal/drain을 확인한다.

같은 프로세스의 최근256개 request_id는 재실행하지 않고 이전 접수 결과를 돌려준다. 같은 ID의 다른 명령은 `request_id_conflict`다. 재시작/캐시 퇴거 이후의 영구 중복 방지는 제공하지 않는다. HTTP timeout/disconnect의 `*_outcome_unknown`은 UE가 이미 실행했을 수 있다는 뜻이다. 새 ID로 즉시 재시도하지 말고 status를 확인한다. 서비스 호출은 매번 새 ID를 쓰므로 이 topic 중복 보호를 대신하지 않는다.

## ROS2-02 시간 규칙

기본은 `/clock` 없음·consumer `use_sim_time=false`다. `up -PublishClock` 또는 직접 노드의 `--publish-clock`을 명시하면 **유효 프레임의 기존 acquisition UTC 최대값**을 `rosgraph_msgs/msg/Clock`으로 발행한다(reliable/volatile/depth10). 단일 브리지에서만 켜고 다른 `/clock` 발행자와 동시에 사용하지 않는다. 소비자는 `ros2 param set /<consumer> use_sim_time true` 또는 시작 시 `--ros-args -p use_sim_time:=true`로 선택한다. 첫 프레임 전 ROS time은0이며 측정이 없으면 clock도 멈춘다. 브리지의 queue/metrics/API polling timer는 steady clock이므로 `/clock` 초기0이나 정지에 묶이지 않는다.

PointCloud2/Image/CameraInfo/TF의 header.stamp는 계속 **원본 측정 UTC**다. Windows/WSL의 현재 시각·중계 지연·clock high-water로 덮어쓰지 않는다. 여러 센서/비동기 결과가 역순이면 오래된 stamp의 프레임은 그대로 발행하고 clock만 뒤로 가지 않는다(`clock_reordered` 집계). 따라서 clock은 모든 프레임 stamp 이상이며, 단일 순차 PCD에서는 같은 stamp 집합을 갖는다. 이는 acquisition 기반 ROS 시각이며 UE world elapsed-time·0초 epoch·외부 pause/step/reset 제어를 추가한 계약이 아니다. 브리지 재시작의 단조성은 세션 범위이며 Windows UTC가 역행하면 이전 최대값을 넘는 새 측정까지 clock은 유지된다.

`python Tools/Ros2Bridge/measure_clock.py --output Saved/Reports/ROS2-02/clock-offset.json`은 Windows precise UTC와 전용 WSL UTC를 pipe 왕복 midpoint로 비교한다. 최소RTT/2와 원시 sample을 함께 기록하고 시계를 조정하지 않는다. ROS `use_sim_time`은 소비자의 기준을 맞추며 OS 시계 오차를 고치지는 않는다. UTC 수신지연에는 이 오차와 drift가 들어간다. [ROS2 시간 설계](https://design.ros2.org/articles/clock_and_time.html)를 따른다.

검증 뒤 `down -ReportRoot Saved/Reports/ROS2-02/wsl` → 중계 종료 → `remove -ReportRoot Saved/Reports/ROS2-02/wsl`로 **소유 배포판만 unregister**하고 시작/종료 목록을 비교한다. 실측/실패 원본/skip/보호·복원 증거는 `Saved/Reports/ROS2-02/RESULT.md`를 따른다. `control_acceptance.py`는 지정 fixture 센서·Slab을 실제로 제어하므로 개인 운영 세션에 실행하지 않는다.

## 토픽·frame·시간·QoS

구독 기본값은 기존 `topic.virtual.sensor.export.0`, `topic.virtual.sensor.lidar.0`, `topic.virtual.sensor.camera.0`다. 직접 노드를 실행할 때 `--topic`을 반복해 바꿀 수 있다. 업무 `topic.scenario`나 ACK topic은 구독하지 않는다.

`<key> = s_ + 원본 SensorId의 UTF-8 바이트를 소문자 hex로 인코딩`. 영숫자·하이픈·한글·leading digit의 충돌을 피한다. 예: `LIDAR-01 → s_4c494441522d3031`. SensorId는 payload/통계에서 원문을 유지한다.

| 입력 | ROS 토픽 | frame / 정보 범위 |
|---|---|---|
| `virtual-pointcloud.pcd.v1` | `/ma0t10/<key>/points` (`PointCloud2`) | `ma0t10/<key>`; 원본 로컬 m·33바이트 유지 |
| `virtual-lidar.v1` | `/ma0t10/<key>/points_legacy` (`PointCloud2`) | `ma0t10_world`; hit만 UE 월드 cm→ROS 월드 m |
| `virtual-lidar.telemetry.v1` | `/ma0t10/<key>/telemetry` (`std_msgs/String`) | 통계 JSON; point/pose가 없어 점군·TF를 만들지 않음 |
| binary JPEG / `virtual-camera.v1` | `/ma0t10/<key>/image/compressed` (`CompressedImage`), 선택 `/image` (`Image rgb8`) | `ma0t10/<key>_optical` |
| Camera | `/ma0t10/<key>/camera_info` | binary: width/height만, `K[0]=0`(미보정). JSON: FOV 기반 pinhole 추정, 실장비 보정 아님 |
| PCD pose / JSON Camera pose | `/tf` | `ma0t10_world → ma0t10/<key>`; 각 acquisition 시각의 동적 TF |
| Camera optical basis | `/tf_static` | body→optical: z전방/x우측/y아래. 알려진 고정 축 관계만 발행 |

`header.stamp`는 ISO UTC payload 측정 시각을 nanosecond 단위로 변환한다. 수신/송신 현재 시각으로 덮지 않는다. `sensor_frame_id`는 정수 원형을 내부 통계에 유지하고, ROS Header에 sequence 필드가 없어 frame_id 문자열에 섞지 않는다. Slab frame_no/RunUUID와 혼동하지 않는다. 기존 legacy JSON/telemetry의 큰 frameId는 double 반올림 가능성이 있으므로 STOMP 문자열 `frame-id`를 기준으로 사용한다. JSON identity 검사는 2^53 이상에서 double 값이 일치하는 알려진 반올림만 허용하며 일반 불일치는 거절한다. 헤더 없는 JSON에서 사라진 정밀도를 복원할 수는 없다.

기본은 **`/clock` 없음·`use_sim_time=false`**다. ROS2-02의 명시적 acquisition UTC clock 옵션은 위 시간 규칙을 따른다. 전역 UE simulation time·pause/reset 계약을 추정하지 않는다. 여러 PC의 latency는 시계 오차를 포함하며 음수 latency도 숨기지 않는다.

기본 QoS는 best effort·volatile·keep last 5다. 직접 노드의 `--reliable --qos-depth N`으로 바꿀 수 있다(1~100). TF는 tf2 기본 QoS를 사용한다. STOMP는 비영구 multicast 구독·`client-individual`·8MiB byte window를 사용하고 변환/명시적 거절 후 ACK한다. 이 ACK는 브리지 처리/폐기 확인이며 ROS subscriber 수신·외부 업무 완료 증명이 아니다. Artemis의 `activemq.prefetchSize`도 메시지 수가 아니라 **바이트**로 해석되므로 사용하지 않는다.

## 좌표·필드 계약

PCD는 **이미 센서 로컬 m, X전방/Y좌측/Z위쪽**이다. PointCloud2 data는 원본 binary point 바이트 그대로이며 cm→m 또는 로컬 Y 반전을 반복하지 않는다. `sensor_to_world_m=T`는 로컬 m→UE 월드 m인 row-major 4×4/column-vector 행렬이다. ROS 월드는 `S=diag(1,-1,1,1)`로 정의하므로 TF는 **`T_ros=S*T`**다. 오른손 회전·직교성·행렬 convention을 검사하고 quaternion으로 바꾼다. pose 없는 외부 PCD에는 월드 TF를 추정하지 않는다. legacy JSON worldLocation만 `(x,y,z) cm → (x,-y,z)/100 m`로 변환한다.

| PointCloud2 필드 | offset | ROS datatype |
|---|---:|---|
| x / y / z | 0 / 4 / 8 | FLOAT32 |
| intensity / ring / horizontal_index | 12 / 14 / 16 | UINT16 |
| return_index / return_count | 18 / 19 | UINT8 |
| time_offset_ns_lo / time_offset_ns_hi | 20 / 24 | UINT32 / INT32 |
| validity | 28 | UINT8 |
| confidence | 29 | FLOAT32 |

Jazzy PointField에는 INT64가 없어 `time_offset_ns`를 두 필드로 손실 없이 노출한다. Python 복원은 `(hi << 32) | lo`(signed int64 ns)다. 부동소수점 시각으로 재포장하지 않는다. point_step=33, row_step=33×POINTS, little endian, height=1, is_dense=false(보수적 선언). validity·confidence의 의미/수치는 그대로 유지한다. POINTS=0은 정상 프레임이다. 필드 정렬 padding을 추가하지 않는다.

JSON Camera pose는 acquisition `forward/up/location`으로 오른손 basis를 계산하고 UE 월드 cm를 변환한다. binary JPEG에는 pose/FOV가 없어 센서 월드 TF·intrinsics를 추정하지 않는다. 영상은 optical 축으로 표시하고 픽셀을 좌우 반전하지 않는다.

## 검증·관찰·제한

```powershell
python -m unittest discover -s Tools/Ros2Bridge -p 'test_*.py' -v
wsl -d ma0t10-ros2 -u root -- /opt/ma0t10-ros2-venv/bin/python -m unittest discover -s '/mnt/c/Unreal Projects/ma0t10_dt/Tools/Ros2Bridge' -p 'test_*.py' -v
wsl -d ma0t10-ros2 -u root -- bash '/mnt/c/Unreal Projects/ma0t10_dt/Tools/Ros2Bridge/check-messages.sh'
# 실행 중인 bridge/Artemis와 credential 환경이 있을 때만 synthetic golden을 보낸다.
wsl -d ma0t10-ros2 -u root -- bash '/mnt/c/Unreal Projects/ma0t10_dt/Tools/Ros2Bridge/test-ros.sh'
wsl -d ma0t10-ros2 -u root -- bash '/mnt/c/Unreal Projects/ma0t10_dt/Tools/Ros2Bridge/ros-cli.sh' topic list
```

`probe.sh --seconds 365 --output <WSL에서 보는 Saved/Reports/...json>`은 독립 ROS subscriber로 수신 수·bytes·UTC latency·gap을 기록한다. `measure-cli.sh <report-directory> 310`은 PCD topic을 찾아 `ros2 topic hz --wall-time`과 `ros2 topic bw`를 병행 기록한다. 두 값은 DDS 구독 측 관측이며 UE의 acquisition/submit/receipt와 구분해 대조한다. `Tools/StompFaultServer/run-native.ps1 -Broker Artemis -Interval .05 -Lidars 1 -Seconds 300`(.1도 동일)은 기존 1920×1080 D3D12 PIE·10초 준비·ML-X Native 576×56 fixture로 송신한다. 사용자 맵을 저장하지 않는다. 실행 전 개인 SaveGame·Editor 설정을 백업하고 종료 후 복원한다.

브리지 상한은 body 64MiB·PCD header 64KiB, queue 32건/64MiB, publisher 512개, stream별 latency 최근 6000건이다. queue 초과는 새 수신을 명시 폐기하고 센다. schema·크기·checksum·점 수·유한 좌표·metadata identity·pose convention을 검사해 미지원 schema와 손상 데이터를 발행하지 않는다. ROS 기본 의존성 NumPy가 있으면 33바이트 strided view로 좌표를 검사하고, 없으면 Windows에서도 실행 가능한 표준 Python 검사를 쓴다. 로그에는 집계 카운트만 남기고 자격 증명·header/body·예외 본문을 쓰지 않는다. 인증은 환경변수만 사용하고 core dump는 프로세스 단위로 끈다. live multicast 재연결이므로 끊김 중 영구 replay나 무손실을 보장하지 않는다.

현재 실측·실패 원본·직접 검수 가능 여부·WSL 삭제·다운로드량은 `Saved/Reports/ROS2-01/RESULT.md`에 기록한다. 구현, synthetic golden, 실제 UE PIE, 직접 3D 표시 수용을 구분한다.

2026-10-15 검증 상태는 **80%**다. Windows25·WSL23 Success/skip2(Pillow 없음), ROS 직렬화3·실제 Artemis adapter4가 통과했다. UE의 준비10초+측정300초 송신 회귀는20/10Hz 모두 Success지만 외부 수신 총계는 각각 UE6200→브리지6200→독립ROS6119, UE3099→브리지3098→독립ROS3083이다. 브리지 폐기/queue 초과0이며10Hz 미수신1의 위치는 미확정이다. best-effort/발견을 포함한 구독 측 차이를 숨기거나 전 구간 무손실로 표시하지 않는다. `ros2 topic hz/bw`와 독립 probe는 서로 다른 subscriber다. 직접3D/축 비교·실제 WSL Image 디코더 수용은 보류다. 테스트 WSL은 두 설치 모두 삭제했고 기존3개/기본Ubuntu(v1) 및 보호 파일을 보존했다.

Windows localhost의 `check_foxglove.py`는 설치된 SDK bridge의 `foxglove.sdk.v1` 프로토콜로 HTTP 101·PointCloud2 채널 광고를 확인한다. 브라우저 3D 표시 검사는 별도다. `summarize.py Saved/Reports/ROS2-01/<run>`은 `<run>-bridge.json`, `<run>-ros.json`, native ledger와 CLI 로그를 대조한다. source 전체 수에는 준비 10초가 포함되고 ledger는 측정 300초만 포함하므로 분모를 섞지 않는다.

참고: [ROS Jazzy 공식 apt 절차](https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html), [REP-103](https://www.ros.org/reps/rep-0103.html), [Artemis STOMP flow control](https://artemis.apache.org/components/artemis/documentation/latest/stomp.html), [WSL commands](https://learn.microsoft.com/en-us/windows/wsl/basic-commands).
