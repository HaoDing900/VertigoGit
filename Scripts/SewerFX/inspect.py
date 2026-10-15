import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/SewerFX'
paths=['/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg','/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster','/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_AcidProjectile','/Game/AI/BossAIToolkit/Blueprints/Abilities/Damage/BP_Ability_Projectile','/Game/AI/BossAIToolkit/Blueprints/Misc/BP_Projectile_Base','/Game/AI/BossAIToolkit/Blueprints/Components/BPC_Boss_Behavior','/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases','/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_SeaMonsterEgg_ReleaseGas','/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_EggProducer_AcidSpit']
for p in paths:
 a=unreal.load_asset(p)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+('.copy' if isinstance(a,unreal.Blueprint) else '.t3d')));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
a=unreal.load_asset(paths[6])
