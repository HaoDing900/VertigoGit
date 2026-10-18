import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/SewerAcidPolish'
for p in ['/Game/Characters/Monster/SeaMonsterBoss/ABP_EggProducer_L3Anchored','/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_EggProducer_GotHit','/Game/Characters/Sa/Anm/Sa_Hammer/Combo/Hammer_Attack_Infinite_1_Montage','/Game/Resource/VFX/BloodPack/_Commons/Materials/MI_BloodDirection_07_Green']:
 a=unreal.load_asset(p)
 if a:
  t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+('.copy' if isinstance(a,unreal.Blueprint) else '.t3d')));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
  if isinstance(a,unreal.MaterialInstanceConstant):
   b=a.get_editor_property('parent')
   while isinstance(b,unreal.MaterialInstanceConstant): b=b.get_editor_property('parent')
   unreal.log('ACID MATERIAL DOMAIN '+str(b.get_editor_property('material_domain')))
