import re,pathlib
base=pathlib.Path('Scripts/BlackMarketLookDiagnosis')
def nodes(f):
 s=(base/(f+'.latest.summary.txt')).read_text(encoding='utf-8-sig')
 out={}
 for b in re.split(r'(?=^\s*Begin Object Name=)',s,flags=re.M):
  m=re.search(r'Begin Object Name="([^"]+)".*?:EventGraph\.',b)
  if m:out[m[1]]=b
 return out
for f in ['BP_Player_Sa','BPLM_BlackMarket','L4_BlackMarketInvestigation_Still_MainInvestigation']:
 ns=nodes(f)
 def desc(n):
  b=ns.get(n,''); m=re.search(r'(?:CustomFunctionName|MemberName)="([^"]+)"',b)
  return n+' '+(m[1] if m else '')
 def back(n,seen=None,depth=0):
  seen=set() if seen is None else seen
  if n in seen or depth>14:return
  seen.add(n);print('  '*depth+desc(n))
  b=ns.get(n,'')
  for l in b.splitlines():
   if 'PinName="execute"' in l:
    for x in re.findall(r'LinkedTo=\((\w+) ',l):back(x,seen,depth+1)
 def forward(n,seen=None,depth=0):
  seen=set() if seen is None else seen
  if n in seen or depth>6:return
  seen.add(n);print('  '*depth+desc(n))
  for l in ns.get(n,'').splitlines():
   if any(x in l for x in ['PinName="then"','PinName="then_0"','PinName="then_1"']):
    for x in re.findall(r'LinkedTo=\((\w+) ',l):forward(x,seen,depth+1)
 print('\nASSET',f)
 for n,b in ns.items():
  if re.search(r'MemberName="Turn(?:Invisibale|Visible)"',b):back(n)
 if f=='BP_Player_Sa':
  for n,b in ns.items():
   if re.search(r'CustomFunctionName="Turn(?:Invisibale|Visible)"',b):forward(n)
