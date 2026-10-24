import pathlib,re
for p in pathlib.Path('Scripts/DroneFacing').glob('*.copy'):
 print('\nASSET',p.name)
 text=p.read_text(encoding='utf8')
 for m in re.finditer(r'^\s*Begin Object Name="([^"]+)"[^\n]*\n(.*?)^\s*End Object',text,re.M|re.S):
  name,body=m.groups()
  if any(w in body for w in ['MoveComponentTo','ModifyBone','RotateRootBone','RotationOffset','Follow','CalculateDirection','-90','90.000','SetActorRotation']):
   print(name,body[:230].replace('\n',' '))
   for line in body.splitlines():
    if 'PinName=' in line: print(' ',re.sub(r',PinType.*?(?=,DefaultValue=|,LinkedTo=|,PersistentGuid=)', '',line.strip()).split(',PersistentGuid=')[0][:480])
