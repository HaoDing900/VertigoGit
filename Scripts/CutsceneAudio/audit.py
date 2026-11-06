import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/CutsceneAudio';ar=unreal.AssetRegistryHelpers.get_asset_registry()
for p in ['/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager','/Game/LevelNSequence/BPLM','/Game/LevelNSequence/LevelAudio/BPLM_AudioExample','/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager_Example','/Game/LevelNSequence/LevelAudio/NE/NE_VTG_Audio_PlayPsych','/Game/LevelNSequence/LevelAudio/NE/NE_VTG_Audio_StopPsych']:
 a=unreal.load_asset(p);assert a,p
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 unreal.log('PSY_REFERENCERS '+p+' '+str(ar.get_referencers(p,unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True))))
