from pathlib import Path
import re
raw=Path('Scripts/PoliceFootsteps/Sa.copy').read_bytes();s=raw.decode('utf-16' if raw[:2]==b'\xff\xfe' else 'utf-8-sig');nodes={}
for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^"]+:EventGraph\.[^"]+"(.*?)End Object',s,re.S):
 k,b=m.groups();fn=re.search(r'(?:FunctionReference|VariableReference)=\(.*?MemberName="([^"]+)"',b);ce=re.search(r'CustomFunctionName="([^"]+)"',b);pins=[]
 for l in b.splitlines():
  if 'CustomProperties Pin (' not in l:continue
  match=re.search(r'PinName="([^"]+)"',l)
  if not match:continue
  p=match.group(1);link=re.search(r'LinkedTo=\((.*?)\)',l);val=re.search(r'DefaultValue="([^"]+)"',l);pins.append((p,link.group(1) if link else val.group(1) if val else ''))
 nodes[k]={'fn':fn.group(1) if fn else ce.group(1) if ce else k,'pins':pins,'set':'.K2Node_VariableSet' in m.group(0)}
for k,n in nodes.items():
 if n['fn']=='IsDodging?' and n['set']:
  print(k,n)
  for p,l in n['pins']:
   if p=='execute':
    for x in l.rstrip(',').split(','):
     if x:print(' UPSTREAM',x.split()[0],nodes.get(x.split()[0]))
