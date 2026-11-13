"""LEGACY v1 creator. Current volume uses update_material.py and headache.hlsl.
Run with UE 5.3 PythonScript commandlet, with commandlet rendering enabled.
Creates only new assets; deliberately refuses to overwrite existing user assets.
"""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()) / 'Scripts/HeadachePostProcess'
DEST = '/Game/VTG/Materials/PostProcess/Headache'
L = u.MaterialEditingLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
names = ['M_PP_VTG_Headache', 'MI_PP_VTG_Headache_Light',
         'MI_PP_VTG_Headache_Medium', 'MI_PP_VTG_Headache_Heavy',
         'MI_PP_VTG_Headache_LowCost']
assert not any(u.EditorAssetLibrary.does_asset_exist(DEST + '/' + n) for n in names), 'Assets already exist; do not overwrite.'
m = tools.create_asset(names[0], DEST, u.Material, u.MaterialFactoryNew())
assert m
m.set_editor_property('material_domain', u.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property('blendable_location', u.BlendableLocation.BL_AFTER_TONEMAPPING)

def node(cls, x, y):
    return L.create_material_expression(m, cls, x, y)

def wire(src, dst, pin, output=''):
    assert L.connect_material_expressions(src, output, dst, pin), pin

def custom(file, inputs, x, y):
    n = node(u.MaterialExpressionCustom, x, y)
    n.set_editor_property('description', file)
    n.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
    pins = []
    for name, src in inputs.items():
        pin = u.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    n.set_editor_property('inputs', pins)
    n.set_editor_property('code', (ROOT / file).read_text())
    for name, src in inputs.items():
        wire(src, n, name)
    return n

params = {}
defaults = [
    ('Intensity', 0.0, '00 Control', 'Headache strength 0..1. Neutral default for blendable interpolation.'),
    ('GrainAmount', 0.10, '01 Dirty Grain', 'Shadow grain amplitude; start at 0.06..0.14.'),
    ('GrainPixels', 2.5, '01 Dirty Grain', 'Grain cell size in output pixels; minimum 1.'),
    ('GrainFPS', 18.0, '01 Dirty Grain', 'Grain refreshes per second.'),
    ('GhostAmount', 0.28, '02 Image Ghosting', 'Peak blend on high contrast edges, 0..1.'),
    ('GhostPixels', 2.0, '02 Image Ghosting', 'Spatial echo offset in output pixels.'),
    ('PressureAmount', 0.65, '03 Peripheral Pressure', 'Peripheral darkening, 0..1.'),
    ('ClearRadius', 0.50, '03 Peripheral Pressure', 'Protected central radius; 0.2..0.85.'),
]
for i, (name, value, group, desc) in enumerate(defaults):
    p = node(u.MaterialExpressionScalarParameter, -1500, i * 170)
    p.set_editor_property('parameter_name', name)
    p.set_editor_property('default_value', value)
    p.set_editor_property('group', group)
    p.set_editor_property('desc', desc)
    params[name] = p
tint = node(u.MaterialExpressionVectorParameter, -1500, 1400)
tint.set_editor_property('parameter_name', 'PressureTint')
tint.set_editor_property('default_value', u.LinearColor(0.25, 0.045, 0.065, 1.0))
tint.set_editor_property('group', '03 Peripheral Pressure')
scene = node(u.MaterialExpressionSceneTexture, -1100, -300)
scene.set_editor_property('scene_texture_id', u.SceneTextureId.PPI_POST_PROCESS_INPUT0)
clock = node(u.MaterialExpressionTime, -1100, -100)
clock.set_editor_property('ignore_pause', False)
ghost = custom('ghost.hlsl', dict(Base=scene, Clock=clock, **{
    k: params[k] for k in ['Intensity', 'GhostPixels', 'GhostAmount']}), -700, 0)
switch = node(u.MaterialExpressionStaticSwitchParameter, -350, 0)
switch.set_editor_property('parameter_name', 'EnableGhosting')
switch.set_editor_property('default_value', True)
switch.set_editor_property('group', '02 Image Ghosting')
wire(ghost, switch, 'True')
wire(scene, switch, 'False')
finish = custom('finish.hlsl', dict(Source=switch, Clock=clock, PressureTint=tint, **{
    k: params[k] for k in ['Intensity', 'GrainAmount', 'GrainPixels', 'GrainFPS', 'PressureAmount', 'ClearRadius']}), 50, 0)
assert L.connect_material_property(finish, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
L.recompile_material(m)
assert u.EditorAssetLibrary.save_loaded_asset(m, False)
instances = []
for name, intensity in zip(names[1:], [0.30, 0.60, 1.0, 0.60]):
    mi = tools.create_asset(name, DEST, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    assert mi
    L.set_material_instance_parent(mi, m)
    # UE 5.3's scalar setter always returns false; verify by reading the value back.
    L.set_material_instance_scalar_parameter_value(mi, 'Intensity', intensity)
    assert abs(L.get_material_instance_scalar_parameter_value(mi, 'Intensity') - intensity) < 1e-5
    if name.endswith('LowCost'):
        L.set_material_instance_static_switch_parameter_value(mi, 'EnableGhosting', False)
    L.update_material_instance(mi)
    assert L.get_material_instance_static_switch_parameter_value(mi, 'EnableGhosting') == (not name.endswith('LowCost'))
    assert u.EditorAssetLibrary.save_loaded_asset(mi, False)
    instances.append(mi)
report = {'assets': [x.get_path_name() for x in [m] + instances], 'defaults': {x[0]: x[1] for x in defaults},
          'design_scene_color_samples': {'standard': 2, 'low_cost': 1},
          'note': 'GPU milliseconds must be measured in target gameplay at target resolution.'}
for obj in [m] + instances:
    stats = L.get_statistics(obj)
    report[obj.get_name()] = {k: stats.get_editor_property(k) for k in
        ['num_pixel_shader_instructions', 'num_pixel_texture_samples', 'num_samplers']}
(ROOT / 'result.json').write_text(json.dumps(report, indent=2))
u.log('VTG_HEADACHE_CREATE_PASS ' + json.dumps(report))
