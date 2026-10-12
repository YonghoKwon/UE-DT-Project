"""MAP-01: create only two approved maps. Abort before any write if either exists.

UnrealEditor-Cmd ma0t10_dt.uproject -run=pythonscript -script=Scripts/setup_portable_sensor_maps.py
Does not save shared assets, settings, tables or existing maps.
"""
import unreal

paths = ['/Game/MA0T10/Maps/Tests/PortableSetupMap',
         '/Game/MA0T10/Maps/Tests/PortableNoSlabMap']
for path in paths:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError('Refusing to overwrite existing map: ' + path)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
classes = {}
for name in ['VirtualSensorToolRig', 'SlabTrackReferenceActor', 'SlabActor']:
    classes[name] = unreal.load_class(None, '/Script/ma0t10_dt.' + name)
    if not classes[name]:
        raise RuntimeError('Build Editor first: ' + name)
cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
if not cube:
    raise RuntimeError('Missing engine Cube')

for path in paths:
    if not levels.new_level(path):
        raise RuntimeError('Could not create ' + path)
    rig = actors.spawn_actor_from_class(classes['VirtualSensorToolRig'], unreal.Vector())
    rig.set_actor_label('Sensor Tools')
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -25))
    floor.static_mesh_component.set_static_mesh(cube)
    floor.set_actor_scale3d(unreal.Vector(200, 100, .5))
    floor.set_actor_label('Floor')
    if path == paths[0]:
        track = actors.spawn_actor_from_class(classes['SlabTrackReferenceActor'], unreal.Vector())
        slab = actors.spawn_actor_from_class(classes['SlabActor'], unreal.Vector(0, 0, 125))
        slab.set_editor_property('track_reference', track)
        slab.set_editor_property('dimension_unit', unreal.SlabInputUnit.MILLIMETERS)
        slab.set_editor_property('position_unit', unreal.SlabInputUnit.CENTIMETERS)
        rig.set_editor_property('target_slab', slab)
        actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(-60, 0, 20))
        actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3000))
    if not levels.save_current_level():
        raise RuntimeError('Could not save ' + path)
    unreal.log('PORTABLE_MAP_CREATED: ' + path)
