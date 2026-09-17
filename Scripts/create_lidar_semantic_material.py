"""Create only the sensor-owned ID material; never modify source scene materials."""
import unreal

path = '/Game/MA0T10/Sensor/Materials/M_LidarSemanticId'
material = unreal.load_asset(path)
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_LidarSemanticId', '/Game/MA0T10/Sensor/Materials', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('used_with_instanced_static_meshes', True)
    node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    node.set_editor_property('parameter_name', 'SemanticId')
    node.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 1))
    if not unreal.MaterialEditingLibrary.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Cannot connect SemanticId output to emissive')
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
if unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR) is None:
    node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    node.set_editor_property('parameter_name', 'SemanticId')
    if not unreal.MaterialEditingLibrary.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Cannot repair owned ID material')
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
unreal.log('LIDAR_SEMANTIC_MATERIAL_READY ' + path)
