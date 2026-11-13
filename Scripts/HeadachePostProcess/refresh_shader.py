"""Refresh only the existing Custom node, preserving instance overrides and Blueprint settings."""
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()) / 'Scripts/HeadachePostProcess'
L = u.MaterialEditingLibrary
m = u.load_asset('/Game/VTG/Materials/PostProcess/Headache/M_PP_Headache')
assert m
effect = L.get_material_property_input_node(m, u.MaterialProperty.MP_EMISSIVE_COLOR)
assert isinstance(effect, u.MaterialExpressionCustom)
assert [str(n) for n in L.get_scalar_parameter_names(m)] == ['HeadacheIntensity']
effect.set_editor_property('code', (root / 'headache.hlsl').read_text())
L.recompile_material(m)
stats = L.get_statistics(m)
assert u.EditorAssetLibrary.save_loaded_asset(m, False)
assert L.get_material_default_scalar_parameter_value(m, 'HeadacheIntensity') == 0.0
u.log('VTG_HEADACHE_RED_GHOST_PASS instructions=' + str(stats.get_editor_property('num_pixel_shader_instructions')))
