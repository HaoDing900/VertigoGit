import pathlib,re
s=pathlib.Path('Scripts/BlackMarketLookDiagnosis/BPLM_BlackMarket.current.copy').read_text(encoding='utf-8-sig'); ns={}
for m in re.finditer(r'Begin Object Name="([^"]+)"[^\n]*:EventGraph\.',s):
 e=s.find('\n      End Object',m.end()); ns[m[1]]=s[m.start():e]
def visit(n,d=0,seen=None):
 seen=set() if seen is None else seen
 if n in seen or d>16:return
 seen.add(n);b=ns.get(n,''); print(' '*d+n+' '+str(re.findall(r'(?:CustomFunctionName|MemberName)="([^"]+)"',b)))
 for l in b.splitlines():
  if 'PinName=' in l:
   name=re.search(r'PinName="([^"]+)',l)[1]
   if name in ['then','else','execute','Condition','A','ReturnValue','PlayParams_StartFromID'] or 'Cyan' in name:
    print(' '*d+' '+name+' '+str(re.findall(r'(?:DefaultValue="([^"]*)"|LinkedTo=\(([^)]*))',l)))
   if 'Direction="EGPD_Output"' in l and 'PinCategory="exec"' in l:
    for x in re.findall(r'LinkedTo=\((\w+) ',l):visit(x,d+1,seen)
for n,b in ns.items():
 if 'CustomFunctionName="L4_Main_CyanOrderBuilding"' in b:visit(n)
