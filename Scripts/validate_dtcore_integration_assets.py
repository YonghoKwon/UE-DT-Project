"""Read-only compatibility inventory and in-memory Blueprint compilation."""
import json
import os
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = {"blueprints": [], "widget_configs": [], "saved_packages": 0}
for data in registry.get_assets_by_path('/Game/MA0T10', recursive=True):
    class_name = str(data.asset_class_path.asset_name)
    if class_name not in ('Blueprint', 'WidgetBlueprint', 'DxWidgetConfigData'):
        continue
    asset = data.get_asset()
    if not asset:
        raise RuntimeError('Failed to load ' + str(data.package_name))
    if class_name in ('Blueprint', 'WidgetBlueprint'):
        success = unreal.DTCoreContractListener.compile_blueprint_for_validation(asset)
        item = {"path": str(data.package_name), "compiled": success}
        report['blueprints'].append(item)
        if not success:
            raise RuntimeError('Blueprint compile failed: ' + item['path'])
    else:
        keys = sorted(int(key) for key in asset.get_editor_property('widget_map').keys())
        report['widget_configs'].append({"path": str(data.package_name), "identifier_keys": keys})

directory = os.path.join(unreal.Paths.project_saved_dir(), 'Reports', 'DTCoreSync')
os.makedirs(directory, exist_ok=True)
with open(os.path.join(directory, 'assets.json'), 'w', encoding='utf-8') as output:
    json.dump(report, output, ensure_ascii=False, indent=2)
unreal.log('DTCORE_ASSETS_PASSED blueprints=' + str(len(report['blueprints'])) + ' saved=0')
