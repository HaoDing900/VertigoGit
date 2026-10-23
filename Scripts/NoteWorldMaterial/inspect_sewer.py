import unreal, json
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/L_SewerUnderApartment')
rows=[]
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if 'L3Switch' in a.get_actor_label() or 'BPLM_Sewer' in a.get_class().get_name():
        r={'name':a.get_name(),'label':a.get_actor_label(),'class':a.get_class().get_path_name(),'location':str(a.get_actor_location())}
        for p in ['L3_SwitchColi','override_stage_for_testing','test_stage']:
            try:r[p]=str(a.get_editor_property(p))
            except:pass
        r['components']=[{'name':c.get_name(),'mobility':str(c.mobility),'location':str(c.get_world_location())} for c in a.get_components_by_class(unreal.SceneComponent)]
        rows.append(r)
open(unreal.Paths.project_dir()+'Scripts/NoteWorldMaterial/sewer-actors.json','w').write(json.dumps(rows,indent=2))
