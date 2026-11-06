import re,pathlib
p=pathlib.Path('Scripts/SpatialActorAudio/BPLM_PoliceStation.copy'); b=p.read_bytes(); s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^\n]+:EventGraph\.[^\n]+\n(.*?)End Object',s,re.S):
 name,block=m.groups()
 if name in ['K2Node_CustomEvent_16','K2Node_IfThenElse_4','K2Node_IfThenElse_5'] or 'Bello' in block:
  print(name)
  for line in block.splitlines():
   if any(x in line for x in ['FunctionReference=','VariableReference=','CustomFunctionName=']): print(line.strip())
   if 'CustomProperties Pin' in line:
    pin=re.search('PinName="([^"]+)"',line); link=re.search('LinkedTo=\((.*?)\)',line)
    if pin and (link or 'DefaultObject=' in line): print(pin.group(1),link.group(1) if link else line[-240:])
