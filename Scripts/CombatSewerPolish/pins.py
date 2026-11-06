from pathlib import Path
import re
p=Path('Scripts/CombatSewerPolish/BPLM_SewerUnderApt.copy');b=p.read_bytes();s=b.decode('utf-16' if b.startswith(b'\xff\xfe') else 'utf-8-sig')
for node in ['K2Node_CallFunction_5','K2Node_CallFunction_11']:
 m=re.search('      Begin Object Name="'+node+'" ExportPath="[^\n]+:EventGraph.'+node+"'\""+r'(.*?)      End Object',s,re.S)
 print(node)
 if m:
  for line in m[1].splitlines():
   if 'CustomProperties Pin ' not in line:continue
   n=re.search('PinName="([^"]+)"',line);v=re.search('Default(?:Value|Object)="([^"]+)"',line)
   if n and v:print(n[1],v[1])
