# 학습 배치 운영 경계

기본 꺼짐이며 과제 프로젝트가 소유합니다. 입력은 `ma0t10.train.job.v1`, 시간 의미는 20Hz `sampled_timeline`입니다. 실시간 물리 학습 모드는 포함하지 않습니다. [TrainBatchSubsystem](../Source/ma0t10_dt/MA0T10/Training/TrainBatchSubsystem.h)과 [전용 화면](../Source/ma0t10_dt/MA0T10/UI/Topview/DxWidgetTrainBatch.h)을 사용합니다.

1. Job manifest를 받아 상한·ID·길이·경로를 검증하고 journal을 저장합니다. 기존 일반 시나리오 우선 정책은 유지합니다.
2. 코디네이터가 독립 워커 M개에 시나리오를 분배합니다. heartbeat/lease/재할당·디스크 resume를 사용하며 고유 UserDir로 설정·native 안전 상태를 분리합니다. 한 워커 실패가 다른 워커를 종료하지 않습니다.
3. 목록에서 확정 완료 시나리오를 선택해 저장 PCD를 보기 전용으로 조회합니다. 생성 중 partial 파일을 완료 결과로 읽지 않습니다.
4. 위젯과 같은 [Delivery](../Source/ma0t10_dt/MA0T10/Training/TrainBatchDelivery.h) 경로로 artifact/checksum을 생성합니다. Windows는 ZIP과 tar.gz를 지원하며 ZIP 기본값은 유지합니다. Linux는 tar.gz만 지원하고 ZIP은 명시적으로 거절합니다. tar.zst는 보류/미지원입니다. 기본은 Job 전체 성공만 전달하며, 실패 목록 확인 후 성공분만 선택할 때 제외 목록을 포함합니다.
5. [SFTP](../Source/ma0t10_dt/MA0T10/Training/TrainBatchSftp.h) 설정 후 전송 버튼을 사용합니다. 이번 범위는 HTTP 업로드가 아닙니다.

Windows 고정 시스템 OpenSSH 경로의 sftp.exe/ssh.exe를 사용하며 없으면 거절합니다. SSH 키와 ssh-agent, BatchMode=yes, StrictHostKeyChecking=yes를 사용합니다. 첫 지문은 사용자가 확인해 앱 전용 known_hosts에 등록합니다. 비밀번호/passphrase를 저장하지 않습니다. 서버 설정은 사용자 Saved에만 저장합니다. 실제 인증값·개인키 내용·passphrase는 로그/압축물/커밋에 넣지 않고 화면의 민감 부분을 마스킹합니다.

`.partial` 업로드·reput 재개·크기 확인·rename과 sidecar SHA256/artifact.json으로 완료를 구분합니다. 선택 remote sha256sum은 고정 검증 명령이며 임의 shell 실행이 아닙니다. [도구](../Tools/TrainBatchTools/verify_archive.py)로 압축물을 독립 검증합니다. 실제 Linux 서버 수용은 별도 환경에서 확인하고 로컬 단위/합성 성공과 구분합니다.

과거 입력 계약·상한·단계 설계 원문은 [보관본](archive/TRAIN_BATCH_01_DESIGN.md)을 참고하되 현재 타입·기본값은 소스와 이 문서가 우선입니다.


## 플랫폼별 압축 지원 (STRUCTURE-12 / r57b)

| 대상 | ZIP | tar.gz | UI/기본값 |
|---|---|---|---|
| Windows | 기존 libzip 구현·기본값 유지 | 기존 zlib 구현 유지 | 기존 ZIP/tar.gz 선택과 저장 형식 유지 |
| Linux | 미지원, 생성 요청을 파일 생성 전 오류로 거절 | zlib 스트리밍·OpenSSL SHA-256 유지 | 형식은 tar.gz, ZIP 선택 비활성 및 미지원 사유 표시 |

`ma0t10_dt.Build.cs`는 libzip 의존을 Win64에만 추가하고 `WITH_TRAIN_BATCH_ZIP`을 정의합니다. 엔진/외부 라이브러리는 변경하지 않습니다. OpenSSL과 zlib는 명시적 공통 의존입니다. 프로그램에서 Linux 압축을 요청할 때는 `FTrainArchiveOptions.Format = ETrainArchiveFormat::TarGz`를 지정합니다. ZIP 요청은 `ZIP creation is unavailable ... use tar.gz` 오류를 돌려줍니다. 지원 여부는 `TrainBatchArchive::IsFormatSupported`로 확인할 수 있습니다.

Linux UI는 기존 Windows 설정 파일을 자동 변경하지 않고 현재 화면에서 지원하는 tar.gz만 사용합니다. Windows 설정·enum 값·기본값·ZIP writer 실행 코드는 그대로입니다. Linux 전체 학습 워커/저널/SFTP 수용을 이번 압축·패키징 검증으로 대신하지 않습니다. 기존 coordinator 원자적 저널 writer는 Windows 전용이며 일반 IPC 이식은 별도 범위입니다.

Development Linux 패키지의 `-TrainBatchArchiveProbe=<프로젝트 Saved/Reports 아래 JSON 경로>`는 진단 전용입니다. ZIP 거절·tar.gz·SHA-256을 작은 고정 입력으로 확인하고 종료하며, 플래그가 없으면 생성/실행하지 않습니다. 화면 없는 NullRHI 실행은 실제 Linux ELF 기동과 압축 검증이며 렌더/GPU/UI 수용을 뜻하지 않습니다. 결과 원본은 `Saved/Reports/STRUCTURE-12/r57b`에 보관합니다.
