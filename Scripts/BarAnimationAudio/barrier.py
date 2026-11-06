import re,pathlib
p=pathlib.Path('Scripts/L2FightDiagnosis/BPLM_L2StreetBarFight.copy');b=p.read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^\n]+\n(.*?)End Object',s,re.S):
 n,t=m.groups()
 if 'SpawnActorFromClass' in n or 'Trigger Enemy Fight' in t or 'TriggerEnemyFight' in t:
  print(n)
  for l in t.splitlines():
   if 'CustomFunctionName=' in l:print(l.strip())
   if 'PinName="Class"' in l:print(re.findall(r'DefaultObject="([^"]+)"',l))
