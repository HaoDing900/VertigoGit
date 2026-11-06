import unreal
bp=unreal.load_asset('/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket')
cdo=unreal.get_default_object(bp.generated_class())
assert not cdo.get_editor_property('L4_CyanConvHappened?')
unreal.log('CYAN_DEFAULT PASS: Blueprint default false')
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
# Inspect serialized map actors without starting gameplay.
for obj in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if obj.get_class() == bp.generated_class():
        assert not obj.get_editor_property('L4_CyanConvHappened?')
        unreal.log('CYAN_DEFAULT PASS: map instance false: '+obj.get_name())
asset=unreal.load_asset('/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation')
t=unreal.AssetExportTask();t.object=asset;t.filename=unreal.Paths.project_dir()+'Scripts/CyanOrderFlow/after.copy';t.automated=True;t.prompt=False
assert unreal.Exporter.run_asset_export_task(t)

