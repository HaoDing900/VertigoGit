import unreal, json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/BelloAnimationAudio'
base='/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/'
for name,count in [('ANM_Bello_FixCar1',4),('ANM_Bello_Loco_Walk_Fwd_NoRM_Mirror',2)]:
 a=unreal.load_asset(base+name)
 events=unreal.AnimationLibrary.get_animation_notify_events(a)
 assert len(events)==count
 for e in events: unreal.log('BELLO_VERIFIED_EVENT '+str(e))
 t=unreal.AssetExportTask(); t.object=a; t.filename=str(out/(name+'.after.copy')); t.automated=True; t.prompt=False
 assert unreal.Exporter.run_asset_export_task(t)
# Read the current player component template, not the historical setup report.
bp=unreal.load_asset('/Game/ThirdPerson/Blueprints/BP_Player_Sa')
c=unreal.find_object(bp,'PlayerAudio_GEN_VARIABLE')
if c: unreal.log('BELLO_SA_DEFAULTS '+str(c.get_editor_property('default_steps')))
unreal.log('BELLO_VERIFY_SUCCESS')
