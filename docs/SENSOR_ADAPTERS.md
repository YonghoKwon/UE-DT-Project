# 외부 센서 입력과 adapter

공통 입력 기반은 [RealSensorSourceComp](../Plugins/DTDevice/Source/DeviceRuntime/Public/Device/Sensor/RealSensorSourceComp.h)입니다. CSV/JSONL replay, JSON live 및 HTTP/UDP live bridge는 DeviceRuntime의 Sensor/Transport에 있습니다. 과제 전문·서버 정책은 host가 소유합니다.

- 단위·축·좌표를 정규화한 immutable frame을 `SubmitExternalFrame` 경로로 넘깁니다. 측정/송신과 진단 수신을 섞지 않습니다.
- 카메라 입력은 [VirtualCameraCaptureComp](../Plugins/DTDevice/Source/DeviceRuntime/Public/Device/Camera/VirtualCameraCaptureComp.h), LiDAR는 [VirtualLidarScanComp](../Plugins/DTDevice/Source/DeviceRuntime/Public/Device/Sensor/VirtualLidarScanComp.h)와 연결합니다.
- 업무 TC 등록은 DTCore 및 프로젝트 DataTable 정책을 따릅니다. [샘플](../Samples/websocket/lidar_json_live_frame_sample.json)과 [검사](../Scripts/validate_websocket_lidar_live_sample.ps1)를 사용합니다.
- 준비도/배포 보고서는 실장비 수용을 대신하지 않습니다. source/clock/프레임·UTC/context를 유지하고 종료·세대 변경 뒤 늦은 결과를 적용하지 않습니다.

상세 정규화·전송 계약은 [LiDAR](lidar_payload_schema.md), [Camera](camera_payload_schema.md), [서버](server_transport_contract.md)를 따릅니다. 기존 구현의 설계·이전 장치 예시 원문은 [archive](archive/real_sensor_adapter_plan.md)에 보존합니다.
