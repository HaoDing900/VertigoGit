exec(open('Scripts/BlackMarketLookDiagnosis/trace_visibility.py',encoding='utf-8-sig').read().split('for f in [')[0])
ns=nodes('BP_Player_Sa')
def walk(n,d=0,seen=None):
 seen=set() if seen is None else seen
 if n in seen or d>10:return
 seen.add(n);b=ns.get(n,'');m=re.search(r'(?:CustomFunctionName|MemberName)="([^"]+)"',b);print(' '*d+n+' '+(m[1] if m else ''))
 for l in b.splitlines():
  if 'PinName="execute"' in l:
   for x in re.findall(r'LinkedTo=\((\w+) ',l):walk(x,d+1,seen)
for n,b in ns.items():
 if 'MemberName="SetVisibility"' in b and re.search('PinName="bNewVisibility" DefaultValue="false"',b):
  walk(n)
  for l in b.splitlines():
   if 'PinName="self"' in l:print(' TARGET',l)
for n in ['K2Node_VariableGet_150','K2Node_VariableGet_63','K2Node_VariableGet_154']:print(ns[n])
