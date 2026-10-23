import pathlib,re
p=pathlib.Path('Scripts/BlackMarketLookDiagnosis/L4_BlackMarketInvestigation_Still_MainInvestigation.current.copy');data=p.read_bytes();s=data.decode('utf-16' if data[:2]==b'\xff\xfe' else 'utf-8-sig');ns={}
for m in re.finditer(r'Begin Object Name="([^"]+)"[^\n]*:EventGraph\.',s):
 end=s.find('\n      End Object',m.end());ns[m[1]]=s[m.start():end]
def visit(n,d=0,seen=None):
 seen=set() if seen is None else seen
 if n in seen or d>16:return
 seen.add(n);b=ns.get(n,'');desc=re.findall(r'(?:CustomFunctionName|MemberName)="([^"]+)"',b);print(' '*d+n+' '+str(desc))
 for l in b.splitlines():
  if 'PinName="self"' in l:print(' '*d+'TARGET '+str(re.findall(r'LinkedTo=\(([^)]*)',l)))
  if 'PinName="then"' in l:
   for x in re.findall(r'LinkedTo=\((\w+) ',l):visit(x,d+1,seen)
for n,b in ns.items():
 if 'CustomFunctionName="OnDialogueNode Started/Finished Playing - L4_BlackMarketInvestigation_Still_MainInvestigation_DialogueNode_NPC_227"' in b:visit(n)
