# ANALYSIS-24-22 / #24 구현 후속 — Shipping semantic 진단

2026-10-23 · 작업 `C:/Unreal Projects/wt_ana`, `chore/analysis-24-22` · 시작 HEAD `ea27753bebd5af0ed6866be405c4dc0962a97cb6`(master `98e2131` 병합됨) · DTCore `b2504d1`.

## 결론·이번 선택

**확정하지 못한 Tag/ID 정책 수정 대신, 요청의 예비안인 Shipping용 1회 진단을 구현했다.** 빌드·패키징·Editor·GPU·성능 실행은 하지 않았다. 새 C++ 순수 로직 테스트 3개는 작성만 했으며 컴파일/실행 미검증이다. 이 결과를 Slab 누락 해결 완료로 표시하지 않는다.

이전 [정적 분석](ANALYSIS-24-22.md)의 100파일/3,225,600점 `Other(1)/instance0` 증거는 **Development / `lidar_backend=2` GpuDepthProjection**이다. 현재 sensor-owned BLAS/TLAS의 **HardwareRayTracing은 enum 3**이다(`Sensor/VirtualSensorDeviceProfileTypes.h:49–55`). 이전 패키지 결과로 HWRT 결함을 확정할 수 없다. 두 backend의 진단을 구분해 추가한 이유다.

명시적 `-LidarSemanticDiagnostics` flag가 있을 때만 센서 component 수명 동안 **HWRT 첫 시도/결과 1회**, **DepthProjection 첫 semantic 요청 완료 1회**를 기록한다. 기본 실행은 추가 파일·캡처·readback·버퍼 복사·스캔을 만들지 않는다. HWRT의 BLAS/TLAS·shader·worker 정규화·Tag 매핑·default/fallback·PCD 33B·라벨 5B·Blueprint/SaveGame 계약을 바꾸지 않았다. #22 UI/Camera 코드는 변경하지 않았다.

## HWRT hit → identity 경로와 후보 지점

아래 프로젝트 경로는 `Source/ma0t10_dt/MA0T10/` 기준이다. Bootstrap/Shader는 저장소 루트 기준이다. 실제 수정된 전체 경로·행 발췌는 worktree `Saved/Reports/ANALYSIS-24-22/implementation/source-evidence.json`에 남긴다.

| 단계 | 근거 파일:행 | Shipping 정적 판정 |
|---|---|---|
| component → acquisition-local surface | `Sensor/VirtualLidarStaticCollisionQuery.cpp:67–72,99–101` | runtime `UPrimitiveComponent` weak 참조를 Surfaces에 넣고 surface 정수 bits를 삼각형 normal.W에 저장한다. render LOD/ActorLabel/metadata/CustomPrimitiveData를 읽지 않는다. cooked convex data가 없으면 `86–93`에서 전체 CPU fallback이며 조용한 semantic 대체가 아니다. |
| surface → 동결 Actor/Tag/label | `Sensor/VirtualLidarScanSceneQuery.cpp:130–146`; `Sensor/VirtualLidarScanComponent.cpp:288–310` | GPU submit 전에 game thread에서 Surface와 같은 index의 FrozenSurfaces를 만든다. `GetOwner()`를 넣은 FHitResult → `Hit.GetActor()` → Actor 이름·class·Tags·semantic 규칙을 복사한다. 해당 코드에 WITH_EDITOR/WITH_EDITORONLY_DATA 가드가 없다. weak component 없음은 snapshot bValid=false로 남는다. |
| BLAS primitive/attribute | `Source/ma0t10_dtBootstrap/Private/SensorOwnedRayTracing.cpp:83–100,251–255` | geometry revision cache의 vertex 삼각형 순서와 attribute 삼각형 순서를 함께 사용한다. submit은 vertex 수÷3과 attribute 수를 대조한다. 이름/Tag를 GPU로 직렬화하지 않는다. |
| TLAS mesh slot | 같은 파일 `170–200,211–217` | `DefaultUserData=I`는 원래 Request.Meshes index이다. 빈 mesh를 descriptor에서 생략해도 request slot을 재번호화하지 않는다. GPUScene 입력은 nullptr, Transform은 자체 CPU instance upload다. |
| shader hit ID | `Shaders/Private/SensorOwnedRayTracing.usf:30–38,43–46` | `InstanceID()` → Mesh, `PrimitiveIndex()` → attribute row, `asuint(Data.w)` → Surface다. `InstanceIndex`를 component ID로 사용하지 않는다. Shader에 Shipping/Editor 분기가 없다. |
| frozen identity 적용 | `Sensor/VirtualLidarScanSceneQuery.cpp:87–101,187–200` | mesh/surface 범위, snapshot validity·ignore를 통과한 identity만 적용한다. worker 경로는 동결 Label/ActorName/Tags를 복사한다. 범위 밖 또는 snapshot 부재는 메타데이터 없이 continue하는 구체적 진단 지점이다. |
| dataset instance 번호 | `Scenario/VirtualDatasetSubsystem.cpp:429–435` | `ClassId(Point.SemanticLabel)` + `Names.FindRef(Point.HitActorName)`이다. frozen actor 이름이 없거나 manifest 이름 lookup이 안 되면 instance0이다. ID1은 Other이며 Ground라는 해석을 사용하지 않는다. |

로컬 UE5.3 원문도 대조했다. `Engine/Source/Runtime/Renderer/Private/RayTracing/RayTracingInstanceBufferUtil.cpp:182–191,234–241`은 user-data array가 없으면 DefaultUserData를 native descriptor InstanceId에 넣는다. `Engine/Shaders/Private/RayTracing/RayTracingCommon.ush:298–301`도 FDefaultPayload.InstanceID의 출처를 geometry instance UserData로 명시한다. 이것을 GPUScene primitive ID로 대체할 근거는 없다.

**정적 분석에서 확정한 Shipping 전용 단절은 없다.** 특히 HWRT의 invalid mesh/surface는 정상 metadata 없는 유효 점을 만들어내기보다 hit 적용을 건너뛰는 경로다. `Other/instance0`과 바로 등치하면 안 된다. 현재 code에서 좁혀야 할 곳은 동결 metadata 생성(`snapshot_named/slab_surfaces`)과 raw hit ID 대응(`invalid_*`, `mapped_*`) 사이이며, 두 값이 정상인데 export만 instance0이면 dataset 이름 lookup을 조사한다.

## 진단 구현과 판독

새 `Sensor/VirtualLidarSemanticDiagnostics.h/.cpp`는 UObject가 없는 snapshot 판정과 저장 adapter를 분리한다. HWRT `ReportHardwareSemanticDiagnostics`는 이미 돌아온 raw hit와 이미 동결한 surface metadata만 요약한다. Depth 진단은 기존 ID/depth buffer와 변환된 점을 비교한다. 결과를 센서나 Transport에 다시 적용하지 않는다.

| 요약 값 | 의미 / 다음 확인 |
|---|---|
| `stage=snapshot_unavailable/not_admitted/readback_failed` | 실제 HWRT frame 성공이 아니다. CPU fallback을 HWRT 통과로 세지 않는다. `hwrt_available`, `expected_rays`, `returned_rays`, `pipeline_failure`와 snapshot 개수를 확인한다. not_admitted는 queue/budget 등 여러 거절 원인을 단독으로 구분하지 못한다. |
| `snapshot_valid_surfaces`, `snapshot_named_surfaces`, `snapshot_slab_surfaces`, `snapshot_slab_tagged_surfaces` | submit 때 동결된 테이블에 Actor/Slab 정보가 있었는지 구분한다. Tag가 없어도 기존 이름/class 규칙으로 Slab일 수 있다. 원본 Actor 이름·Tag 목록·전문 body는 기록하지 않는다. |
| `invalid_mesh/triangle/surface`, `missing_snapshot`, `expired_component`, `ignored` | raw mesh/primitive/surface ID와 metadata table, weak 수명, 의도된 제외를 각각 분리한다. sparse mesh slot을 descriptor 순서로 바꾸지 않는다. |
| HWRT `mapped`, `mapped_named_hits`, `mapped_slab_hits`, `mapped_unclassified_hits` | raw readback과 snapshot identity 대응이다. physical model 이후의 최종 유효 점/파일 수가 아니므로 최종 라벨 분포와 따로 대조한다. |
| Depth `semantic_requested`, `semantic_status`, `identity_count`, `semantic_pixels` | 미요청·재질/shader/proxy 준비 실패·버퍼 부재를 구분한다. 첫 미요청 frame은 one-shot을 소모하지 않는다. |
| Depth `no_depth/no_semantic_pixel/nonfinite_semantic/depth_mismatch/missing_identity/mapped` | 기존 1cm 대응 기준을 그대로 관찰한다. 진단의 `mapped`는 beam/pixel 대응 수이며 detection/거리 조건을 적용한 최종 점 수와 다르다. |
| Depth `output_hits/output_named_hits/output_slab_hits` | 이미 변환된 출력 점의 identity 상태다. buffer 대응은 정상인데 출력 metadata가 사라졌는지 구분한다. |

`Source/ma0t10_dt.Target.cs:16–23`은 Shipping logging 활성화가 주석이다. 로컬 UBT `Configuration/TargetRules.cs:1268`의 기본값도 false이므로 UE_LOG에만 의존하지 않았다. 기존 `UVirtualSensorFileSaveSubsystem::RequestAnalysisJson`이 bounded async writer로 저장한다. Shipping의 글로벌 logging/Target/Build.cs를 바꾸지 않았다.

저장 위치는 **실제 실행의 Saved** 아래 `SensorCaptures/LidarSemantic_<backend>_<sensor>/Analysis/*.json`이다. filename/request ID는 기존 FileSave가 생성하며 SensorId는 기존 SafeId 처리를 따른다. schema는 `ma0t10.lidar-semantic-diagnostic.v1`, backend는 `hardware_ray_tracing` 또는 `gpu_depth_projection`이다. Development에서는 `[LidarSemantic]` 한 줄과 request/state도 볼 수 있다. `Saving`은 파일 확정 성공이 아니므로 JSON 존재·완료 상태를 확인해야 한다.

한계: explicit flag 실행에서만 최초 snapshot/hit 수에 비례한 CPU 요약과 작은 JSON 직렬화 비용이 한 번 생긴다. 이 실행을 성능 gate로 사용하지 않는다. FileSave가 busy/종료/디스크 오류로 거절되거나 owner가 사라지면 파일이 없을 수 있다. 자동 재시도나 별도 writer는 만들지 않았다. flag를 준 새 실행에서 재현하고 기존 FileSave 실패 상태를 확인한다. 첫 frame이 miss-only면 그 요약을 양성 Slab 수용으로 해석하지 않는다. 정상 실행의 성능·UI 수용은 이번에 측정하지 않았다.

## 작성한 순수 로직 테스트 — 컴파일·실행 미수행

`Sensor/Tests/LidarSemanticDiagnosticsTests.cpp`는 `WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR` 아래 다음 3개를 등록한다. renderer/world/UObject를 만들지 않고 synthetic hit와 immutable metadata만 사용한다.

- `MA0T10.LidarSemanticDiagnostics.HardwareSnapshotMapping`: sparse mesh slot2/서로 다른 Floor·Slab surface, actor 이름/Tag, miss, 진단 후 원본 table 변경 분리.
- `MA0T10.LidarSemanticDiagnostics.HardwareFailureStages`: NaN/Inf, unsigned sentinel, primitive/surface 경계, snapshot 부재·weak 만료·ignore 및 0거리 hit 분리.
- `MA0T10.LidarSemanticDiagnostics.DepthIdentityStages`: 1cm 경계 유지, 없는 buffer/identity/invalid pixel, nonfinite 값, output metadata와 pixel 대응 분리.

이는 이번에 실행한 통과 수가 아니다. quick/layer는 C++ 컴파일이나 이 테스트를 실행하지 않는다.

## Claude 직렬 단계 명령 — 이 worktree에서 실행하지 않음

모든 Editor/PIE/Live Coding/다른 GPU 작업이 없는 직렬 슬롯에서만 수행한다. 개인 SaveGame/Editor 설정·창 배치와 보호 경로 hash를 먼저 보존하고 종료 뒤 복원한다. 운영맵/자산을 저장하지 않는다. 이 작업은 새 worktree 생성·merge/push나 보호 경로 변경 권한을 부여하지 않는다.

### 1. Editor 빌드·회귀

```powershell
Set-Location 'C:\Unreal Projects\ma0t10_dt'
$engine = 'C:\Program Files\Epic Games\UE_5.3'
$project = Join-Path $PWD 'ma0t10_dt.uproject'
$serial = Join-Path $PWD ('Saved/Reports/ANALYSIS-24-22-SERIAL/' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Force -Path $serial | Out-Null
& "$engine/Engine/Build/BatchFiles/Build.bat" ma0t10_dtEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE) { throw 'Editor build failed' }
$groups = @('MA0T10.LidarSemanticDiagnostics', 'MA0T10.GpuLidar.PointModelSnapshot',
    'MA0T10.GpuLidar.StaticCollisionRhi', 'MA0T10.GpuLidar.OwnedCollisionProof',
    'MA0T10.GpuLidar.RigidInstanceRhi', 'MA0T10.GpuLidar.FallbackSelection',
    'MA0T10.LidarRegression.GpuSemanticRhi', 'MA0T10.Inspection.UiFeedbackLifecycle',
    'MA0T10.SensorV2.UI.DualCameraSelection', 'MA0T10.SensorFiles.CameraCaptureDispatchIsolation')
& ./Scripts/run_smoke_tests.ps1 -ProjectPath $project -EngineRoot $engine -SkipBuild -Rhi D3D12 -TestGroups $groups -ReportRoot "$serial/editor"
```

각 index.json의 정확한 test name/Success/Failed/NotRun/skip과 assertion을 읽는다. process exit0이나 Success 안의 NullRHI skip은 통과가 아니다. 위 C++ 순수 로직 3개도 Editor binary가 실제로 만들어진 후에야 실행된다. `GpuSemanticRhi`는 기존 DepthProjection 회귀이며 HWRT 시험과 구분한다. Shipping에 Editor test 이름을 주는 것으로 패키지 검증을 대신하지 않는다.

### 2. Development/Shipping 패키징

```powershell
foreach ($configuration in @('Development','Shipping')) {
    $destination = Join-Path $serial $configuration
    & "$engine/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$project" -noP4 -platform=Win64 "-clientconfig=$configuration" -build -skipbuildeditor -cook '-map=/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap' -stage -pak -archive "-stagingdirectory=$destination/staging" "-archivedirectory=$destination/archive" -unattended -utf8output
    if ($LASTEXITCODE) { throw "$configuration BuildCookRun failed" }
}
```

각 구성에서 먼저 flag 없는 실행은 진단 파일이 새로 생기지 않는지 확인한다. 다음은 flag를 준 **표시형 package 실행**이다. 앞 실행을 정상 종료한 뒤 다음 구성으로 넘어간다. 이 블록의 `$configuration`은 실제 검사할 한 구성으로 명시한다.

```powershell
$configuration = 'Development' # 이후 Shipping으로 바꾸어 별도 실행
$destination = Join-Path $serial $configuration
& "$destination/archive/Windows/ma0t10_dt.exe" /Game/MA0T10/Maps/Tests/SlabScenarioValidationMap -d3d12 -windowed -ResX=1920 -ResY=1080 -LidarSemanticDiagnostics -MA0T10Control
```

기존 UI에서 `SLAB-LIDAR-001`의 측정 방식 **GPU 광선 추적**을 명시하고 Slab이 실제 관측되는 pose를 확인한다. AUTO-01 `sensor.set`은 backend 필드를 지원하지 않으므로 가짜 `backend` 인자를 job/API에 넣지 않는다. 새 기능/endpoint도 추가하지 않았다. 현재 native UI options는 Auto/GPU/CPU다. legacy DepthProjection은 기존 backend2를 보존한 fixture/설정에서만 별도 실행하고, 이를 위해 운영맵을 임의 저장하거나 새 cooked fixture를 조용히 만들지 않는다.

기존 loopback API를 명시적으로 켠 위 실행에서 UI backend 선택 후 다음처럼 기존 dataset 기능을 요청할 수 있다. endpoint는 자격증명 없는 로컬 기본값 예시이며 다른 서비스/설정을 바꾸지 않는다. 사전에 현재 장비 ID와 Slab 이름이 맞는지 UI/`GET /v1/sensors`로 확인한다.

```powershell
$body = @{name=('ANALYSIS24_' + $configuration + '_' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'));
    scenes=1; seed=22015; settle_frames=10; slab='SlabActor_0'; lidar_id='SLAB-LIDAR-001';
    camera_id='VCAM-001'; camera_truth=$true; lidar_fidelity=$false; export_format='none';
    ranges=@{yaw=@(0,0)}} | ConvertTo-Json -Depth 4
Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:8099/v1/dataset/start' -ContentType 'application/json' -Body $body | Out-Null
```

수락 응답은 완료가 아니다. UI/기존 dataset 상태가 완료되고 dataset.json/scene 파일이 확정될 때까지 기다린다. 실제 output root를 기록한다. 진단의 `stage=readback_complete`, active backend **3**, mapped 값 및 파일 label의 `lidar_backend=3`을 함께 대조한다. CPU fallback/미지원 장면은 미수용이다. Depth2의 경우 active backend2/converted 및 semantic_requested=true를 확인한다. 이 1장면 양성 시험은 과거 testset100개나 소비자 업무 ACK 수용을 대신하지 않는다.

### 3. 패키지 라벨 분포 확인 — 오프라인, GPU 사용 없음

`$datasetRoot`를 **해당 실행이 실제로 만든 dataset.json이 있는 폴더**로 지정한다. 같은 명령을 Development/Shipping 각각 수행한다. class 이름은 반드시 해당 manifest에서 읽으며 ID1을 Ground로 간주하지 않는다. 아래 positive assertion은 Slab이 알려진 위치에서 관측되는 fixture 조건이다.

```powershell
$datasetRoot = 'C:\actual-output\dataset-folder'
@'
from pathlib import Path
from collections import Counter
import json, re, struct, sys
root = Path(sys.argv[1])
m = json.loads((root / 'dataset.json').read_text(encoding='utf-8-sig'))
classes = {int(x['id']): x['name'] for x in m['classes']}
instances = {int(x['id']) for x in m['instances']}
assert m['completed_scenes'] == m['requested_scenes'] > 0
files = sorted(root.glob('scene_*/lidar_labels.bin'))
assert len(files) == m['completed_scenes'], 'missing scene labels'
counts, ids, backends = Counter(), Counter(), Counter()
for p in files:
    pcd = (p.parent / 'lidar.pcd').read_bytes()
    header, body = pcd.split(b'DATA binary\n', 1)
    n = int(re.search(rb'^POINTS (\d+)', header, re.M)[1])
    labels = p.read_bytes()
    assert len(body) == n * 33 and len(labels) == n * 5, p
    scene = json.loads((p.parent / 'label.json').read_text(encoding='utf-8-sig'))
    backends[scene.get('lidar_backend', 'unknown')] += 1
    for semantic, instance in struct.iter_unpack('<BI', labels):
        assert semantic in classes, ('unknown semantic', semantic)
        assert instance == 0 or instance in instances, ('unknown instance', instance)
        counts[semantic] += 1; ids[instance] += 1
print(json.dumps({'classes': {str(k): {'name': classes[k], 'points': v} for k,v in counts.items()},
    'instance_zero': ids[0], 'instances': dict(ids), 'backends': dict(backends)}, indent=2))
assert set(backends) == {3}, 'this run did not exclusively exercise sensor-owned HWRT'
assert sum(v for k,v in counts.items() if classes[k] == 'Slab') > 0, 'no positive Slab points'
assert sum(v for k,v in ids.items() if k != 0) > 0, 'all instance IDs are zero'
'@ | python -X utf8 - $datasetRoot
```

legacy Depth2를 의도적으로 시험했다면 backend assertion을 **그 별도 실행에서만 `{2}`**로 바꾸어 명시한다. HWRT 실패를 숨기기 위해 assertion을 지우지 않는다. 양성 Slab 점 개수만으로 의미 정확도 전체를 통과시키지 말고 known footprint/occluder 음성·acquisition pose·ID/geometry 불변도 확인한다.

실제 Saved root에서 `SensorCaptures/LidarSemantic_*/Analysis/*.json`을 찾아 schema/backend/stage/카운터를 대조한다. raw HWRT mapping의 named/Slab 양성인데 파일만 Other/instance0이면 그 frame의 dataset manifest 이름 lookup 단계로 조사 범위를 좁힌다. 파일 부재/거절/취소는 진단 성공이 아니다.

### 4. #22 — 현재 SHA 재현만, 코드 변경 없음

위 `MA0T10.Inspection.UiFeedbackLifecycle`의 실제 primary/secondary cached geometry와 failure/skip을 읽는다. 과거 `fa3d6ea` 수정의 존재를 현재 실행 PASS로 바꾸지 않는다. PIE에서 두 고유 ID Camera를 명시적으로 시작하고 RGB 듀얼 → 70:30/50:50/30:70 → 최소 카드 → 숨김/재열기 → Main/Viewport 재호스팅 → 주 Camera만 중지/재개 → 선택 변경을 직접 조작한다. client1920×1080/1280×720·font100/150%·실제 DPI, 단계별 누른 것/기대/실제/스크린샷을 기록한다.

실패 시 두 RT pointer/outer/TextureTarget, CaptureResolution/SizeX/Y, available/runtime FrameId, brush ImageSize, SImage desired/cached geometry와 visibility를 같은 시점에 남긴다. **RT/JPEG 양수인데 geometry만0**, **실제 RT0×0**, **유효 크기지만 검은 픽셀**, **파일만 없음**을 분리한다. 모니터용 추가 capture/readback은 만들지 않는다. 개인 설정을 복원하고 소유 Editor/PIE를 정상 종료한다.

Runtime 진단 코드의 최종 수용은 Editor/Game Development 및 Shipping 빌드·관련 회귀·직접 UI·표준 RT ProjectDefault ABAB를 직렬로 수행한 뒤 판단한다. 이번 quick의 합성 cohort fixture는 실측이 아니다. 커밋·정적 gate·보호 감사·schema/merge_check는 worktree `Saved/Reports/ANALYSIS-24-22/result.json` 및 RESULT.md를 따른다.
