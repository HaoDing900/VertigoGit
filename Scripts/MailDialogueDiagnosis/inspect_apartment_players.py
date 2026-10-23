import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/SaApartment/SaApartment')
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.LevelSequenceActor):
    unreal.log('SCENE SEQUENCE '+a.get_name()+' sequence='+str(a.get_sequence()))
    s=a.get_editor_property('playback_settings')
    for p in ['auto_play','play_rate','disable_movement_input','disable_look_at_input','pause_at_end','loop_count']:
        try: unreal.log('  '+p+'='+str(s.get_editor_property(p)))
        except Exception as e: unreal.log(str(e))
for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
    if 'BPLM_SaApartment' in a.get_class().get_name():
        for p in ['bIsEditorOnlyActor','Is Editor Only Actor','Play Rate','L3_LS_Intro','TestStage','bOverrideStageForTesting']:
            try: unreal.log('MANAGER '+p+'='+str(a.get_editor_property(p)))
            except: pass
