"""UXR-09: reproducibly rebuild only the explicitly approved oxide-scale material.

UE 5.3: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>
Only M_SlabSurfaceOxideScale is saved. No new asset or map is created.
One non-tiling, single-level computational Gradient Noise supplies all variation.
The mesh-local pattern follows moving Slabs; no textures, time, WPO or bump pass.
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
noise = node(unreal.MaterialExpressionNoise, -1400, 0)
noise.set_editor_property('noise_function', unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
noise.set_editor_property('scale', .095)
noise.set_editor_property('levels', 1)
noise.set_editor_property('quality', 1)
noise.set_editor_property('tiling', False)
noise.set_editor_property('turbulence', False)
noise.set_editor_property('output_min', 0.)
noise.set_editor_property('output_max', 1.)
link(local, noise, 'Position')

# Reuse the same noise for grey-brown/blue-grey mottling and sharp peeled patches.
oxide_color = lerp(color('ColdColor', (.006, .009, .014, 1), -1300, -400),
                   color('ScaleBlueColor', (.018, .026, .038, 1), -1300, -250), noise, -950, -350)
subtract = node(unreal.MaterialExpressionSubtract, -1200, 200)
subtract.set_editor_property('const_b', .69)
link(noise, subtract, 'A')
contrast = node(unreal.MaterialExpressionMultiply, -1000, 200)
contrast.set_editor_property('const_b', 22.)
link(subtract, contrast, 'A')
peeled = node(unreal.MaterialExpressionSaturate, -800, 200)
link(contrast, peeled, '')
coated = node(unreal.MaterialExpressionOneMinus, -600, 200)
link(peeled, coated, '')
coverage = multiply(coated, scalar('Oxidation', .95, -800, 450), -400, 200)
cold = lerp(color('ExposedSteelColor', (.22, .27, .32, 1), -800, -550), oxide_color, coverage, -200, -350)
hotness = scalar('Hotness', 0., -200, -700)
base = lerp(cold, color('HotColor', (1., .18, .018, 1), -200, -850), hotness, 100, -350)
emissive = multiply(multiply(base, hotness, 350, -200), scalar('EmissiveStrength', 0., 100, 0), 550, -100)
rough = lerp(scalar('ExposedRoughness', .78, -100, 450), scalar('Roughness', .96, -100, 600), coverage, 300, 400)
metal = lerp(scalar('ExposedMetallic', .05, -100, 800), scalar('Metallic', 0., -100, 950), coverage, 300, 750)
for expression, prop in [(base, unreal.MaterialProperty.MP_BASE_COLOR),
                         (rough, unreal.MaterialProperty.MP_ROUGHNESS),
                         (metal, unreal.MaterialProperty.MP_METALLIC),
                         (emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)]:
    if not M.connect_material_property(expression, '', prop):
        raise RuntimeError('Material output connection failed: ' + str(prop))

asset.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
M.recompile_material(asset)
if M.get_used_textures(asset):
    raise RuntimeError('Unexpected texture dependency; refusing to save')
if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
    raise RuntimeError('New material save failed: ' + PATH)
unreal.log('UXR09_OXIDE_UPDATED: ' + PATH + ' (only this approved package saved)')
