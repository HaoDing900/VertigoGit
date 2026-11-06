import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/CombatSewerPolish'
paths=['/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/Characters/Sa/ABP_Sa','/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt','/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Freemove','/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Still','/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer','/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_AcidProjectile','/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases']
for p in paths:
 a=unreal.load_asset(p);assert a,p
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;assert unreal.Exporter.run_asset_export_task(t)
unreal.log('COMBAT_SEWER_AUDIT_PASS')
