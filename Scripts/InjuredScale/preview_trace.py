"""Temporary independent editor scene. Never saves a map, asset, or game slot."""
import unreal,time,json,traceback
from pathlib import Path

out=Path('G:/Epic/Unreal projects/VertigoGit/Scripts/InjuredScale/preview-trace.json')
state={'started':time.monotonic(),'samples':[],'error':None}
try:
    # Use the independent editor process world; never save it.
    subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cls=unreal.load_class(None,'/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C')
    defaults=unreal.get_default_object(cls)
    anim_defaults=unreal.get_default_object(unreal.load_class(None,'/Game/Characters/Sa/ABP_Sa.ABP_Sa_C'))
    pawn=subsystem.spawn_actor_from_class(cls,unreal.Vector(0,0,100))
    mesh=pawn.get_editor_property('mesh')
    mesh.set_update_animation_in_editor(True)
    anim=mesh.get_anim_instance()
    anim.set_editor_property('bCameraLookEnabled',False)
    names=[str(mesh.get_bone_name(i)) for i in range(mesh.get_num_bones())]
    names=[n for n in names if any(x in n.lower() for x in ['hand','thumb','index','middle','ring','pinky','arm_stretch','forearm_stretch'])]
    state['bones']=names
    state['anim']=anim.get_path_name()
    state['started']=time.monotonic()
    state['last']=0
    state['case']=-1

    def tick(dt):
        global pawn,mesh,anim
        try:
            elapsed=time.monotonic()-state['started']
            case=int(elapsed/3)
            if case>=3:
                out.write_text(json.dumps(state,indent=2,default=str),encoding='utf-8')
                unreal.unregister_slate_post_tick_callback(state['handle'])
                unreal.log('INJURED_PREVIEW_TRACE_COMPLETE')
                unreal.SystemLibrary.quit_editor()
                return
            if case!=state['case']:
                state['case']=case
                defaults.set_editor_property('Health',100 if case==0 else 10)
                defaults.set_editor_property('ShowLowHealth?',case!=0)
                anim_defaults.set_editor_property('Low Health',case!=0)
                subsystem.destroy_actor(pawn)
                pawn=subsystem.spawn_actor_from_class(cls,unreal.Vector(0,0,100))
                mesh=pawn.get_editor_property('mesh')
                mesh.set_update_animation_in_editor(True)
                anim=mesh.get_anim_instance()
                mesh.set_editor_property('disable_post_process_blueprint',case==2)
            if elapsed-state['last']<.04 or elapsed%3<.6:
                return
            state['last']=elapsed
            bones={}
            for name in names:
                transform=mesh.get_socket_transform(name,unreal.RelativeTransformSpace.RTS_COMPONENT)
                scale=transform.scale3d
                pos=transform.translation
                parent=mesh.get_parent_bone(name)
                p=mesh.get_socket_transform(parent,unreal.RelativeTransformSpace.RTS_COMPONENT)
                q=transform.rotation
                bones[name]={'scale':[scale.x,scale.y,scale.z], 'length':(pos-p.translation).length(),
                             'rotation':[q.x,q.y,q.z,q.w], 'position':[pos.x,pos.y,pos.z]}
            state['samples'].append({'time':elapsed,'case':case,'low':anim.get_editor_property('Low Health'),'bones':bones})
        except Exception:
            state['error']=traceback.format_exc()
            out.write_text(json.dumps(state,indent=2,default=str),encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.SystemLibrary.quit_editor()

    state['handle']=unreal.register_slate_post_tick_callback(tick)
except Exception:
    state['error']=traceback.format_exc()
    out.write_text(json.dumps(state,indent=2,default=str),encoding='utf-8')
    unreal.SystemLibrary.quit_editor()




