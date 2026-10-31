import unreal,json
from pathlib import Path
out={}
c=unreal.load_class(None,'/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C')
player=unreal.get_default_object(c)
mesh=player.get_editor_property('mesh')
out['mesh']=str(mesh.get_editor_property('skeletal_mesh_asset'))
out['postprocess']=str(mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('post_process_anim_blueprint'))
pa=unreal.load_asset('/Game/Characters/Sa/Sa_PhysicsAsset_WithHairNCape')
out['bodies']=[]
try:
    for b in pa.get_editor_property('SkeletalBodySetups'):
        out['bodies'].append({'bone':str(b.get_editor_property('bone_name')),'type':str(b.get_editor_property('physics_type'))})
except Exception as e:
    out['physics_error']=str(e)
out['animations']=[]
for path in ['/Game/Characters/Sa/Anm/ANM_Sa_InjuredForBlend','/Game/Characters/May/Anim/May_RetargetAnm/ANM_May_Walk_Injured1','/Game/Characters/Sa/Anm/Loco/Sa_am_Stand_Idle_02']:
    a=unreal.load_asset(path)
    names=unreal.AnimationLibrary.get_animation_track_names(a)
    names=[n for n in names if 'hand' in str(n).lower() or 'arm' in str(n).lower() or any(x in str(n).lower() for x in ['thumb','index','middle','ring','pinky'])]
    length=unreal.AnimationLibrary.get_sequence_length(a)
    poses=[unreal.AnimPoseExtensions.get_anim_pose_at_time(a,length*i/60,unreal.AnimPoseEvaluationOptions()) for i in range(61)]
    row={'asset':path,'skeleton':str(a.get_editor_property('skeleton')),'bones':[]}
    for n in names:
        ts=[unreal.AnimPoseExtensions.get_bone_pose(p,n,unreal.AnimPoseSpaces.LOCAL) for p in poses]
        lengths=[t.translation.length() for t in ts]
        row['bones'].append({'bone':str(n),'length_min':min(lengths),'length_max':max(lengths)})
    out['animations'].append(row)
(Path(unreal.Paths.project_dir())/'Scripts/InjuredScale/details.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
unreal.log('INJURED_DETAILS_COMPLETE')

