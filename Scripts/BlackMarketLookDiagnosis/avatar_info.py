import unreal
cls=unreal.load_class(None,'/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation.L4_BlackMarketInvestigation_Still_MainInvestigation_C')
d=unreal.get_default_object(cls)
for p in ['PlayerSpeakerInfo','Speakers']:
 try:unreal.log('HIDEDBG '+p+'='+str(d.get_editor_property(p)))
 except Exception as e:unreal.log(str(e))
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if any(s in a.get_class().get_name() for s in ['BP_Player_Sa','BP_Cine_Sa']):unreal.log('HIDEDBG actor '+a.get_path_name()+' tags='+str(a.tags)+' hidden='+str(a.is_hidden()))
