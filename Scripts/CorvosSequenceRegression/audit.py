import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/CorvosSequenceRegression'
for p in ['/Game/Narrative/Dialogues/Levels/L3PoliceStation/L3PoliceStation_Still','/Game/LevelNSequence/In_Level/L3PoliceStation/NLS_L3_CorvosConv_SaApproach','/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation']:
 a=unreal.load_asset(p);assert a,p
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;assert unreal.Exporter.run_asset_export_task(t)
 unreal.log('CORVOS_ASSET '+p+' class='+a.get_class().get_name())
unreal.log('CORVOS_AUDIT_PASS')
