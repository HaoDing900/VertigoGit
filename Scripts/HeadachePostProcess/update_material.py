"""Create the single-control material and update the original preview instances."""
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()) / 'Scripts/HeadachePostProcess'
dest = '/Game/VTG/Materials/PostProcess/Headache/'
L = u.MaterialEditingLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
path = dest + 'M_PP_Headache'
m = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('M_PP_Headache', dest.rstrip('/'), u.Material, u.MaterialFactoryNew())
assert m
L.delete_all_material_expressions(m)
m.set_editor_property('material_domain', u.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property('blendable_location', u.BlendableLocation.BL_AFTER_TONEMAPPING)

def make(cls, x, y):
    return L.create_material_expression(m, cls, x, y)

scene = make(u.MaterialExpressionSceneTexture, -650, -180)
scene.set_editor_property('scene_texture_id', u.SceneTextureId.PPI_POST_PROCESS_INPUT0)
clock = make(u.MaterialExpressionTime, -650, 30)
clock.set_editor_property('ignore_pause', False)
strength = make(u.MaterialExpressionScalarParameter, -650, 200)
strength.set_editor_property('parameter_name', 'HeadacheIntensity')
strength.set_editor_property('default_value', 0.0)
strength.set_editor_property('slider_min', 0.0)
strength.set_editor_property('slider_max', 1.0)
strength.set_editor_property('group', 'Headache')
effect = make(u.MaterialExpressionCustom, -200, -100)
effect.set_editor_property('description', 'Analog headache: grain / double image / peripheral pressure')
effect.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
inputs = []
for name in ['Base', 'Clock', 'HeadacheIntensity']:
    pin = u.CustomInput()
    pin.set_editor_property('input_name', name)
    inputs.append(pin)
effect.set_editor_property('inputs', inputs)
effect.set_editor_property('code', (root / 'headache.hlsl').read_text())
for src, name in [(scene, 'Base'), (clock, 'Clock'), (strength, 'HeadacheIntensity')]:
    assert L.connect_material_expressions(src, '', effect, name)
assert L.connect_material_property(effect, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
L.recompile_material(m)
L.get_statistics(m)
assert u.EditorAssetLibrary.save_loaded_asset(m, False)

# Keep previous preview instances useful after this revision. The BP uses FullStrength.
for name, value in [('MI_PP_Headache_FullStrength', 1.0), ('MI_PP_VTG_Headache_Light', 0.4),
                    ('MI_PP_VTG_Headache_Medium', 0.7), ('MI_PP_VTG_Headache_Heavy', 1.0)]:
    p = dest + name
    mi = u.load_asset(p) if u.EditorAssetLibrary.does_asset_exist(p) else tools.create_asset(name, dest.rstrip('/'), u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    L.clear_all_material_instance_parameters(mi)
    L.set_material_instance_parent(mi, m)
    L.set_material_instance_scalar_parameter_value(mi, 'HeadacheIntensity', value)
    L.update_material_instance(mi)
    assert abs(L.get_material_instance_scalar_parameter_value(mi, 'HeadacheIntensity') - value) < 1e-5
    assert u.EditorAssetLibrary.save_loaded_asset(mi, False)
assert [str(n) for n in L.get_scalar_parameter_names(m)] == ['HeadacheIntensity']
u.log('VTG_HEADACHE_STRONG_MATERIAL_PASS')
