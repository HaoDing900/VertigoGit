import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/SewerBossRefinement'
paths=['/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg','/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer','/Game/Characters/Monster/SeaMonsterBoss/ABP_EggProducer','/Game/Characters/Monster/SeaMonsterBoss/ABP_SeaMonsterEgg','/Game/AI/BossAIToolkit/Blueprints/BPC_Boss_Behavior','/Game/AI/BossAIToolkit/Blueprints/Abilities/Damage/BP_Ability_Projectile','/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases','/Game/VFX/VFX_EnemyAttack/VFX_AcidSpit/NS_AcidSpit_Burst','/InventorySystemX/InventorySystemX/Materials/Material_Instances/MI_Inventory_FogMadeIntoCaustic']
for p in paths:
 a=unreal.load_asset(p)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+('.copy' if isinstance(a,unreal.Blueprint) else '.t3d')));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 if isinstance(a,unreal.MaterialInstanceConstant):
  unreal.log('MATERIAL BASE '+a.get_editor_property('parent').get_path_name())
