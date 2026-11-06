import unreal, shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir()); out=root/'Scripts/MedicineAudio'; backup=out/'Backup'
mp='/Game/ThirdPerson/Maps/SaApartment/SaApartment'
src=root/'Content'/(mp[6:]+'.umap'); dst=backup/src.name
if not dst.exists(): shutil.copy2(src,dst)
unreal.EditorLoadingAndSavingUtils.load_map(mp)
actors=unreal.EditorLevelLibrary.get_all_level_actors()
cls=unreal.load_class(None,'/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager.BP_VTG_AudioManager_C')
assert not [a for a in actors if a.get_class()==cls]
lms=[a for a in actors if a.get_class().get_name()=='BPLM_SaApartment_C']; assert len(lms)==1
lm=lms[0]; assert not lm.get_editor_property('AudioManagerRef')
a=unreal.EditorLevelLibrary.spawn_actor_from_class(cls,lm.get_actor_location()); assert a
a.set_actor_label('BP_VTG_AudioManager_SaApartment')
for c in a.get_components_by_class(unreal.AudioComponent):
 c.set_editor_property('auto_activate',False); c.set_editor_property('sound',None)
lm.set_editor_property('AudioManagerRef',a)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log('MED_FIX_SAVED '+a.get_path_name())

