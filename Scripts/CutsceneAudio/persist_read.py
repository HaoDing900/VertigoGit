import unreal,shutil
from pathlib import Path
base='/Game/__VTG_CutsceneAudioTest'
unreal.EditorLoadingAndSavingUtils.load_map(base+'/InstanceTest')
actors=unreal.EditorLevelLibrary.get_all_level_actors();a=next(a for a in actors if a.get_actor_label()=='VTG_Enabled');b=next(a for a in actors if a.get_actor_label()=='VTG_DefaultOff')
assert unreal.VTGCutsceneAudioLibrary.get_follow_ambient_sequence_mute(a.get_component_by_class(unreal.AudioComponent))
assert not unreal.VTGCutsceneAudioLibrary.get_follow_ambient_sequence_mute(b.get_component_by_class(unreal.AudioComponent))
assert a.get_component_by_class(unreal.AudioComponent).get_editor_property('sound')==b.get_component_by_class(unreal.AudioComponent).get_editor_property('sound')
seq=unreal.load_asset(base+'/ProfileTest');settings=unreal.VTGCutsceneAudioLibrary.get_sequence_audio_settings(seq)
assert settings and settings.get_editor_property('preset')==unreal.VTGCutsceneAudioPreset.MUTE_WORLD_BEDS
unreal.log('CUTSCENE_PERSIST_RELOAD_PASS instance switches and runtime profile survive separate process reload')
for mp in ['/Game/ThirdPerson/Maps/SaApartment/SaApartment','/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar']:
 unreal.EditorLoadingAndSavingUtils.load_map(mp)
 found=0
 for actor in unreal.EditorLevelLibrary.get_all_level_actors():
  if actor.get_class().get_name().startswith('BP_VTG_AudioManager'):
   names=[c.get_name() for c in actor.get_components_by_class(unreal.AudioComponent)]
   assert sorted(names)==['AC_Ambience','AC_Music'],(mp,names)
   found+=1
 unreal.log('CUTSCENE_REAL_MAP_PASS '+mp+' managers='+str(found)+' no Psych instance channel')
# Delete the two temporary files externally after this editor process exits.
# Avoid deleting a world package while Python still holds actor references.

