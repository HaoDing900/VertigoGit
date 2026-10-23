import pathlib,re
s=pathlib.Path('Saved/Logs/Vertigo.log').read_text(encoding='utf-8',errors='replace').splitlines()
for i,l in enumerate(s):
 if 'Hit breakpoint on node' in l and any('NPC_227' in x for x in s[i:i+12]):
  print('\nEVENT227',l)
  for x in s[max(0,i-80):i]:
   if 'Starting new camera cut' in x:print(x)
