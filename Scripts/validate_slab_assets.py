"""Compile owned WBP in memory and inspect committed asset references; no saves."""
import unreal

parents = {
    'WBP_VirtualSensorMonitorPanel': 'VirtualSensorMonitorPanelWidget',
    'WBP_VirtualSensorSettingsPanel': 'VirtualSensorSettingsPanelWidget',
    'WBP_VirtualSensorCaptureExportPanel': 'VirtualSensorCaptureExportPanelWidget',
    'WBP_SlabScenarioReplayPanel': 'SlabScenarioReplayPanelWidget',
    'WBP_SlabChartsPanel': 'SlabChartsPanelWidget',
    'WBP_SlabProgressPanel': 'SlabProgressPanelWidget',
}
for name, parent in parents.items():
    path = '/Game/MA0T10/UI/' + name
    bp = unreal.load_asset(path)
    if not bp:
        raise RuntimeError('Missing ' + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = unreal.load_class(None, path + '.' + name + '_C')
    expected = getattr(unreal, parent)
    if not cls or not isinstance(unreal.get_default_object(cls), expected):
        raise RuntimeError('Wrong parent/generated class: ' + path)
    unreal.log('SLAB_WBP_VALIDATED ' + name)
for path in ['/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap',
             '/Game/MA0T10/Slab/Materials/M_SlabSurface',
             '/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain',
             '/Game/MA0T10/Slab/Materials/M_SlabAnalysisOverlay']:
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError('Missing ' + path)
unreal.log('SLAB_ASSET_VALIDATION_PASSED; no asset saved')
