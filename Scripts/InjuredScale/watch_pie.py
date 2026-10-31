"""Read-only 8-second trace of the current PIE Sa. Does not change actor/asset/save state."""
import unreal,time,json,math
from pathlib import Path

old=getattr(unreal,'_vtg_injured_trace_handle',None)
if old is not None:
    unreal.unregister_slate_post_tick_callback(old)
trace={'samples':[],'error':None}
start=time.monotonic()
last=0
path=Path('G:/Epic/Unreal projects/VertigoGit/Scripts/InjuredScale/pie-trace.json')

def finish():
    unreal.unregister_slate_post_tick_callback(unreal._vtg_injured_trace_handle)
    unreal._vtg_injured_trace_handle=None
    path.write_text(json.dumps(trace,indent=2),encoding='utf-8')
    if not trace['samples']:
        unreal.log_warning('VTG injured trace: no PIE player found. Start PIE before running this script.')
        return
    changed=[]
    for bone in trace['samples'][0]['bones']:
        values=[s['bones'][bone] for s in trace['samples'] if bone in s['bones']]
        delta=max(max(v['scale'][i] for v in values)-min(v['scale'][i] for v in values) for i in range(3))
        length=max(v['parent_distance'] for v in values)-min(v['parent_distance'] for v in values)
        if delta>.001 or length>.01:
            changed.append({'bone':bone,'scale_variation':delta,'distance_variation_cm':length})
    trace['summary']=changed
    path.write_text(json.dumps(trace,indent=2),encoding='utf-8')
    unreal.log('VTG injured trace complete: '+str(path)+'; changed bones='+str(changed))

def sample(dt):
    global last
    elapsed=time.monotonic()-start
    if elapsed>=8:
        finish()
        return
    if elapsed-last<.1:
        return
    last=elapsed
    try:
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        pawn=unreal.GameplayStatics.get_player_character(world,0)
        if not pawn:
            return
        mesh=pawn.get_editor_property('mesh')
        anim=mesh.get_anim_instance()
        row={'time':elapsed,'pawn':pawn.get_path_name(),'health':pawn.get_editor_property('Health'),
             'low_health':anim.get_editor_property('Low Health'),
             'montage':str(anim.get_current_active_montage()),'bones':{}}
        for i in range(mesh.get_num_bones()):
            name=str(mesh.get_bone_name(i))
            if not name.endswith('_l') or not any(x in name.lower() for x in ['hand','thumb','index','middle','ring','pinky','arm']):
                continue
            t=mesh.get_socket_transform(name,unreal.RelativeTransformSpace.RTS_COMPONENT)
            p=mesh.get_socket_transform(mesh.get_parent_bone(name),unreal.RelativeTransformSpace.RTS_COMPONENT)
            s=t.scale3d
            q=t.rotation
            row['bones'][name]={'scale':[s.x,s.y,s.z],'rotation':[q.x,q.y,q.z,q.w],
                                'parent_distance':(t.translation-p.translation).length()}
        trace['samples'].append(row)
    except Exception as e:
        trace['error']=str(e)
        finish()
        unreal.log_warning('VTG injured trace stopped: '+str(e))

unreal._vtg_injured_trace_handle=unreal.register_slate_post_tick_callback(sample)
unreal.log('VTG injured trace recording for 8 seconds. Keep Sa in the visibly injured idle state.')
