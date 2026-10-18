import pathlib
import shutil
import unreal

asset = unreal.load_asset('/Game/ThirdPerson/PC_Game_VTG')
parent = unreal.load_class(None, '/Script/Vertigo.VTGPlayerController')
assert asset and parent
source = pathlib.Path(unreal.Paths.project_content_dir()) / 'ThirdPerson/PC_Game_VTG.uasset'
backup = pathlib.Path(unreal.Paths.project_saved_dir()) / 'Backups/NoteInputOwnership/PC_Game_VTG.uasset'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(source, backup)
unreal.BlueprintEditorLibrary.reparent_blueprint(asset, parent)
unreal.BlueprintEditorLibrary.compile_blueprint(asset)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log('NOTE INPUT: project player controller now guards modal input')
