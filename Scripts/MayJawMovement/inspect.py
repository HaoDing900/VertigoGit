import unreal
from pathlib import Path

folder = Path(unreal.Paths.project_dir()) / 'Scripts/MayJawMovement'
for path in ['/Game/Characters/May/ABP_NPC_May', '/Game/Characters/May/BP_May_NPC']:
    asset = unreal.load_asset(path)
    assert asset, path
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(folder / (asset.get_name() + '.copy'))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert unreal.Exporter.run_asset_export_task(task)
anim = unreal.load_asset('/Game/Characters/May/Anim/May_Mouth')
length = anim.get_editor_property('sequence_length')
unreal.log('MAY JAW length=' + str(length) + ' rate_scale=' + str(anim.get_editor_property('rate_scale')) + ' skeleton=' + str(anim.get_editor_property('skeleton')))
for bone in unreal.AnimationLibrary.get_animation_track_names(anim):
    if 'jaw' in str(bone).lower():
        for fraction in [0., .25, .5, .75, 1.]:
            pose = unreal.AnimationLibrary.get_bone_pose_for_time(anim, bone, length * fraction, False)
            unreal.log('MAY JAW bone=' + str(bone) + ' phase=' + str(fraction) + ' pose=' + str(pose))
