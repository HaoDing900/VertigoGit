"""Read-only snapshot of the current PIE input state (UE 5.3 APIs verified)."""
import unreal
import pathlib

lines = []
def log(message):
    text = '[ApartmentInput] ' + str(message)
    lines.append(text)
    unreal.log(text)
def read(label, operation):
    try:
        value = operation()
        log(label + ' = ' + str(value))
        return value
    except Exception as exc:
        log(label + ' = unavailable (' + str(exc) + ')')
        return None

def inspect():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:
        log('No active PIE world. Keep the stuck PIE session running; do not Stop PIE.')
        return
    prefix = world.get_path_name()
    log('World = ' + prefix)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if not pc:
        log('No PlayerController 0')
        return
    log('Controller = ' + pc.get_path_name())
    pawn = read('ControlledPawn', lambda: pc.get_controlled_pawn())
    read('MoveInputIgnored', lambda: pc.is_move_input_ignored())
    read('LookInputIgnored', lambda: pc.is_look_input_ignored())
    read('ShowMouseCursor', lambda: pc.get_editor_property('show_mouse_cursor'))
    read('Paused', lambda: unreal.GameplayStatics.is_game_paused(world))
    contexts = {}
    for name in ['ThirdPersonIMC', 'FixedCamIMC', 'CurrentIMC']:
        contexts[name] = read(name, lambda n=name: pc.get_editor_property(n))
    for sub in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem):
        if 'Default__' in sub.get_path_name():
            continue
        try:
            controller = sub.get_outer().get_editor_property('PlayerController')
        except:
            controller = None
        log('EnhancedInputSubsystem = ' + sub.get_path_name() + ' controller=' + str(controller))
        if controller and controller != pc:
            continue
        for name, context in contexts.items():
            if context:
                read(name + '.IsApplied', lambda c=context,s=sub: s.has_mapping_context(c))
    if pawn:
        for prop in ['IsInCineCutscene_LockInput(NotWorkForMont)', 'IsReadingMail', 'FixedCameraLevel?']:
            read('Pawn.' + prop, lambda p=prop: pawn.get_editor_property(p))
        read('Pawn.Collision', lambda: pawn.get_actor_enable_collision())
        read('Pawn.MovementMode', lambda: pawn.get_movement_component().get_editor_property('movement_mode'))
    # Include shared Narrative components on actors other than the Controller/Pawn.
    for comp in unreal.ObjectIterator(unreal.ActorComponent):
        if comp.get_path_name().startswith(prefix) and 'Narrative' in comp.get_class().get_name():
            log('NarrativeComponent = ' + comp.get_path_name())
            if hasattr(comp, 'get_active_owner'):
                read('  ActiveInputOwner', lambda c=comp: c.get_active_owner())
                read('  HasInputOwners', lambda c=comp: c.has_owners())
            if hasattr(comp, 'get_current_dialogue'):
                read('  CurrentDialogue', lambda c=comp: c.get_current_dialogue())
    for dialogue in unreal.ObjectIterator(unreal.Dialogue):
        if dialogue.get_path_name().startswith(prefix):
            log('DialogueInstance = ' + dialogue.get_path_name())
            for prop in ['OwningComp', 'OwningController', 'bFreeMovement']:
                read('  ' + prop, lambda d=dialogue,p=prop: d.get_editor_property(p))
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False)
    for widget in widgets or []:
        if widget.is_visible() and (widget.is_in_viewport() or widget.has_user_focus(pc) or widget.has_user_focused_descendants(pc) or isinstance(widget, unreal.VTGNoteInputWidget)):
            log('VisibleWidget = ' + widget.get_path_name())
            read('  UserFocus', lambda w=widget: w.has_user_focus(pc))
            read('  FocusedDescendants', lambda w=widget: w.has_user_focused_descendants(pc))
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        if 'BPLM_SaApartment' in name:
            for prop in ['Player Pawn', 'Player Controller', 'bCameraLocked', 'bHasCameraLockUsedInputLock']:
                read('Manager.' + prop, lambda a=actor,p=prop: a.get_editor_property(p))
        if isinstance(actor, unreal.LevelSequenceActor):
            player = actor.get_editor_property('SequencePlayer')
            if player and (player.is_playing() or player.is_paused()):
                read('ActiveSequence', lambda a=actor,p=player: str(a.get_sequence()) + ' playing=' + str(p.is_playing()) + ' paused=' + str(p.is_paused()))

try:
    inspect()
except Exception as exc:
    log('Snapshot stopped: ' + str(exc))
finally:
    destination = pathlib.Path(unreal.Paths.project_saved_dir()) / 'Logs' / 'ApartmentInputSnapshot.txt'
    destination.write_text('\n'.join(lines), encoding='utf-8')
    unreal.log('[ApartmentInput] Report: ' + str(destination))
