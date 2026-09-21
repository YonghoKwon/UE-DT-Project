"""Create only the two owned presentation materials; never modify maps/custom assets."""
import unreal

M = unreal.MaterialEditingLibrary
E = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = '/Game/MA0T10/Slab/Materials'


def build(name, overlay=False):
    path = ROOT + '/' + name
    if E.does_asset_exist(path):
        unreal.log('SLAB_ANALYSIS_MATERIAL_PRESERVED: ' + path)
        return
    asset = TOOLS.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
    if not asset:
        raise RuntimeError('Material creation failed: ' + path)

    def node(kind, x, y):
        return M.create_material_expression(asset, kind, x, y)

    def scalar(name, value, y):
        n = node(unreal.MaterialExpressionScalarParameter, -600, y)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('default_value', value)
        return n

    def color(name, value, y):
        n = node(unreal.MaterialExpressionVectorParameter, -800, y)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('default_value', unreal.LinearColor(*value))
        return n

    def link(a, b, pin):
        if not M.connect_material_expressions(a, '', b, pin):
            raise RuntimeError('Material pin failed: ' + pin)

    def output(a, prop):
        if not M.connect_material_property(a, '', prop):
            raise RuntimeError('Material output failed: ' + str(prop))

    if overlay:
        asset.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        output(color('Color', (.05, .9, .75, 1), 0), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        cold = color('ColdColor', (.075, .085, .095, 1), -200)
        hot = color('HotColor', (1.0, .18, .018, 1), 0)
        heat = scalar('Hotness', 0, 150)
        base = node(unreal.MaterialExpressionLinearInterpolate, -350, -100)
        link(cold, base, 'A'); link(hot, base, 'B'); link(heat, base, 'Alpha')
        emit = node(unreal.MaterialExpressionMultiply, -250, 160)
        link(base, emit, 'A'); link(heat, emit, 'B')
        strength = node(unreal.MaterialExpressionMultiply, 0, 160)
        link(emit, strength, 'A'); link(scalar('EmissiveStrength', 3, 350), strength, 'B')
        output(base, unreal.MaterialProperty.MP_BASE_COLOR)
        output(strength, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        output(scalar('Roughness', .65, 500), unreal.MaterialProperty.MP_ROUGHNESS)
        output(scalar('Metallic', .8, 650), unreal.MaterialProperty.MP_METALLIC)
        # Deliberately no Noise, WorldPosition, Oxidation, or procedural variation.
    M.recompile_material(asset)
    E.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log('SLAB_ANALYSIS_MATERIAL_CREATED: ' + path)


build('M_SlabSurfacePlain')
build('M_SlabAnalysisOverlay', overlay=True)
