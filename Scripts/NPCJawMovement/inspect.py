import unreal
from pathlib import Path

folder = Path(unreal.Paths.project_dir()) / 'Scripts/NPCJawMovement'
entries = [
    ('FemaleClient', '/Game/Characters/NPC/FemaleClient/ABP_FemaleClientLS', '/Game/Characters/NPC/FemaleClient/ANM/ANM_FemaleClient_Mouth', '/Game/Characters/NPC/FemaleClient/BP_FemaleClient_StillNPC'),
    ('Bello', '/Game/Characters/NPC/Bello/ABP_Bello_NPC', '/Game/Characters/NPC/Bello/ANM_Bello_Mouth', '/Game/Characters/NPC/Bello/BP_Bello_NPC'),
    ('Corvos', '/Game/Characters/NPC/Corvos/ABP_NPC_Corvos', '/Game/Characters/NPC/Corvos/ANM_Corvos_Mouth', '/Game/Characters/NPC/Corvos/BP_Corvos_NPC'),
    ('Ulises', '/Game/Characters/Ulises/ABP_Ulises', '/Game/Characters/Ulises/ANM/Mouth_Ulises', '/Game/Characters/Ulises/BP_Ulises_NPC'),
]
for tag, abp_path, anim_path, actor_path in entries:
    for path in [abp_path, actor_path]:
        asset = unreal.load_asset(path)
        assert asset, path
        task = unreal.AssetExportTask()
        task.object = asset
        task.filename = str(folder / (asset.get_name() + '.copy'))
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        assert unreal.Exporter.run_asset_export_task(task)
    anim = unreal.load_asset(anim_path)
    unreal.log('NPC JAW INSPECT ' + tag + ' skeleton=' + str(anim.get_editor_property('skeleton')) + ' length=' + str(anim.get_editor_property('sequence_length')) + ' additive=' + str(anim.get_editor_property('additive_anim_type')))
    tracks = unreal.AnimationLibrary.get_animation_track_names(anim)
    unreal.log('NPC JAW TRACKS ' + tag + ' ' + str(tracks))
    length = anim.get_editor_property('sequence_length')
    for bone in tracks:
        if any(word in str(bone).lower() for word in ['jaw', 'mouth', 'lip']):
            samples = [unreal.AnimationLibrary.get_bone_pose_for_time(anim, bone, length * p, False) for p in [0., .25, .5, .75, 1.]]
            unreal.log('NPC JAW BONE ' + tag + ' ' + str(bone) + ' ' + str(samples))
