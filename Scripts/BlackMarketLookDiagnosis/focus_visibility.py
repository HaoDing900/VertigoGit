import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
m=next(a for a in actors if a.get_class().get_name()=='BPLM_BlackMarket_C')
sa=next(a for a in actors if a.get_class().get_name()=='BP_Player_Sa_C')
for p in ['Player Pawn','L4_Seq_Intro_SaObserveIPHA']:
 try:unreal.log('FOCUS manager '+p+'='+str(m.get_editor_property(p)))
 except Exception as e:unreal.log('FOCUS '+str(e))
for c in sa.get_components_by_class(unreal.PostProcessComponent):
 unreal.log('FOCUS PP '+c.get_name()+' visible='+str(c.is_visible()))
 for p in ['enabled','unbound','blend_weight','blend_radius','priority']:
  unreal.log('FOCUS '+p+'='+str(c.get_editor_property(p)))
 s=c.get_editor_property('settings')
 for p in ['auto_exposure_bias','auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_method','auto_exposure_apply_physical_camera_exposure','color_saturation','color_gain','color_gamma','film_slope','vignette_intensity','bloom_intensity','weighted_blendables']:
  try:
   override=s.get_editor_property('override_'+p)
   if override:unreal.log('FOCUS OVERRIDE '+p+'='+str(s.get_editor_property(p)))
  except:pass
 unreal.log('FOCUS blendables='+str(s.get_editor_property('weighted_blendables')))
m.set_editor_property('Player Pawn',sa)
unreal.log('FOCUS before '+str(sa.get_actor_location()))
m.call_method('L4_Position_SaIPHAInfrontOfAdultStore')
unreal.log('FOCUS direct after '+str(sa.get_actor_location()))
sa.set_actor_location(unreal.Vector(0,4000,250),False,True)
dlg=unreal.new_object(unreal.load_class(None,'/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation.L4_BlackMarketInvestigation_Still_MainInvestigation_C'),outer=m)
comp=unreal.new_object(unreal.NarrativeComponent,outer=m);dlg.set_editor_property('OwningComp',comp)
dlg.call_method('OnDialogueNode Started/Finished Playing - L4_BlackMarketInvestigation_Still_MainInvestigation_DialogueNode_NPC_227',(None,True))
unreal.log('FOCUS node227 after '+str(sa.get_actor_location()))

