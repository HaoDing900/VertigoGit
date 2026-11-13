import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/L2AfterFightPose/Bowerbird/Materials';out.mkdir(exist_ok=True)
seen=set(); rows=[]
def export(a):
 if not a or a.get_path_name() in seen:return
 seen.add(a.get_path_name());t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D();unreal.Exporter.run_asset_export_task(t)
 row={'asset':a.get_path_name()}
 for p in ['scalar_parameter_values','vector_parameter_values','base_property_overrides','blend_mode','opacity_mask_clip_value','use_material_attributes','scalar_parameters']:
  try:row[p]=str(a.get_editor_property(p))
  except Exception:pass
 rows.append(row)
 if isinstance(a,unreal.MaterialInstance):export(a.get_editor_property('parent'))
mesh=unreal.load_asset('/Game/Characters/BowerBird/Bowerbird_RIG_ue_renamed_')
for p in ['physics_asset','positive_bounds_extension','negative_bounds_extension']:
 try: unreal.log('BIRD BOUNDS asset '+p+' '+str(mesh.get_editor_property(p)))
 except Exception as e: unreal.log(str(e))
bird=unreal.get_default_object(unreal.load_asset('/Game/Characters/BowerBird/BP_Bowerbird_Still').generated_class())
for p in ['component_use_fixed_skel_bounds','use_attach_parent_bound','use_bounds_from_leader_pose_component','physics_asset_override']:
 try: unreal.log('BIRD BOUNDS component '+p+' '+str(bird.mesh.get_editor_property(p)))
 except Exception as e: unreal.log(str(e))
for slot in mesh.get_editor_property('materials'):export(slot.get_editor_property('material_interface'))
export(unreal.load_asset('/Game/Characters/BowerBird/ABP_Bowerbird_StillNPC'))
export(unreal.load_asset('/Game/Characters/BowerBird/PPABP_Bowerbird_RIG_ue_renamed__Skeleton_AnimBlueprint'))
export(unreal.load_asset('/Game/Characters/VTGCharacterShader/Player_LightColor'))
(out/'report.json').write_text(json.dumps(rows,indent=2,ensure_ascii=False),encoding='utf-8')
