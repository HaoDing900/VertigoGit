from pathlib import Path
import re
for p in Path('Scripts/RatioAndBoundaryPolish').glob('WBP*.copy'):
 b=p.read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig');print(p.name)
 for l in s.splitlines():
  if l.strip().startswith('Text='):print(l.strip()[:400])
