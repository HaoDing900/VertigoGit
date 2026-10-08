import unreal, json
m = unreal.load_asset('/Game/EnvArt/EnvMaterial/Sci-fi/M__ScifiPipe')
assert m
lib = unreal.MaterialEditingLibrary
nodes = {}
def visit(n):
    if not n or n.get_path_name() in nodes: return
    entry = {'path':n.get_path_name(), 'class':n.get_class().get_name(), 'inputs':list(lib.get_material_expression_input_names(n))}
    nodes[n.get_path_name()] = entry
    for p in ['parameter_name','texture','coordinates','coordinate_index','material_function','desc']:
        try: entry[p] = str(n.get_editor_property(p))
        except Exception: pass
    ins = lib.get_inputs_for_material_expression(m,n)
    entry['sources'] = [x.get_path_name() if x else None for x in ins]
    for x in ins: visit(x)
for p in unreal.MaterialProperty:
    try: visit(lib.get_material_property_input_node(m,p))
    except Exception: pass
with open(unreal.Paths.project_dir()+'Scripts/CodexMaterial/inspect.json','w') as f: json.dump(nodes,f,indent=2)
unreal.log('SCIFIPIPE_INSPECT '+str(len(nodes)))
