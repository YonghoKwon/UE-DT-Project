"""COIL-01: one new, texture-free HR/CR surface material. No existing asset/map save."""
import unreal

root = '/Game/MA0T10/Coil/Materials'
name = 'M_CoilSurface'
path = root + '/' + name
updating = '-CoilMaterialUpdate' in unreal.SystemLibrary.get_command_line()
if unreal.EditorAssetLibrary.does_asset_exist(path) and not updating:
    raise RuntimeError('Refusing to overwrite existing material: ' + path)
material = unreal.load_asset(path) if updating else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, root, unreal.Material, unreal.MaterialFactoryNew())
if not material:
    raise RuntimeError('Cannot create approved coil surface')
edit = unreal.MaterialEditingLibrary
edit.delete_all_material_expressions(material)

def node(kind, x, y):
    return edit.create_material_expression(material, kind, x, y)

def scalar(label, value, y):
    expr = node(unreal.MaterialExpressionScalarParameter, -700, y)
    expr.set_editor_property('parameter_name', label)
    expr.set_editor_property('default_value', value)
    return expr

uv = node(unreal.MaterialExpressionTextureCoordinate, -900, 0)
vertex = node(unreal.MaterialExpressionVertexColor, -900, 100)
color = node(unreal.MaterialExpressionVectorParameter, -900, 200)
color.set_editor_property('parameter_name', 'ColdColor')
color.set_editor_property('default_value', unreal.LinearColor(.09, .115, .145, 1))
cold = scalar('ColdRolled', 0, 350)
metallic = scalar('Metallic', .35, 450)
rough = scalar('Roughness', .8, 550)
surface = node(unreal.MaterialExpressionCustom, -250, 0)
surface.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
surface.set_editor_property('code', '''
float2 p=UV*4;
float2 cell=floor(p), f=frac(p); f=f*f*(3-2*f);
float h=dot(cell,float2(127.1,311.7));
float4 n=frac(sin(float4(h,h+127.1,h+311.7,h+438.8))*43758.5453);
float patch=lerp(lerp(n.x,n.y,f.x),lerp(n.z,n.w,f.x),f.y);
float footprint=max(length(ddx(UV)),length(ddy(UV)));
float grain=(frac(sin(dot(floor(UV*250),float2(13.11,79.7)))*43758.5453)-.5)*saturate(.005/max(footprint,.0001));
float oxide=smoothstep(.74,.94,patch);
float3 hr=C.rgb*(.8+patch*.35+grain*.12);
hr=lerp(hr,float3(.18,.075,.032),oxide*.28);
// Representative6mm lap pitch, visual only. Derivative fading avoids aliasing.
float radius=length(UV), lap=abs(sin(radius*1047.19755));
float lapFade=saturate(.004/max(footprint,.0001));
hr*=1-V.r*(1-smoothstep(.0,.22,lap))*.48*lapFade;
float brushed=sin(UV.x*580)*saturate(.002/max(footprint,.0001));
float3 cr=C.rgb*(.98+grain*.025+brushed*.008);
return lerp(hr,cr,saturate(CR));
''')
inputs = []
for pin in ['UV', 'C', 'CR', 'V']:
    value = unreal.CustomInput()
    value.set_editor_property('input_name', pin)
    inputs.append(value)
surface.set_editor_property('inputs', inputs)
for source, pin in [(uv, 'UV'), (color, 'C'), (cold, 'CR'), (vertex, 'V')]:
    if not edit.connect_material_expressions(source, '', surface, pin):
        raise RuntimeError('Material input failed: ' + pin)
for source, prop in [(surface, unreal.MaterialProperty.MP_BASE_COLOR), (metallic, unreal.MaterialProperty.MP_METALLIC), (rough, unreal.MaterialProperty.MP_ROUGHNESS)]:
    if not edit.connect_material_property(source, '', prop):
        raise RuntimeError('Material output failed')
edit.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError('Cannot save owned coil material')
unreal.log('COIL_MATERIAL_CREATED: ' + path)
