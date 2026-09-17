"""Create a NEW tag-free geometry validation map. Existing maps are never overwritten."""
import unreal

path = '/Game/MA0T10/Maps/Tests/LidarSemanticValidationMap'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.log('GEOMETRY_MAP_EXISTS: preserved ' + path)
else:
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level.new_level(path):
        raise RuntimeError('Cannot create dedicated validation map')
    def spawn(cls, label, location, rotation=unreal.Rotator()):
        actor = actors.spawn_actor_from_class(cls, location, rotation)
        actor.set_actor_label(label)
        actor.set_editor_property('tags', [])
        return actor
    cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
    material = unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial')
    def box(label, z, scale):
        actor = spawn(unreal.StaticMeshActor, label, unreal.Vector(0,0,z))
        mesh = actor.static_mesh_component
        mesh.set_mobility(unreal.ComponentMobility.MOVABLE)
        mesh.set_static_mesh(cube)
        mesh.set_material(0, material)
        actor.set_actor_scale3d(scale)
        return actor
    plate = box('GeometryReferenceSurface', -10, unreal.Vector(4,8,.2))
    moving = box('GeometryMovingObject', 12.5, unreal.Vector(1,1,.25))
    sensor = spawn(unreal.load_class(None,'/Script/ma0t10_dt.VirtualLidarSensorActor'), 'GeometryMeasurement', unreal.Vector(0,0,1000), unreal.Rotator(pitch=-90,yaw=0,roll=0))
    coordinator = spawn(unreal.load_class(None,'/Script/ma0t10_dt.VirtualSensorCoordinator'), 'GeometryCoordinator', unreal.Vector())
    location = unreal.Vector(900,-1000,900)
    camera = spawn(unreal.CameraActor, 'GeometryObservation', location, unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector()))
    camera.camera_component.set_editor_property('field_of_view', 65)
    light = spawn(unreal.DirectionalLight, 'GeometryLight', unreal.Vector(0,0,1000), unreal.Rotator(pitch=-55,yaw=-30,roll=0))
    light.light_component.set_editor_property('intensity', 4.0)
    spawn(unreal.SkyLight, 'GeometryAmbient', unreal.Vector(0,0,1200))
    spawn(unreal.PlayerStart, 'GeometryPlayerStart', unreal.Vector(700,-700,300))
    rig = spawn(unreal.load_class(None,'/Script/ma0t10_dt.LidarGeometryValidationRig'), 'GeometryValidationControls', unreal.Vector())
    for key, value in [('plate',plate),('moving_object',moving),('sensor',sensor),('coordinator',coordinator),('observation_camera',camera)]:
        rig.set_editor_property(key,value)
    assert plate.get_editor_property('tags') == [] and moving.get_editor_property('tags') == []
    assert plate.static_mesh_component.get_material(0) == moving.static_mesh_component.get_material(0)
    if not level.save_current_level():
        raise RuntimeError('Cannot save new validation map')
    unreal.log('GEOMETRY_MAP_CREATED: same material, no classification tags, ' + path)
