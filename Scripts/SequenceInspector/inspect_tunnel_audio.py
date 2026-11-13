import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/SequenceInspector/TunnelAudio';out.mkdir(exist_ok=True)
a=unreal.load_asset('/Game/LevelNSequence/In_Level/L4Tunnel_NearMarket/LS_L3Tunnel_DontLookBack1')
assert a
t=unreal.AssetExportTask();t.object=a;t.filename=str(out/'LS_L3Tunnel_DontLookBack1.t3d');t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D();unreal.Exporter.run_asset_export_task(t)
r={'class':a.get_class().get_name(),'tracks':[],'scene':{}}
for p in ['read_only','playback_range_locked','evaluation_type','clock_source']:
 try:r['scene'][p]=str(a.get_movie_scene().get_editor_property(p))
 except Exception:pass
for tr in a.get_tracks():r['tracks'].append({'name':tr.get_name(),'type':tr.get_class().get_name(),'sections':[s.get_class().get_name() for s in tr.get_sections()]})
(out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8')
