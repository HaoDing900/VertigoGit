import re,pathlib
for name in ['NS_Wind_Flows_02_Fast','BP_Player_Sa']:
 b=pathlib.Path('Scripts/FistWind/'+name+'.copy').read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
 if name.startswith('NS'):
  print(sorted(set(re.findall(r'User\.[A-Za-z0-9 _.-]{1,45}(?=")',s))))
  for l in s.splitlines():
   if 'EmitterHandles(' in l: print(l[:350])
 else:
  for l in s.splitlines():
   if 'CustomFunctionName=' in l and any(w.lower() in l.lower() for w in ['heal','health','injur','punch','resetcombo','combat']):print(l.strip())
