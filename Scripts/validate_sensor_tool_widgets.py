"""Compile the four project-owned panels without saving any asset or level."""
import unreal

panels = [
    ("WBP_VirtualSensorMonitorPanel", unreal.VirtualSensorMonitorPanelWidget),
    ("WBP_VirtualSensorSettingsPanel", unreal.VirtualSensorSettingsPanelWidget),
    ("WBP_VirtualSensorCaptureExportPanel", unreal.VirtualSensorCaptureExportPanelWidget),
    ("WBP_SlabScenarioReplayPanel", unreal.SlabScenarioReplayPanelWidget),
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
unreal.log("[SensorToolWidgets] all four compiled; assets were not saved")
