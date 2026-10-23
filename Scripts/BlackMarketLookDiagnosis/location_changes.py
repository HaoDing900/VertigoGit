import json,pathlib
p=pathlib.Path('Scripts/BlackMarketLookDiagnosis/pie-visibility.jsonl');last=None
for i,l in enumerate(p.read_text(encoding='utf-8').splitlines()):
 r=json.loads(l)
 key=(r['world'],str([(a['location'],a['hidden'],[(c['path'].split('.')[-1],c.get('visible'))for c in a['components']])for a in r['players']]),str([(s['sequence'],s['playing'],s['paused'])for s in r['sequences']]))
 if key!=last:print('CHANGE',i,'time',r['elapsed'],'players',[(a['location'],a['hidden'])for a in r['players']],'sequences',r['sequences']);last=key
