import json,pathlib
rs=[json.loads(x) for x in pathlib.Path('Scripts/BlackMarketLookDiagnosis/pie-visibility.jsonl').read_text(encoding='utf-8').splitlines()]
print('samples',len(rs))
last=None
for i,r in enumerate(rs):
 k=(r.get('view_target'),str(r.get('manager_targets')),str([(a['location'],a['hidden'],[(c['path'],c.get('visible'),c.get('world_location'),c.get('root_world_location')) for c in a['components']]) for a in r['players']]),str([(s['sequence'],s['playing'],s['paused']) for s in r['sequences']]))
 if k!=last:
  print('CHANGE',i,json.dumps(r,ensure_ascii=False));last=k
