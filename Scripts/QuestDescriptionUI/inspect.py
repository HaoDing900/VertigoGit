import unreal
bp=unreal.load_asset('/Narrative/NarrativeUI/Widgets/Editor/WBP_DefaultQuestNode')
t=unreal.AssetExportTask()
t.object=bp
t.filename=r'G:/Epic/Unreal projects/VertigoGit/Scripts/QuestDescriptionUI/QuestWidget.copy'
t.automated=True
t.prompt=False
unreal.Exporter.run_asset_export_task(t)
