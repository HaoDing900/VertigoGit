import re
from pathlib import Path
for name in ['BPLM_PoliceStation','L3PoliceStation_Still']:
 data=Path('Scripts/BelloSequenceDiagnosis/'+name+'.copy').read_bytes();text=data.decode('utf-16' if data.startswith(b'\xff\xfe') else 'utf-8-sig')
 nodes={}
 for m in re.finditer(r'(?ms)^      Begin Object Name="([^"]+)"[^\n]*:EventGraph\.[^\n]*\n(.*?)^      End Object',text):
  n,b=m.groups();nodes[n]=b
 def show(n,depth=0,seen=None):
  if seen is None:seen=set()
  if n in seen or depth>9:return
  seen.add(n);b=nodes.get(n,'')
  refs=re.findall(r'(?:MemberName|CustomFunctionName)="([^"]+)"',b)
  print('  '*depth+n+' '+','.join(dict.fromkeys(refs)))
  for l in b.splitlines():
   pn=re.search(r'PinName="([^"]+)"',l)
   if not pn:continue
   p=pn[1]
   if p in ['Condition','NewTransform','NewLocation','L3_HadBelloConv?','L3_Bello_Helped','self']:
    link=re.search(r'LinkedTo=\(([^)]+)',l); val=re.search(r'Default(?:Value|Object)="([^"]*)"',l)
    print('  '*depth+' '+p+' '+(link[1] if link else val[1] if val else ''))
   if p in ['then','else'] and 'LinkedTo=' in l:
    for nxt in re.findall(r'(K2Node_\w+) [A-F0-9]{32}',l):show(nxt,depth+1,seen)
 if name.startswith('L3Police'):
  for n,b in nodes.items():
   if re.search(r'CustomFunctionName="OnDialogueNode Started/Finished Playing - L3PoliceStation_Still_DialogueNode_NPC_(12|30)"',b):show(n)
 else:
  for n,b in nodes.items():
   if 'CustomFunctionName="L3_SetBelloIdle"' in b:show(n)

