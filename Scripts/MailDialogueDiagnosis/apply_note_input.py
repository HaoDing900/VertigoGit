import pathlib
import shutil
import unreal

asset_path = '/Game/Inventory/InventoryXBased/UMG/WB_NoteMainWidget'
asset = unreal.load_asset(asset_path)
assert asset
parent = unreal.load_class(None, '/Script/Vertigo.VTGNoteInputWidget')
assert parent
source = pathlib.Path(unreal.Paths.project_content_dir()) / 'Inventory/InventoryXBased/UMG/WB_NoteMainWidget.uasset'
backup = pathlib.Path(unreal.Paths.project_saved_dir()) / 'Backups/NoteInputOwnership/WB_NoteMainWidget.uasset'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(source, backup)
unreal.BlueprintEditorLibrary.reparent_blueprint(asset, parent)
unreal.BlueprintEditorLibrary.compile_blueprint(asset)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log('NOTE INPUT: migrated root to VTGNoteInputWidget')
