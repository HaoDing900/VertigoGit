import unreal
for p, props in [('/Game/ThirdPerson/PC_Game_VTG',['ThirdPersonIMC','FixedCamIMC']),('/Game/ThirdPerson/Blueprints/BP_Player_Sa',['IsInCineCutscene_LockInput(NotWorkForMont)','IsReadingMail'])]:
    o=unreal.get_default_object(unreal.load_class(None,p+'.'+p.rsplit('/',1)[-1]+'_C'))
    for prop in props:
        try: unreal.log('INPUT DEFAULT '+p+' '+prop+'='+str(o.get_editor_property(prop)))
        except Exception as e: unreal.log(str(e))
a=unreal.load_asset('/Game/ThirdPerson/Input/IMC_Default');unreal.log('DEFAULT IMC LOAD='+str(a))

