import unreal
bp=unreal.load_asset('/Game/Narrative/NarrativeUI/Widgets/BP_QuestTask')
t=unreal.load_asset('/Game/Narrative/NarrativeUI/Assets/DT_RichText_NarrativeQuestUpdate')
print('JSONMETHODS', [x for x in dir(unreal.DataTableFunctionLibrary) if 'json' in x])
task=unreal.AssetExportTask()
task.object=t
task.filename=r'G:/Epic/Unreal projects/VertigoGit/Scripts/QuestDescriptionUI/styles.json'
task.automated=True
task.prompt=False
print('EXPORT',unreal.Exporter.run_asset_export_task(task))
print('DEPENDENCIES',unreal.EditorAssetLibrary.find_package_referencers_for_asset('/Game/Narrative/NarrativeUI/Widgets/BP_QuestTask'))
