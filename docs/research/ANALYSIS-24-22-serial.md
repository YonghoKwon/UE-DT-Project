# ANALYSIS-24 — Slab 의미 분류 직렬 재현과 최소 수정

작업 위치 `C:/Unreal Projects/ma0t10_dt`, `chore/analysis-24-22`. 기준은 `origin/master` **d2ca1d1d9a5c2d968a0dad39643007ac245c6f4a**다. 이전 병렬 단계의 빌드 금지는 해제됐으며, 이번 직렬 작업의 package 원본/요약은 `Saved/Reports/ANALYSIS-24-22/package/`에 둔다. 실행별 최종 commit·검증 수·실패·skip·복원·정리 결과는 [RESULT](../../Saved/Reports/ANALYSIS-24-22/RESULT.md) 및 result.json을 따른다.

## 확인한 원인

Tag가 Shipping에서 직렬화되지 않는 문제가 아니었다. **private scene의 editor 전용 visibility 구성과 cooked 실행의 PSO 준비 지연**이라는 두 초기화 문제가 있었다. metadata table은 정상인데 실제 semantic pass에 primitive가 그려지지 않아 ID/depth가 배경값으로 남았다.

- `Sensor/VirtualSensorSemanticLabels.cpp:27–42`는 runtime Actor Tags/이름/class/component 규칙을 사용하며 ActorLabel/editor metadata를 읽지 않는다.
- `Sensor/VirtualLidarSemanticScene.cpp`의 `FPreviewScene::ConstructionValues` 기본값은 bEditor=true다. 로컬 UE5.3 `Engine/Source/Runtime/Engine/Public/PreviewScene.h:23–34,63` 및 `Private/PreviewScene.cpp:31–40`에서 이것이 **EditorPreview world + editor hit proxies**를 선택하는 것을 확인했다.
- 기준 `698a895`의 Editor `-game` 및 실제 Development 패키지 모두 기존 `/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap`에서 32,256점을 생성했지만 **Slab0, instance0=32,256**이었다. diagnostic에는 named surfaces4/Slab surface1이 있고 semantic pixels81,792개도 존재했지만 **depth_mismatch32,256 / mapped0**이었다. 즉 metadata가 동결된 이후의 depth 대응 단계에서 누락됐다.
- 유일한 production 동작 변경을 `.SetEditor(GIsEditor)`로 한 로컬 A/B에서 Editor `-game` 결과가 **Slab1,800 / instance0=0 / depth_mismatch0 / mapped32,256**으로 바뀌었다. Slab/sensor pose·치수는 같고 PCD binary body의 SHA256도 전후 **ac492cd014eee9876035bd9f5b4026506579ce02ecedf175baf6f03ea0b8e16b**로 완전히 같았다. 측정 geometry/body는 그대로 두고 semantic identity 대응을 복구한 것이다.

world type 문제는 UE5.3 `SceneComponent.cpp:3093–3102`의 ShouldRender/UsesGameHiddenFlags 분기로도 확인했다. 그러나 `efa5848`의 world 구성 수정만 적용한 실제 Development 패키지는 첫 장면이 여전히 Slab0이었다. 그 실패를 보존하고 Editor 성공을 패키지 통과로 옮기지 않았다.

추가 native 진단은 registered/visible/should_render/render_state가 각각4인 반면 **scene_proxy0 / precaching4**임을 보였다. semantic depth는 배경1e8cm, main depth는 약1800cm였다. UE5.3 `Engine/Private/StaticMeshRender.cpp:2621–2624`는 PSO가 준비되지 않은 `DelayUntilPSOPrecached` 상태에서 CreateSceneProxy가 nullptr을 반환한다. 실제 cold 3장면 실행도 **첫 장면 Slab0 → 2·3번째 1800**이었다. PSO 준비 전 pass를 완료로 읽은 것이 남은 패키지 누락 원인이었다.

두 번째 수정은 전역 PSO 정책을 바꾸지 않는다. private proxy geometry/identity/pose를 acquisition 시점에 동결하고, PSO/scene proxy 준비가 끝날 때까지 기존 비동기 Poll에서 semantic pass만 보류한다. LiDAR는 이미 캡처한 main depth target을 유지한다. Camera GT는 원래 RGB/depth를 즉시 staging copy하고 semantic copy만 나중에 큐에 넣어 RGB target이 다른 frame으로 바뀌어도 영향을 받지 않는다. 새 main capture/readback/동기 wait나 다음 frame으로의 조용한 교체는 없다. cancel/generation과 기존 timeout을 유지하며 준비 실패는 명시적 실패로 남는다.

이 WIP native trial은 첫 장면부터 3개 모두 named Slab1800, instance0=0, depth mismatch0이었다. camera_truth=true의 별도 cold 3장면에서도 LiDAR1800씩, Camera raw class2 mask19,257 pixels씩을 얻었다. package baseline과 수정 trial의 PCD body SHA256도 위 값과 같았다. 최종 committed-SHA 패키지 실행은 RESULT의 commit/binary hash와 판정만 기준으로 한다.

1cm 허용치 확대·Tag 강제 추가·ID 재번호화·좌표/해상도 변경·CPU fallback으로 해결하지 않았다. Editor 안의 기존 scene 구성은 유지한다. Camera GT와 legacy GPU LiDAR가 같은 private semantic scene을 사용하므로 #24의 양쪽 pending/readback만 수정했다. #22 듀얼 미리보기 코드는 바꾸지 않았다.

## #28b 병합 보존

`698a895`는 d2ca1d1과 이전 진단 commit4c815bd를 병합했다. 헤더/Sampling/SceneQuery의 conflict에서 양쪽 include·timing trace를 보존했다. 광선 packet 병렬화, readback 직후 worker 정규화, CPU snapshot worker, lease 비동기 reset을 되돌리지 않았다.

#28b의 기본 readback worker는 기존 GT Poll의 raw hit 진단 위치를 지나지 않는다. 따라서 명시적 진단 때만 acquisition GT에서 작은 immutable metadata snapshot을 만들고, 기존 normalization worker에서 순수 요약을 만들며, WorkerReady release/acquire 뒤 GT의 기존 async FileSave로 전달한다. renderer worker에서 UObject를 읽거나 새 capture/readback을 만들지 않는다. 취소/generation 검사는 기존 경로를 유지한다. 빠른 readback 경로와 기존 Poll 경로가 모두 같은 one-shot 보고를 할 수 있도록 연결했다.

## 회귀와 파일 감사

`Scripts/audit_lidar_semantics.py`는 dataset.json의 class/instance table을 원본으로 사용한다. 해당 dataset의 모든 scene에서 다음을 대조한다.

1. requested/completed scene 수와 실제 scene 폴더 수.
2. binary PCD POINTS×33B와 `<BI` little-endian label POINTS×5B.
3. semantic/instance 번호가 같은 dataset manifest에 존재하는지(instance0은 미연결 sentinel).
4. 실제 `label.json` backend가 명시한 기대 backend와 같은지. backend2 DepthProjection을 backend3 HWRT 통과로 옮기지 않는다.
5. `--require-slab`이면 **모든 scene에서 named Slab point >0**. 한 scene 양성으로 다른 all-Other scene을 숨기지 않는다.

```powershell
python -X utf8 Scripts/audit_lidar_semantics.py '<실제 dataset.json이 있는 폴더>' --expect-backend 2 --require-slab --summary Saved/Reports/ANALYSIS-24-22/package/<실행>/semantic-audit.json
python -X utf8 -m unittest discover -s Scripts -p test_audit_lidar_semantics.py -v
```

5개 Python 회귀는 named Slab/LE instance, 구조적 성공과 all-Other 품질 실패 분리, backend 혼동, 잘린 label/미등록 instance, scene별 양성 조건을 검사한다. fixture는 Saved 아래의 명시된 임시 폴더에만 만들고 절대 경로 경계를 확인한 뒤 정리한다. 양성 Slab 수는 알려진 Slab fixture의 국소 gate이며 모든 물체의 semantic 정확도·외부 소비자 ACK를 보장하지 않는다.

기존 diagnostic C++ 순수 테스트3개와 다음 RHI 회귀를 사용한다. 모든 실제 결과와 SHA는 RESULT를 따른다. process exit0/Success 내부 skip을 실제 통과로 세지 않는다.

- `MA0T10.LidarSemanticDiagnostics`(3개)
- `MA0T10.GpuLidar.PointModelSnapshot`, `.StaticCollisionRhi`, `.RigidInstanceRhi`, `.FallbackSelection`
- `MA0T10.LidarRegression.GpuSemanticRhi`
- `MA0T10.CameraGroundTruth.DepthSemanticInstanceRhi`
- `MA0T10.GpuCaptureLifetime`(취소/반복/설정 변경 4개)

## 직렬 package 재현 조건

Editor/Live Coding 정상 종료 뒤 UE5.3.2 Editor Development를 먼저 빌드한다. 일반 PC의 script execution policy가 검증 스크립트 로딩을 막으면 `powershell -NoProfile -ExecutionPolicy Bypass -File ...`의 **프로세스 한정 옵션**을 사용하고 전역 정책을 바꾸지 않는다.

기존 검증맵의 native rig는 첫 Tick에서 `SLAB-LIDAR-001`을 backend2/ML-X Native/FullSpec/IdealTruth로 설정한다. `VCAM-001`과 `SlabActor_0`를 사용한다. 첫 wait3초로 등록을 기다리고 두 센서를 stop, wait10초 뒤 기존 AUTO-01 dataset.start(scene1/seed22015/settle10/yaw0)를 요청한다. baseline은 camera_truth=false/lidar_fidelity=false이며 main depth/geometry와 identity를 구분한다. 완료 뒤 FileSave 종료를 위해 wait2초를 둔다. UserDir를 각 실행 폴더 아래로 지정해 실제 생성 위치도 기록한다.

`DT_DxLevel`의 World soft ref를 읽기 전용으로 조사해 `/Game/MA0T10/Maps/TestMap`도 cook 목록에 포함한다. 선택 맵만 cook하고 간접 맵을 누락시키지 않는다. 조사/검증을 위해 SensorTestMap이나 다른 원본 자산을 저장하지 않는다.

```powershell
$root = 'C:/Unreal Projects/ma0t10_dt'
$engine = 'C:/Program Files/Epic Games/UE_5.3/Engine'
$destination = "$root/Saved/Reports/ANALYSIS-24-22/package/<실행>"
& "$engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$root/ma0t10_dt.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -skipbuildeditor -cook '-map=/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap+/Game/MA0T10/Maps/TestMap' -stage -pak -archive "-stagingdirectory=$destination/staging" "-archivedirectory=$destination/archive" -unattended -utf8output
```

Shipping은 clientconfig만 Shipping으로 바꾸고 별도 destination을 사용한다. `-LidarSemanticDiagnostics -UserDir=<destination>/runtime/user`와 같은 map/job/1920×1080/D3D12 조건으로 실행한다. job result의 실제 dataset 경로와 새 diagnostic JSON을 대조한다. native 로그가 컴파일되지 않는 Shipping에서도 기존 async FileSave JSON이 증거가 된다. listener/실제 Broker가 필요 없는 AUTO-01 job이며 DTCore 연결 URL은 세션에서 인증 없는 loopback으로, login/passcode는 빈 값으로 지정한다. 공유 config/서비스/인증정보를 바꾸거나 사용하지 않는다.

HWRT3 추가 비교는 기존 UI/public Actor API로 backend를 명시적으로 선택할 수 있을 때 수행하고 실제 backend3 및 named Slab 양성을 대조한다. AUTO-01 sensor.set에 없는 backend 필드를 임의로 추가하지 않는다. fallback/화면 접근 실패는 backend3 수용으로 세지 않는다. #22는 현재 코드의 재현 절차만 유지하며 UI/Camera 제품 코드를 변경하지 않는다.

최종 정리 시 package 실행 프로세스가 모두 정상 종료했는지 확인한다. root가 `Saved/Reports/ANALYSIS-24-22/package/` 안인지 절대 경로로 검증하고, 소유 staging/archive/user binary를 정리한다. 삭제 전 build/runtime exit·binary hash·label/diagnostic counts·실패/skip을 요약 JSON에 남기며 결과 문서와 요약 JSON을 보존한다. 개인 설정의 원본 집합/해시와 보호 경로를 마지막에 복원·대조한다.
