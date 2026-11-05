# 학습 배치 운영 경계

기본 꺼짐이며 과제 프로젝트가 소유합니다. 입력은 `ma0t10.train.job.v1`, 시간 의미는 20Hz `sampled_timeline`입니다. 실시간 물리 학습 모드는 포함하지 않습니다. [TrainBatchSubsystem](../Source/ma0t10_dt/MA0T10/Training/TrainBatchSubsystem.h)과 [전용 화면](../Source/ma0t10_dt/MA0T10/UI/Topview/DxWidgetTrainBatch.h)을 사용합니다.

1. Job manifest를 받아 상한·ID·길이·경로를 검증하고 journal을 저장합니다. 기존 일반 시나리오 우선 정책은 유지합니다.
2. 코디네이터가 독립 워커 M개에 시나리오를 분배합니다. heartbeat/lease/재할당·디스크 resume를 사용하며 고유 UserDir로 설정·native 안전 상태를 분리합니다. 한 워커 실패가 다른 워커를 종료하지 않습니다.
3. 목록에서 확정 완료 시나리오를 선택해 저장 PCD를 보기 전용으로 조회합니다. 생성 중 partial 파일을 완료 결과로 읽지 않습니다.
4. 위젯과 같은 [Delivery](../Source/ma0t10_dt/MA0T10/Training/TrainBatchDelivery.h) 경로로 artifact/checksum을 생성합니다. ZIP과 tar.gz를 지원하고 tar.zst는 보류/미지원입니다. 기본은 Job 전체 성공만 전달하며, 실패 목록 확인 후 성공분만 선택할 때 제외 목록을 포함합니다.
5. [SFTP](../Source/ma0t10_dt/MA0T10/Training/TrainBatchSftp.h) 설정 후 전송 버튼을 사용합니다. 이번 범위는 HTTP 업로드가 아닙니다.

Windows 고정 시스템 OpenSSH 경로의 sftp.exe/ssh.exe를 사용하며 없으면 거절합니다. SSH 키와 ssh-agent, BatchMode=yes, StrictHostKeyChecking=yes를 사용합니다. 첫 지문은 사용자가 확인해 앱 전용 known_hosts에 등록합니다. 비밀번호/passphrase를 저장하지 않습니다. 서버 설정은 사용자 Saved에만 저장합니다. 실제 인증값·개인키 내용·passphrase는 로그/압축물/커밋에 넣지 않고 화면의 민감 부분을 마스킹합니다.

`.partial` 업로드·reput 재개·크기 확인·rename과 sidecar SHA256/artifact.json으로 완료를 구분합니다. 선택 remote sha256sum은 고정 검증 명령이며 임의 shell 실행이 아닙니다. [도구](../Tools/TrainBatchTools/verify_archive.py)로 압축물을 독립 검증합니다. 실제 Linux 서버 수용은 별도 환경에서 확인하고 로컬 단위/합성 성공과 구분합니다.

과거 입력 계약·상한·단계 설계 원문은 [보관본](archive/TRAIN_BATCH_01_DESIGN.md)을 참고하되 현재 타입·기본값은 소스와 이 문서가 우선입니다.
