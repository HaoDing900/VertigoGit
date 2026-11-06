import unreal,json,re
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/IndependentNarrativeAudio'
paths=['/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1','/Game/Narrative/Dialogues/Levels/L2BarNStreetOutside/L2_Bar_NPC','/Game/Narrative/Dialogues/Levels/L2BarNStreetOutside/L2_Bar_FClient_AfterChoice']
rows=[]
for p in paths:
 bp=unreal.load_asset(p); assert bp
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 dest=out/(bp.get_name()+'.verified.copy')
 task=unreal.AssetExportTask(); task.object=bp; task.filename=str(dest); task.automated=True; task.prompt=False
 assert unreal.Exporter.run_asset_export_task(task)
 raw=dest.read_bytes(); text=raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8-sig')
 for path in sorted(set(re.findall(r"ExportPath=\"[^']+'([^']+NE_VTG_Audio_PlayOneShot_C_\d+)'\"",text))):
  event=unreal.load_object(None,path); assert event
  sound=event.get_editor_property('NewSound')
  row={'event':event.get_path_name(),'sound':sound.get_path_name() if sound else None}
  for key in ['PlaybackID','Loop','StopWhenDialogueFinished','FadeInTime','VolumeMultiplier','StopFadeOutTime']:
   row[key]=str(event.get_editor_property(key))
  assert event.get_editor_property('StopWhenDialogueFinished') and not event.get_editor_property('Loop')
  rows.append(row)
  unreal.log('INDEPENDENT_EXISTING '+json.dumps(row))
assert rows
(out/'existing-events.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
unreal.log('INDEPENDENT_EXISTING_SUCCESS '+str(len(rows)))

