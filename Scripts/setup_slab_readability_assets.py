"""Create only new project-owned diagnostic assets. Never edits maps/engine assets."""
import unreal

E = unreal.EditorAssetLibrary
M = unreal.MaterialEditingLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = '/Game/MA0T10/Slab/Materials'
FONT_PATH = ROOT + '/F_SlabDiagnostics'
font = unreal.load_asset(FONT_PATH) if E.does_asset_exist(FONT_PATH) else None
if not font:
    factory = unreal.TrueTypeFontFactory()
    options = factory.get_editor_property('import_options')
    data = options.get_editor_property('data')
    data.set_editor_property('font_name', 'Malgun Gothic')
    data.set_editor_property('height', 32.0)
    data.set_editor_property('chars', ''.join(chr(i) for i in range(32, 127)) + '침범')
    data.set_editor_property('use_distance_field_alpha', True)
    data.set_editor_property('distance_field_scale_factor', 4)
    data.set_editor_property('texture_page_width', 512)
    data.set_editor_property('texture_page_max_height', 512)
    options.set_editor_property('data', data)
    font = T.create_asset('F_SlabDiagnostics', ROOT, unreal.Font, factory)
    if not font:
        raise RuntimeError('Cannot create diagnostic font (Malgun Gothic required on asset-authoring PC)')
    E.save_loaded_asset(font, only_if_is_dirty=False)

for name, text in [('M_SlabAnalysisReadable', False), ('M_SlabTextReadable', True)]:
    path = ROOT + '/' + name
    if E.does_asset_exist(path):
        unreal.log('READABLE_ASSET_PRESERVED ' + path)
        continue
    asset = T.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
    asset.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    asset.set_editor_property('two_sided', True)
    def node(cls):
        return M.create_material_expression(asset, cls, -300, 0)
    if text:
        asset.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        color = node(unreal.MaterialExpressionVertexColor)
        sample = node(unreal.MaterialExpressionFontSampleParameter)
        sample.set_editor_property('parameter_name', 'Font')
        sample.set_editor_property('font', font)
        sample.set_editor_property('font_texture_page', 0)
        M.connect_material_property(sample, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    else:
        color = node(unreal.MaterialExpressionVectorParameter)
        color.set_editor_property('parameter_name', 'Color')
        color.set_editor_property('default_value', unreal.LinearColor(.01, .18, .95, 1))
    inverse = node(unreal.MaterialExpressionEyeAdaptationInverse)
    alpha = node(unreal.MaterialExpressionConstant)
    alpha.set_editor_property('r', 1.0)
    inputs = M.get_material_expression_input_names(inverse)
    unreal.log('READABLE_EXPOSURE_INPUTS ' + str(inputs))
    if not M.connect_material_expressions(color, '', inverse, inputs[0]):
        raise RuntimeError('EyeAdaptationInverse LightValue connection failed')
    if not M.connect_material_expressions(alpha, '', inverse, inputs[1]):
        raise RuntimeError('EyeAdaptationInverse Alpha connection failed')
    M.connect_material_property(inverse, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    M.recompile_material(asset)
    E.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log('READABLE_ASSET_CREATED ' + path)
unreal.log('SLAB_READABILITY_ASSETS_READY')
