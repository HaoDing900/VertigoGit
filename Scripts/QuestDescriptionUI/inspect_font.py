import unreal
for path in ['/Game/NarrativeUI/Widgets/BP_QuestTask','/Game/NarrativeUI/Widgets/BP_Narrative3Overlay']:
    a=unreal.load_asset(path)
    unreal.log('RESOLVED '+path+' -> '+a.get_path_name())
t=unreal.load_asset('/Game/Narrative/NarrativeUI/Assets/DT_RichText_NarrativeQuestUpdate')
print(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(t))
