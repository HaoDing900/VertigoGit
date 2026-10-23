import unreal
seen=set()
def visit(p):
 if p in seen:return
 seen.add(p)
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/'+a.get_name()+'.t3d';t.automated=True;t.prompt=False;t.replace_identical=True
 unreal.Exporter.run_asset_export_task(t)
 for tr in a.get_master_tracks():
  for s in tr.get_sections():
   if isinstance(s,unreal.MovieSceneSubSection):
    child=s.get_sequence()
    if child:visit(child.get_path_name())
visit('/Game/LevelNSequence/In_Level/L2BarStreet/FLS_L2BarbackFlashback_Bowerbird')
