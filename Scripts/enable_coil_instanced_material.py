"""COIL-01b: enable the existing owned coil material's instanced rendering usage only."""
import unreal
path='/Game/MA0T10/Coil/Materials/M_CoilSurface'
m=unreal.load_asset(path)
if not m: raise RuntimeError('Missing owned coil material')
unreal.MaterialEditingLibrary.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
if not unreal.MaterialEditingLibrary.has_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES):
    raise RuntimeError('Instanced usage was not enabled')
unreal.MaterialEditingLibrary.recompile_material(m)
if not unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False): raise RuntimeError('Owned material save failed')
unreal.log('COIL_QUAY_INSTANCING_ENABLED: '+path)
