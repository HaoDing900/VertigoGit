import unreal,pathlib,json
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/FadeCustomization'
p='/Game/2DArt/ScreenTransition/WBP_Fade';b=unreal.load_asset(p);c=unreal.get_default_object(unreal.load_object(None,p+'.WBP_Fade_C'))
report={n:str(c.get_editor_property(n)) for n in ['TimeBeforeKill','HoldDurationUntilFade','DisableFade?']}
refs=unreal.AssetRegistryHelpers.get_asset_registry().get_referencers(p,unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True))
report['referencers']=[str(x) for x in refs]
(out/'before.json').write_text(json.dumps(report,indent=2),encoding='utf8')
for p in refs:
 a=unreal.load_asset(str(p))
 if isinstance(a,unreal.Blueprint):
  t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
