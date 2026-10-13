"""UXR-08: reproducibly rebuild only the existing oxide-scale material.

UE 5.3: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>
Only M_SlabSurfaceOxideScale is saved. No new asset or map is created.
Mesh-local multi-octave engine ALU noise: small torn flakes, fine cracks and grain.
No textures, time, WPO, new packages or map writes. Colour inputs are linear.
"""
from pathlib import Path
import unreal

ROOT = '/Game/MA0T10/Slab/Materials'
NAME = 'M_SlabSurfaceOxideScale'
PATH = ROOT + '/' + NAME
disk = Path(unreal.Paths.project_content_dir()) / 'MA0T10/Slab/Materials' / (NAME + '.uasset')
if not disk.exists() or not unreal.EditorAssetLibrary.does_asset_exist(PATH):
    raise RuntimeError('UXR09_OXIDE_EXPECT_EXISTING_APPROVED_ASSET: ' + PATH)

M = unreal.MaterialEditingLibrary
asset = unreal.load_asset(PATH)
if not asset:
    raise RuntimeError('Material load failed: ' + PATH)
M.delete_all_material_expressions(asset)


def node(kind, x, y):
    result = M.create_material_expression(asset, kind, x, y)
    if not result:
        raise RuntimeError('Expression creation failed: ' + str(kind))
    return result


def scalar(name, value, x, y):
    result = node(unreal.MaterialExpressionScalarParameter, x, y)
    result.set_editor_property('parameter_name', name)
    result.set_editor_property('default_value', value)
    return result


def color(name, value, x, y):
    result = node(unreal.MaterialExpressionVectorParameter, x, y)
    result.set_editor_property('parameter_name', name)
    result.set_editor_property('default_value', unreal.LinearColor(*value))
    return result


def link(source, target, pin):
    if not M.connect_material_expressions(source, '', target, pin):
        raise RuntimeError('Material pin connection failed: ' + pin)


def lerp(a, b, alpha, x, y):
    result = node(unreal.MaterialExpressionLinearInterpolate, x, y)
    link(a, result, 'A'); link(b, result, 'B'); link(alpha, result, 'Alpha')
    return result


def multiply(a, b, x, y):
    result = node(unreal.MaterialExpressionMultiply, x, y)
    link(a, result, 'A'); link(b, result, 'B')
    return result


position = node(unreal.MaterialExpressionWorldPosition, -1800, 0)
local = node(unreal.MaterialExpressionTransformPosition, -1600, 0)
local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
link(position, local, '')
direction = multiply(local, color('GrainDirection', (1., .72, 1.15, 1), -1750, 220), -1500, 150)


def engine_noise(scale, levels, x, y):
    result = node(unreal.MaterialExpressionNoise, x, y)
    for key, value in dict(noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU,
                           scale=scale, levels=levels, quality=1, tiling=False,
                           turbulence=False, output_min=0., output_max=1.).items():
        result.set_editor_property(key, value)
    link(direction, result, 'Position')
    return result


def custom(code, inputs, output, x, y):
    result = node(unreal.MaterialExpressionCustom, x, y)
    result.set_editor_property('code', code)
    result.set_editor_property('output_type', output)
    pins = []
    for name, source in inputs:
        pin = unreal.CustomInput(); pin.set_editor_property('input_name', name); pins.append(pin)
    result.set_editor_property('inputs', pins)
    for name, source in inputs: link(source, result, name)
    return result


noise = engine_noise(.48, 3, -1250, 0)
grain = engine_noise(2.4, 1, -1250, 180)
peeled = custom('return smoothstep(0.67,0.69,N + (G-0.5)*0.075);',
                [('N', noise), ('G', grain)], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -950, 100)
cracks = custom('return (1-smoothstep(0.004,0.014,abs(N-0.47))) * smoothstep(0.34,0.58,G);',
                [('N', noise), ('G', grain)], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -950, 250)
# About 0.06--0.10 display-referred dark scale, 0.35--0.45 exposed steel.
# Convert these dark authoring colours to linear to avoid the former white islands.
oxide_color = lerp(color('ColdColor', (.005, .007, .009, 1), -1300, -450),
                   color('ScaleBlueColor', (.009, .012, .016, 1), -1300, -300), grain, -950, -350)
rust = custom('return smoothstep(0.66,0.78,G)*(1-P)*0.16;', [('G', grain), ('P', peeled)],
              unreal.CustomMaterialOutputType.CMOT_FLOAT1, -900, -180)
oxide_color = lerp(oxide_color, color('TraceRustColor', (.026, .012, .006, 1), -850, -500), rust, -600, -350)
oxide_color = lerp(oxide_color, color('CrackColor', (.002, .0025, .003, 1), -600, -550), cracks, -350, -350)
coated = node(unreal.MaterialExpressionOneMinus, -600, 200)
link(peeled, coated, '')
coverage = multiply(coated, scalar('Oxidation', .95, -800, 450), -400, 200)
cold = lerp(color('ExposedSteelColor', (.13, .145, .16, 1), -800, -700), oxide_color, coverage, -200, -350)
hotness = scalar('Hotness', 0., -200, -700)
base = lerp(cold, color('HotColor', (1., .18, .018, 1), -200, -850), hotness, 100, -350)
emissive = multiply(multiply(base, hotness, 350, -200), scalar('EmissiveStrength', 0., 100, 0), 550, -100)
rough_base = lerp(scalar('ExposedRoughness', .32, -100, 450), scalar('Roughness', .72, -100, 600), coverage, 300, 400)
rough = custom('return saturate(R+(G-0.5)*0.16+C*0.12);', [('R', rough_base), ('G', grain), ('C', cracks)],
               unreal.CustomMaterialOutputType.CMOT_FLOAT1, 500, 400)
metal = lerp(scalar('ExposedMetallic', .82, -100, 800), scalar('Metallic', .22, -100, 950), coverage, 300, 750)
height = custom('return (G*0.025-C*0.035)*(1-P*0.7);', [('G', grain), ('C', cracks), ('P', peeled)],
                unreal.CustomMaterialOutputType.CMOT_FLOAT1, 200, 1100)
normal = custom('float3 dx=ddx(P),dy=ddy(P); float3 r1=cross(dy,N),r2=cross(N,dx); '
                'float det=dot(dx,r1); float3 grad=(ddx(H)*r1+ddy(H)*r2)*sign(det)/max(abs(det),0.00001); '
                'return normalize(N-grad);',
                [('P', position), ('N', node(unreal.MaterialExpressionVertexNormalWS, 0, 1250)), ('H', height)],
                unreal.CustomMaterialOutputType.CMOT_FLOAT3, 500, 1100)
for expression, prop in [(base, unreal.MaterialProperty.MP_BASE_COLOR),
                         (rough, unreal.MaterialProperty.MP_ROUGHNESS),
                         (metal, unreal.MaterialProperty.MP_METALLIC),
                         (emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                         (normal, unreal.MaterialProperty.MP_NORMAL)]:
    if not M.connect_material_property(expression, '', prop):
        raise RuntimeError('Material output connection failed: ' + str(prop))

asset.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
asset.set_editor_property('tangent_space_normal', False)
M.recompile_material(asset)
if M.get_used_textures(asset):
    raise RuntimeError('Unexpected texture dependency; refusing to save')
if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
    raise RuntimeError('New material save failed: ' + PATH)
unreal.log('UXR08_OXIDE_UPDATED: ' + PATH + ' (only this existing package saved)')
