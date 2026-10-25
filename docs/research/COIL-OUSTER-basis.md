# COIL-OUSTER 기본 프로필 변경 근거

사용자 결정2026-10-25. 시작 cd19f94, 결과/merge 기준 a0ca52a. DTCore b2504d1 유지.

- 코일 `ConfigureLidar()`의 신규 생성 기본값만 HesaiXT32→OusterOS1_64_Rev7로 변경한다. 원료 하역과 같은 기존 `IndustrialLidar` 프로필·FullSpec 경로를 쓰며1024×64/20Hz/360°/수직−21.2~21.2°/90m envelope/0.5m 최소거리/30mm noise parameter를 적용한다. 설치 위치·자세·material response·미리보기 stride2/12000점은 유지한다.
- ID os_lidar_coil_quay, topic, PCD33-byte 레코드, FIFO, 예산/예약 상한, 슬롯/버전과 enum 순서는 유지한다. 기존 옛 ID 읽기 별칭도 유지한다.
- `ConfigureLidar()`는 BeginPlay에서 새 센서를 만들 때만 호출된다. 저장 배치의 명시 프로필과 사용자 지정 간격/미리보기 수를 실제 layout loader로 복원한 뒤 장면 재구성에서도 그대로 유지하는 회귀를 추가한다. 코일 적재 SaveGame/계획 JSON에는 원래 센서 프로필 필드가 없으며 새 migration이나 설정 덮어쓰기를 추가하지 않는다.
- 코일 UI는 현재 `GetDeviceSpec()`의 제조사/모델과 실제 설정 간격의 목표Hz를 표시한다. Ouster를 UI 문자열에 고정하지 않으므로 명시적으로 복원한 XT32 등도 실제 모델/목표Hz로 표시된다. 기존5Hz 갱신 경로 안에서 처리한다.
- 기존 환경 캡처 테스트가 Ouster를 강제 적용하던 구문을 제거하고 실제 신규 기본값을 검사하도록 강화한다. KeyGeometry의 SI 계산, cylinder fit의 잔차2cm·중심 편차p95 3cm 등 기존 판정값은 변경하지 않는다.

## 송신 예산

기본은 multi-hit=false의 단일 반환이다. 모든65,536개 ray가 반환되어 Full33-byte 레코드로 전송된다고 가정하면 프레임2,162,688바이트,20Hz에서43,253,760바이트/초=41.25MiB/s다. PCD/STOMP 헤더는 별도이나80MiB/s 대비38.75MiB/s 여유가 있다. 실제 hit-only Full 비압축 경로를 자격증명 없는 TCP STOMP loopback에서 별도로 검증한다. 획득/직렬화/접수/receipt/내부 검증/외부 수신의 수와Hz를 나눠 기록하며 receipt를 업무 ACK로 취급하지 않는다.

사용자가 dual/multi-return을 명시적으로 선택한 경우는 기본 단일 반환과 다르다.2반환 최악가정은82.5MiB/s+헤더로80MiB/s를 초과할 수 있다. 이 설정도 임의로 덮어쓰지 않으며 기존 예산 경고·rate control/FIFO 경계를 유지한다.

## 검증 범위

관련 코일/해치 회귀·quick/layer/guard, 실제 PCD 송신/검증 수신,1920×1080 점군 포함 캡처1~2장과 코일 저각 RT on ABAB2%를 사용한다. 성능 전/후는 이 변경 직전 cd19f94와 새 제품 커밋으로 고정해 같은 크레인 형상·카메라·코일 수를 비교한다. 프로필별 점 예산의 변화는 의도된 입력 차이로 명시하고 품질/Hz/노이즈/정확도 기준을 낮추지 않는다. 누적 result/merge 기준 a0ca52a와 성능 비교 기준을 구분한다.

거리 envelope와 교정 한계는 기존 [HATCH-07 근거](HATCH-07-basis.md)를 따른다.90m/10Hz 자료 조건을20Hz에서 같은 검출확률 보장으로 바꾸지 않는다. 새 vendor SDK/교정·현장 수용을 주장하지 않는다. 실제 수치·실패·미수행은 [RESULT](../../Saved/Reports/COIL-OUSTER/RESULT.md)와 result.json을 따른다.
