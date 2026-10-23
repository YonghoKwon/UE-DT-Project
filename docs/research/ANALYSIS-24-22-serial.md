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

두 번째 수정은 전역 PSO 정책을 바꾸지 않는다. private proxy geometry/identity/pose를 acquisition 시점에 동결하고, PSO/scene proxy 준비가 끝날 때까지 기존 비동기 Poll에서 semantic pass만 보류한다. LiDAR는 이미 캡처한 main depth target을 유지한다. Camera GT는 원래 RGB/depth를 즉시 staging copy하고 semantic copy만 나중에 큐에 넣어 RGB target이 다른 frame으로 바뀌어도 영향을 받지 않는다. 새 main capture/readback/동기 wait나 다음 frame으로의 조용한 교체는 없다. cancel/generation을 유지한다. 아래 리뷰 수정에서 optional semantic PSO timeout은 깊이 프레임을 미분류로 완료하도록 바꿨다.

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

Shipping은 clientconfig를 Shipping으로 바꾸고 별도 destination을 사용한다. Shipping의 URL map 인자는 무시돼 최초 실행은 기본 SensorTestMap에서 ID를 찾지 못했고 이를 실패로 보존했다. 재현용 **해당 실행 UserDir만** `Saved/Config/Windows/Engine.ini`에 아래 설정을 만들면 같은 검증맵으로 실행된다. 프로젝트 원본 DefaultEngine.ini/SensorTestMap은 수정하지 않는다.

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap.SlabScenarioValidationMap
```

`-LidarSemanticDiagnostics -UserDir=<destination>/runtime/user`와 같은 job/1920×1080/D3D12 조건으로 실행한다. job result의 실제 map/dataset 경로와 새 diagnostic JSON을 대조한다. native 로그가 컴파일되지 않는 Shipping에서도 기존 async FileSave JSON이 증거가 된다. listener/실제 Broker가 필요 없는 AUTO-01 job이며 DTCore 연결 URL은 세션에서 인증 없는 loopback으로, login/passcode는 빈 값으로 지정한다. 공유 config/서비스/인증정보를 바꾸거나 사용하지 않는다.

HWRT3 추가 비교는 기존 UI/public Actor API로 backend를 명시적으로 선택할 수 있을 때 수행하고 실제 backend3 및 named Slab 양성을 대조한다. Development에서는 UE built-in `-ExecCmds="defer ke VirtualSensorCoordinator_0 SetLidarAcquisitionBackend 3"`가 기존 public UFUNCTION을 호출하는 것을 실제 결과 backend3으로 대조했다. KE는 UE5.3 Exec_Dev의 !UE_BUILD_SHIPPING 분기이므로 Shipping에는 이 방법을 통과로 적용하지 않는다. AUTO-01 sensor.set에 없는 backend 필드를 임의로 추가하지 않는다. fallback/화면 접근 실패는 backend3 수용으로 세지 않는다. #22 듀얼 표시 코드는 바꾸지 않았다. Camera GT 변경은 #24의 같은 semantic PSO/readback 문제에 한정된다.

## 최종 local 검증 범위

실행 Runtime SHA는 `5bf813a17cbbb436d58ef90cd9fa7ea867ee850f`이며 이후 문서는 runtime source를 변경하지 않는다. 같은 SHA의 Editor Development/Development package/Shipping package 빌드가 exit0, 관련 D3D12 13개가 Success/실패·skip0, Python 감사5개와 quick9/layer/guard가 통과했다.

| 구성·backend | GT | 장면 | 각 장면 named Slab | Camera class2 pixels |
|---|---|---:|---:|---:|
| Development / Depth2 | off | 3 | 1,800 | 미요청 |
| Development / Depth2 | on | 3 | 1,800 | 각19,257 |
| Shipping / Depth2 | off | 3 | 1,800 | 미요청 |
| Shipping / Depth2 | on | 3 | 1,800 | 각19,257 |
| Development / HWRT3 | off | 3 | 1,792 | 미요청 |

모든 첫 장면을 포함했다. backend2의 12개 PCD body SHA256은 위 기준과 같았다. Camera dataset `seg.png`는 preview palette가 아닌 **8-bit raw class ID**다(`VirtualDatasetSubsystem.cpp:444`); class2를 세었다. package cold failures·world-kind-only 실패·Shipping 맵 선택 실패도 요약에 보존했다. 최종 근거는 `package/FINAL_PACKAGE_SUMMARY.json`, 각 acceptance/semantic/diagnostic 요약, `serial/editor-regression-final-summary.json`, `serial/restoration-and-protection-final.json`이다. package binary/raw scene은 사용자 정리 규칙에 따라 삭제했고 hash·수치는 요약으로 남겼다.

표준 RT ProjectDefault ABAB, Shipping HWRT3, #22 직접 마우스 수용, 실제 장비/회사/소비자 ACK는 미수행이다. job tick wall 수치는 표준 perf gate를 대신하지 않는다.

최종 정리 시 package 실행 프로세스가 모두 정상 종료했는지 확인한다. root가 `Saved/Reports/ANALYSIS-24-22/package/` 안인지 절대 경로로 검증하고, 소유 staging/archive/user binary를 정리한다. 삭제 전 build/runtime exit·binary hash·label/diagnostic counts·실패/skip을 요약 JSON에 남기며 결과 문서와 요약 JSON을 보존한다. 개인 설정의 원본 집합/해시와 보호 경로를 마지막에 복원·대조한다.

## 독립 리뷰 R1 — semantic PSO timeout

기존 `PollAcquisition`은 timeout 상태 문자열의 failed를 Failed로 반환하고 `VirtualLidarScanComponentAcquisition.cpp`는 in-flight 해제 및 CPU fallback을 수행했다. 따라서 영구 in-flight 정지로 단정할 수는 없지만, optional 라벨 때문에 정상 깊이 프레임을 버리는 문제는 있었다.

PSO가 1초 내 준비되지 않으면 frozen semantic identities를 지우고 semantic staging copy를 생략하며, 이미 캡처한 원래 깊이 프레임은 Completed로 반환한다. 상태는 `미분류 (PSO 대기 초과, 깊이 측정 유지)`다. 같은 scene이 계속 준비되지 않는 다음 프레임은 추가 대기하지 않고 미분류로 완료한다. PSO가 준비된 acquisition은 그 프레임의 새 snapshot으로 라벨을 복구한다. stale ID/depth나 이전 transform을 재사용하지 않는다. 1초는 기존 깊이 texture 초기화 ceiling을 상한으로 재사용한 최초 cold-start 유예이며 장비별 PSO 완료를 보장하는 값이 아니다. 최초 지연은 최대 약1초이며 그 이후 지속 pending 상태에 프레임별1초 지연을 반복하지 않는다. 선택 라벨 target 미준비는 같은1초 ceiling 안에서 다음 poll의 RHI 생성 완료를 기다린 뒤, 초과한 경우에만 미분류로 완료한다. 즉시 미분류로 처리했던 첫 R1 시도는 기존 정확도 회귀 첫 프레임에서 실패해 보존했고, 이 초기화 유예를 복원한 재실행은 같은>=99% 기준으로 통과했다.

`MA0T10.LidarRegression.SemanticPsoTimeoutRecoveryRhi`는 component 단위 강제 pending seam(Shipping에는 컴파일되지 않음)으로 실제 D3D12 depth/readback을 검사한다. 최초 유예 종료 후60개 깊이 프레임, requested20Hz 대비>=99%, 빈 semantic pixels/identities, 불변 frame ID, ready 전환 뒤 실제 Slab 픽셀/identity 복구를 검사한다. 첫 실행은19.996390Hz였다. pure mock 성공이나 CPU fallback을 GPU 완료로 세지 않는다.

private scene은 snapshot 변경 때 한번 flush하고, pending poll에는 cached proxy list의 PSO readiness만 확인한다. PSO 완료 task가 render state dirty를 설정한 뒤 필요한 refresh만 한 번 더 flush한다(UE5.3 PrimitiveComponent.cpp:4497–4556). 매 poll world flush/TMap 순회는 제거했다. 표준 RT-on ABAB 및 default Rev7x3 90초 visible 결과와 startup Error 분류는 RESULT/result.json의 같은 source SHA 증거를 따른다.

startup Error22줄은 수정 전 Editor-game reference와 R1 Development GT-off/on에서 모두 frame0의 `SwapChain4.GetReference()` handled ensure1건(헤더1/조건1/스택헤더1/콜스택16/빈줄3)으로 설명됐다. UE5.3 `WindowsD3D12Viewport.cpp`의 `RenderOffScreen` swapchain 생략 분기(생성자), `EnsureColorSpace` line473을 대조했으며 엔진은 바꾸지 않았다. 기존 native package raw log는 정리돼 직접 stack 재비교가 불가능한 한계가 있다. 이번 native 재현에는 별도의 센서/semantic startup 오류가 없었고 22줄을 오류0으로 숨기지 않았다.

R1 성능 gate는 **미수행**이다. Brave 프로세스의 GPU VideoDecode 사용1%를 여러 번 확인해 base-rules의 다른 GPU 작업 중 측정 금지에 따라 시작하지 않았다. 영상 중지 확인 질문은 대기 중이며 브라우저를 조작하지 않았다. 조건 해제 후 `Scripts/verify.ps1 -Level perf -BaseCommit d2ca1d1 -PerfRayTracing ProjectDefault` 및 `Saved/Reports/ANALYSIS-24-22/review-r1/run-runtime-perf.ps1`로 default Rev7x3 90초 visible/GT-on 60초 visible을 수행해야 한다. 새 성능 값·GT 증가·2% 통과·slot gate 통과를 주장하지 않는다. 전체 release suite, Shipping HWRT3, #22 UI/회사 수용도 기존처럼 별도다.

R1 최종 코드 근거(`Source/ma0t10_dt/MA0T10/` 기준): `Sensor/VirtualLidarGpuDepthProjectionComponent.cpp:167,177,183,268`(유예·미분류·깊이완료), `Sensor/VirtualLidarSemanticScene.cpp:192,203,208`(준비만 poll/조건부1회 flush), `Sensor/VirtualLidarScanComponentAcquisition.cpp:194,199`(기존 Failed fallback), `Sensor/Tests/LidarSemanticPsoTimeoutTests.cpp:60,67,73`(유예/60frame/복구 검사).

## 2026-10-24 성능 재개 — fd5fac1 / 7e6e04d

[최신 RESULT](../../Saved/Reports/ANALYSIS-24-22/RESULT.md)와 `resume-20261024/perf-final.json`이 현재 판정이다. 앞 R1의 영상 작업 대기는 해소됐다. README/history conflict는 양쪽을 보존했고 원래 cfa의 준비 완료 후1회 flush를 유지했으며 추가 production/threshold 변경은 없다.

RT on 표준 ABAB12회(A/B각6중앙값)는 FPS저하0.004979%, p95증가0%, GameThread변화-0.034683%로2%PASS다. 같은1920x1080visible·10초warmup+60초·Camera1/LiDAR1 조건과 동일 instrument를 사용했다. 기본Rev7×3 90.0062795초는 각1800frames/19.998605Hz/ratio0.999930248/expiredslots0이며 기존 strict19.9Hz/.995와 canonical슬롯하한1799를 통과했다. 단일3센서 검사다.

CameraGT-on은1회이며 비교 가능한 동일메인GT-off control1회를 추가했다. GameThread off5.096733→on6.147050ms(+1.050317ms/+20.607661%). 이 비용 관측에 원래 GT기능 비용이 포함되며, 코드 변경의2% 판정은 동일조건 표준ABAB를 따른다. pipeline truthframe완료와 Camera30/LiDAR20Hz를 실제JSON으로 확인했다. rawCSV/automation result와 실행인자 모두 저장했다.

관련21RHI/logic tests와 Python5/quick9/layer/guard통과, 현재SHA의native패키징·전체release·직접마우스/현장/소비자ACK는 별도다. 운영맵/SensorTestMap은 저장하지 않았다. 새372개 설정 집합과753개 보호해시를 복원·대조했으며 이전353개 백업으로 덮어쓰지 않았다.

## GT-on / Rev7 전후 비교 조건 추가

기존 표준ABAB는 CameraGT-off/MLX80 조건이므로 CameraGT-on 변경 비용의 전후 검증과 구분한다. 제품코드 변경 없이 기존 Perf01/verify 경로에 조건 인자만 추가했다. `-PerfCameraGroundTruth -PerfLidarProfile Rev7 -PerfRequireGpu`는 같은 SlabScenarioValidationMap의 CameraD455+Rev7/HWRT·의미분류on·1920x1080visible·10초warmup+60초·ABABx3를 실행한다. GPU평균도 같은2% 기준에 포함하며 누락/0GPU값은 실패다. 기본MLX80/GT-off 및 기존Scheduled의GPU/RSS/Hz 판정은 유지한다. 두revision에 동일한 committed instrument와 session helper를 overlay하고 hash를 검증한다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/verify.ps1 -Level perf -BaseCommit fd5fac1 -PerfRayTracing ProjectDefault -PerfCameraGroundTruth -PerfLidarProfile Rev7 -PerfRequireGpu
```

도구회귀는 기존5합성cohort에 GPU-only조건 성공/3%악화/측정값누락3개를 더해8개이며 실측으로 세지 않는다. WindowsPowerShell5의 단일문자열 native splat 문제는 명시적string[]로 수정했고 초기실패로그를 보존했다. 실측 판정과 커밋은 최신 RESULT/result.json을 따른다.
