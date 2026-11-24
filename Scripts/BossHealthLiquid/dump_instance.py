import unreal
m = unreal.load_asset('/Game/Widget/BossHealthBar/M_BossHealth_Caustics')
mi = unreal.load_asset('/Game/Widget/BossHealthBar/MI_BossHealth_Caustics')
lib = unreal.MaterialEditingLibrary
for n in ('CausticGain', 'RimIntensity', 'SurfacePosition', 'CausticOpacity'):
    print('SCALAR', n, 'material=', lib.get_material_default_scalar_parameter_value(m, n),
          'instance=', lib.get_material_instance_scalar_parameter_value(mi, n))
for n in ('ShallowColor', 'DeepColor'):
    print('VECTOR', n, 'material=', lib.get_material_default_vector_parameter_value(m, n),
          'instance=', lib.get_material_instance_vector_parameter_value(mi, n))
names = [e.get_editor_property('parameter_name') for e in lib.get_material_selected_nodes(m)] if False else None
count = {}
for e in m.get_editor_property('expression_collection').get_editor_property('expressions'):
    try:
        pn = str(e.get_editor_property('parameter_name'))
    except Exception:
        continue
    count[pn] = count.get(pn, 0) + 1
print('DUPLICATES', {k: v for k, v in count.items() if v > 1})
print('PARAMCOUNT', len(count))
