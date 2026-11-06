import pathlib,re
p=pathlib.Path('Scripts/FistWind/NS_Wind_Flows_02_Fast.copy');b=p.read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
for l in s.splitlines():
 if ('Meshes(' in l or 'Mesh=' in l or 'FacingMode=' in l or 'MeshAlignment=' in l) and len(l)<1200:print(l)
