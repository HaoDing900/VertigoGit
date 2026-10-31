import unreal
import json
from pathlib import Path

paths = [
 '/Game/Characters/Sa/Anm/ANM_Sa_InjuredForBlend',
 '/Game/Characters/Sa/Anm/MoveStop/ANM_Sa_InjuredWalking',
 '/Game/Characters/Sa/Anm/Face/Face_Additive_Sa_3Injured',
 '/Game/Characters/Sa/Anm/Face/Face_Sa_NoAdd_3Injured1',
 '/Game/Characters/May/Anim/May_RetargetAnm/ANM_May_Walk_Injured1',
 '/Game/Characters/Sa/Anm/Loco/Sa_am_Stand_Idle_02',
]
result=[]
for path in paths:
    asset=unreal.load_asset(path)
    row={'asset':path, 'properties':{}, 'tracks':[]}
    for key in ['additive_anim_type','ref_pose_type','ref_pose_seq','ref_frame_index','data_model_interface']:
        try: row['properties'][key]=str(asset.get_editor_property(key))
        except Exception as e: row['properties'][key]=str(e)
    try:
        model=asset.get_editor_property('data_model_interface')
        names=unreal.AnimationLibrary.get_animation_track_names(asset)
        row['model_methods']=[x for x in dir(model) if 'bone' in x or 'track' in x]
        length=unreal.AnimationLibrary.get_sequence_length(asset)
        times=[length*i/60 for i in range(61)]
        evaluated=[unreal.AnimPoseExtensions.get_anim_pose_at_time(asset,t,unreal.AnimPoseEvaluationOptions()) for t in times]
        for name in names:
            poses=[unreal.AnimPoseExtensions.get_bone_pose(p,name,unreal.AnimPoseSpaces.LOCAL) for p in evaluated]
            scales=[p.scale3d for p in poses]
            lo=[min(getattr(s,a) for s in scales) for a in ['x','y','z']]
            hi=[max(getattr(s,a) for s in scales) for a in ['x','y','z']]
            variation=max(b-a for a,b in zip(lo,hi))
            if 'hand' in str(name).lower() or variation>.005 or max(abs(a-1) for a in lo)>.005:
                row['tracks'].append({'bone':str(name),'min':lo,'max':hi,'variation':variation})
    except Exception as e:
        row['error']=str(e)
    result.append(row)
out=Path(unreal.Paths.project_dir())/'Scripts/InjuredScale/inspection.json'
out.write_text(json.dumps(result,indent=2),encoding='utf-8')
unreal.log('INJURED_PYTHON_DIAGNOSIS_COMPLETE '+str(out))
