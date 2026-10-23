import unreal
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();sa=next(a for a in actors if a.get_class().get_name()=='BP_Player_Sa_C');m=next(a for a in actors if a.get_class().get_name()=='BPLM_BlackMarket_C');m.set_editor_property('Player Pawn',sa)
s=unreal.load_asset('/Game/LevelNSequence/In_Level/L4BlackMarket/PreAdult/NLS_L4Market_PreAdult_IPHAPauseInfrontOfAdult2')
def report(label):unreal.log('RESTORE227 '+label+' position='+str(sa.get_actor_location())+' rotation='+str(sa.get_actor_rotation()))
for restore in [True,False]:
 sa.set_actor_location_and_rotation(unreal.Vector(-255.483,4046.688,249.206),unreal.Rotator(0,2.26,0),False,True)
 report('before restore='+str(restore))
 settings=unreal.MovieSceneSequencePlaybackSettings();settings.restore_state=restore
 player,actor=unreal.LevelSequencePlayer.create_level_sequence_player(w,s,settings)
 player.play();player.pause()
 player.set_playback_position(unreal.MovieSceneSequencePlaybackParams(frame=unreal.FrameTime(unreal.FrameNumber(120)),position_type=unreal.MovieScenePositionType.FRAME,update_method=unreal.UpdatePositionMethod.JUMP))
 report('during sequence')
 m.call_method('L4_Position_SaIPHAInfrontOfAdultStore');report('after actual BPLM positioning event')
 player.stop();report('after stop restore='+str(restore))
 actor.destroy_actor()
event=unreal.get_default_object(unreal.load_class(None,'/Game/Narrative/Events/1PlayLevelSequence.1PlayLevelSequence_C'))
for p in ['Restore State','End Line when Finished','Stop when Line Ends']:
 unreal.log('RESTORE227 event default '+p+'='+str(event.get_editor_property(p)))
