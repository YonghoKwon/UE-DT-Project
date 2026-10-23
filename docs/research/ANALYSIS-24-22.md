# ANALYSIS-24-22 — 패키지 Slab 분류와 듀얼 Camera 0 px 정적 분석

2026-10-23 / `chore/analysis-24-22` / 기준 `b4e83275f059c058a093877ebc3b0d421e39db74` / DTCore `b2504d1dfea5659f832ff0d750515edee146a28e`.

## 결론과 검증 경계

- **#24:** 보관된 Development 패키지의 점별 라벨은 Ground가 아니라 **Other(1), instance 0**이다. 100파일·3,225,600점을 이번에 오프라인 재감사했다. Slab Actor의 런타임 이름도 manifest에 남아 있으므로, Tag가 Shipping에서 사라진다는 가설보다 **GPU semantic identity가 점에 연결되지 않는 경로**를 먼저 검증해야 한다. 구체적인 렌더/재질/깊이 실패 지점은 미확정이다. Shipping에서 같은 현상이 난다는 사용자 제보와 이 Development 증거를 구분한다.
- **#22:** 증상과 일치하는 과거 로그는 RenderTarget 해상도가 아니라 **Slate 이미지 geometry의 0×0**을 기록한다. 원인은 준비 중인 `SImage`의 Collapsed 상태와 `SScaleBox` desired size였고, 해당 수정은 `fa3d6ea`에 들어 있으며 현재 기준 SHA에도 남아 있다. 현재 SHA에서 다시 발생하는지는 미검증이다. 신규 readback 경합으로 단정하거나 같은 수정을 중복 적용하지 않는다.
- **제품 코드 변경 0.** 아래 diff는 조건부 실험 초안이며 적용·컴파일·실행하지 않았다. Editor, UBT/Build.bat, 패키징, GPU, 성능 측정, Computer Use를 실행하지 않았다. 현장 수용이나 버그 해결 완료 보고가 아니다.

분석 원본은 이 worktree의 `Saved/Reports/ANALYSIS-24-22/`에 있다. 메인에는 허용된 `Saved/Reports/ANALYSIS-24-22/RESULT.md`만 복사한다. 과거 검증 원본과 관리 문서의 이력 문장을 수정하지 않는다.

## #24 — 확인한 사실

1. [검증 이력](../VALIDATION_HISTORY.md) 31–39행은 TESTSET-04의 **Game Development** 패키지 성공과 Slab 정답 품질 미수용을 기록한다. Shipping 검증은 별도다. 메인 `Saved/Reports/TESTSET-04/RESULT.md:73–80` 및 `package_nogit.ps1:24`도 Development를 명시한다.
2. 메인 `Saved/Reports/TESTSET-04/package-nogit/runtime-subscribed/semantic-audit.json:2–4`는 ID 1이 3,225,600개라고 기록한다. 당시 RESULT의 “Ground(1)” 해석은 데이터의 class table과 맞지 않는다. 첫 `dataset.json`은 `0 Background, 1 Other, 2 Slab, 3 Roller, 4 Conveyor, 5 Floor, 6 Sensor`이며, 보관된 `1a75969` 소스의 class mapping도 같다.
3. 이번 `historical-label-audit.json`은 원본 100개의 `lidar_labels.bin`을 **little-endian `<BI` / 5B**로 다시 읽었다. 파일별 잔여 바이트 0, 전체 semantic **1:3,225,600**, instance **0:3,225,600**이다. 첫 manifest의 instances에는 `id=19, actor_name=SlabActor_0, actor_class=SlabActor`가 있다. 전체 런타임 Actor가 제거된 상황과는 다르다. 원본 경로·파일별 SHA256을 함께 기록했다.
4. [분류 규칙](../../Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorSemanticLabels.cpp) 16, 27–42행은 Actor Tag → `GetName()` → class → component 이름을 검사한다. SlabActor_0/SlabMesh는 이름 규칙에도 부합한다. `GetActorLabel`, 문자열 테이블 또는 Editor 전용 metadata를 사용하지 않는다. 44–49행에서 알 수 없는 이름/NAME_None은 ID 1로 변환된다.
5. [dataset writer](../../Source/ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.cpp) 429–435행은 immutable 점의 `SemanticLabel`과 `HitActorName`을 5B에 기록한다. Actor 이름 lookup 실패는 instance 0이다. 따라서 **Other+instance0은 semantic metadata 미적용과 일치**하지만 이것만으로 GPU 버퍼가 비었는지, 깊이가 불일치했는지까지 구분할 수 없다.

### 원인 후보 순위

| 순위 | 후보 / 근거 파일:행 | 판단·반증 방법 |
|---|---|---|
| 1 | **GPU semantic proxy capture/readback/깊이 대응 실패.** `Sensor/VirtualLidarGpuDepthProjectionComponent.cpp:142–152,192–212`; `Sensor/VirtualLidarScanComponentSampling.cpp:374–397` | 현재 가장 유력한 실패 **구간**. depth 측정은 유지되면서 semantic ID 배열 없음·ID lookup 실패·깊이 차이 >1cm면 identity가 붙지 않는다. 관찰한 모든 instance0과 맞는다. 아래 세부 후보를 실행으로 분리해야 한다. |
| 1a | 런타임에서도 기본 **EditorPreview world**를 사용하는 proxy scene. `Sensor/VirtualLidarSemanticScene.cpp:12–29` | UE5.3 `Engine/Source/Runtime/Engine/Public/PreviewScene.h:23–34,63`의 `bEditor=true` 기본값과 `Private/PreviewScene.cpp:31–40`의 world type 선택을 로컬 엔진 원문에서 확인했다. 패키지 전용 차이를 조사할 구체적 후보지만, 이 값이 실제 누락을 일으켰다는 증거는 없다. 단독 A/B 후보다. |
| 1b | proxy 지원 조건/재질 shader 상태. `Sensor/VirtualLidarSemanticScene.cpp:41–53,77–93,139–154` | 숨김·미등록·SceneCapture 제외 또는 opaque가 아닌 재질/WPO는 proxy에서 제외된다. main depth는 계속 보일 수 있다. Slab source가 제외됐는지, ID 재질/shader가 준비됐는지 기록이 부족하다. 원본 재질을 강제로 바꾸지 않는다. |
| 1c | proxy ID/depth와 main depth의 대응 실패. `Sensor/VirtualLidarScanComponentSampling.cpp:376–395` | 전체 1cm 불일치 또는 유효 ID가 없는 경우도 같은 결과다. projection·pose·크기·프레임별 ID/depth 대응과 mismatch 수를 먼저 확인한다. 1cm 기준 완화는 수정안이 아니다. |
| 2 | runtime 문자열 경로 자산의 cook 누락. `Sensor/VirtualLidarSemanticScene.cpp:41–42`; `Slab/SlabActor.cpp:44`; `Slab/SlabVisualizationComponent.cpp:41–48` | ID material, Slab 재질/폰트가 경로 로드된다. 그러나 현재 `Config/DefaultGame.ini:7,10`에 Slab/Materials 및 Sensor/Materials AlwaysCook이 이미 있다. 같은 패키지의 실제 pak/asset registry 포함 여부·shader permutation은 이번에 검사하지 않았다. “soft ref라 무조건 누락”으로 결론 내릴 수 없다. |
| 3 | Actor/Component Tag 혼동 또는 editor-only component/Actor. `Sensor/VirtualSensorSemanticLabels.cpp:30,35`; `Slab/SlabActor.cpp:35–43` | ActorTags만 검사하며 ComponentTags는 검사하지 않는다. UE5.3 `Classes/GameFramework/Actor.h:1155–1157`의 Tags는 일반 UPROPERTY이고, ActorLabel은 `1050–1061`의 editor data와 구분된다. `Classes/Components/ActorComponent.h:257–259`의 bIsEditorOnly 인스턴스 설정은 자산 검사 필요. 현재 native SlabMesh는 일반 CreateDefaultSubobject이고 이름 규칙도 있으므로 이 증거에서는 낮은 순위다. |
| 4 | WITH_EDITOR/WITH_EDITORONLY_DATA 가드·문자열 테이블 제거 | 위 native 분류/writer/SlabMesh 생성 경로에 해당 가드나 StringTable 참조가 없다. `Slab/SlabVisualizationComponent.cpp:14–19`의 측정 제외는 진단 선/글자용이고 본체 SlabMesh와 별개다. 화면 글자가 안 보인다는 별도 증상에는 이 컴포넌트의 폰트/cook·viewport 조건(`142–158`)을 확인하되 semantic 파일 누락과 섞지 않는다. |

위 표의 프로젝트 경로는 `Source/ma0t10_dt/MA0T10/` 기준이다. UE 소스 경로는 `C:/Program Files/Epic Games/UE_5.3/Engine/Source/Runtime/Engine/` 기준이다.

### 최소 수정안과 예비안

먼저 동일 장면의 Editor / Development / Shipping에서 **분류 전 SlabMesh의 resolve 결과, identity 개수/Slab 개수, semantic capture status, 유효 ID 수, depth mismatch 수**를 수집한다. 기존 캡처/버퍼에서 카운터만 얻고 추가 capture/readback·동기 CPU 스캔·매 Tick 문자열은 만들지 않는다. 자격증명·전문 body를 기록하지 않는다.

| 조건 | 수정/실험 | 검증 |
|---|---|---|
| resolve는 Slab인데 패키지 proxy가 EditorPreview이고 유효 ID0 | 다음 한 줄을 단독 A/B. **원인 확정 전 merge하지 않는 실험 초안** | 같은 소스/맵/프로필/pose의 두 패키지에서 Slab ID2·instance 양성 및 depth 좌표 보존을 비교 |
| ID material load/shader 실패가 기록됨 | 실제 cook 누락 경로 또는 필요한 shader usage만 보완. 기존 AlwaysCook 중복 추가/Config/Game.ini 수정은 하지 않음 | pak/asset registry와 runtime load 모두 확인 |
| 지원 제외/깊이 mismatch가 기록됨 | 제외 조건·동일 snapshot/projection 전달의 재현된 결함만 수정 | 미지원 occluder가 뒤 물체 identity를 받지 않는 음성 대조 유지 |
| 원인을 분리하지 못함 | 문서만 유지 — **이번 선택** | 실행 검증 필요 지점을 그대로 남김 |

```diff
--- a/Source/ma0t10_dt/MA0T10/Sensor/VirtualLidarSemanticScene.cpp
+++ b/Source/ma0t10_dt/MA0T10/Sensor/VirtualLidarSemanticScene.cpp
@@
-    : Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false).SetCreatePhysicsScene(false).SetTransactional(false).SetForceMipsResident(false))
+    : Scene(FPreviewScene::ConstructionValues().SetEditor(GIsEditor).SetCreateDefaultLighting(false).SetCreatePhysicsScene(false).SetTransactional(false).SetForceMipsResident(false))
```

이 초안은 Editor 실행을 기존 EditorPreview로 유지하고 패키지에서는 GamePreview를 선택한다. Tag 자동 추가, class ID 변경, 허용 깊이 확대, 임의 CPU fallback은 하지 않는다. 효과·부작용은 **빌드/실행 미검증**이다.

## #22 — 확인한 사실과 후보 순위

과거 원본은 메인 `Saved/Reports/INSP-FIX-4/` 아래다. 이번 발췌는 `historical-dual-log-hits.json`에 전체 경로·원본 행을 기록했다.

| 순위 | 후보와 근거 | 판단 |
|---|---|---|
| 1 | `focused-layout/MA0T10.Inspection.NumericClampFeedback_MA0T10.Inspection.LidarHelpVocabulary_MA0T10.Inspection.UiFeedbackLifecycle_MA0T10.UXR18_MA0T10.UXR06/automation.log:1290–1291`: primary `0,0`, secondary `153.123,114.842`, empty=0/waiting=0. `RESULT.md:66`에 Collapsed SImage/ScaleToFit 원인 설명 | **보관된 0 px 증상의 가장 유력하고 직접적인 원인**은 Slate layout. 이미지 파일/RT의 0×0 증거는 아니다. |
| 1 검증 이력 | `focused-scale-visibility/MA0T10.Inspection.UiFeedbackLifecycle/automation.log:1273`: primary `87.499,49.218`, secondary `153.123,114.842`. `focused-precommit/.../automation.log:1294`에서도 동일 | 현재 `UI/VirtualSensorMonitorPanelWidgetConstruction.cpp:524–536`은 readiness visibility를 ScaleBox 컨테이너에 둔다. git blame은 `fa3d6ea`, 검수 Runtime `6b42f84`도 HEAD의 조상이다. **기존 수정은 존재**, 현재 SHA의 재현/수용은 미수행. |
| 2 | RT pointer/frame ID만으로 brush 크기를 갱신. `UI/VirtualSensorMonitorPanelWidgetPreview.cpp:518–547,559–569` | 동일 RT를 resize하고 frame이 진행하지 않으면 brush 크기가 오래 남을 수 있다. 실제 RT SizeX/Y, brush ImageSize, SImage geometry를 각각 기록해야 한다. stopped/resize/rehost 조합의 새 재현은 없다. |
| 3 | primary Resource가 null이면 함수 전체 return. 같은 파일 `516–527,557–570` | 주 GT preview 준비 중 null이 되면 정상 secondary RGB brush 갱신까지 막힌다. 조건부 코드 경로는 확인했지만 제보 원인이라는 증거는 없다. 주/보조를 독립 갱신하는 아래 초안의 적용 조건이다. |
| 4 | RT 생성 순서·0 해상도·자산 공유. `Camera/VirtualCameraCaptureComponent.cpp:113–139,736–751`; `.h:193–194,223–224`; `Camera/VirtualCameraSensorActor.cpp:132–151` | 기본 해상도는 640×360이고 BeginPlay/Start/측정에서 Ensure한다. 새 RT의 outer는 각 Camera component라 같은 이름이 공유를 뜻하지 않는다. 다만 RT는 EditAnywhere 포인터라 자산에서 명시 공유했을 가능성은 남는다. 두 pointer/outer/TextureTarget을 비교하고 BP 우회로 0 입력됐는지 확인한다. |
| 5 | 비동기 readback/JPEG 경합·스케줄러 | `Camera/VirtualCameraCaptureComponent.cpp:318–359,401–429`는 각 component의 슬롯/크기/weak owner/generation을 보존한다. `Core/VirtualSensorSchedulerSubsystem.cpp:228–258`은 round-robin admission이다. 전역 encode 한도는 있지만 듀얼 SImage는 RT를 직접 사용(`Preview.cpp:516,559`)하므로 JPEG 실패만으로 Slate geometry가 0이 되는 설명은 약하다. 빈 저장 파일 증거가 확보되면 별도로 추적한다. |

UE5.3 `Classes/Engine/TextureRenderTarget2D.h:200–201`의 GetSurfaceWidth/Height는 SizeX/Y다. `Private/TextureRenderTarget2D.cpp:98–105`의 InitCustomFormat은 양수 check를 사용한다. 그러므로 “UI 0 px”, “실제 RT 0×0”, “유효 해상도지만 검은 픽셀”, “readback/인코딩 파일 없음”을 같은 실패로 집계하지 않는다. Shipping에서는 check에만 의존한 유효성 검증도 충분하지 않다.

### 조건부 수정 초안

기존 layout 수정을 유지한다. **현재 코드에서 primary null + secondary 유효가 로그로 재현될 때만** 아래처럼 primary 작업을 조건부로 감싸 secondary 갱신까지 도달하게 한다. 이것은 과거 layout 수정의 재적용이 아니며, 빌드/실행 미검증이다.

```diff
--- a/Source/ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidgetPreview.cpp
+++ b/Source/ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidgetPreview.cpp
@@
     if (!Resource)
     {
         if(CameraTruthDisplayMode&&NativeViewImage){NativeViewBrush.SetResourceObject(nullptr);NativeViewImage->Invalidate(EInvalidateWidgetReason::Paint);}
-        return;
     }
@@
-    if (ViewImage)
+    if (Resource && ViewImage)
@@
-    if (NativeViewImage)
+    if (Resource && NativeViewImage)
```

동일 RT resize/정지 중 brush 불일치가 원인이면 위 초안 대신 **두 brush 각각의 실제 texture 크기 변경도 갱신 조건에 포함**하고 양수일 때만 ImageSize를 채운다. 0 크기를 고정값으로 덮어 근본 RT 실패를 숨기거나, 모니터를 위해 보조 카메라를 자동 시작/추가 캡처하지 않는다. 확신이 낮아 이번에는 두 초안 모두 적용하지 않았다.

## Claude 직렬 재현·검증 절차 — 이번 세션 실행 금지

다른 Editor/측정/마우스 작업이 없는 직렬 슬롯에서 수행한다. 아래는 메인 작업자용 명령이며 이 worktree에서는 실행하지 않는다. 개인 SaveGame·Editor 설정/창 배치를 먼저 백업하고 이후 원본 집합/해시를 복원한다. 운영맵·WBP는 저장하지 않는다. 각 실행에서 SHA/DTCore/맵 hash/프로필·backend·해상도·ID/RT·출력 설정과 실제 viewport/DPI/font를 기록한다.

### 1. Editor 빌드와 현재 등록된 집중 테스트

```powershell
Set-Location 'C:\Unreal Projects\ma0t10_dt'
$engine = 'C:\Program Files\Epic Games\UE_5.3'
$project = Join-Path $PWD 'ma0t10_dt.uproject'
$serial = Join-Path $PWD ('Saved/Reports/ANALYSIS-24-22-SERIAL/' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Force -Path $serial | Out-Null
& "$engine/Engine/Build/BatchFiles/Build.bat" ma0t10_dtEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE) { throw 'Editor build failed' }
$priorDatasetScenes = $env:MA0T10_DATASET_SCENES
$priorCameraMarker = $env:MA0T10_CAMERA_MARKER_RHI
try {
    $env:MA0T10_DATASET_SCENES = '10'
    $env:MA0T10_CAMERA_MARKER_RHI = '1'
    $groups = @('MA0T10.LidarRegression.GpuSemanticRhi', 'MA0T10.CameraGroundTruth.DepthSemanticInstanceRhi',
        'MA0T10.Dataset.RealRhiGeneration', 'MA0T10.SensorV2.UI.DualCameraSelection',
        'MA0T10.Inspection.UiFeedbackLifecycle', 'MA0T10.SensorFiles.CameraMarkerRhi',
        'MA0T10.SensorFiles.CameraCaptureDispatchIsolation', 'MA0T10.GpuCaptureLifetime')
    & ./Scripts/run_smoke_tests.ps1 -ProjectPath $project -EngineRoot $engine -SkipBuild -Rhi D3D12 -TestGroups $groups -ReportRoot "$serial/editor"
    if ($LASTEXITCODE) { throw 'Editor test process failed' }
} finally {
    $env:MA0T10_DATASET_SCENES = $priorDatasetScenes
    $env:MA0T10_CAMERA_MARKER_RHI = $priorCameraMarker
}
```

`run_smoke_tests.ps1`의 process exit0만으로 PASS 판정하지 않는다. 각 `index.json`의 Failed/NotRun/skip 사유와 positive assertion을 확인한다. Dataset.RealRhiGeneration은 소스 74행에서 최소 10장면으로 clamp하므로 opt-in도 10으로 명시했다. 위 tests는 **Editor 전용**이며 Shipping의 Automation 이름으로 실행한다고 패키지 검증이 되지 않는다. opt-in 미설정 Success 안의 skip도 통과에서 제외한다.

### 2. #24 동일 맵 Development/Shipping cook과 실행

```powershell
foreach ($configuration in @('Development','Shipping')) {
    $destination = Join-Path $serial $configuration
    & "$engine/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$project" -noP4 -platform=Win64 "-clientconfig=$configuration" -build -skipbuildeditor -cook '-map=/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap' -stage -pak -archive "-stagingdirectory=$destination/staging" "-archivedirectory=$destination/archive" -unattended -utf8output
    if ($LASTEXITCODE) { throw "$configuration BuildCookRun failed" }
}
```

각 구성의 packaged `archive/Windows/ma0t10_dt.exe`에서 같은 map과 아래 job을 실행한다. 기존 AUTO-01 job 경로만 사용하며 Broker 연결이나 공유 설정을 바꾸지 않는다. job은 각 구성의 `$destination/job.json`, result_path는 구성별로 다른 폴더의 `job-result.json`을 사용한다. dataset name에도 구성/실행 시각을 붙여 기존 출력과 충돌하지 않게 한다.

```json
{
  "schema": "ma0t10.job.v1", "result_path": "job-result.json", "timeout_sec": 300,
  "steps": [
    {"op": "sensor.stop", "sensor_id": "VCAM-001"},
    {"op": "sensor.stop", "sensor_id": "SLAB-LIDAR-001"},
    {"op": "dataset.start", "name": "ANALYSIS24", "scenes": 1, "seed": 22015,
     "settle_frames": 10, "slab": "SlabActor_0", "lidar_id": "SLAB-LIDAR-001",
     "camera_id": "VCAM-001", "camera_truth": true, "lidar_fidelity": false,
     "export_format": "none", "ranges": {"yaw": [0, 0]}}
  ]
}
```

```powershell
# 위 job.json을 각 구성 destination에 저장한 뒤, 구성별로 순차 실행한다.
foreach ($configuration in @('Development','Shipping')) {
    $destination = Join-Path $serial $configuration
    python Tools/AutomationClient/run_job.py --executable "$destination/archive/Windows/ma0t10_dt.exe" --map /Game/MA0T10/Maps/Tests/SlabScenarioValidationMap --job "$destination/job.json" --timeout 360 -- -d3d12 -RenderOffscreen -unattended -nosplash -NoLoadingScreen
    if ($LASTEXITCODE) { throw "$configuration runtime job failed" }
}
```

패키지 루트/LOCALAPPDATA를 추정하지 말고 새 job result의 실제 dataset 경로를 사용한다. 같은 이름의 이전 결과를 재사용하지 않는다. 현재 맵의 ID가 다르면 먼저 `/v1/sensors`/맵 native 등록값을 확인해 job의 ID만 명시적으로 맞춘다. Shipping job 지원/완료 자체도 이번에는 미검증이며, result 미생성은 PASS가 아니다. 이 1장면 시험은 과거 100개 testset 재현과 별개다. 양성 Slab이 없는 pose라면 파일 생성 성공만 보고하고, 같은 맵의 알려진 양성 fixture pose로 별도 재현한다.

판정: PCD POINTS×33B, 라벨 POINTS×5B, class table/instance lookup, Slab 양성 점과 camera semantic 양성 mask를 대조한다. **Slab ID2 >0만으로 끝내지 말고** 알려진 Slab footprint의 양성·뒤쪽/미지원 물체 음성·기존 depth/geometry 불변을 검사한다. Editor와 Game 모두 실패하면 패키지 전용 원인 가설을 낮춘다. runtime resolve는 Slab인데 instance0이면 이름/Tag 수정부터 하지 않는다.

### 3. #22 직접 UI와 자원 분리 확인

- 위 `MA0T10.Inspection.UiFeedbackLifecycle`에서 현재 SHA의 최소 카드·150%·비율/재호스팅을 먼저 확인한다. 테스트가 만드는 임시 카메라와 맵의 카메라를 구분한다.
- PIE에서 고유 ID 두 카메라를 **명시적으로** 시작하고 RGB 듀얼 → 70:30/50:50/30:70 → 최소 크기 → 숨김/재열기 → Main/Viewport 재호스팅 → 주 카메라만 중지/재개 → 선택 변경을 직접 눌러 확인한다. 1920×1080/1280×720, font100/150%, 실제 DPI를 기록한다. 누른 것/기대/실제/스크린샷을 직렬 보고서 `manual/`에 남긴다.
- 실패 시 각 카메라의 SensorId, RT pointer/outer, TextureTarget pointer, CaptureResolution, SizeX/Y, available FrameId/runtime FrameId, brush ImageSize, SImage desired/cached geometry, visibility를 같은 시점에 기록한다. 정지/해상도 변경·primary GT 준비 중 null 조건도 별도로 검사한다.
- RT/유효 JPEG는 양수인데 geometry만 0이면 Slate/brush 경로다. RT 자체가 0이면 생성/설정 경로, 파일만 비면 readback/encode 경로다. 미리보기 확인용 추가 capture/readback은 만들지 않는다. 기존 CameraMarker/DispatchIsolation은 저장/dispatch 회귀이며 두 RGB texture의 비공유·연속 영상 수용을 전부 대체하지 않는다.

실제 Runtime 수정이 채택되면 layer/guard와 관련 회귀 후 기존 규칙의 `Scripts/verify.ps1 -Level perf -BaseCommit <수정 전 SHA> -PerfRayTracing ProjectDefault` ABAB를 직렬로 수행한다. 이번 문서 작업의 quick에 포함되는 **합성 cohort fixture**는 성능 측정이 아니다.

## 인계

실행 확신이 낮은 제품 수정은 적용하지 않는 예비안을 선택했다. 보호 경로/계약/정책 변경 필요 없음. Claude의 다음 작업은 #24의 동일 조건 package 양성 시험과 GPU identity 실패 지점 계수 확인, #22의 현재 SHA layout 재현 여부 확인이다. 자동 merge 승인은 하지 않는다. 세션별 commit·quick/layer/guard·schema/merge_check 실제 결과는 worktree `Saved/Reports/ANALYSIS-24-22/result.json` 및 `RESULT.md`를 따른다.
