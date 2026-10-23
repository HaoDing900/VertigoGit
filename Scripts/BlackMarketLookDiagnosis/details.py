import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/BlackMarketLookDiagnosis'
paths=['/Game/LevelNSequence/BPLM','/Game/Narrative/Events/LockCam/NE_Cam_LockCameraByID','/Game/Characters/VTGCharacterShader/Player_LightColor','/Game/LevelNSequence/In_Level/L4BlackMarket/PreAdult/L4Market_PreAdult_Intro_SaObserveIPHA']
reg=unreal.AssetRegistryHelpers.get_asset_registry();reg.search_all_assets(True)
refs=reg.get_referencers('/Game/Characters/VTGCharacterShader/Player_LightColor',unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True))
for r in refs:
 unreal.log('LIGHT REF '+str(r))
 if any(s in str(r) for s in ['LevelNSequence','Blueprint','Light','Shader']): paths.append(str(r))
for p in list(dict.fromkeys(paths)):
 a=unreal.load_asset(p)
 if not a:continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.detail.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if any(x in a.get_class().get_name() for x in ['BPLM_BlackMarket','BP_Player_Sa','CameraLock']):
  unreal.log('MAPDIAG '+a.get_actor_label()+' '+a.get_path_name())
  t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.detail.t3d'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
