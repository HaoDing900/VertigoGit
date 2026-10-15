import unreal,pathlib,json
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/ActionHUD'
paths=['/Game/Narrative/NarrativeUI/WBP_NHUD','/Game/Widget/Texture/WBP_BattleModeHUD','/Game/Widget/EnemyHealthBar/WBP_EnemyHealthBar','/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base','/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg']
report={}
for p in paths:
 a=unreal.load_asset(p);t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 if isinstance(a,unreal.WidgetBlueprint):
  report[p]={}
(out/'inspect.json').write_text(json.dumps(report,indent=2),encoding='utf8')
reg=unreal.AssetRegistryHelpers.get_asset_registry()
for p in paths[:3]:
 report[p]['refs']=[str(x) for x in reg.get_referencers(p,unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True))]
(out/'inspect.json').write_text(json.dumps(report,indent=2),encoding='utf8')

for p in ['/Game/ThirdPerson/Input/IMC_Default','/Game/ThirdPerson/Input/IMC_Shooter_May','/Game/ThirdPerson/Input/IMC_Default_May']:
 a=unreal.load_asset(p)
 if not a: continue
 report[p]=[{'action':str(m.action),'key':str(m.key)} for m in a.get_editor_property('mappings')]
(out/'inspect.json').write_text(json.dumps(report,indent=2),encoding='utf8')

