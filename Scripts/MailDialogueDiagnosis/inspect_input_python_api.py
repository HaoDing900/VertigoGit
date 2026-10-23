import unreal
for name in ['WidgetLibrary','ObjectIterator','EnhancedInputLocalPlayerSubsystem','LocalPlayer','NarrativeComponent','NarrativeUIInputComponent']:
    cls=getattr(unreal,name,None)
    unreal.log('INPUT API '+name+' '+str([x for x in dir(cls) if any(s in x for s in ['subsystem','sub_system','object','widget','dialogue','mapping'])]) if cls else 'INPUT API '+name+' absent')
cls=unreal.load_class(None,'/Game/ThirdPerson/PC_Game_VTG.PC_Game_VTG_C')
o=unreal.get_default_object(cls)
for p in ['Pawn','Player','InputComponent','PlayerInput']:
    try: unreal.log('INPUT API PROP '+p+'='+str(o.get_editor_property(p)))
    except Exception as e: unreal.log('INPUT API PROP '+str(e))
