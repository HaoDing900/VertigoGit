import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/MailSequencePolish'
for p in ['/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight']:
 a=unreal.load_asset(p);assert a,p
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;assert unreal.Exporter.run_asset_export_task(t)
unreal.log('MAIL_SEQUENCE_AUDIT_PASS')
