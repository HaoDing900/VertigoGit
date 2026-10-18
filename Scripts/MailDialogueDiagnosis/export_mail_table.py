import json
import pathlib
import unreal
table = unreal.load_asset('/Game/Narrative/Mail/DT_Mail_ID_FillHere')
api = unreal.DataTableFunctionLibrary
columns = api.get_data_table_column_names(table)
result = {'rows': [str(n) for n in api.get_data_table_row_names(table)], 'columns': {str(n): list(api.get_data_table_column_as_string(table, n)) for n in columns}}
(pathlib.Path(unreal.Paths.project_dir()) / 'Scripts/MailDialogueDiagnosis/mail_table.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
