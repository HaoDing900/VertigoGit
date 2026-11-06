from pathlib import Path
import re,json
for filename in ['Parent','Level']:
 raw=Path('Scripts/PoliceContractAudio/'+filename+'.copy').read_bytes();s=raw.decode('utf-16' if raw[:2]==b'\xff\xfe' else 'utf-8-sig');n={}
 for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^"]+:EventGraph\.[^"]+"(.*?)End Object',s,re.S):
  key,b=m.groups();fn=re.search(r'(?:FunctionReference|VariableReference)=\(.*?MemberName="([^"]+)"',b);custom=re.search(r'CustomFunctionName="([^"]+)"',b);ev=re.search(r'EventReference=\(.*?MemberName="([^"]+)"',b)
  pins=[]
  for l in b.splitlines():
   if 'CustomProperties Pin (' not in l:continue
   p=re.search(r'PinName="([^"]+)"',l).group(1);link=re.search(r'LinkedTo=\((.*?)\)',l);default=re.search(r'DefaultValue="([^"]+)"',l);pins.append((p,link.group(1) if link else default.group(1) if default else ''))
  n[key]={'fn':fn.group(1) if fn else custom.group(1) if custom else ev.group(1) if ev else key,'pins':pins}
 print('\n',filename)
 keys=['K2Node_CallFunction_29','K2Node_CallFunction_7','K2Node_VariableGet_35','K2Node_CallFunction_35','K2Node_CallFunction_42','K2Node_VariableGet_12'] if filename=='Parent' else ['K2Node_VariableGet_39','K2Node_VariableGet_19','K2Node_CustomEvent_24','K2Node_CallFunction_70']
 for k in keys: print(k,n.get(k))
 if filename=='Parent':
  visited=set()
  def back(k,d=0):
   if k in visited or d>8:return
   visited.add(k);v=n.get(k)
   if not v:return
   print(' '*d,k,v['fn'])
   for p,l in v['pins']:
    if p=='execute':
     for x in l.rstrip(',').split(','):
      if x:back(x.split()[0],d+1)
  back('K2Node_CallFunction_29');back('K2Node_CallFunction_35')
