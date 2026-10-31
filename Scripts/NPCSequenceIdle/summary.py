from pathlib import Path
import re
for p in Path('Scripts/NPCSequenceIdle').glob('*.t3d'):
 if '.cdo.' in p.name:continue
 raw=p.read_bytes();s=raw.decode('utf-16' if raw[:2]==b'\xff\xfe' else 'utf-8-sig')
 print('\n'+p.stem)
 for line in s.splitlines():
  if line.startswith('   TargetSkeleton='):print(line.strip())
 g=re.search(r'^   Begin Object Name="AnimGraph"[^\n]*\n(.*?)^   End Object',s,re.M|re.S)
 if not g:print('NO ANIMGRAPH');continue
 for m in re.finditer(r'^      Begin Object Name="(AnimGraphNode_(?:Slot|SaveCachedPose|UseCachedPose|BlendListByBool|ApplyAdditive)[^"]*)"[^\n]*\n(.*?)^      End Object',g[1],re.M|re.S):
  print(m[1],end=' ')
  for l in m[2].splitlines():
   if 'Node=(' in l or 'CacheName=' in l or 'SaveCachedPoseNode=' in l:print(l.strip(),end=' ')
   if 'CustomProperties Pin' in l:
    n=re.search(r'PinName="([^"]*)"',l)
    link=re.search(r'LinkedTo=\(([^)]+)\)',l)
    if n: print(n[1]+'->'+(link[1].split()[0] if link else '-'),end=' ')
  print()
