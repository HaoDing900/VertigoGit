import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/L2EyeTremor'
a=unreal.load_asset('/Game/LevelNSequence/In_Level/L2BarStreet/Anim/LSFace__l2BarFightSuprisedAnger')
t=unreal.AssetExportTask();t.object=a;t.filename=str(out/'before.t3d');t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
unreal.Exporter.run_asset_export_task(t)
unreal.log('EYE animation length '+str(a.get_editor_property('sequence_length')))
