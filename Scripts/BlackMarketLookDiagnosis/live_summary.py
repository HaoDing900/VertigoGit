import json,pathlib,time
p=pathlib.Path('Scripts/BlackMarketLookDiagnosis/pie-visibility.jsonl');rs=[json.loads(l) for l in p.read_text(encoding='utf-8').splitlines()]
print('file_age_seconds',round(time.time()-p.stat().st_mtime,1),'samples',len(rs))
for i in sorted(set([0,max(0,len(rs)-2),len(rs)-1])):
 r=rs[i];print('SAMPLE',i,'elapsed',r['elapsed'],'world',r['world']);print('VIEW',r.get('view_target'),r.get('camera_location'));print('MANAGER',r['manager_targets']);print('SEQUENCES',r['sequences'])
 for a in r['players']:
  print('PLAYER',a['actor'],'position',a['location'],'hidden',a['hidden'])
  for c in a['components']:
   if c['path'].endswith('CharacterMesh0'): print('MESH',{k:c.get(k) for k in ['visible','hidden_in_game','owner_no_see','world_location','relative_location','root_world_location','animation_mode']})
print('ALL_LOCATIONS',sorted(set(tuple(a['location']) for r in rs for a in r['players'])))
