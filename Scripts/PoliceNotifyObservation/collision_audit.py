import re
from pathlib import Path
for filename in ['Scripts/PoliceContractAudio/Level.copy','Scripts/PoliceContractAudio/Parent.copy','Scripts/CorvosSequenceRegression/NLS_L3_CorvosConv_SaApproach.copy','Scripts/PoliceFootsteps/Sa.copy']:
 raw=Path(filename).read_bytes();s=raw.decode('utf-16' if raw[:2] in [b'\xff\xfe',b'\xfe\xff'] else 'utf-8-sig')
 print('\nFILE',filename)
 for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="([^"]+)"(.*?)End Object',s,re.S):
  name,path,b=m.groups()
  if ':ExecuteUbergraph_' in path:continue
  fn=re.search(r'FunctionReference=\(.*?MemberName="([^"]+)"',b)
  if fn and any(t in fn.group(1) for t in ['Collision','MovementMode','Sa_LockInput','Sa_RestoreInput']):
   print(name,fn.group(1),'context='+path.split(':')[-1])
   for line in b.splitlines():
    if 'CustomProperties Pin (' in line:
     pname=re.search(r'PinName="([^"]+)"',line).group(1)
     if pname in ['execute','then','self','NewType','NewCollisionType','bNewActorEnableCollision','MovementMode','NewMovementMode','Channel','NewResponse']:
      links=re.search(r'LinkedTo=\((.*?)\)',line);default=re.search(r'DefaultValue="([^"]+)"',line)
      print(' ',pname,links.group(1) if links else default.group(1) if default else '')
