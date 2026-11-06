from pathlib import Path
import re
for p in sorted(Path('Scripts/PoliceNotifyObservation').glob('NLS_*.copy')):
 raw=p.read_bytes();s=raw.decode('utf-16' if raw[:2]==b'\xff\xfe' else 'utf-8-sig')
 names=re.findall(r'FunctionReference=\([^\n]*MemberName="([^"]+)"',s)
 print(p.stem,sorted(set(n for n in names if any(x in n.lower() for x in ['collision','lockinput','restoreinput','playerinput','movementmode']))))
 for name in sorted(set(n for n in names if 'collision' in n.lower())):print(' COLLISION',name)
