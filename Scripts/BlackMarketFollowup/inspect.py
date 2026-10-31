import unreal
from pathlib import Path
folder=Path('G:/Epic/Unreal projects/VertigoGit/Scripts/BlackMarketFollowup')
paths=[
 '/Game/LevelNSequence/BPLM',
 '/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay',
 '/Game/Blueprints/BPA_0SmartTrigger',
 '/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket',
 '/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation',
 '/Game/NarrativeUI/Widgets/BP_Narrative3Overlay',
 '/Game/ThirdPerson/Blueprints/E_NDialogueStyle',
]
for path in paths:
    asset=unreal.load_asset(path)
    if not asset:
        unreal.log_warning('FOLLOWUP MISSING '+path)
        continue
    task=unreal.AssetExportTask()
    task.object=asset
    task.filename=str(folder/(path.split('/')[-1]+'.t3d'))
    task.automated=True
    task.prompt=False
    task.exporter=unreal.ObjectExporterT3D()
    unreal.log('FOLLOWUP EXPORT '+path+' '+str(unreal.Exporter.run_asset_export_task(task)))
