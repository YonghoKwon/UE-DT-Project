"""Read the default test map's transport settings without playing or saving it."""
import json
from pathlib import Path
from urllib.parse import urlsplit
import unreal


def endpoint(value):
    parsed = urlsplit(str(value))
    return {"scheme": parsed.scheme, "host": parsed.hostname, "port": parsed.port}


world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/MA0T10/Maps/SensorTestMap')
if not world:
    raise RuntimeError('SensorTestMap could not be read')
records = []
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    for obj in [actor] + list(actor.get_components_by_class(unreal.ActorComponent)):
        values = {}
        for name in ('transport_mode', 'output_mode', 'auto_start_source', 'auto_start_topic_receivers', 'start_sensors_on_begin_play'):
            try:
                values[name] = str(obj.get_editor_property(name))
            except Exception:
                pass
        for name in ('http_endpoint', 'broker_url'):
            try:
                values[name] = endpoint(obj.get_editor_property(name))
            except Exception:
                pass
        try:
            profile = obj.get_editor_property('transport_profile')
            values['transport_profile'] = {
                key: endpoint(profile.get_editor_property(key)) for key in ('broker_url', 'http_endpoint')}
        except Exception:
            pass
        if values:
            records.append({'actor': actor.get_name(), 'object': obj.get_name(), 'class': obj.get_class().get_name(), 'settings': values})
result = {'map': world.get_path_name(), 'actorCount': len(actors), 'saved': False, 'transports': records}
output = Path(unreal.Paths.project_saved_dir()) / 'Reports' / 'LauncherPackageMapInspection.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, indent=2), encoding='utf-8')
unreal.log('Read-only packaging transport report: ' + str(output))
