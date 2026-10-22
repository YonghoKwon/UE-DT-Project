"""COIL-01: create exactly one new map through Unreal APIs; never overwrite any map."""
import unreal

path = '/Game/MA0T10/Maps/Tests/CoilLoadingTestMap'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError('Refusing to overwrite existing map: ' + path)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sim_cls = unreal.load_class(None, '/Script/ma0t10_dt.CoilLoadingSimulation')
host_cls = unreal.load_class(None, '/Script/ma0t10_dt.CoilLoadingMonitorHost')
if not sim_cls or not host_cls:
    raise RuntimeError('Build COIL-01 Editor first')
if not levels.new_level(path):
    raise RuntimeError('Cannot create approved coil map')
sim = actors.spawn_actor_from_class(sim_cls, unreal.Vector())
sim.set_actor_label('COIL-01 - product quay, portal crane and box holds')
sim.set_editor_property('coil_surface_asset', unreal.load_asset('/Game/MA0T10/Coil/Materials/M_CoilSurface'))
sim.set_editor_property('structure_material_asset', unreal.load_asset('/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain'))
sim.set_editor_property('structure_cube_asset', unreal.load_asset('/Engine/BasicShapes/Cube'))
host = actors.spawn_actor_from_class(host_cls, unreal.Vector())
host.set_actor_label('COIL-01 - DTCore monitor host')
host.set_editor_property('simulation', sim)
camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-9500, -10500, 7500))
host.set_editor_property('view_camera', camera)
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-3000, -2000, 2000))
light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 7000), unreal.Rotator(pitch=-50, yaw=-25, roll=0))
light.light_component.set_editor_property('intensity', 5)
light.light_component.set_editor_property('atmosphere_sun_light', True)
light.light_component.set_editor_property('cast_shadows', False)
world = unreal.EditorLevelLibrary.get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.load_class(None, '/Script/ma0t10_dt.CoilLoadingGameMode'))
if not levels.save_current_level():
    raise RuntimeError('Could not save approved coil map')
unreal.log('COIL_MAP_CREATED: ' + path)
