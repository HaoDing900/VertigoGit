import json,pathlib
for p in pathlib.Path('Scripts/BattleBoundaryStyle').glob('*.json'):
 d=json.loads(p.read_text());print(p.name,d['actor'])
 for c in d['components']:
  if 'static_mesh' in c:print(c['name'],c['static_mesh'].split("'")[1],c.get('relative_location'),c.get('relative_scale3d'))
  if c['class']=='BoxComponent':print(c['name'],c.get('box_extent'),c['relative'])
