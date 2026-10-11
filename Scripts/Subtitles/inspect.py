import unreal,pathlib
s=unreal.load_asset('/Game/LevelNSequence/In_Level/TestSeq');m=s.get_movie_scene();unreal.log('SUBTITLE_SAMPLE_RANGE '+str(m.get_playback_range()) if hasattr(m,'get_playback_range') else 'SUBTITLE_SAMPLE '+str(s.get_playback_start())+' '+str(s.get_playback_end()))
