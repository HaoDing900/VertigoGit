import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/MedicineAudio'
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/SaApartment/SaApartment')
actors=unreal.EditorLevelLibrary.get_all_level_actors()
lm=next(a for a in actors if a.get_class().get_name()=='BPLM_SaApartment_C')
manager=lm.get_editor_property('AudioManagerRef'); assert manager
components=manager.get_components_by_class(unreal.AudioComponent); assert len(components)==3
assert all(not c.get_editor_property('auto_activate') for c in components)
psych=next(c for c in components if c.get_name()=='AC_Psych'); assert psych.get_editor_property('volume_multiplier')>0
bp=unreal.load_asset('/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1')
event=unreal.find_object(bp,'DialogueTemplate.DialogueNode_NPC_88.NE_VTG_Audio_PlayOneShot_C_0'); assert event
sound=event.get_editor_property('NewSound'); assert sound.get_name()=='SFX_Pill'
assert event.get_editor_property('VolumeMultiplier')==1.0 and event.get_editor_property('FadeInTime')==0.0
unreal.log('MED_PASS saved map manager reference, disabled auto-activation, SFX_Pill event settings, AC_Psych volume='+str(psych.get_editor_property('volume_multiplier')))
a=unreal.load_asset('/Game/LevelNSequence/In_Level/L2Bar/ANM/CANM_NPC_DrunkByDoor_Vomiting1_Montage')
assert len(unreal.AnimationLibrary.get_animation_notify_events(a))==2
t=unreal.AssetExportTask(); t.object=a; t.filename=str(out/'montage.verified.copy'); t.automated=True; t.prompt=False
assert unreal.Exporter.run_asset_export_task(t)
unreal.log('MED_VERIFY_SUCCESS')


