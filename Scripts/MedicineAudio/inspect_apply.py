import unreal,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir()); out=root/'Scripts/MedicineAudio'; backup=out/'Backup'; backup.mkdir(exist_ok=True)
p='/Game/LevelNSequence/In_Level/L2Bar/ANM/CANM_NPC_DrunkByDoor_Vomiting1_Montage'
a=unreal.load_asset(p); s=unreal.load_asset('/Game/SFX/0InLevel/Bar/SFX_DrunkVomit'); assert a and s
unreal.log('MED_OLD_NOTIFIES '+str(unreal.AnimationLibrary.get_animation_notify_events(a)))
src=root/'Content'/(p[6:]+'.uasset'); dst=backup/src.name
assert not dst.exists(); shutil.copy2(src,dst)
track='Vomit_SFX'; unreal.AnimationLibrary.add_animation_notify_track(a,track)
n=unreal.AnimationLibrary.add_animation_notify_event(a,track,1.74,unreal.AnimNotify_PlaySound); assert n
n.set_editor_property('sound',s); n.set_editor_property('follow',True)
assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
paths=[p,'/Game/LevelNSequence/In_Level/SaApartment/BPLM_SaApartment','/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1']
ar=unreal.AssetRegistryHelpers.get_asset_registry()
for d in ar.get_all_assets():
 if str(d.asset_name)=='NE_VTGAudioPlayOneShot': paths.append(str(d.package_name))
for p in paths:
 a=unreal.load_asset(p); assert a,p
 t=unreal.AssetExportTask(); t.object=a; t.filename=str(out/(a.get_name()+'.copy')); t.automated=True; t.prompt=False
 assert unreal.Exporter.run_asset_export_task(t)
unreal.log('MED_DONE')
