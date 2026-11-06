import unreal,re,os
raw=open(unreal.Paths.project_dir()+'Scripts/PoliceContractAudio/Dialogue.copy','rb').read();s=raw.decode('utf-16' if raw[:2]==b'\xff\xfe' else 'utf-8-sig')
paths=sorted(set(re.findall(r'Level Sequence="/Script/LevelSequence.LevelSequence\x27([^\x27]+)',s)))
root=unreal.Paths.project_dir()+'Scripts/PoliceNotifyObservation/'
for path in paths:
 a=unreal.load_asset(path)
 if a:
  t=unreal.AssetExportTask();t.object=a;t.filename=root+a.get_name()+'.copy';t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
  unreal.log('COLLISION_SEQ_READ '+a.get_name())
