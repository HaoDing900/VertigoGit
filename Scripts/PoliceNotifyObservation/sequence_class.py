from pathlib import Path
import re
b=Path('Scripts/PoliceFootsteps/Sa.copy').read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
for m in re.finditer(r'Begin Object Name="K2Node_CallFunction_238" ExportPath="[^\n]+:EventGraph\.[^\n]+"(.*?)End Object',s,re.S):
 for l in m.group(1).splitlines():
  if 'PinName="ActorClass"' in l:
   print(re.search(r'DefaultObject="([^"]+)"',l).group(1))
