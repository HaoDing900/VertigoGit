import unreal,pathlib,json
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/L2FightDiagnosis'
report={}
for name in ['FLS_L2BarStreet_AfterFight_Animation','LS_L2BarStreet_PreDrunkFight']:
 a=unreal.load_asset('/Game/LevelNSequence/In_Level/L2BarStreet/'+name)
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(name+'.t3d'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 rows=[]
 for b in a.get_bindings():
  tracks=[]
  for tr in b.get_tracks():
   sections=[]
   for s in tr.get_sections():
    r={'type':s.get_class().get_name()}
    for prop in ['completion_mode']:
     try:r[prop]=str(s.get_editor_property(prop))
     except:pass
    chs=[]
    for ch in s.get_all_channels():
     keys=[]
     for k in ch.get_keys():
      try: keys.append({'time':str(k.get_time()),'value':str(k.get_value())})
      except:pass
     chs.append({'name':str(ch.get_name()),'keys':keys})
    r['channels']=chs;sections.append(r)
   tracks.append({'type':tr.get_class().get_name(),'sections':sections})
  rows.append({'binding':b.get_name(),'id':str(b.get_id()),'tracks':tracks})
 report[name]=rows
 try:
  bp=a.get_editor_property('director_blueprint')
  if bp:
   t=unreal.AssetExportTask();t.object=bp;t.filename=str(out/(name+'_Director.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 except:pass
(out/'sequences.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
