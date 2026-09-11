"""Create/compile only the replay WBP. Never loads or changes a map."""
import unreal

path = "/Game/MA0T10/UI/WBP_SlabScenarioReplayPanel"
parent = unreal.load_class(None, "/Script/ma0t10_dt.SlabScenarioReplayPanelWidget")
if not parent:
    raise RuntimeError("Build the native replay widget first")
asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not asset:
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("ParentClass", parent)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WBP_SlabScenarioReplayPanel", "/Game/MA0T10/UI", unreal.WidgetBlueprint, factory)
if not asset:
    raise RuntimeError("Missing replay asset")
unreal.BlueprintEditorLibrary.compile_blueprint(asset)
generated = unreal.load_class(None, path + ".WBP_SlabScenarioReplayPanel_C")
if not generated or not isinstance(unreal.get_default_object(generated), unreal.SlabScenarioReplayPanelWidget):
    raise RuntimeError("Incompatible native parent; not saving")
if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
    raise RuntimeError("Replay WBP save failed")
unreal.log("[SlabReplayAsset] WBP compiled and saved; no map modified")
