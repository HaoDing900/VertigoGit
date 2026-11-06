import json
from pathlib import Path
x=json.loads(Path('Scripts/SaFootstepInventory/inventory.json').read_text(encoding='utf-8'))
for a in x['animations']:print(a['path'],sorted(set(n['kind'] for n in a['notifies'])))
