import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/HUDUnification'
paths=['/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/Blueprints/Widget/Unused/WBP_LegacyActionHUD','/Game/Blueprints/Widget/WBP_Action','/Game/LevelNSequence/BPLM','/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight','/NarrativeCommonUI/Widgets/WBP_NarrativeHUD','/NarrativeCommonUI/Widgets/WBP_NarrativeCommonBoundActionButton','/NarrativeCommonUI/ControllerData/ControllerData_PC_Keyboard','/NarrativeCommonUI/ControllerData/ControllerData_PC_Gamepad_PS4','/NarrativeCommonUI/ControllerData/ControllerData_PC_Gamepad_Xbox']
for p in paths:
 a=unreal.load_asset(p)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
