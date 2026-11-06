import unreal, os, shutil
root=unreal.Paths.project_dir()+'Scripts/PoliceContractAudio/Backup/'
def backup(a):
 p=unreal.Paths.project_content_dir()+a.get_path_name().split('.',1)[0].removeprefix('/Game/')+'.uasset'
 os.makedirs(root,exist_ok=True)
 dest=root+a.get_name()+'.uasset'
 if not os.path.exists(dest):shutil.copy2(p,dest)
b=unreal.load_asset('/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/ANM_Bello_Loco_Walk_Fwd_NoRM_Mirror')
backup(b)
w=unreal.load_asset('/Game/SFX/Character/SaFootstep/Waves/Footstep_Boots_06')
count=0
for e in unreal.AnimationLibrary.get_animation_notify_events(b):
 n=e.get_editor_property('notify')
 if isinstance(n,unreal.AnimNotify_PlaySound):n.set_editor_property('sound',w);count+=1
assert count==2,count
unreal.EditorAssetLibrary.save_loaded_asset(b,False)
c=unreal.load_asset('/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/ANM_Corvos_Walk_Fwd');backup(c)
s=unreal.load_asset('/Game/SFX/FootstepAndFoleySounds/Footsteps/StereoVersions/Wavs/footstep_concrete_walk_04')
tracks=unreal.AnimationLibrary.get_animation_notify_track_names(c)
if 'Footsteps' not in [str(t) for t in tracks]:unreal.AnimationLibrary.add_animation_notify_track(c,'Footsteps')
existing=[e for e in unreal.AnimationLibrary.get_animation_notify_events(c) if isinstance(e.get_editor_property('notify'),unreal.AnimNotify_PlaySound) and e.get_editor_property('notify').get_editor_property('sound')==s]
assert len(existing) in (0,2),len(existing)
if not existing:
 for t in [.26,.90]:
  n=unreal.AnimationLibrary.add_animation_notify_event(c,'Footsteps',t,unreal.AnimNotify_PlaySound)
  n.set_editor_property('sound',s)
unreal.EditorAssetLibrary.save_loaded_asset(c,False)
unreal.log('POLICE_ANIMATION_AUDIO_APPLIED Bello=2 Corvos=2')
