import pathlib,re
s=pathlib.Path('Scripts/BlackMarketLookDiagnosis/BPLM_BlackMarket.current.copy').read_text(encoding='utf-8-sig')
ns={}
for m in re.finditer(r'Begin Object Name="([^"]+)"[^\n]*:EventGraph\.',s):
 end=s.find('\n      End Object',m.end());ns[m[1]]=s[m.start():end]
def visit(n,d=0,seen=None):
 seen=set() if seen is None else seen
 if n in seen or d>12:return
 seen.add(n);b=ns.get(n,'');desc=re.findall(r'(?:CustomFunctionName|MemberName)="([^"]+)"',b);print(' '*d+n+' '+str(desc))
 for l in b.splitlines():
  if 'PinName="NewLocation"' in l:print(re.findall(r'DefaultValue="([^"]*)"',l))
  if 'PinName="self"' in l:print(' '*d+'TARGET '+str(re.findall(r'LinkedTo=\(([^)]*)',l)))
  if 'PinName="then"' in l:
   for x in re.findall(r'LinkedTo=\((\w+) ',l):visit(x,d+1,seen)
for n,b in ns.items():
 if 'CustomFunctionName="L4_Position_SaIPHAInfrontOfAdultStore"' in b:visit(n)

