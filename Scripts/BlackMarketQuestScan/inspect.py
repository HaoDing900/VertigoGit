import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/BlackMarketQuestScan'
paths=['/Game/Narrative/Dialogues/Levels/L4BlackMarket/Q_L4BlackMarketInvestigation','/Narrative/DefaultEvents/NE_BeginQuest','/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation','/Game/Characters/Drone_IPHA/BP_Drone_IPHA_NPC']
reg=unreal.AssetRegistryHelpers.get_asset_registry()
for a in reg.get_all_assets():
 n=str(a.asset_name)
 if '3Overlay' in n or ('Quest' in n and ('Overlay' in n or 'Tracker' in n)):
  paths.append(str(a.package_name))
for p in paths:
 a=unreal.load_asset(p)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 unreal.log('QUEST_SCAN_EXPORT '+p)
unreal.log('QUEST_SCAN_DONE')

