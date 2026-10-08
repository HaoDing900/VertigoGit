import unreal, json, shutil, pathlib, datetime
root=pathlib.Path(unreal.Paths.project_dir())
asset='/Game/EnvArt/EnvMaterial/Sci-fi/M__ScifiPipe'
m=unreal.load_asset(asset)
assert m
L=unreal.MaterialEditingLibrary
assert 'TextureRotationDegrees' not in [str(x) for x in L.get_scalar_parameter_names(m)], 'Already installed'
source=root/'Content/EnvArt/EnvMaterial/Sci-fi/M__ScifiPipe.uasset'
backup=root/'Scripts/CodexMaterial/Backup'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
backup.mkdir(parents=True)
shutil.copy2(source,backup/source.name)
nodes={}
def visit(n):
    if not n or n.get_path_name() in nodes:return
    nodes[n.get_path_name()]=n
    for i in L.get_inputs_for_material_expression(m,n):visit(i)
for p in unreal.MaterialProperty:
    try:visit(L.get_material_property_input_node(m,p))
    except Exception:pass
samples=[n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionTextureSample)]
assert len(samples)==4
uv=L.get_inputs_for_material_expression(m,samples[0])[0]
assert uv and all(L.get_inputs_for_material_expression(m,s)[0]==uv for s in samples)
normal=L.get_material_property_input_node(m,unreal.MaterialProperty.MP_NORMAL)
assert normal in samples
normal_out=L.get_material_property_input_node_output_name(m,unreal.MaterialProperty.MP_NORMAL)
old_textures=sorted(t.get_path_name() for t in L.get_used_textures(m))
def make(cls,x,y):return L.create_material_expression(m,cls,x,y)
def connect(a,out,b,pin):assert L.connect_material_expressions(a,out,b,pin),pin
def custom_input(name):
    i=unreal.CustomInput(); i.set_editor_property('input_name',name); return i
angle=make(unreal.MaterialExpressionScalarParameter,-1400,500)
angle.set_editor_property('parameter_name','TextureRotationDegrees')
angle.set_editor_property('default_value',90.0)
angle.set_editor_property('group','Texture Rotation')
angle.set_editor_property('desc','Texture rotation in degrees. 90 = quarter turn. Negative values reverse the direction.')
rotate=make(unreal.MaterialExpressionCustom,-1100,200)
rotate.set_editor_property('description','Rotate UV around center (degrees)')
rotate.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT2)
rotate.set_editor_property('inputs',[custom_input('UV'),custom_input('Degrees')])
rotate.set_editor_property('code','float a = Degrees * 0.0174532925199433; float s, c; sincos(a, s, c); float2 p = UV - float2(0.5, 0.5); return float2(c*p.x-s*p.y, s*p.x+c*p.y) + float2(0.5, 0.5);')
connect(uv,'',rotate,'UV');connect(angle,'',rotate,'Degrees')
def switch(x,y):
    n=make(unreal.MaterialExpressionStaticSwitchParameter,x,y)
    n.set_editor_property('parameter_name','EnableTextureRotation')
    n.set_editor_property('default_value',False)
    n.set_editor_property('group','Texture Rotation')
    n.set_editor_property('desc','Enable shared texture UV rotation and matching tangent-space normal correction.')
    return n
sw=switch(-750,200)
connect(rotate,'',sw,'True');connect(uv,'',sw,'False')
for s in samples:connect(sw,'',s,'UVs')
# A rotated UV domain also requires the sampled tangent-space normal XY to rotate inversely.
nrotate=make(unreal.MaterialExpressionCustom,100,700)
nrotate.set_editor_property('description','Correct rotated tangent normal')
nrotate.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
nrotate.set_editor_property('inputs',[custom_input('Normal'),custom_input('Degrees')])
nrotate.set_editor_property('code','float a = Degrees * 0.0174532925199433; float s, c; sincos(a, s, c); return float3(c*Normal.x+s*Normal.y, -s*Normal.x+c*Normal.y, Normal.z);')
connect(normal,normal_out,nrotate,'Normal');connect(angle,'',nrotate,'Degrees')
nsw=switch(450,700)
connect(nrotate,'',nsw,'True');connect(normal,normal_out,nsw,'False')
assert L.connect_material_property(nsw,'',unreal.MaterialProperty.MP_NORMAL)
L.recompile_material(m)
assert all(L.get_inputs_for_material_expression(m,s)[0]==sw for s in samples)
assert sorted(t.get_path_name() for t in L.get_used_textures(m))==old_textures
assert not L.get_material_default_static_switch_parameter_value(m,'EnableTextureRotation')
assert L.get_material_default_scalar_parameter_value(m,'TextureRotationDegrees')==90.0
assert unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
report={'asset':asset,'backup':str(backup/source.name),'textures':old_textures,'angle_default':90,'enabled_default':False,'normal_correction':True}
(root/'Scripts/CodexMaterial/result.json').write_text(json.dumps(report,indent=2))
unreal.log('SCIFIPIPE_APPLY_PASS '+json.dumps(report))


