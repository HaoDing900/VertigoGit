import unreal
s=unreal.load_asset('/Game/LevelNSequence/In_Level/L4BlackMarket/PreAdult/NLS_L4Market_PreAdult_IPHAPauseInfrontOfAdult1')
t=unreal.AssetExportTask();t.object=s;t.filename=unreal.Paths.project_dir()+'Scripts/PreAdultFootsteps/sequence.copy';t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
for b in unreal.MovieSceneSequenceExtensions.get_bindings(s):
 for track in unreal.MovieSceneBindingExtensions.get_tracks(b):
  if isinstance(track,unreal.MovieSceneSkeletalAnimationTrack):
   for sec in track.get_sections():
    p=sec.get_editor_property('params')
    unreal.log('PRE_ADULT_ANIMATION binding='+str(unreal.MovieSceneBindingExtensions.get_display_name(b))+' skip='+str(p.get_editor_property('skip_anim_notifiers'))+' anim='+str(p.get_editor_property('animation')))
