"""Run inside UE 5.3 with PythonScriptPlugin; only edits the named material/presets.

HighQuality defaults to True, preserving existing material instances. A backup
and verification report are written under Saved/MaterialQuality. Re-running
after installation refuses to overwrite the material or existing presets.
"""
import json
import hashlib
from pathlib import Path
import shutil
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT_DIR = ROOT / 'Saved/MaterialQuality'
ASSET_DIR = '/Game/EnvArt/EnvMaterial/Glass'
MATERIAL_PATH = ASSET_DIR + '/T_TransparentPlastic'
LIB = unreal.MaterialEditingLibrary


def stats(asset):
    value = LIB.get_statistics(asset)
    return {name: value.get_editor_property(name) for name in (
        'num_pixel_shader_instructions', 'num_vertex_shader_instructions',
        'num_samplers', 'num_pixel_texture_samples', 'num_vertex_texture_samples')}


def connect(source, target, pin, output=''):
    assert LIB.connect_material_expressions(source, output, target, pin), pin


def create(mat, cls, x, y, **properties):
    node = LIB.create_material_expression(mat, cls, x, y)
    assert node
    for name, value in properties.items():
        node.set_editor_property(name, value)
    return node


def main():
    mat = unreal.load_asset(MATERIAL_PATH)
    assert isinstance(mat, unreal.Material)
    assert 'HighQuality' not in [str(n) for n in LIB.get_static_switch_parameter_names(mat)], 'Already installed'
    for suffix in ['High', 'Low']:
        assert not unreal.EditorAssetLibrary.does_asset_exist(ASSET_DIR + '/MI_TransparentPlastic_' + suffix), 'Preset already exists'

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    backup = REPORT_DIR / 'T_TransparentPlastic.before_quality.uasset'
    source_file = ROOT / 'Content/EnvArt/EnvMaterial/Glass/T_TransparentPlastic.uasset'
    if backup.exists():
        assert hashlib.sha256(backup.read_bytes()).digest() == hashlib.sha256(source_file.read_bytes()).digest(), 'Material differs from backup; do not overwrite it'
    else:
        shutil.copy2(source_file, backup)
    report = {'material': MATERIAL_PATH, 'before': stats(mat), 'connections': {}}

    nodes = {node.get_name(): node for node in unreal.ObjectIterator(unreal.MaterialExpression) if node.get_outer() == mat}
    eye = LIB.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert isinstance(eye, unreal.MaterialExpressionEyeAdaptationInverse)
    eye_inputs = LIB.get_inputs_for_material_expression(mat, eye)
    detailed_glow = eye_inputs[0]
    assert detailed_glow == nodes['MaterialExpressionMultiply_1']
    plain_glow = nodes['MaterialExpressionMultiply_7']
    plain_inputs = LIB.get_inputs_for_material_expression(mat, plain_glow)
    assert {str(n.get_editor_property('parameter_name')) for n in plain_inputs} == {'BaseColor', 'GlowAmount'}

    low_opacity = create(mat, unreal.MaterialExpressionScalarParameter, 3712, 1536,
        parameter_name='LowQualityOpacity', default_value=0.10, group='Performance',
        slider_min=0.0, slider_max=1.0, desc='Constant opacity used only when HighQuality is disabled.')
    low_glow_scale = create(mat, unreal.MaterialExpressionScalarParameter, 3216, 640,
        parameter_name='LowQualityGlowScale', default_value=0.20, group='Performance',
        slider_min=0.0, slider_max=1.0,
        desc='Scales BaseColor x GlowAmount without Fresnel. Exposure compensation is retained.')
    low_glow = create(mat, unreal.MaterialExpressionMultiply, 3536, 752)
    connect(plain_glow, low_glow, 'A')
    connect(low_glow_scale, low_glow, 'B')
    neutral_ior = create(mat, unreal.MaterialExpressionConstant, 3712, 2176, r=1.0,
        desc='Neutral Index Of Refraction; this does not disable the translucency rendering pass.')
    low_specular = create(mat, unreal.MaterialExpressionConstant, 3712, 1856, r=0.5)

    def quality_switch(high, low, x, y, high_output=''):
        switch = create(mat, unreal.MaterialExpressionStaticSwitchParameter, x, y,
            parameter_name='HighQuality', default_value=True, group='Performance',
            desc='ON: original appearance. OFF: constant opacity/specular, simpler glow, neutral refraction.')
        pins = LIB.get_material_expression_input_names(switch)
        assert len(pins) == 2
        connect(high, switch, pins[0], high_output)
        connect(low, switch, pins[1])
        assert LIB.get_inputs_for_material_expression(mat, switch) == [high, low]
        return switch

    # Switch before exposure compensation, so both variants keep stable brightness.
    glow_switch = quality_switch(detailed_glow, low_glow, 3840, 992)
    eye_pin = LIB.get_material_expression_input_names(eye)[0]
    connect(glow_switch, eye, eye_pin)
    eye.set_editor_property('material_expression_editor_x', 4144)
    eye.set_editor_property('material_expression_editor_y', 1088)
    report['connections']['emissive'] = {'high': detailed_glow.get_name(), 'low': low_glow.get_name(), 'exposure_compensation': True}

    for prop, low, y in [
        (unreal.MaterialProperty.MP_OPACITY, low_opacity, 1408),
        (unreal.MaterialProperty.MP_SPECULAR, low_specular, 1728),
        (unreal.MaterialProperty.MP_REFRACTION, neutral_ior, 2048),
    ]:
        high = LIB.get_material_property_input_node(mat, prop)
        assert high
        output = LIB.get_material_property_input_node_output_name(mat, prop)
        switch = quality_switch(high, low, 4144, y, output)
        assert LIB.connect_material_property(switch, '', prop)
        report['connections'][str(prop)] = {'high': high.get_name(), 'low': low.get_name()}

    LIB.recompile_material(mat)
    assert LIB.get_material_default_static_switch_parameter_value(mat, 'HighQuality')
    report['high_default'] = stats(mat)
    presets = []
    for label, value in [('High', True), ('Low', False)]:
        preset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'MI_TransparentPlastic_' + label, ASSET_DIR, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        assert preset
        LIB.set_material_instance_parent(preset, mat)
        # UE 5.3 returns False unconditionally here; verify the resulting value below.
        LIB.set_material_instance_static_switch_parameter_value(preset, 'HighQuality', value)
        LIB.update_material_instance(preset)
        assert LIB.get_material_instance_static_switch_parameter_value(preset, 'HighQuality') == value
        report[label.lower()] = stats(preset)
        presets.append(preset)

    (REPORT_DIR / 'compile_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    # Do not save if a rendering-enabled invocation failed to produce valid shaders.
    for key in ['before', 'high_default', 'high', 'low']:
        assert report[key]['num_pixel_shader_instructions'] > 0, 'Shader statistics unavailable: ' + key
    assert report['high_default'] == report['before'], 'High-quality shader changed unexpectedly'
    assert report['high'] == report['before'], 'High preset differs from original'
    assert report['low']['num_pixel_shader_instructions'] < report['high']['num_pixel_shader_instructions'], 'No measured instruction saving'

    for asset in [mat] + presets:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    report['saved'] = [asset.get_path_name() for asset in [mat] + presets]
    (REPORT_DIR / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('MATERIAL_QUALITY_COMPLETE ' + json.dumps(report))


if __name__ == '__main__':
    main()
