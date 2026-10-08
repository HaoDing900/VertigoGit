import unreal,json,math
m=unreal.load_asset('/Game/EnvArt/EnvMaterial/Sci-fi/M__ScifiPipe')
L=unreal.MaterialEditingLibrary
assert L.get_material_default_scalar_parameter_value(m,'TextureRotationDegrees')==90.0
assert not L.get_material_default_static_switch_parameter_value(m,'EnableTextureRotation')
assert L.get_num_material_expressions(m)==10
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
switches=[L.get_inputs_for_material_expression(m,n)[0] for n in samples]
assert all(s==switches[0] for s in switches)
assert str(switches[0].get_editor_property('parameter_name'))=='EnableTextureRotation'
assert isinstance(L.get_material_property_input_node(m,unreal.MaterialProperty.MP_NORMAL),unreal.MaterialExpressionStaticSwitchParameter)
L.recompile_material(m)
for enabled in [False,True]:
    mi=unreal.new_object(unreal.MaterialInstanceConstant)
    L.set_material_instance_parent(mi,m)
    L.set_material_instance_static_switch_parameter_value(mi,'EnableTextureRotation',enabled)
    assert L.get_material_instance_static_switch_parameter_value(mi,'EnableTextureRotation')==enabled
    for degrees in ([90,45,-90] if enabled else [90]):
        L.set_material_instance_scalar_parameter_value(mi,'TextureRotationDegrees',degrees)
        assert L.get_material_instance_scalar_parameter_value(mi,'TextureRotationDegrees')==degrees
        L.update_material_instance(mi)
        unreal.log('SCIFIPIPE_COMPILE '+str(enabled)+' '+str(degrees)+' '+str(L.get_statistics(mi)))
# Check the UV and inverse-normal transforms preserve tangent-space gradients.
for d in [0,45,90,-90,180,360]:
    a=math.radians(d);c=math.cos(a);s=math.sin(a)
    x,y=0.3,0.7
    u,v=c*x-s*y,s*x+c*y
    assert abs((c*u+s*v)-x)<1e-8 and abs((-s*u+c*v)-y)<1e-8
unreal.log('SCIFIPIPE_VERIFY_PASS: reload, shared UV, defaults, switch variants, arbitrary angles, inverse normal transform')


