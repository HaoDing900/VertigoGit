import unreal
root=unreal.Paths.project_dir()+'Scripts/PoliceContractAudio/'
for name,path in [('Level','/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation'),('Dialogue','/Game/Narrative/Dialogues/Levels/L3PoliceStation/L3PoliceStation_Still'),('Bello','/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/ANM_Bello_Loco_Walk_Fwd_NoRM_Mirror'),('Corvos','/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/ANM_Corvos_Walk_Fwd')]:
 a=unreal.load_asset(path)
 t=unreal.AssetExportTask();t.object=a;t.filename=root+name+'.copy';t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
