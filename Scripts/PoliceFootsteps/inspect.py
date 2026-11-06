import unreal, json
root=unreal.Paths.project_dir()+'Scripts/PoliceFootsteps/'
for name,path in [('Sa','/Game/ThirdPerson/Blueprints/BP_Player_Sa'),('Dialogue','/Game/Narrative/Dialogues/Levels/L3PoliceStation/L3PoliceStation_Still'),('Level','/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation')]:
 a=unreal.load_asset(path)
 t=unreal.AssetExportTask();t.object=a;t.filename=root+name+'.copy';t.automated=True;t.prompt=False
 unreal.Exporter.run_asset_export_task(t)
r=unreal.AssetRegistryHelpers.get_asset_registry()
for a in r.get_assets_by_path('/Game',recursive=True):
 if str(a.asset_class_path.asset_name) in ['Material','MaterialInstanceConstant']:
  p=str(a.package_name)
  if 'M_Wall_Concrete1' in p:
   x=unreal.load_asset(p)
   unreal.log('CONCRETE '+p+' parent='+str(x.get_editor_property('parent') if isinstance(x,unreal.MaterialInstanceConstant) else None))
