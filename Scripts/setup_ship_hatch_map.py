"""HATCH-01: create exactly the approved new map, using Unreal APIs. Existing targets abort."""
import unreal

path = '/Game/MA0T10/Maps/Tests/ShipHatchTestMap'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError('Refusing to overwrite existing map: ' + path)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sim_cls = unreal.load_class(None, '/Script/ma0t10_dt.ShipHatchSimulation')
host_cls = unreal.load_class(None, '/Script/ma0t10_dt.ShipHatchMonitorHost')
if not sim_cls or not host_cls:
    raise RuntimeError('Build HATCH-01 Editor first')
if not levels.new_level(path):
    raise RuntimeError('Cannot create approved map')
sim = actors.spawn_actor_from_class(sim_cls, unreal.Vector())
sim.set_actor_label('Ship Hatch - procedural hold and cargo')
host = actors.spawn_actor_from_class(host_cls, unreal.Vector())
host.set_editor_property('simulation', sim)
host.set_actor_label('Ship Hatch - monitoring')
# All scene content belongs to this explicitly requested map. No automatic environment in other maps.
camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-3400, -3800, 6500), unreal.Rotator(pitch=-46, yaw=48, roll=0))
camera.camera_component.set_editor_property('field_of_view', 67)
host.set_editor_property('view_camera', camera)
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 4200))
light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 7000), unreal.Rotator(pitch=-65, yaw=-25, roll=0))
light.light_component.set_editor_property('intensity', 5)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 5000))
sky.light_component.set_editor_property('intensity', 1.5)
world = unreal.EditorLevelLibrary.get_editor_world()
# Map-local Engine GameMode; no project/controller/DTCore global setting replacement.
world.get_world_settings().set_editor_property('default_game_mode', unreal.load_class(None, '/Script/Engine.GameModeBase'))
if not levels.save_current_level():
    raise RuntimeError('Could not save approved hatch map')
unreal.log('HATCH_MAP_CREATED: ' + path)
