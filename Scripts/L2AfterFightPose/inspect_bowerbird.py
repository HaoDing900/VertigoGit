import unreal, json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/L2AfterFightPose/Bowerbird'
out.mkdir(exist_ok=True)
def export(a):
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D();unreal.Exporter.run_asset_export_task(t)
paths=['/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarAfterFight','/Game/LevelNSequence/In_Level/L2BarStreet/FLS_L2BarStreet_AfterFight_Animation','/Game/LevelNSequence/In_Level/L2BarStreet/FLS_L2BarbackFlashback_Bowerbird','/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight','/Game/Characters/BowerBird/BP_Bowerbird_Still','/Game/Characters/BP_StillNPC']
for p in paths: export(unreal.load_asset(p))
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
try: export(unreal.GameplayStatics.get_world_settings(w))
except Exception as e: unreal.log('BIRD worldsettings '+str(e))
report=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if 'BPLM_L2StreetBarFight' in a.get_class().get_name(): export(a)
 if 'bowerbird' in (a.get_name()+' '+a.get_actor_label()+' '+a.get_class().get_name()).lower():
  export(a)
  row=dict(actor=a.get_path_name(),label=a.get_actor_label(),properties={},components=[])
  for p in ['hidden','actor_hidden_in_game','is_cutscene','IsCutscene?(NeedsManualDetect)','Stage','Stages','IdleAnim']:
   try: row['properties'][p]=str(a.get_editor_property(p))
   except Exception: pass
  if 'BP_Bowerbird_Still' in a.get_class().get_name():
   try:
    loc=a.get_actor_location()
    row['floor_trace']=str(unreal.SystemLibrary.line_trace_single(w,loc+unreal.Vector(0,0,200),loc-unreal.Vector(0,0,20000),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[a],unreal.DrawDebugTrace.NONE,True))
    row['movement']=str(a.character_movement.get_editor_property('movement_mode'))
   except Exception as e: row['floor_trace_error']=str(e)
  for c in a.get_components_by_class(unreal.SceneComponent):
   r=dict(name=c.get_name(),type=c.get_class().get_name(),props={})
   for p in ['visible','hidden_in_game','bounds_scale','cached_max_draw_distance','desired_max_draw_distance','skeletal_mesh_asset','anim_class']:
    try:r['props'][p]=str(c.get_editor_property(p))
    except Exception:pass
   row['components'].append(r)
  report.append(row)
(out/'map.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
