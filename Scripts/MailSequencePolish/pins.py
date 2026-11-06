from pathlib import Path
import re
p=Path('Scripts/MailSequencePolish/BP_Player_Sa.copy');b=p.read_bytes();s=b.decode('utf-16' if b.startswith(b'\xff\xfe') else 'utf-8-sig')
for graph,node in [('SetMailData','K2Node_FunctionEntry_0'),('EventGraph','K2Node_Self_7')]:
 pattern='      Begin Object Name="'+node+'" ExportPath="[^\n]+:'+graph+'.'+node+"'\""+r'(.*?)      End Object'
 m=re.search(pattern,s,re.S)
 print(graph,node, re.findall('PinName="([^"]+)"',m[1]) if m else 'MISSING')
