from pathlib import Path
import re,json
s=Path('Scripts/PoliceContractAudio/Level.copy').read_text(encoding='utf-8-sig')
nodes={}
for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^"]+:EventGraph\.[^"]+"(.*?)End Object',s,re.S):
 name,b=m.groups(); typ=re.search(r'(?:FunctionReference|VariableReference)=\(.*?MemberName="([^"]+)"',b); custom=re.search(r'CustomFunctionName="([^"]+)"',b)
 pins=[]
 for line in b.splitlines():
  if 'CustomProperties Pin (' not in line:continue
  pname=re.search(r'PinName="([^"]+)"',line).group(1)
  links=re.search(r'LinkedTo=\((.*?)\)',line); default=re.search(r'Default(?:Value|Object)="([^"]+)"',line)
  pins.append((pname,links.group(1) if links else default.group(1) if default else ''))
 nodes[name]={'fn':typ.group(1) if typ else custom.group(1) if custom else name,'pins':pins}
for name,n in nodes.items():
 if any(t in n['fn'] for t in ['Contract','SetPawn','SetActorLocationAndRotation']):
  print(name,json.dumps(n,ensure_ascii=True))
Path('Scripts/PoliceContractAudio/graph.json').write_text(json.dumps(nodes,indent=2),encoding='utf-8')
