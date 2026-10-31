import unreal
from pathlib import Path
p=Path(unreal.Paths.project_dir())/'Scripts/InjuredScale/physics.t3d'
task=unreal.AssetExportTask()
task.object=unreal.load_asset('/Game/Characters/Sa/Sa_PhysicsAsset_WithHairNCape')
task.filename=str(p)
task.automated=True
task.prompt=False
task.exporter=unreal.ObjectExporterT3D()
unreal.log('PHYSICS_EXPORT '+str(unreal.Exporter.run_asset_export_task(task)))
