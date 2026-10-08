"""Create only the new placement WBP; never saves an existing asset or map."""
import unreal
path = "/Game/MA0T10/UI/WBP_VirtualSensorPlacementPanel"
parent = unreal.load_class(None, "/Script/ma0t10_dt.VirtualSensorPlacementPanelWidget")
if not parent:
    raise RuntimeError("Build placement panel first")
asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not asset:
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("ParentClass", parent)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WBP_VirtualSensorPlacementPanel", "/Game/MA0T10/UI", unreal.WidgetBlueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Placement WBP save failed")
else:
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
cls = unreal.load_class(None, path + ".WBP_VirtualSensorPlacementPanel_C")
if not cls or not isinstance(unreal.get_default_object(cls), unreal.VirtualSensorPlacementPanelWidget):
    raise RuntimeError("Unexpected placement WBP parent")
unreal.log("[PlacementAsset] new placement WBP compiled; existing assets/maps untouched")
