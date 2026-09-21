"""Manual readability fixture: adjusts only the transient PIE copy, never saves a map."""
import os
import time
import unreal

settings = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.LevelEditorPlaySettings'))
settings.set_editor_property('NewWindowWidth', int(os.getenv('SLAB_PIE_WIDTH', '1280')))
settings.set_editor_property('NewWindowHeight', int(os.getenv('SLAB_PIE_HEIGHT', '720')))
settings.set_editor_property('CenterNewWindow', True)
deadline = time.monotonic() + 180
handle = None

def configure_pie(_delta):
    global handle
    world = unreal.EditorLevelLibrary.get_game_world()
    if world and 'UEDPIE_' in world.get_path_name() and 'SlabScenarioValidationMap' in world.get_path_name():
        for sun in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight):
            sun.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(100000.0)
        unreal.log('SLAB_BRIGHT_PIE_READY: 100000 lux, transient PIE only; no exposure/map save')
        unreal.unregister_slate_post_tick_callback(handle)
    elif time.monotonic() > deadline:
        unreal.log_warning('SLAB_BRIGHT_PIE_NOT_STARTED: manual Play required within 180 seconds')
        unreal.unregister_slate_post_tick_callback(handle)

handle = unreal.register_slate_post_tick_callback(configure_pie)
unreal.log('SLAB_BRIGHT_PIE_ARMED: click New Editor Window Play manually')
