"""HATCH-06: rebuild only the explicitly approved single cargo material.

Only the approved M_HatchCargo is saved; no map is saved. Shared iron/coal MID parameters, procedural
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
    for entry in inputs:
        name, source = entry[:2]
        p = unreal.CustomInput(); p.set_editor_property('input_name', name); pins.append(p)
    n.set_editor_property('inputs', pins)
    for entry in inputs: link(entry[1], n, entry[0], entry[2] if len(entry) > 2 else '')
    return n

pos = node(unreal.MaterialExpressionWorldPosition, -1200, 0)
coal = scalar('Coal', 0, -1200, 220)
tank = scalar('TankTop', 0, -1200, 320)
hardware = scalar('Hardware', 0, -1200, 420)
clast = custom('''
if(Hardware>.5 || TankTop>.5) return float3(1,.5,0);
// Screen derivatives attenuate grain once it is below a pixel (shader LOD).
float footprint=max(length(ddx(P)),length(ddy(P)));
float2 q=P.xy/lerp(3.2,6.0,Coal); float2 g=floor(q); float2 f=frac(q);
float d1=9,d2=9,id=0;
[unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) {
 float2 c=float2(x,y); float2 h=frac(sin(float2(dot(g+c,float2(127.1,311.7)),dot(g+c,float2(269.5,183.3))))*43758.5453);
 float2 r=c+h-f; float d=dot(r,r);
 if(d<d1){d2=d1;d1=d;id=h.x;}else d2=min(d2,d);
}
float edge=smoothstep(0.018,0.13,d2-d1);
float fine=lerp(.5,frac(sin(dot(floor(P*2),float3(12.9898,78.233,37.719)))*43758.5453),saturate(2-footprint));
float face=saturate(1-sqrt(d1));
// A second18-28cm aggregate scale survives the wide hold camera; tiny grains
// fade by screen derivatives instead of turning into a distant noise shimmer.
float2 coarse=P.xy/lerp(18.,28.,Coal),cg=floor(coarse),cf=frac(coarse);
float cd=9,cid=0;
[unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) {
 float2 k=float2(x,y);float2 h=frac(sin(float2(dot(cg+k,float2(127.1,311.7)),dot(cg+k,float2(269.5,183.3))))*43758.5453);
 float2 r=k+h-cf;float dist=lerp(dot(r,r),max(abs(r.x),abs(r.y)),Coal*.65);
 if(dist<cd){cd=dist;cid=h.x;}
}
float largeFade=saturate(lerp(18.,28.,Coal)/max(footprint*1.5,1.));
float macro=.5+.25*sin(P.x*.007+sin(P.y*.009)*2)+.25*sin(P.y*.013+P.x*.003);
float wet=saturate(macro*.8+cid*.25);
float dust=smoothstep(.55,.8,.5+.5*sin(P.x*.032+sin(P.y*.018)*4));
float fineFade=saturate(lerp(3.2,6.0,Coal)/max(footprint*1.4,1.));
float pellet=lerp(face*face,face,Coal);
float height=lerp(.5,edge*(.3+pellet*.7),fineFade);
height+=largeFade*(saturate(1-sqrt(cd))-.5)*.35;
float tone=lerp(1.,(0.55+id*.65)*(.67+edge*.33)+(fine-.5)*.12,fineFade);
tone*=lerp(1.,.79+cid*.38,largeFade);
tone=lerp(tone,tone*.86+.12,dust*.20);
return float3(tone,height,wet);
''', [('P', pos), ('Coal', coal), ('Hardware', hardware), ('TankTop', tank)], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -900, 0)
vertex = node(unreal.MaterialExpressionVertexColor, -900, 700)
base = custom('''
float3 dry=lerp(C,float3(.12,.105,.083),saturate(G.z*.40)*(1-Coal));
float fresh=saturate(Fresh);
float3 ore=dry*G.x*V.rgb*lerp(1.04,.68,G.z)*lerp(1.,.82,fresh);
// Hardware stays in the same existing material. World-space weathering has
// no texture upload, WPO, extra capture or collision cost.
float2 cell=floor(P.xy/45.);float stain=frac(sin(dot(cell,float2(12.9898,78.233)))*43758.5453);
float streak=smoothstep(.75,.96,stain)*(.5+.5*sin(P.z*.013+stain*7));
float top=saturate(N.z);float teal=saturate((V.g-V.r)*8);
float paint=smoothstep(.50,.91,.5+.25*sin(P.x*.003+P.y*.005)+.25*sin(P.y*.009-P.z*.004));
float3 hardwareColor=V.rgb*lerp(.87,1.03,paint);
hardwareColor=lerp(hardwareColor,float3(.16,.065,.026),streak*lerp(.07,.22,teal)*(1-top*.7));
hardwareColor=lerp(hardwareColor,hardwareColor*.75+float3(.055,.048,.033),top*paint*.24);
// The blue water section shares this material for batching, but is not paint
// or concrete. Do not stamp metal rust/dust cells across the sea.
float water=saturate((V.b-V.g-.008)*100)*saturate((V.b-.07)*50);
hardwareColor=lerp(hardwareColor,V.rgb,water);
float2 seam=abs(frac((P.xy+float2(23,37))/float2(240,160))-.5);
float weld=1-smoothstep(.003,.012,min(seam.x,seam.y));
float rust=smoothstep(.65,.9,frac(sin(dot(floor(P.xy/35),float2(12.98,78.23)))*43758.54));
// Preserve tank-top chroma under the higher exposure of a dark coal hold.
float3 steel=lerp(float3(.065,.18,.30),float3(.24,.15,.085),rust*.28);
return lerp(lerp(ore,steel*(1-.23*weld),TankTop),hardwareColor,Hardware);
''', [('C', color('ColdColor', (.20,.07,.04,1), -900, 350)), ('G', clast), ('V', vertex), ('Fresh', vertex, 'A'), ('P', pos), ('N', node(unreal.MaterialExpressionVertexNormalWS, -1200, 600)), ('Coal', coal), ('TankTop', tank), ('Hardware', hardware)], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -480, 0)
normal = custom('''
float3 dp1=ddx(P),dp2=ddy(P),r1=cross(dp2,N),r2=cross(N,dp1);
float det=dot(dp1,r1);float height=G.y+Fresh*.08*(1-Hardware)*sin(P.x*.17+sin(P.y*.09)*2);
float3 grad=(r1*ddx(height)+r2*ddy(height))/max(abs(det),1e-6)*sign(det);
return normalize(N-grad*1.5*(1-TankTop));
''', [('P', pos), ('N', node(unreal.MaterialExpressionVertexNormalWS, -900, 520)), ('G', clast), ('Fresh', vertex, 'A'), ('Hardware', hardware), ('TankTop', tank)], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -480, 200)
rough = custom('return lerp(lerp(.87,.63,Coal)-G.z*.19-Fresh*.06,lerp(.65,.78,Hardware),max(TankTop,Hardware));', [('Coal', coal), ('G', clast), ('Fresh', vertex, 'A'), ('TankTop', tank), ('Hardware', hardware)], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -480, 400)
for source, prop in [(base, unreal.MaterialProperty.MP_BASE_COLOR), (normal, unreal.MaterialProperty.MP_NORMAL), (rough, unreal.MaterialProperty.MP_ROUGHNESS), (scalar('Metallic', .06, -480, 600), unreal.MaterialProperty.MP_METALLIC)]:
    if not M.connect_material_property(source, '', prop): raise RuntimeError('HATCH03_MATERIAL_OUTPUT')
M.recompile_material(asset)
if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
    raise RuntimeError('HATCH03_MATERIAL_SAVE_FAILED')
unreal.log('HATCH03_MATERIAL_SAVED ' + ROOT + '/' + NAME)
