import unreal, shutil, json
from pathlib import Path
root=Path(unreal.Paths.project_dir()); out=root/'Scripts/BelloAnimationAudio'; backup=out/'Backup'; backup.mkdir(exist_ok=True)
base='/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/'
metal='/Game/SFX/0InLevel/Station/METLMvmt___Tinkering_Antique_Lock_344_Audio_Antique_Small_Metals'
jobs=[('ANM_Bello_FixCar1',[(t,metal,1.0) for t in [0.0,4.30,8.30,12.30]]),('ANM_Bello_Loco_Walk_Fwd_NoRM_Mirror',[(0.36,'/Game/SFX/SaFootstep/Cues/Footstep_Boots_01_Cue',0.6),(1.11,'/Game/SFX/SaFootstep/Cues/Footstep_Boots_02_Cue',0.6)])]
prepared=[]
for name, entries in jobs:
 a=unreal.load_asset(base+name); assert a
 assert len(unreal.AnimationLibrary.get_animation_notify_events(a))==0, 'Existing events; abort to avoid duplicates'
 sounds=[unreal.load_asset(p) for t,p,v in entries]; assert all(sounds)
 assert all(0<=t<a.get_editor_property('sequence_length') for t,p,v in entries)
 src=root/'Content'/((base+name)[6:]+'.uasset'); dst=backup/src.name
 assert not dst.exists(), 'Backup already exists'
 shutil.copy2(src,dst)
 prepared.append((a,entries,sounds))
for a,entries,sounds in prepared:
 track='Bello_SFX'
 unreal.AnimationLibrary.add_animation_notify_track(a,track)
 for (t,p,v),sound in zip(entries,sounds):
  notify=unreal.AnimationLibrary.add_animation_notify_event(a,track,t,unreal.AnimNotify_PlaySound)
  assert notify
  notify.set_editor_property('sound',sound)
  notify.set_editor_property('volume_multiplier',v)
  notify.set_editor_property('pitch_multiplier',1.0)
  notify.set_editor_property('follow',True)
 assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
 unreal.log('BELLO_SAVED '+a.get_path_name())
unreal.log('BELLO_APPLY_SUCCESS')
