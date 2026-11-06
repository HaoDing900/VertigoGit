import unreal, json, os
r=unreal.AssetRegistryHelpers.get_asset_registry()
sa_skeletons=set()
for a in r.get_assets_by_path('/Game/Characters/Sa',recursive=True):
 if str(a.asset_class_path.asset_name)=='SkeletalMesh':
  tag=a.get_tag_value('Skeleton')
  if tag:sa_skeletons.add(str(tag).split("'")[-2] if "'" in str(tag) else str(tag))
seed=unreal.load_asset('/Game/Characters/Sa/Anm/Loco/MM_Sa_Land')
sa_skeletons.add(seed.get_editor_property('skeleton').get_path_name())
unreal.log('SA_FOOT_SCAN skeletons='+str(sorted(sa_skeletons)))
assets=[]
for typ in ['AnimSequence','AnimMontage']:
 for a in r.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine',typ),True):
  if not str(a.package_name).startswith('/Game/'):continue
  tag=str(a.get_tag_value('Skeleton') or '')
  if any(s in tag for s in sa_skeletons):assets.append(a)
unreal.log('SA_FOOT_SCAN candidates='+str(len(assets)))
rows=[]
for i,a in enumerate(assets):
 anim=a.get_asset()
 entries=[]
 for event in unreal.AnimationLibrary.get_animation_notify_events(anim):
  n=event.get_editor_property('notify');name=str(event.get_editor_property('notify_name'));cls=n.get_class().get_name() if n else ''
  sound=None
  if isinstance(n,unreal.AnimNotify_PlaySound):sound=n.get_editor_property('sound')
  st=sound.get_path_name() if sound else ''
  kind=None
  if cls=='VTGFootstepNotify':kind='VTGFootstep'
  elif 'footstep' in cls.lower() or name.lower() in ['stealth_fs','footstep','footsteps']:kind='Legacy/Event'
  elif st and any(t in sound.get_name().lower() for t in ['footstep','foot_step']):kind='PlaySound'
  if kind:
   try:time=round(float(event.get_editor_property('link_value')),4)
   except:time=None
   item={'kind':kind,'notify_class':cls,'notify_name':name,'time':time}
   if st:item['sound']=st
   if kind=='VTGFootstep':item['bone']=str(n.get_editor_property('foot_bone'));item['landing']=bool(n.get_editor_property('landing'))
   entries.append(item)
 if entries:rows.append({'path':anim.get_path_name().split('.')[0],'type':str(a.asset_class_path.asset_name),'notifies':entries})
 if i%100==0:unreal.log('SA_FOOT_SCAN progress='+str(i))
rows.sort(key=lambda x:x['path'])
root=unreal.Paths.project_dir()+'Scripts/SaFootstepInventory/'
with open(root+'inventory.json','w',encoding='utf-8') as f:json.dump({'skeletons':sorted(sa_skeletons),'scanned':len(assets),'animations':rows},f,ensure_ascii=False,indent=2)
lines=['# Sa animations with footstep notifies','',str(len(rows))+' assets from '+str(len(assets))+' candidates. Read only scan; no assets saved.','', '| Animation | Type | Notifies |','|---|---|---|']
for row in rows:
 detail='; '.join(x['kind']+' '+str(x['time'])+((' '+x.get('bone','')) if x['kind']=='VTGFootstep' else '') for x in row['notifies'])
 lines.append('| `'+row['path']+'` | '+row['type']+' | '+detail+' |')
with open(root+'inventory.md','w',encoding='utf-8') as f:f.write('\n'.join(lines)+'\n')
unreal.log('SA_FOOT_SCAN complete assets='+str(len(rows)))
