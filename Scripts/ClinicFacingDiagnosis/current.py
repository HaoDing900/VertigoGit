import unreal
from pathlib import Path
a=unreal.load_asset('/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic')
t=unreal.AssetExportTask();t.object=a;t.filename=str(Path(unreal.Paths.project_dir())/'Scripts/ClinicFacingDiagnosis/BPLM_Clinic.before-placement.t3d');t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
unreal.log('PLACEMENT SNAPSHOT '+str(unreal.Exporter.run_asset_export_task(t)))
