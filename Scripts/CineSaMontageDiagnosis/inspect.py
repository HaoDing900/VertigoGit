import unreal
for path in ['/Game/Characters/Sa/Anm/EmoteNGestures/ANM_Sa_Emote_Think_Montage', '/Game/Characters/Sa/ABP_SaCine', '/Game/Characters/Sa/BP_Cine_Sa']:
    asset=unreal.load_asset(path)
    unreal.log('CINE INSPECT '+str(asset))
    if isinstance(asset,unreal.AnimMontage):
        for prop in ['slot_anim_tracks','skeleton','blend_in','blend_out','sequence_length','composite_sections']:
            try: unreal.log('CINE PROPERTY '+prop+'='+str(asset.get_editor_property(prop)))
            except Exception as e: unreal.log(str(e))
    task=unreal.AssetExportTask()
    task.object=asset
    task.filename=unreal.Paths.project_dir()+'Scripts/CineSaMontageDiagnosis/'+asset.get_name()+'.copy'
    task.automated=True
    task.prompt=False
    task.replace_identical=True
    unreal.log('CINE EXPORT '+str(unreal.Exporter.run_asset_export_task(task)))
