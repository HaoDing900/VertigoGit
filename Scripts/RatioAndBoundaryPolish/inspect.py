import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/RatioAndBoundaryPolish'
bp=unreal.load_asset('/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision_BarHologram')
ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
 obj=lib.get_object(lib.get_data(h))
 if isinstance(obj,unreal.StaticMeshComponent) and obj.get_editor_property('static_mesh'):
  mesh=obj.get_editor_property('static_mesh');unreal.log('STYLE_MESH_BOUNDS '+str(mesh.get_bounds()));break
for p in ['/Game/Widget/Tutorials/WBP_Tutorial_Battle','/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu']:
 a=unreal.load_asset(p);t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
