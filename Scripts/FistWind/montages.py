import re,pathlib
b=pathlib.Path('Scripts/FistWind/BP_Player_Sa.copy').read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
print('\n'.join(sorted(set(re.findall(r'/Game/[^\s"\x27,()]*?(?:Punch|Fist|Combo)[^\s"\x27,()]*',s)))))
