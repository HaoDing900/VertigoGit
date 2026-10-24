import pathlib,re,sys
p=pathlib.Path(sys.argv[1]);text=p.read_text(encoding='utf8');blocks=re.split(r'(?=^Begin Object )',text,flags=re.M)
for b in blocks:
 if any(('Name="'+n+'"') in b.split('\n')[0] for n in sys.argv[2:]): print(b[:4500])
