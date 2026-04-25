# Virtual Camera + LiDAR 센서 모듈 사용 가이드

## 1) 이번 리팩토링 핵심

- 카메라/라이다 센서 데이터 공통 포맷(`FVirtualSensorPacket`) 추가
- 카메라 캡처 모듈(`UVirtualCameraComp`) 정리
  - 주기 캡처
  - JPEG + Base64 패킷화
  - 이벤트 브로드캐스트
  - 선택적 HTTP 자동 전송
- 라이다 모듈(`UVirtualLidarComp`) 신규 구현
  - 다채널/다샘플 라인 트레이스 스캔
  - 디버그 라인 시각화
  - 포인트클라우드 형태 JSON 패킷 생성
  - 선택적 HTTP 자동 전송
- 외부 시스템 전달용 공통 전송 컴포넌트(`UVirtualSensorPublisherComp`) 추가

## 2) 클래스 구성

- `AVirtualCameraAct`
  - `UVirtualCameraComp`
  - `UVirtualSensorPublisherComp`
- `AVirtualSensorAct`
  - `UVirtualLidarComp`
  - `UVirtualSensorPublisherComp`

## 3) 레벨 세팅 방법

1. UE 에디터에서 대상 레벨(예: `BasicMap`)을 엽니다.
2. 월드 아웃라이너에 아래 액터를 배치합니다.
   - `VirtualCameraAct`
   - `VirtualSensorAct`
3. `VirtualCameraAct` 설정
   - `CaptureInterval`: 권장 0.1 ~ 0.5
   - `CaptureResolution`: 예) 1280x720
   - `JpegQuality`: 권장 70 ~ 90
   - `bPublishToHttpAutomatically`: 외부 전송 필요 시 true
4. `VirtualSensorAct` 설정
   - `ScanInterval`: 권장 0.05 ~ 0.2
   - `HorizontalSamples`: 예) 180
   - `VerticalChannels`: 예) 16
   - `MaxDistanceCm`: 예) 10000
   - `bDrawDebug`: 테스트 단계 true 권장
   - `bPublishToHttpAutomatically`: 외부 전송 필요 시 true
5. 전송 필요 시 각 액터의 `VirtualSensorPublisherComp`에서
   - `bEnableHttpPublish = true`
   - `EndpointUrl` 지정 (예: `http://127.0.0.1:8080/digital-twin/sensor`)
   - 필요 헤더 입력

## 4) 실행 후 확인 포인트

- 카메라
  - 렌더 타겟 갱신 + 로그(`[VirtualCameraComp]`) 확인
  - `OnCameraPacketReady` 블루프린트 바인딩 가능
- 라이다
  - 레벨 뷰포트에서 디버그 레이/히트포인트 확인
  - 로그(`[VirtualLidarComp]`) 확인
  - `OnLidarPacketReady` 블루프린트 바인딩 가능
- 전송
  - HTTP 응답 로그(`[VirtualSensorPublisher]`) 확인

## 5) 외부 시스템 연동 팁

- 백엔드에서 `sensorType`(camera/lidar) 기준 라우팅
- `timestampUnixMs`로 시간 정렬
- 카메라는 base64 decode 후 JPEG 처리
- 라이다는 `points` 배열을 point cloud ingestion 로직에 바로 전달

## 6) 성능 튜닝 가이드

- 카메라 부하가 크면
  - `CaptureInterval` 증가
  - 해상도/품질 감소
- 라이다 부하가 크면
  - `HorizontalSamples`, `VerticalChannels` 감소
  - `ScanInterval` 증가
  - `bDrawDebug` false

