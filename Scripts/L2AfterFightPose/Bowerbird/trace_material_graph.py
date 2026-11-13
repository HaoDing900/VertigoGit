import re
from pathlib import Path
p=Path(__file__).parent/'Materials/MM_VTGChara_RimLightShadowMap.t3d'
text=p.read_text(encoding='utf-8-sig')
nodes={m.group(1):m.group(2) for m in re.finditer(r'^   Begin Object Name="([^"]+)"[^\n]*\n(.*?)^   End Object',text,re.M|re.S)}
seen=set()
def walk(n,depth=0):
 if n in seen or depth>16:return
 seen.add(n);print(' '*depth+n)
 data=nodes.get(n,'')
 for line in data.splitlines():
  if any(x in line for x in ['MaterialExpressionEditor','MaterialExpressionGuid','Material=','ExpressionGUID=','SortPriority']):continue
  print(' '*(depth+1)+line.strip()[:600])
 for child in re.findall(r'Expression="[^"]*:([^\']+)\'"',data):walk(child,depth+2)
walk('MaterialExpressionReroute_48')
