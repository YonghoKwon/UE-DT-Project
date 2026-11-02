# TrainBatchTools

TRAIN-BATCH-01 결과 압축 파일을 엔진과 독립된 Python 표준 라이브러리로 검증하는 도구입니다.

```powershell
python -I Tools/TrainBatchTools/verify_archive.py <job.zip|job.tar.gz> <checksums.sha256> [--prefix job-1/]
python -m unittest test_verify_archive   # Tools/TrainBatchTools 폴더에서
```

- 압축을 풀어 디스크에 쓰지 않고, 스트리밍으로 각 항목의 SHA-256을 계산합니다.
- 다음 경우를 실패로 판정합니다: 체크섬 목록의 항목이 없음, 해시가 다름, 목록에 없는 항목이 있음, 경로가 압축 루트를 벗어남(`..`, 절대 경로, `\`, 드라이브 경로).
- ZIP은 CRC도 확인합니다(`testzip`). tar.gz는 일반 파일이 아닌 항목(링크, 장치 파일)이 있으면 거절합니다.
- 자동화 테스트 `MA0T10.TrainBatch.Archive`가 `Saved/Automation/TrainBatchIo/output`에 남기는 `job-1.zip`, `job-1.tar.gz`, `checksums.sha256`을 이 도구로 다시 검증할 수 있습니다.

tar.zst는 사용자 결정(2026-11-02)으로 보류했습니다. zstd 소스를 포함하지 않았으므로 이 도구도 지원하지 않습니다.

## 로컬 SFTP 수신 서버 (테스트 전용, Docker)

`sftp-test/`는 공식 `alpine:3.20` 이미지에 OpenSSH만 추가한 테스트용 수신 서버입니다. 키 로그인만 허용하고(비밀번호·root 로그인 불가), 계정은 `trainer` 하나, 업로드 폴더는 `/data/inbox`입니다. `127.0.0.1`에만 열어 PC 밖에서는 접속할 수 없습니다.

```powershell
# 1) 테스트 전용 키 (Saved 아래, 커밋되지 않음)
& "$env:SystemRoot\System32\OpenSSH\ssh-keygen.exe" -q -t ed25519 -N '""' -f "Saved\TrainBatch\SftpTest\id_ed25519"
# 2) 서버 빌드·실행
docker build -t ma0t10-train-sftp-test:local Tools\TrainBatchTools\sftp-test
docker run -d --name ma0t10-train-sftp-test -p 127.0.0.1:2222:22 -v "${PWD}\Saved\TrainBatch\SftpTest\id_ed25519.pub:/etc/ssh/authorized_keys/trainer:ro" ma0t10-train-sftp-test:local
# 3) 라이브 테스트 (환경 변수가 없으면 이 테스트는 아무것도 올리지 않고 건너뜀)
$env:MA0T10_SFTP_LIVE_HOST='127.0.0.1'; $env:MA0T10_SFTP_LIVE_PORT='2222'; $env:MA0T10_SFTP_LIVE_USER='trainer'
$env:MA0T10_SFTP_LIVE_KEY="${PWD}\Saved\TrainBatch\SftpTest\id_ed25519"; $env:MA0T10_SFTP_LIVE_DIR='/data/inbox'
# UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MA0T10.TrainBatch.SftpLive; Quit"
# 4) 서버 쪽 확인 및 정리
docker exec ma0t10-train-sftp-test sh -c 'cd /data/inbox/run-* && sha256sum -c *.zip.sha256'
docker rm -f ma0t10-train-sftp-test
```

`MA0T10.TrainBatch.SftpLive`가 확인하는 것: 등록되지 않은 서버 키 거절 → 지문 확인 후 등록, `.partial` 업로드와 크기 확인 후 이름 변경(결과물 본체는 마지막), 서버에서 내려받은 바이트의 SHA-256 일치, 같은 이름 재업로드 시 덮어쓰지 않음(Conflict), 반쯤 올라간 `.partial`을 `reput`으로 이어 올리기, 바뀐 서버 키 거절. `known_hosts` 경로에 공백이 있어도(예: `C:\Unreal Projects`) 동작해야 하므로 테스트 경로에도 일부러 공백을 넣었습니다.
