import unreal
for name in ['BP_Narrative3Overlay','BP_QuestTask']:
    bp=unreal.load_asset('/Game/Narrative/NarrativeUI/Widgets/'+name)
    t=unreal.AssetExportTask()
    t.object=bp
    t.filename='G:/Epic/Unreal projects/VertigoGit/Scripts/QuestDescriptionUI/'+name+'.copy'
    t.automated=True
    t.prompt=False
    unreal.Exporter.run_asset_export_task(t)
