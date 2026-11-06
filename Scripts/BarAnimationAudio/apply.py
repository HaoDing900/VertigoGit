import unreal,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Scripts/BarAnimationAudio';backup=out/'Backup';backup.mkdir(exist_ok=True)
boots='/Game/SFX/SaFootstep/Cues/Footstep_Boots_'
jobs=[('/Game/LevelNSequence/In_Level/L2Bar/ANM/CANM_LS_Sa_KickDrunk1',[(.312,'/Game/SFX/FootstepAndFoleySounds/ProSoundCollectionBonusSounds/StereoVersions/Wavs/whoosh_swish_high_fast_03',1.),(1.32,'/Game/SFX/FootstepAndFoleySounds/Footsteps/MonoVersions/Wavs/footstep_concrete_land_09',1.)]),('/Game/LevelNSequence/In_Level/L2BarStreet/Anim/LSAM_Sa_L2IntroPushDoor',[(t,boots+('%02d_Cue'%(i%6+1)),.6) for i,t in enumerate([.31,.74,1.24,1.60])]),('/Game/Characters/NPC/GenericOnes/ANM/CosmosMiamiDudes/CosmosDude_Deformed_am_Drunk_Loco_Walk_01_NoRM',[(t,boots+('%02d_Cue'%(i%6+1)),.6) for i,t in enumerate([.22,.77,1.37,1.90,2.36,3.04,3.52,3.92])])]
for path,entries in jobs:
 a=unreal.load_asset(path);assert a,path
 sounds=[unreal.load_asset(p) for t,p,v in entries];assert all(sounds)
 assert max(t for t,p,v in entries)<a.get_editor_property('sequence_length')
 src=root/'Content'/(path[6:]+'.uasset');dest=backup/src.name
 assert not dest.exists(),'Already applied; check before rerun'
 shutil.copy2(src,dest)
 unreal.AnimationLibrary.add_animation_notify_track(a,'Requested_SFX')
 for (t,p,v),s in zip(entries,sounds):
  n=unreal.AnimationLibrary.add_animation_notify_event(a,'Requested_SFX',t,unreal.AnimNotify_PlaySound);assert n
  n.set_editor_property('sound',s);n.set_editor_property('volume_multiplier',v);n.set_editor_property('follow',True)
 assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
 unreal.log('BAR_AUDIO_SAVED '+path+' '+str(len(entries)))
for p in ['/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight']:
 a=unreal.load_asset(p)
 if a:
  t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
unreal.log('BAR_AUDIO_DONE')
