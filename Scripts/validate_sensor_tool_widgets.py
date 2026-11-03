"""Compile seven project-owned panels in memory without saving an asset or level."""
import unreal

panels = [
    ("WBP_VirtualSensorMonitorPanel", unreal.DxWidgetVirtualSensorMonitorPanel),
    ("WBP_VirtualSensorSettingsPanel", unreal.DxWidgetVirtualSensorSettingsPanel),
    ("WBP_VirtualSensorCaptureExportPanel", unreal.DxWidgetVirtualSensorCaptureExportPanel),
    ("WBP_SlabScenarioReplayPanel", unreal.DxWidgetSlabScenarioReplayPanel),
    ("WBP_SlabChartsPanel", unreal.DxWidgetSlabChartsPanel),
    ("WBP_SlabProgressPanel", unreal.DxWidgetSlabProgressPanel),
    ("WBP_VirtualSensorPlacementPanel", unreal.DxWidgetVirtualSensorPlacementPanel),
]
for name, native in panels:
    path = "/Game/MA0T10/UI/" + name
    bp = unreal.load_asset(path)
    if not bp:
        raise RuntimeError("Missing owned panel: " + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = unreal.load_class(None, path + "." + name + "_C")
    if not cls or not isinstance(unreal.get_default_object(cls), native):
        raise RuntimeError("Unexpected native parent: " + path)
    unreal.log("[SensorToolWidgets] compiled " + path)
unreal.log("[SensorToolWidgets] all seven compiled; assets were not saved")
