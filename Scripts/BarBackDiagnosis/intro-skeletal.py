import unreal
p='/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarStreet_IntroScene/FLS_L2BarStreet_IntroScene_SkeletalAnim'
a=unreal.load_asset(p)
t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/IntroSkeletal.t3d';t.automated=True;t.prompt=False;t.replace_identical=True
unreal.Exporter.run_asset_export_task(t)
for b in a.get_bindings():
 unreal.log('SUBCHECK binding '+b.get_name())
 for tr in b.get_tracks():
  unreal.log('SUBCHECK track '+tr.get_class().get_name())
  for s in tr.get_sections():
   if isinstance(s,unreal.MovieSceneSkeletalAnimationSection):
    unreal.log('SUBCHECK anim '+str(s.get_editor_property('params')))
