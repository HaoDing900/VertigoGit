import unreal
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/L_SewerUnderApartment')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    if not any(s in actor.get_class().get_name() for s in ['SmartTrigger','CameraLock','BPLM_Sewer']):continue
    unreal.log('SEWER ACTOR '+actor.get_path_name()+' label='+actor.get_actor_label()+' class='+actor.get_class().get_path_name())
    for prop in ['ActiveStage','TRIGGER ID','TriggerBPLMEvent?','DoOnce?','Lock Cam on Trigger?','Lock Cam Point','Active Stage?','_____CAMERA ID_','Look At Target','BlendInTime','FreezeMovement','FreezeLook','Tags']:
        try:unreal.log('SEWER PROP '+prop+'='+str(actor.get_editor_property(prop)))
        except:pass
    task=unreal.AssetExportTask();task.object=actor;task.filename=unreal.Paths.project_dir()+'Scripts/CineSaMontageDiagnosis/'+actor.get_name()+'.t3d';task.automated=True;task.prompt=False;task.replace_identical=True
    unreal.Exporter.run_asset_export_task(task)
