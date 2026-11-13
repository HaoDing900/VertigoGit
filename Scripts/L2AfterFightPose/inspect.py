import unreal, json
from pathlib import Path
out = Path(unreal.Paths.project_dir()) / 'Scripts/L2AfterFightPose'
seen = set()
report = []
def scan(seq):
    if seq.get_path_name() in seen: return
    seen.add(seq.get_path_name())
    task = unreal.AssetExportTask()
    task.object = seq
    task.filename = str(out / (seq.get_name() + '.t3d'))
    task.automated = True
    task.prompt = False
    task.exporter = unreal.ObjectExporterT3D()
    unreal.Exporter.run_asset_export_task(task)
    row = dict(path=seq.get_path_name(), rate=str(seq.get_display_rate()), start=seq.get_playback_start(), end=seq.get_playback_end(), tracks=[])
    tracks = [('master', t) for t in seq.get_tracks()]
    for b in seq.get_bindings():
        tracks += [(b.get_name(), t) for t in b.get_tracks()]
    for name, t in tracks:
        tr = dict(binding=name, type=t.get_class().get_name(), sections=[])
        for s in t.get_sections():
            sr = dict(type=s.get_class().get_name(), start=s.get_start_frame() if s.has_start_frame() else None, end=s.get_end_frame() if s.has_end_frame() else None)
            for p in ['params', 'animation_mode', 'completion_mode']:
                try: sr[p] = str(s.get_editor_property(p))
                except Exception: pass
            if isinstance(s, unreal.MovieSceneSubSection):
                child = s.get_sequence()
                sr['child'] = child.get_path_name() if child else None
                sr['parameters'] = str(s.get_editor_property('parameters'))
                if child: scan(child)
            tr['sections'].append(sr)
        row['tracks'].append(tr)
    report.append(row)
scan(unreal.load_asset('/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarAfterFight'))
(out / 'audit.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
