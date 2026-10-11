import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/WeaponDiagnosis'
paths=['/Game/Characters/NPC/StreetGoons/Meshes/StreetGoon3Brosky_NPC_DefaultRig','/Game/AI/AIBehaviorSystem/Demo/Characters/Mannequins/Meshes/UE5RTG_SKM_Manny','/Game/AI/AIBehaviorSystem/Demo/Characters/Mannequins/Meshes/UE5RTG_EnmAI_Mannequin']
for p in paths:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.exporter=unreal.ObjectExporterT3D();t.filename=str(out/(a.get_name()+'.t3d'));t.automated=True;t.prompt=False
 unreal.Exporter.run_asset_export_task(t)
 lines=[]
 if isinstance(a,unreal.SkeletalMesh):
  lines.append('ASSET '+a.get_path_name())
 (out/(a.get_name()+'.bones.txt')).write_text('\n'.join(lines),encoding='utf8')

