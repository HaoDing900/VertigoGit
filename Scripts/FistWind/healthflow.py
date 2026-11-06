import pathlib,re
b=pathlib.Path('Scripts/FistWind/BP_Player_Sa.copy').read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
nodes={}
for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^\n]+:EventGraph\.[^\n]+\n(.*?)End Object',s,re.S): nodes[m[1]]=m[2]
queue=['K2Node_CustomEvent_20'];seen=set()
while queue and len(seen)<35:
 n=queue.pop(0)
 if n in seen: continue
 seen.add(n);block=nodes.get(n,'');print(n)
 for line in block.splitlines():
  if any(x in line for x in ['FunctionReference=','VariableReference=']):print(line.strip())
  if 'CustomProperties Pin' in line:
   pin=re.search('PinName="([^"]+)"',line);link=re.search('LinkedTo=\((.*?)\)',line)
   if pin and link:
    print(pin[1],link[1])
    if 'PinCategory="exec"' in line and 'Direction="EGPD_Output"' in line:queue+=re.findall(r'(K2Node_\w+) ',link[1])
