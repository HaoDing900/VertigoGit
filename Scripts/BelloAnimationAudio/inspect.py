import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/BelloAnimationAudio'
paths=['/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/ANM_Bello_FixCar1','/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/ANM_Bello_Loco_Walk_Fwd_NoRM_Mirror','/Game/ThirdPerson/Blueprints/BP_Player_Sa']
for p in paths:
 a=unreal.load_asset(p)
 unreal.log('BELLO_ASSET '+p+' '+str(a))
 if isinstance(a,unreal.AnimSequence):
  unreal.log('BELLO_LENGTH '+str(a.get_editor_property('sequence_length')))
  unreal.log('BELLO_EVENTS '+str(unreal.AnimationLibrary.get_animation_notify_events(a)))
 t=unreal.AssetExportTask(); t.object=a; t.filename=str(out/(a.get_name()+'.copy')); t.automated=True; t.prompt=False
 unreal.Exporter.run_asset_export_task(t)
unreal.log('BELLO_API '+str([n for n in dir(unreal) if 'SoundCue' in n or 'SoundNodeRandom' in n]))
