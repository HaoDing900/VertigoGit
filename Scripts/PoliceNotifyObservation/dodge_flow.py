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

for k in ['K2Node_VariableSet_69','K2Node_IfThenElse_74','K2Node_Knot_82','K2Node_CallFunction_196','K2Node_MacroInstance_12']:
 print(k,nodes.get(k))
seen=set()
def forward(k,d=0):
 if k in seen or d>9 or k not in nodes:return
 seen.add(k);v=nodes[k];print(' '*d,k,v['fn'])
 for p,l in v['pins']:
  if p in ['then','else','Completed','OutputPin','Is Valid','Is Not Valid','On Completed','On Interrupted','On Blend Out']:
   for x in l.rstrip(',').split(','):
    if x:forward(x.split()[0],d+1)
forward('K2Node_VariableSet_69')
