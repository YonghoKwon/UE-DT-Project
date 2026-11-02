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
