import pathlib,re
text=pathlib.Path('Scripts/BlackMarketQuestScan/L4_BlackMarketInvestigation_Still_MainInvestigation.nodes.txt').read_text(encoding='utf8');ns={}
for b in re.split(r'(?=^Begin Object )',text,flags=re.M):
 h=b.split('\n')[0]
 if ':EventGraph.' in h:
  name=re.search(r'Name="([^"]+)"',h).group(1);ns[name]=b
for start in ['K2Node_CustomEvent_45','K2Node_CustomEvent_48']:
 print('FLOW',start);q=[(start,0)];seen=set()
 while q:
  n,d=q.pop(0)
  if n in seen or d>18:continue
  seen.add(n);b=ns.get(n,''); title=next((l for l in b.splitlines() if l.startswith(('FunctionReference','CustomFunctionName','VariableReference','MacroGraphReference'))),'')
  print(' '*d,n,title[:180])
  for line in b.splitlines():
   if 'Direction="EGPD_Output"' in line and re.search(r'PinName="(then|Completed|Is Valid|else|OnCompleted|OnInterrupted)"',line):
    m=re.search(r'LinkedTo=\(([^)]*)',line)
    if m:
     for link in m.group(1).split(','):
      if link.strip():q.append((link.split()[0],d+1))
