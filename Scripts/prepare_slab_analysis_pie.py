"""Session-only PIE window settings for manual evidence; never SaveConfig/save a map."""
import os
import unreal

settings = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.LevelEditorPlaySettings'))
settings.set_editor_property('NewWindowWidth', int(os.getenv('SLAB_PIE_WIDTH', '1280')))
settings.set_editor_property('NewWindowHeight', int(os.getenv('SLAB_PIE_HEIGHT', '720')))
settings.set_editor_property('CenterNewWindow', True)
unreal.log('SLAB_MANUAL_PIE_PREPARED: select New Editor Window and click Play manually; no config saved')
