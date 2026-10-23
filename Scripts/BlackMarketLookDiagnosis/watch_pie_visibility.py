"""Read-only PIE visibility trace. Run this file from Unreal's Python console.
Run it again to replace the previous watcher. No assets or actor state are modified.
"""
import unreal
import json
import time
from pathlib import Path

_previous = getattr(unreal, '_vtg_sa_visibility_watch', None)
if _previous is not None:
    unreal.unregister_slate_post_tick_callback(_previous)

_trace_path = Path(unreal.Paths.project_dir()) / 'Scripts/BlackMarketLookDiagnosis/pie-visibility.jsonl'
_trace_path.parent.mkdir(parents=True, exist_ok=True)
_started = time.monotonic()
_last_sample = 0.0
_last_summary = None

def _read(call):
    try:
        return call()
    except Exception as error:
        return {'unavailable': str(error)}

def _path(obj):
    return obj.get_path_name() if obj else None

def _xyz(vector):
    return [round(vector.x, 3), round(vector.y, 3), round(vector.z, 3)]

def _component(component):
    result = {'path': _path(component), 'class': component.get_class().get_name()}
    for prop in ['hidden_in_game', 'owner_no_see', 'only_owner_see', 'render_in_main_pass',
                 'visible', 'absolute_location', 'absolute_rotation', 'bounds_scale']:
        result[prop] = _read(lambda p=prop: component.get_editor_property(p))
    result['world_location'] = _read(lambda: _xyz(component.get_world_location()))
    result['relative_location'] = _read(lambda: _xyz(component.get_editor_property('relative_location')))
    if isinstance(component, unreal.SkeletalMeshComponent):
        result['animation_mode'] = _read(lambda: str(component.get_editor_property('animation_mode')))
        result['anim_instance'] = _read(lambda: _path(component.get_anim_instance()))
        result['root_bone'] = _read(lambda: str(component.get_bone_name(0)))
        result['root_world_location'] = _read(lambda: _xyz(component.get_socket_location(component.get_bone_name(0))))
        result['materials'] = _read(lambda: [_path(m) for m in component.get_materials()])
    return result

def _watch(delta):
    global _last_sample, _last_summary
    now = time.monotonic()
    if now - _last_sample < 0.5:
        return
    _last_sample = now
    world = unreal.EditorLevelLibrary.get_game_world()
    if not world:
        return
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    players = unreal.GameplayStatics.get_all_actors_of_class(
        world, unreal.load_class(None, '/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C'))
    managers = unreal.GameplayStatics.get_all_actors_of_class(
        world, unreal.load_class(None, '/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket.BPLM_BlackMarket_C'))
    record = {'elapsed': round(now - _started, 2), 'world': _path(world),
              'possessed_pawn': _read(lambda: _path(pc.get_pawn())) if pc else None,
              'view_target': _read(lambda: _path(pc.get_view_target())) if pc else None,
              'players': [], 'manager_targets': [], 'sequences': []}
    if pc and pc.player_camera_manager:
        record['camera_location'] = _read(lambda: _xyz(pc.player_camera_manager.get_camera_location()))
        record['camera_rotation'] = _read(lambda: str(pc.player_camera_manager.get_camera_rotation()))
    for manager in managers:
        record['manager_targets'].append({'manager': _path(manager),
            'Player Pawn': _read(lambda: _path(manager.get_editor_property('Player Pawn')))})
    for actor in players:
        record['players'].append({'actor': _path(actor), 'hidden': actor.get_editor_property('hidden'),
            'location': _xyz(actor.get_actor_location()), 'rotation': str(actor.get_actor_rotation()),
            'components': [_component(c) for c in actor.get_components_by_class(unreal.PrimitiveComponent)
                           if c.get_name() in ['CharacterMesh0', 'Mesh', 'Outline', 'StaticMesh_HandR']]})
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LevelSequenceActor):
        player = actor.get_sequence_player()
        if player:
            record['sequences'].append({'actor': _path(actor), 'sequence': _read(lambda: _path(actor.get_sequence())),
                'playing': player.is_playing(), 'paused': player.is_paused(),
                'time': _read(lambda: str(player.get_current_time()))})
    with _trace_path.open('a', encoding='utf-8') as stream:
        stream.write(json.dumps(record, ensure_ascii=False, default=str) + '\n')
    summary = [(a['actor'], a['hidden'], [c.get('visible') for c in a['components']], a['location'])
               for a in record['players']]
    if summary != _last_summary:
        unreal.log('VTG PIE VISIBILITY ' + json.dumps(summary, ensure_ascii=False, default=str))
        _last_summary = summary

unreal._vtg_sa_visibility_watch = unreal.register_slate_post_tick_callback(_watch)
unreal.log('VTG PIE visibility watcher ready. Trace: ' + str(_trace_path))
