# DATASET-01 standard export (EXPORT-01)

Offline conversion never changes the source dataset or sensor transport. No converter runs until explicitly selected. Default output remains PCD transmission. Python 3.12 on Windows is the tested environment.

```powershell
python -m venv Saved/DatasetExportEnv
Saved/DatasetExportEnv/Scripts/python.exe -m pip install -r Tools/DatasetTools/requirements.txt
$env:MA0T10_DATASET_PYTHON = (Resolve-Path Saved/DatasetExportEnv/Scripts/python.exe).Path
# Set before launching UnrealEditor or AutomationClient. Never install packages from the UE UI.
& $env:MA0T10_DATASET_PYTHON Tools/DatasetTools/export_coco.py D:/datasets/input D:/exports/coco
& $env:MA0T10_DATASET_PYTHON Tools/DatasetTools/export_kitti.py D:/datasets/input D:/exports/kitti
# Explicit approximation for arbitrary camera/object pitch and roll:
& $env:MA0T10_DATASET_PYTHON Tools/DatasetTools/export_dataset.py D:/datasets/input D:/exports/both --formats coco,kitti --upright-envelope --semantic-kitti
& $env:MA0T10_DATASET_PYTHON Tools/DatasetTools/validate_export.py coco D:/exports/both/coco --source D:/datasets/input --report D:/exports/coco-validation.json
& $env:MA0T10_DATASET_PYTHON Tools/DatasetTools/validate_export.py kitti D:/exports/both/kitti --source D:/datasets/input --report D:/exports/kitti-validation.json
& $env:MA0T10_DATASET_PYTHON -m unittest discover -s Tools/DatasetTools -v
```

Use a **new output directory disjoint from the input**. Existing outputs, ancestors/descendants, incomplete report-only samples, unknown schema/palette, malformed images/PCD and mismatched label counts are rejected. A failed/cancelled conversion removes only its own staging directory. Successful output is published by directory rename after reloading and validation; large conversions keep one scene's image/point arrays resident, plus COCO annotation JSON metadata. Exporting large datasets takes additional disk space for copied RGB/points. No archive, upload or training runs automatically.

In `데이터 → 파일 저장 → 데이터셋 생성`, **생성 후 내보내기 형식** cycles through 없음 / COCO / KITTI 3D / both. Default is 없음. Camera GT must be enabled. Tilt approximation has its own default-off checkbox. The source completes and original Actor/sensor settings restore first, then an external Python process converts to the sibling `<dataset>_export/`. UI shows export success/failure separately from source completion; the AutomationClient step fails on export failure. Stop/PIE teardown signals cooperative cancellation; conversion never calls Unreal objects or blocks its game thread. The source dataset survives export failure, cancellation and missing Python dependencies. Packaged deployments require separately distributing `Tools/DatasetTools` and a configured Python environment; no embedded Python/plugin is added.

For AutomationClient use [dataset_export.example.json](../AutomationClient/dataset_export.example.json), updating Actor/sensor names for your map. `dataset.start` now accepts `export_format` (`none`, `coco`, `kitti`, `coco,kitti`) and `export_upright_envelope` (bool). Omit both for existing behavior. Run through `run_job.py` as documented in the project README; the result's dataset step includes `detail.directory` and `detail.export_directory`. Sensors/Slab must already be stopped, as for DATASET-01. The batch timeout includes the optional conversion.

## Schema and semantic mapping

Existing `schema_version` values remain `ma0t10.dataset.v1` / `ma0t10.dataset.scene.v1`; additive **schema_minor=1** is the new minor version. Absence means minor 0. COCO accepts minor 0 and 1. KITTI explicitly refuses minor 0 because Actor pose alone does not prove mesh pivot/relative geometry. Regenerate it with minor 1; never infer a box from PCA. Future unsupported minors are rejected.

Scene minor 1 adds `boxes_3d`: currently one selected Slab entry with `instance_id`, `semantic_id=2`, actual rendered mesh OBB `center_m`, local XYZ `size_m`, full `quaternion_xyzw`, `yaw_deg`, and `coordinates="UE world meters; local XYZ dimensions; quaternion_xyzw"`. Center includes mesh bounds origin and component-relative transform; dimensions include component scale. It is ground truth, independent of PCA validity. Other actors lack 3D labels and **must not be treated as annotated negatives** in detection training. COCO covers all supported visible Actor/class pairs. WPO/masked/translucent/skeletal geometry keeps CAM-GT-01 limitations. Instance IDs are session-local; no tracking identity across datasets.

| Source semantic | COCO category | KITTI label_2 | Optional point .label low 16 bits |
|---|---|---|---|
| 0 Background/invalid | omitted | omitted | 0 |
| 1 Other | 1 Other | unboxed | 1 |
| 2 Slab | 2 Slab | Slab, selected Actor with OBB only | 2 |
| 3 Roller | 3 Roller | unboxed | 3 |
| 4 Conveyor | 4 Conveyor | unboxed | 4 |
| 5 Floor | 5 Floor | unboxed | 5 |
| 6 Sensor | 6 Sensor | unboxed | 6 |

Custom class names/IDs require the training tool's dataset configuration. They are neither COCO's 80 object classes nor KITTI's road categories; Slab is never relabeled as Car. No benchmark evaluation or trained-model accuracy is claimed.

## COCO instances

`images/*.png` and `annotations/instances.json`, with `images`, `annotations`, `categories`, `info`, `licenses`. Images use unique 1-based IDs. An annotation represents **visible instance/class pixels**, including multiple disconnected regions and holes. RLE is lossless compressed column-major COCO encoding from official pycocotools, `iscrowd=0`; a polygon approximation would lose holes. `bbox=[x,y,width,height]` includes both occupied pixel endpoints, `area` is exact visible pixel count. Category 0 and instance 0 are omitted. `source_instance_id` is an optional extension ignored by standard COCO readers. Reload validates every RLE pixel against source masks using `COCO.annToMask`, area/bbox and category/image references. Empty scenes/images are retained.

Reference: [official COCO data format](https://cocodataset.org/#format-data), [COCO mask API](https://github.com/cocodataset/cocoapi/blob/master/PythonAPI/pycocotools/mask.py). This is a synthetic custom-category dataset in the COCO instance format.

## KITTI object format and limits

`training/{velodyne,image_2,calib,label_2}/000000.*`, `ImageSets/train.txt`, `class-map.json`, `source-boxes.json`, `export-report.json`. Split is all completed scenes in `train`; no random validation/test split or temporal independence is invented.

| Field | Conversion / assumption |
|---|---|
| Velodyne `.bin` | little-endian float32 XYZI (16 bytes), same point order/count as packed PCD hits. PCD is already KITTI RH FLU `(X forward,Y left,Z up)` in meters, so **no second Y flip**. Intensity is original modeled uint16 divided by 65535; not calibrated reflectance. |
| Optical frame | Camera local UE `(X forward,Y right,Z up)` → optical `(Y,-Z,X)`, x right/y down/z forward. |
| Extrinsic | `Tr_velo_to_cam = [O Rc^T Rl S | O Rc^T(tl-tc)]`, `S=diag(1,-1,1)`, actual acquisition quaternion/translation. `S` converts PCD-local to UE-local only; no UE-world Y reflection is added. Consistent with ROS2-01's existing local FLU and output-world reflection convention. |
| Intrinsic | P2=`[camera_K | 0]`, actual resolution/FOV-derived pinhole intrinsics; `R0_rect=I`, no distortion. P0/P1/P3 are unavailable zero placeholders, no stereo/IMU calibration is fabricated. Only image_2/P2 are supported. |
| label_2 | 15 fields: class, truncation, occlusion, alpha, 0-based inclusive 2D box, height/width/length meters, bottom-center optical XYZ meters, rotation_y radians. No detection score. 2D box is the visible mask; truncation estimates projected actual OBB clipped by image bounds, **occlusion=3 (unknown)**. Only visible selected Slab boxes are emitted. |
| Box rotation | KITTI has only rotation around camera Y. Default **rejects tilt >0.01°**. `--upright-envelope` uses the actual length-axis yaw projected into camera XZ and an upright box enclosing all eight actual mesh OBB corners. This changes height/width/length for tilt; the original full OBB, tilt angle and envelope/original volume ratio remain in `source-boxes.json`. It is not exact tilted OBB truth. Box near-plane intersections are refused. |
| alpha | wrap(`rotation_y - atan2(bottom_center_x,bottom_center_z)`) to [-pi,pi]. |

Quantitative reload uses a strict independent text/binary KITTI parser and NumPy/Pillow, not an official Python KITTI API. It checks record counts, proper RH calibration, copied RGB, finite/range values and actual-OBB containment in the reloaded 3D label. Reproject the exported points with the **reloaded** calib, compare same-instance Slab projected point bounds with visible 2D bbox (IoU and fraction within 1px), and report actual 3D corner projection IoU. Missing projected points are counted separately with null metrics. Sparse sampling, occlusion and noise can reduce point-bbox IoU; structural validity is distinct from geometric alignment acceptance.

Reference: [official KITTI object devkit](https://www.cvlibs.net/datasets/kitti/eval_object.php?obj_benchmark=3d). Generic KITTI loaders can read the files with custom-class configuration; stereo/IMU or road-class-only loaders need adaptation. Unboxed background objects and arbitrary sensor poses make this unsuitable as an unqualified replacement for the KITTI road benchmark.

Optional `--semantic-kitti` adds `sequences/00/velodyne/*.bin` and `labels/*.label`: uint32 little-endian `semantic_id | (instance_id << 16)` in unchanged point order, instance >65535 refused. Canonical semantic IDs **0..6 are custom**, not SemanticKITTI road IDs/learning_map; use a custom training mapping. This is the point-label storage layout, not a full odometry sequence: no poses/timestamps/calib are invented. Source PCD+JSON remains available unchanged without a conversion.

Reference: [official SemanticKITTI point label API](https://github.com/PRBonn/semantic-kitti-api/blob/master/auxiliary/laserscan.py). No Docker, WSL, ROS service or broker is required.
