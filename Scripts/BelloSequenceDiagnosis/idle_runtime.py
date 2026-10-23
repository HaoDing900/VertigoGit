import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream')
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=es.get_all_level_actors()
bello=next(x for x in actors if x.get_name()=='BP_Bello_NPC_C_1')
cls=unreal.load_class(None,'/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation.BPLM_PoliceStation_C')
manager=next((x for x in actors if x.get_class()==cls),None) or es.spawn_actor_from_class(cls,unreal.Vector(0,0,0))
manager.set_editor_property('Bello',bello)
dlg=unreal.new_object(unreal.load_class(None,'/Game/Narrative/Dialogues/Levels/L3PoliceStation/L3PoliceStation_Still.L3PoliceStation_Still_C'),outer=manager)
comp=unreal.new_object(unreal.NarrativeComponent,outer=manager)
dlg.set_editor_property('OwningComp',comp)
initial=bello.get_actor_location()
def log(label):
 unreal.log('IDLEFLOW '+label+' HadConv='+str(manager.get_editor_property('L3_HadBelloConv?'))+' Helped='+str(manager.get_editor_property('L3_Bello_Helped'))+' location='+str(bello.get_actor_location()))
log('loaded')

for node in [12,30]:
 dlg.call_method('OnDialogueNode Started/Finished Playing - L3PoliceStation_Still_DialogueNode_NPC_'+str(node),(None,True))
 log('after actual NPC_'+str(node)+' start callback')
manager.call_method('L3_BackgroundConv_BelloCar')
manager.call_method('L3_SetBelloIdle')
log('control HadConv=true then same event')


