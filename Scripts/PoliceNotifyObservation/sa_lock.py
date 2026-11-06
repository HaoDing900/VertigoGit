from pathlib import Path
import re
s=Path('Scripts/PoliceFootsteps/Sa.copy').read_text(encoding='utf-16');n={}
for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^"]+:EventGraph\.[^"]+"(.*?)End Object',s,re.S):
 k,b=m.groups();fn=re.search(r'(?:FunctionReference|VariableReference)=\(.*?MemberName="([^"]+)"',b);ce=re.search(r'CustomFunctionName="([^"]+)"',b);n[k]=(fn.group(1) if fn else ce.group(1) if ce else k,b)
def walk(k,d=0,seen=None):
 if seen is None:seen=set()
 if k in seen or d>12 or k not in n:return
 seen.add(k);name,b=n[k];print(' '*d,k,name)
 for line in b.splitlines():
  if 'CustomProperties Pin (' in line and any('PinName="'+p+'"' in line for p in ['then','else','OutputPin']):
   l=re.search(r'LinkedTo=\((.*?)\)',line)
   if l:
    for x in l.group(1).rstrip(',').split(','):
     if x:walk(x.split()[0],d+1,seen)
walk('K2Node_CustomEvent_13');walk('K2Node_CustomEvent_15')

