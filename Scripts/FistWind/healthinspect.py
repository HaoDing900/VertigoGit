import re,pathlib
b=pathlib.Path('Scripts/FistWind/BP_Player_Sa.copy').read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^\n]+\n(.*?)End Object',s,re.S):
 name,block=m.groups()
 if 'CustomFunctionName="UpdateHealthByItem"' in block or 'CustomFunctionName="Punch"' in block or 'CustomFunctionName="ResetCombo"' in block:
  print(name)
  for line in block.splitlines():
   if 'CustomFunctionName=' in line:print(line.strip())
   if 'CustomProperties Pin' in line:
    pin=re.search('PinName="([^"]+)"',line);link=re.search('LinkedTo=\((.*?)\)',line)
    if pin:print(pin.group(1),link.group(1) if link else '')
for l in s.splitlines():
 if 'Begin Object Name=' in l and 'BrassKnuckle' in l:print(l[:250])
