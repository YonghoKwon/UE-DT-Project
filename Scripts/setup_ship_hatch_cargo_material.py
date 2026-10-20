"""HATCH-03: create/rebuild only the explicitly approved single cargo material.

No map or existing material is saved. Shared iron/coal MID parameters, procedural
angular clasts and fine grain; no imported textures, WPO or measurement geometry.
"""
import unreal

ROOT = '/Game/MA0T10/Hatch/Materials'
NAME = 'M_HatchCargo'
M = unreal.MaterialEditingLibrary
asset = unreal.load_asset(ROOT + '/' + NAME)
if not asset:
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, ROOT, unreal.Material, unreal.MaterialFactoryNew())
if not asset:
    raise RuntimeError('HATCH03_MATERIAL_CREATE_FAILED')
M.delete_all_material_expressions(asset)
asset.set_editor_property('tangent_space_normal', False)

def node(kind, x, y):
    return M.create_material_expression(asset, kind, x, y)

def link(a, b, pin, out=''):
    if not M.connect_material_expressions(a, out, b, pin):
        raise RuntimeError('HATCH03_MATERIAL_PIN: ' + pin)

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

def custom(code, inputs, output, x, y):
    n = node(unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property('code', code)
    n.set_editor_property('output_type', output)
    pins = []
    for name, source in inputs:
        p = unreal.CustomInput(); p.set_editor_property('input_name', name); pins.append(p)
    n.set_editor_property('inputs', pins)
    for name, source in inputs: link(source, n, name)
    return n

pos = node(unreal.MaterialExpressionWorldPosition, -1200, 0)
coal = scalar('Coal', 0, -1200, 220)
clast = custom('''
float2 q=P.xy/17.0; float2 g=floor(q); float2 f=frac(q);
float d1=9,d2=9,id=0;
[unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) {
 float2 c=float2(x,y); float2 h=frac(sin(float2(dot(g+c,float2(127.1,311.7)),dot(g+c,float2(269.5,183.3))))*43758.5453);
 float2 r=c+h-f; float d=dot(r,r);
 if(d<d1){d2=d1;d1=d;id=h.x;}else d2=min(d2,d);
}
float edge=smoothstep(0.018,0.13,d2-d1);
float fine=frac(sin(dot(floor(P*2),float3(12.9898,78.233,37.719)))*43758.5453);
float face=saturate(1-sqrt(d1));
return float3((0.52+id*0.65)*(0.7+edge*0.3)+(fine-.5)*.12,edge*(.35+face*.65)+fine*.08,id);
''', [('P', pos)], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -900, 0)
vertex = node(unreal.MaterialExpressionVertexColor, -900, 700)
base = custom('return C*G.x*V.rgb;', [('C', color('ColdColor', (.32,.075,.035,1), -900, 350)), ('G', clast), ('V', vertex)], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -480, 0)
normal = custom('''
float3 dp1=ddx(P),dp2=ddy(P),r1=cross(dp2,N),r2=cross(N,dp1);
float det=dot(dp1,r1);float3 grad=(r1*ddx(G.y)+r2*ddy(G.y))/max(abs(det),1e-6)*sign(det);
return normalize(N-grad*1.8);
''', [('P', pos), ('N', node(unreal.MaterialExpressionVertexNormalWS, -900, 520)), ('G', clast)], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -480, 200)
rough = custom('return lerp(.72,.42,Coal)+G.z*.18;', [('Coal', coal), ('G', clast)], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -480, 400)
for source, prop in [(base, unreal.MaterialProperty.MP_BASE_COLOR), (normal, unreal.MaterialProperty.MP_NORMAL), (rough, unreal.MaterialProperty.MP_ROUGHNESS), (scalar('Metallic', .06, -480, 600), unreal.MaterialProperty.MP_METALLIC)]:
    if not M.connect_material_property(source, '', prop): raise RuntimeError('HATCH03_MATERIAL_OUTPUT')
M.recompile_material(asset)
if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
    raise RuntimeError('HATCH03_MATERIAL_SAVE_FAILED')
unreal.log('HATCH03_MATERIAL_SAVED ' + ROOT + '/' + NAME)
