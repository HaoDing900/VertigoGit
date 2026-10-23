import pathlib,re
p=pathlib.Path('Scripts/BlackMarketLookDiagnosis/autosave227.copy');data=p.read_bytes();s=data.decode('utf-16' if data[:2]==b'\xff\xfe' else 'utf-8-sig')
for m in re.finditer(r'^      Begin Object Name="(DialogueNode_NPC_\d+)"[^\n]*:DialogueTemplate\.',s,re.M):
 end=s.find('\n      End Object',m.end());b=s[m.start():end]
 if m[1]=='DialogueNode_NPC_227' or 'NE_StopLevelSequence' in b:
  print('\nNODE',m[1]);print(b[:5000])
for m in re.finditer(r'FunctionReference=.*?MemberName="StopDialogueLevelSequence"',s):print('DIRECTSTOP',s[max(0,m.start()-220):m.end()+100])
