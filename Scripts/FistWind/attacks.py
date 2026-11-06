import re,pathlib
b=pathlib.Path('Scripts/FistWind/BP_Player_Sa.copy').read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
for l in s.splitlines():
 if 'NewVariables(' in l and any(v in l for v in ['Combo','Montage','Attack']):print(l[:300])
for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^\n]+:EventGraph\.[^\n]+\n(.*?)End Object',s,re.S):
 n,t=m.groups()
 if 'PlayMontage' in t or 'Montage_Play' in t:
  print(n)
  for l in t.splitlines():
   if 'PinName="MontageToPlay"' in l or 'PinName="AnimMontage"' in l or 'PinName="Montage' in l:print(l[:100], re.findall(r'LinkedTo=\((.*?)\)|DefaultObject="([^"]+)"',l))
