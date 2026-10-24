import unreal,pathlib,json,re
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/DroneFacing'
paths=['/Game/Characters/Drone_IPHA/BP_Drone_IPHA_NPC','/Game/Characters/Drone_IPHA/BP_Drone_IPHA_StillNPC','/Game/Characters/Drone_IPHA/ABP_DroneIPHA']
seen=set()
while paths:
 p=paths.pop(0)
 if p in seen: continue
 seen.add(p)
 a=unreal.load_asset(p)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 if isinstance(a,unreal.Blueprint):
  cls=a.generated_class()
  parent=unreal.get_default_object(cls).get_class()
  cdo=unreal.get_default_object(cls)
  lines=[str(cls)]
  for n in ['mesh','character_movement']:
   try:
    ob=cdo.get_editor_property(n);lines.append(str(ob))
    for prop in ['relative_rotation','relative_location','orient_rotation_to_movement','use_controller_desired_rotation','anim_class']:
     try: lines.append(prop+'='+str(ob.get_editor_property(prop)))
     except: pass
   except: pass
  (out/(a.get_name()+'.txt')).write_text('\n'.join(lines),encoding='utf8')
  text=(out/(a.get_name()+'.copy')).read_text(encoding='utf8')
  m=re.search(r'ParentClass=.*?(/Game/[^\x27]+)',text)
  if m: paths.append(m.group(1).split('.')[0])
unreal.log('DRONE_INSPECT_DONE')
