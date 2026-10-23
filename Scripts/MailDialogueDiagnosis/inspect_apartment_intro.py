import unreal
paths=['/Game/LevelNSequence/In_Level/L3Apartment/LS/Intro/LS_L3_Apt_Intro','/Game/LevelNSequence/In_Level/L3Apartment/LS/Intro/FLS_L3_AptIntro_WakingUpOnBed','/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/ThirdPerson/PC_Game_VTG']
for path in paths:
    a=unreal.load_asset(path)
    t=unreal.AssetExportTask(); t.object=a; t.filename=unreal.Paths.project_dir()+'Scripts/MailDialogueDiagnosis/'+a.get_name()+'.copy'
    t.automated=True; t.prompt=False; t.replace_identical=True
    unreal.Exporter.run_asset_export_task(t)
