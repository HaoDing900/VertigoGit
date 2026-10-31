import json,math
from pathlib import Path
d=json.loads(Path('Scripts/InjuredScale/preview-trace.json').read_text())
print('ERROR',d.get('error'))
for case in range(3):
    samples=[s for s in d['samples'] if s['case']==case]
    print('CASE',case,'SAMPLES',len(samples),'LOW',set(s['low'] for s in samples))
    if not samples:continue
    for name in d['bones']:
        rows=[s['bones'][name] for s in samples]
        scale_delta=max(max(r['scale'][i] for r in rows)-min(r['scale'][i] for r in rows) for i in range(3))
        length_delta=max(r['length'] for r in rows)-min(r['length'] for r in rows)
        if scale_delta>.001 or length_delta>.01 or name=='hand_l':
            print(name,'scale range',scale_delta,'length range',length_delta,'first scale',rows[0]['scale'])
