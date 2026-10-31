import unreal
paths=['/Game/LevelNSequence/In_Level/L6_Tunnel_MarketToFactory/NLS_L6Tunnel_SaFallAsleep','/Game/LevelNSequence/In_Level/L6_Tunnel_MarketToFactory/NLS_L6_NoticeFish','/Game/LevelNSequence/In_Level/L6_Tunnel_MarketToFactory/NLS_NoticeFish']
for p in paths:
 s=unreal.load_asset(p)
 if not s: continue
 unreal.log('L6 TIMING '+p+' start='+str(s.get_playback_start_seconds())+' end='+str(s.get_playback_end_seconds())+' duration='+str(s.get_playback_end_seconds()-s.get_playback_start_seconds()))
 for t in s.get_master_tracks():
  if isinstance(t,unreal.MovieSceneCameraCutTrack):
   for sec in t.get_sections():
    unreal.log('L6 CUT '+str(sec.get_start_frame_seconds())+' -> '+str(sec.get_end_frame_seconds()))
