"""Create owned Slab assets and a NEW validation map; preserve all existing maps.

Run after the Editor Development build, using Unreal's ExecutePythonScript.
The transaction table is amended without removing existing rows.
"""
import unreal

E = unreal.EditorAssetLibrary
M = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def material():
    path = '/Game/MA0T10/Slab/Materials/M_SlabSurface'
    asset = unreal.load_asset(path)
    if asset:
        return asset
    asset = tools.create_asset('M_SlabSurface', '/Game/MA0T10/Slab/Materials',
                               unreal.Material, unreal.MaterialFactoryNew())
    def node(cls, x, y):
        return M.create_material_expression(asset, cls, x, y)
    def scalar(name, value, x, y):
        n = node(unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('default_value', value)
        return n
    def color(name, value, x, y):
        n = node(unreal.MaterialExpressionVectorParameter, x, y)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('default_value', unreal.LinearColor(*value))
        return n
    def link(a, b, pin):
        if not M.connect_material_expressions(a, '', b, pin):
            raise RuntimeError('Material connection failed: ' + pin)
    cold = color('ColdColor', (.075, .085, .095, 1), -800, -300)
    hot = color('HotColor', (1.0, .18, .018, 1), -800, -100)
    hotness = scalar('Hotness', 0, -800, 100)
    roughness = scalar('Roughness', .65, -300, 550)
    oxidation = scalar('Oxidation', .6, -800, 400)
    glow = scalar('EmissiveStrength', 3, -500, 800)
    metal = scalar('Metallic', .8, -300, 650)
    world = node(unreal.MaterialExpressionWorldPosition, -1200, 250)
    noise = node(unreal.MaterialExpressionNoise, -1000, 250)
    noise.set_editor_property('scale', .06)
    noise.set_editor_property('levels', 2)
    noise.set_editor_property('quality', 1)
    link(world, noise, 'Position')
    oxide = node(unreal.MaterialExpressionMultiply, -600, 300)
    link(noise, oxide, 'A'); link(oxidation, oxide, 'B')
    invert = node(unreal.MaterialExpressionOneMinus, -400, 300)
    link(oxide, invert, '')
    heat = node(unreal.MaterialExpressionLinearInterpolate, -400, -200)
    link(cold, heat, 'A'); link(hot, heat, 'B'); link(hotness, heat, 'Alpha')
    base = node(unreal.MaterialExpressionMultiply, -100, -100)
    link(heat, base, 'A'); link(invert, base, 'B')
    emissive = node(unreal.MaterialExpressionMultiply, -300, 50)
    link(base, emissive, 'A'); link(hotness, emissive, 'B')
    strength = node(unreal.MaterialExpressionMultiply, 0, 100)
    link(emissive, strength, 'A'); link(glow, strength, 'B')
    for n, prop in [(base, unreal.MaterialProperty.MP_BASE_COLOR),
                    (roughness, unreal.MaterialProperty.MP_ROUGHNESS),
                    (metal, unreal.MaterialProperty.MP_METALLIC),
                    (strength, unreal.MaterialProperty.MP_EMISSIVE_COLOR)]:
        if not M.connect_material_property(n, '', prop):
            raise RuntimeError('Material property connection failed')
    M.recompile_material(asset)
    E.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset


def widget(name, parent):
    path = '/Game/MA0T10/UI/' + name
    asset = unreal.load_asset(path)
    if not asset:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/ma0t10_dt.' + parent))
        asset = tools.create_asset(name, '/Game/MA0T10/UI', unreal.WidgetBlueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    generated = unreal.load_class(None, path + '.' + name + '_C')
    if not generated:
        raise RuntimeError('Widget compile failed: ' + path)
    E.save_loaded_asset(asset, only_if_is_dirty=False)
    return generated


def transaction():
    table = unreal.load_asset('/Game/MA0T10/Common/DataTables/DT_TransactionCode')
    if not table:
        raise RuntimeError('Missing project transaction table')
    names = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    if 'IFactory-agent' not in names:
        raise RuntimeError('Run -run=EnsureSlabScenarioTransaction first; do not replace the table')
    unreal.log('SLAB_TRANSACTION_READY: preserved existing handlers')


material()
charts = widget('WBP_SlabChartsPanel', 'SlabChartsPanelWidget')
progress = widget('WBP_SlabProgressPanel', 'SlabProgressPanelWidget')
transaction()

path = '/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap'
if E.does_asset_exist(path):
    unreal.log('SLAB_MAP_EXISTS: preserved ' + path)
else:
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level.new_level(path):
        raise RuntimeError('New validation map creation failed')
    def spawn(cls, label, location=unreal.Vector(), rotation=unreal.Rotator()):
        a = actors.spawn_actor_from_class(cls, location, rotation)
        a.set_actor_label(label)
        a.set_editor_property('tags', ['SlabValidationManaged'])
        return a
    def native(name):
        cls = unreal.load_class(None, '/Script/ma0t10_dt.' + name)
        if not cls:
            raise RuntimeError('Build native class first: ' + name)
        return cls
    cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
    def box(label, location, scale):
        a = spawn(unreal.StaticMeshActor, label, location)
        a.static_mesh_component.set_static_mesh(cube)
        a.set_actor_scale3d(scale)
        return a
    box('SlabTestFloor', unreal.Vector(0, 0, -15), unreal.Vector(34, 22, .3))
    box('LeftGuardrail', unreal.Vector(0, -95, 60), unreal.Vector(26, .1, .4))
    box('RightGuardrail', unreal.Vector(0, 95, 60), unreal.Vector(26, .1, .4))
    track = spawn(native('SlabTrackReferenceActor'), 'SlabTrackReference', unreal.Vector(-600, 0, 40))
    slab = spawn(native('SlabActor'), 'SlabPlayback', unreal.Vector(-600, 0, 40))
    # Explicit demonstration override: production class defaults remain centimetres.
    slab.set_editor_property('track_reference', track)
    slab.set_editor_property('dimension_unit', unreal.SlabInputUnit.MILLIMETERS)
    slab.set_editor_property('position_unit', unreal.SlabInputUnit.CENTIMETERS)
    track.set_editor_property('rails_configured', True)
    track.set_editor_property('left_rail_ycm', -90.0)
    track.set_editor_property('right_rail_ycm', 90.0)
    track.set_editor_property('rail_length_cm', 2600.0)
    track.set_editor_property('rail_height_cm', 40.0)
    coordinator = spawn(native('VirtualSensorCoordinator'), 'SlabSensorCoordinator')
    lidar = spawn(native('VirtualLidarSensorActor'), 'SlabOverheadLidar', unreal.Vector(0, 0, 1800), unreal.Rotator(pitch=-90, yaw=90, roll=0))
    camera_pos = unreal.Vector(-1100, -1100, 1000)
    camera = spawn(native('VirtualCameraSensorActor'), 'SlabDiagonalCamera', camera_pos,
                   unreal.MathLibrary.find_look_at_rotation(camera_pos, unreal.Vector(0, 0, 60)))
    spawn(native('VirtualSensorUiHostActor'), 'SlabSensorUi')
    host = spawn(native('SlabSimulationUiHostActor'), 'SlabSimulationUi')
    host.set_editor_property('slab_actor', slab)
    host.set_editor_property('charts_widget_class', charts)
    host.set_editor_property('progress_widget_class', progress)
    rig = spawn(native('SlabScenarioValidationRig'), 'SlabValidationControls')
    for k, v in [('slab_actor', slab), ('lidar', lidar), ('camera', camera), ('coordinator', coordinator)]:
        rig.set_editor_property(k, v)
    start = spawn(unreal.PlayerStart, 'SlabFreeViewStart', unreal.Vector(-1300, -1100, 1000),
                  unreal.MathLibrary.find_look_at_rotation(unreal.Vector(-1300, -1100, 1000), unreal.Vector()))
    sun = spawn(unreal.DirectionalLight, 'SlabSun', unreal.Vector(0, 0, 1200), unreal.Rotator(pitch=-55, yaw=-35))
    sun.light_component.set_editor_property('intensity', 4)
    spawn(unreal.SkyLight, 'SlabAmbient', unreal.Vector(0, 0, 1000))
    if not level.save_current_level():
        raise RuntimeError('New Slab map save failed')
    unreal.log('SLAB_MAP_CREATED ' + path)

unreal.log('SLAB_ASSETS_READY: no production map loaded or saved')
