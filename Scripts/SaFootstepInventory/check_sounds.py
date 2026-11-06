from pathlib import Path
import json
x=json.loads(Path('Scripts/SaFootstepInventory/inventory.json').read_text(encoding='utf-8'))
for a in x['animations']:
 for n in a['notifies']:
  if n['kind']=='PlaySound':print(a['path'].split('/')[-1],n['sound'])
