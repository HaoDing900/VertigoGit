import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/BelloSequenceDiagnosis'
for p in ['/Game/LevelNSequence/In_Level/L3PoliceStation/NLS_L3_BelloTouchCarBottom','/Game/LevelNSequence/In_Level/L3PoliceStation/LS_L3_BelloTouchCarBottom','/Game/Characters/NPC/Bello/ABP_Bello_NPC','/Game/Characters/NPC/Bello/BP_Bello_NPC']:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask(); t.object=a; t.filename=str(out/(a.get_name()+'.copy')); t.automated=True; t.prompt=False; t.replace_identical=True
 unreal.Exporter.run_asset_export_task(t)
 if isinstance(a,unreal.LevelSequence):
  for b in a.get_bindings():
   unreal.log('BELLO BIND '+b.get_name())
   for tr in b.get_tracks():
    unreal.log('BELLO TRACK '+str(tr.get_class().get_name()))
    for s in tr.get_sections():
     unreal.log('BELLO SECTION '+s.export_text()) if hasattr(s,'export_text') else None
     if isinstance(s,unreal.MovieSceneSkeletalAnimationSection): unreal.log('BELLO PARAM '+str(s.get_editor_property('params')))
