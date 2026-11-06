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

 if filename!='Parent':continue
 seen=set()
 def forward(k,d=0):
  if k in seen or d>15 or k not in n:return
  seen.add(k);v=n[k];print(' '*d,k,v['fn'])
  for p,l in v['pins']:
   if p in ['then','else','OutputPin','Completed','Is Valid','Is Not Valid'] or p.startswith('then_'):
    for x in l.rstrip(',').split(','):
     if x:forward(x.split()[0],d+1)
 for k,v in n.items():
  if v['fn'] in ['LockPlayerInput','UnlockPlayerInput','FinishSequence','FinishSequenceForceLookReturnToPlayer'] or k=='K2Node_CustomEvent_5':
   print('ROOT');forward(k)
