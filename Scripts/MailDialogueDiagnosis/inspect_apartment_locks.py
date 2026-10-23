import unreal
paths=['/Game/ThirdPerson/Blueprints/FixedCamera/BP_0FixedCamera','/Game/LevelNSequence/In_Level/SaApartment/BPLM_SaApartment']
for path in paths:
    a=unreal.load_asset(path);t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/MailDialogueDiagnosis/'+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True;unreal.Exporter.run_asset_export_task(t)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/SaApartment/SaApartment')
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
    name=a.get_class().get_name()
    if any(s in name for s in ['Player_Sa','BPLM_SaApartment','CameraLock','FixedCamera']):
        unreal.log('APARTMENT ACTOR '+a.get_name()+' class='+name)
        for prop in ['IsInCineCutscene_LockInput(NotWorkForMont)','FixCam_Bedroom','Player Pawn','Player Controller','Stage','bCameraLocked','bHasCameraLockUsedInputLock']:
            try: unreal.log('  '+prop+'='+str(a.get_editor_property(prop)))
            except: pass

