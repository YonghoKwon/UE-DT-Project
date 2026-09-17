"""Compile existing sensor panels without saving them or loading user maps."""
import unreal
paths = [
    '/Game/MA0T10/UI/WBP_VirtualSensorMonitorPanel',
    '/Game/MA0T10/UI/WBP_VirtualSensorSettingsPanel',
    '/Game/MA0T10/UI/WBP_VirtualSensorCaptureExportPanel',
]
# Resolve by name to accommodate an integration's folder layout, never guess a missing asset.
registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = registry.get_assets_by_path('/Game/MA0T10', recursive=True)
for wanted in paths:
    name = wanted.rsplit('/', 1)[-1]
    matches = [a for a in assets if str(a.asset_name) == name]
    if len(matches) != 1:
        raise RuntimeError(f'Expected one asset {name}; got {len(matches)}')
    blueprint = matches[0].get_asset()
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.log('LIDAR_WIDGET_COMPILED ' + str(matches[0].package_name))
# Compile only; do not save Designer or Map packages.
