# AGENTS.md - UE-DT-Project ?묒뾽 媛?대뱶

## ?꾨줈?앺듃 湲곗?

- Unreal Engine: 5.3
- 硫붿씤 ?꾨줈?앺듃: `UE-DT-Project`
- Core ?뚮윭洹몄씤: `Plugins/DTCore` submodule
- 湲곗? DTCore 釉뚮옖移? `claude/digital-twin-core-analysis-1lxlcb`
- ?꾩옱 湲곕뒫 釉뚮옖移? `feature/lidar-point-cloud-only-view`

???꾨줈?앺듃??泥좉컯 ?쒖“ ?섍꼍 Digital Twin??紐⑺몴濡??⑸땲?? ?꾩옱 DT-Project 肄붾뱶??媛??移대찓?? 媛??LiDAR, ?쇱꽌 留ㅻ땲?, ?쇱꽌 ?곗씠???꾩넚/??? point-cloud-only view, Slab 媛곷룄 遺꾩꽍 v1, CSV/JSONL replay source, monitor widget/HostActor瑜??ы븿?⑸땲??

DTCore??怨듯넻 API/WebSocket/Data/UI 湲곕컲 ?뚮윭洹몄씤?쇰줈 ?ъ슜?⑸땲?? ????μ냼 ?묒뾽?먯꽌???ъ슜?먭? 紐낆떆?곸쑝濡??덉슜?섍린 ?꾧퉴吏 DTCore ?대? ?뚯뒪瑜?吏곸젒 ?섏젙?섏? ?딆뒿?덈떎.

## ?묒뾽 ?먯튃

- DTCore ?섏젙???꾩슂??蹂댁씠硫?DT-Project ?덉뿉???고쉶?섍굅??蹂꾨룄 ?댁뒋濡??④퉩?덈떎.
- `Plugins/DTCore`??submodule濡??좎??⑸땲?? ?댁옣 ?뚯뒪 蹂듭궗蹂몄쓣 ?ㅼ떆 而ㅻ컠?섏? ?딆뒿?덈떎.
- 而ㅻ컠 ??`Scripts/validate_dtcore_submodule_guard.ps1`濡?`Plugins/DTCore`媛
  `2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7`??怨좎젙?섏뼱 ?덇퀬 staged,
  parent, submodule worktree 蹂寃쎌씠 ?녿뒗吏 ?뺤씤?⑸땲??
- DTCore is an external submodule pinned to that commit; this repository must
  not stage gitlink changes or files under `Plugins/DTCore` during local
  decision cleanup.
- `Binaries/`, `Intermediate/`, `Saved/`, `.vs/` ?앹꽦臾쇱? 而ㅻ컠?섏? ?딆뒿?덈떎.
- ??⑸웾 marketplace/content asset? ?섎룄?곸쑝濡?異붿쟻 ??곸씤吏 ?뺤씤?섍린 ?꾧퉴吏 而ㅻ컠?섏? ?딆뒿?덈떎.
- 媛???쇱꽌???쒕쾭 ?꾩넚 ?곗씠?곗? editor preview ?곗씠?곕뒗 遺꾨━?⑸땲??
- 臾닿굅???듭뀡? 湲곕낯媛믪쑝濡?耳쒖? ?딆뒿?덈떎. FullSpec, MultiHit, ExportOnScan 議고빀? 寃利앹슜?쇰줈留??ъ슜?⑸땲??
- 濡쒖뺄 ?ㅽ뻾 ?꾨줈?앺듃 `C:\Unreal Projects\ma0t10_dt`??諛섏쁺?섎뒗 寃껉낵 git commit? 蹂꾧컻 ?묒뾽?낅땲?? 湲곕뒫 蹂寃???媛?ν븳 ???묒뾽 repo? 濡쒖뺄 ?꾨줈?앺듃 ?묒そ??媛숈? ?댁슜?쇰줈 而ㅻ컠?⑸땲??

## LiDAR ?곗씠???뺤콉

`UVirtualLidarSensorComp`????踰덉쓽 scan ?먮뒗 replay frame?먯꽌 ?앹꽦???꾩껜 痢≪젙媛믪쓣 `LastPoints`???좎??⑸땲??

?쒕쾭/?먮떒 payload:

- `ServerPayloadStride`
- `MaxServerPayloadPoints`
- `bIncludeMissPointsInServerPayload`
- `schemaVersion = virtual-lidar.v1`
- `payloadPolicy`
- `slabAnalysis`

Editor preview:

- `PreviewPointStride`
- `MaxPreviewPoints`
- `bPointCloudPreviewHitOnly`
- `previewPolicy`

湲곕낯 諛⑺뼢? ?쒕쾭?먮뒗 ?먮낯??媛源뚯슫 hit point瑜?蹂대궡怨? Unreal Editor ?붾㈃?먮뒗 ?쒗븳??point留??쒖떆?섎뒗 寃껋엯?덈떎. 湲곗〈 `PayloadPointStride`, `MaxPayloadPoints`, `PointCloudPreviewStride`, `MaxPointCloudPreviewInstances`??Blueprint ?명솚???꾪빐 ?⑥븘 ?덉?留????뺤콉 ?꾨뱶瑜??곗꽑 ?ъ슜?⑸땲??

PointCloudOnly 紐⑤뱶?먯꽌??preview ?뺤콉? ?쒕쾭 payload ?뺤콉??蹂寃쏀븯吏 ?딆븘???⑸땲??

## Replay Source

`ULidarCsvReplaySourceComp`? `ULidarJsonLinesReplaySourceComp`????λ맂 point cloud瑜?`UVirtualLidarSensorComp::InjectPointCloudFrame`?쇰줈 二쇱엯?⑸땲??

吏??CSV ?뺤떇:

```text
row,col,x,y,z
x,y,z
```

吏??JSONL ?뺤떇:

```text
{"x":900,"y":-260,"z":0,"distance":936.8,"hit":true,"semanticLabel":"Slab"}
{"worldLocation":[900,-260,0],"localDirection":[1,0,0],"hit":true,"semanticLabel":"Slab"}
```

replay 寃쎈줈??payload, recorder, transport, preview, Slab 遺꾩꽍??紐⑤몢 ?듦낵?댁빞 ?⑸땲?? ?ㅼ젣 Livox/ROS2/RealSense adapter瑜?異붽????뚮룄 媛?ν븳 ??媛숈? normalized frame 二쇱엯 寃쎈줈瑜?怨듭쑀?⑸땲??

## Slab 媛곷룄 遺꾩꽍

Slab 遺꾩꽍? LiDAR semantic label??`Slab`??hit point瑜?湲곗??쇰줈 ?⑸땲??

沅뚯옣 actor ?ㅼ젙:

```text
Actor Tag = Slab
```

遺꾩꽍 寃곌낵 `FVirtualLidarSlabAnalysisResult`??LiDAR JSON payload??`slabAnalysis`?먮룄 ?ы븿?⑸땲??

?ы븿 媛?

- slab hit point count
- bounds min/max
- center
- estimated yaw
- reference yaw
- angle deviation
- confidence
- status message

`ReferenceSlabYawDegrees`??湲곗? 異뺤쓣 ?삵빀?덈떎. ?ㅻ퉬 醫뚰몴怨꾩뿉???뺤긽 Slab 吏꾪뻾 諛⑺뼢??X異뺤씠硫?`0`?쇰줈 ?〓땲??

## Widget 援ъ꽦

沅뚯옣 Widget parent class:

```text
UVirtualSensorMonitorWidget
```

湲곗〈 Blueprint ?꾩젽怨쇱쓽 ?명솚???꾪빐 binding? optional?낅땲?? 沅뚯옣 ?대쫫:

```text
ViewImage
TitleText
StatusText
ToggleButton
ToggleButtonText
NextCameraButton
NextLidarButton
PointCloudOnlyButton
LidarViewModeButton
LidarViewModeButtonText
LogPointCloudButton
ExportPointCloudButton
LocalSensorCaptureButton
LocalSensorCaptureButtonText
CaptureOnceButton
PreviewMoreButton
PreviewLessButton
```

`StatusText`?먮뒗 sensor id, frame id, scan interval, ray count, hit count, server payload count, preview count, Slab angle, confidence, performance warning???쒖떆?⑸땲??

異붽? 議곗옉:

- `CaptureOnceButton`: SensorManager媛 ?덉쑝硫??좏깮??移대찓??LiDAR 1??capture, ?놁쑝硫?諛붿씤?⑸맂 移대찓??LiDAR 吏곸젒 capture
- `PreviewMoreButton`: ?쒕쾭 payload???좎??섍퀬 editor preview ?쒖떆?됰쭔 利앷?
- `PreviewLessButton`: ?쒕쾭 payload???좎??섍퀬 editor preview ?쒖떆?됰쭔 媛먯냼
- native fallback: `MonitorWidgetClass`媛 鍮꾩뼱 ?덇퀬 `bUseNativeMonitorWidgetFallback`??true????smoke-test??Slate ?⑤꼸???앹꽦

SensorManager Blueprint API:

```text
CaptureSelectedOnce
CaptureAllOnce
SetSelectedLidarPreviewPolicy
AdjustSelectedLidarPreviewBudget
TogglePointCloudOnlyView
SetPointCloudOnlyMode
```

## Editor Smoke Test 沅뚯옣媛?

```text
Camera SimulationQuality = RealTimePreview
LiDAR SimulationQuality = RealTimePreview
SharedSensorTransport.TransportMode = LogOnly
LiDAR bUseMultiHit = false
LiDAR bDrawDebugRays = false
ExportOnScan = false
PointCloudOnly = ?꾩슂 ?쒖뿉留?On
```

?깅뒫 臾몄젣媛 ?덉쑝硫??ㅼ쓬 ?쒖꽌濡???땅?덈떎.

```text
1. LiDAR SimulationQuality = Debug
2. PreviewPointStride 利앷?
3. MaxPreviewPoints 媛먯냼
4. Camera CaptureMode = PreviewOnly
5. MultiHit / ExportOnScan / FullSpec 鍮꾪솢?깊솕
```

## 寃利?紐낅졊

鍮뚮뱶:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" ma0t10_dtEditor Win64 Development "C:\Unreal Projects\ma0t10_dt\ma0t10_dt.uproject" -WaitMutex
```

?먮룞???뚯뒪??

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1"
```

媛쒕퀎 ?뚯뒪??

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\ma0t10_dt\ma0t10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests MA0T10.SensorReplay; Quit" -TestExit="Automation Test Queue Empty"
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\ma0t10_dt\ma0t10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests MA0T10.RealSensorSource; Quit" -TestExit="Automation Test Queue Empty"
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\ma0t10_dt\ma0t10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests MA0T10.SensorMonitor; Quit" -TestExit="Automation Test Queue Empty"
```

## 臾몄꽌 愿由?

- 猷⑦듃 臾몄꽌??`README.md`? `AGENTS.md`留??좎??⑸땲??
- ?댁쟾 ?ㅽ? ?뚯씪紐?`AGETNS.md`???ъ슜?섏? ?딆뒿?덈떎.
- 湲곕뒫 ?ㅻ챸? README?? ?묒뾽?먯슜 ?댁쁺 洹쒖튃? AGENTS???〓땲??
- ?몃? ?ㅽ뻾 臾몄꽌??`docs/` ?꾨옒???〓땲??
