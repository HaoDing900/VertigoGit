import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/BlackMarketCombat'
for p in ['/Game/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base','/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket','/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/AI/AIBehaviorSystem/Blueprint/Component/BP_CollisionComponent','/Game/AI/BossAIToolkit/Blueprints/DamageTypes/BP_DamageType_Default']:
 a=unreal.load_asset(p)
 if not a: continue
 c=unreal.get_default_object(a.generated_class())
 t=unreal.AssetExportTask();t.object=c;t.exporter=unreal.ObjectExporterT3D();t.filename=str(out/(a.get_name()+'.defaults.t3d'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 label=a.get_actor_label()
 if any(x in label.lower() for x in ['goon','bplm','misto','nara','brosky']):
  t=unreal.AssetExportTask();t.object=a;t.exporter=unreal.ObjectExporterT3D();t.filename=str(out/(a.get_name()+'.map.t3d'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
  unreal.log('COMBAT_MAP '+label+' '+a.get_class().get_path_name())
unreal.log('COMBAT_MAP_DONE')
