"""Read-only validation of the current single-control headache material."""
import json
from pathlib import Path
import unreal as u
L = u.MaterialEditingLibrary
dest = '/Game/VTG/Materials/PostProcess/Headache/'
m = u.load_asset(dest + 'M_PP_Headache')
assert m
assert m.get_editor_property('material_domain') == u.MaterialDomain.MD_POST_PROCESS
assert m.get_editor_property('blendable_location') == u.BlendableLocation.BL_AFTER_TONEMAPPING
assert L.get_material_property_input_node(m, u.MaterialProperty.MP_EMISSIVE_COLOR)
assert [str(n) for n in L.get_scalar_parameter_names(m)] == ['HeadacheIntensity']
assert L.get_material_default_scalar_parameter_value(m, 'HeadacheIntensity') == 0.0
for name, value in [('MI_PP_Headache_FullStrength', 1.0), ('MI_PP_VTG_Headache_Light', 0.4),
                    ('MI_PP_VTG_Headache_Medium', 0.7), ('MI_PP_VTG_Headache_Heavy', 1.0)]:
    mi = u.load_asset(dest + name)
    assert mi.get_editor_property('parent') == m
    assert abs(L.get_material_instance_scalar_parameter_value(mi, 'HeadacheIntensity') - value) < 1e-5
    L.get_statistics(mi)
assert u.load_class(None, '/Game/VTG/Blueprints/BP_HeadachePostProcess.BP_HeadachePostProcess_C')
stats = L.get_statistics(m)
report = {'blueprint': '/Game/VTG/Blueprints/BP_HeadachePostProcess',
          'material': m.get_path_name(), 'scalar_parameter': 'HeadacheIntensity',
          'default_volume_intensity': 1.0, 'default_parent_intensity': 0.0,
          'design_scene_color_samples': 2, 'tick': False, 'scope': 'Editor-preview volume only',
          'material_statistics': {k: stats.get_editor_property(k) for k in
              ['num_pixel_shader_instructions', 'num_pixel_texture_samples', 'num_samplers']},
          'note': 'Custom HLSL sample counts are not fully represented by MaterialStatistics. GPU time is not benchmarked.'}
(Path(u.Paths.project_dir()) / 'Scripts/HeadachePostProcess/result.json').write_text(json.dumps(report, indent=2))
u.log('VTG_HEADACHE_SAVED_ASSETS_VERIFY_PASS ' + json.dumps(report))
