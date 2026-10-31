import re,sys
from pathlib import Path
raw=Path(sys.argv[1]).read_bytes();s=raw.decode('utf-16' if raw[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig')
nodes={}
for m in re.finditer(r'^( {6})Begin Object Name="([^"]+)"[^\n]*\n(.*?)^\1End Object',s,re.M|re.S):
 body=m[3]
 if 'CustomProperties Pin' not in body: continue
 rows=[m[2]]
 for l in body.splitlines():
  if re.search(r'(FunctionReference|VariableReference|CustomFunctionName|NodeComment|EventReference)=',l): rows.append(l.strip())
  if 'CustomProperties Pin' in l:
   def val(p):
    x=re.search(p,l);return x[1] if x else ''
   rows.append('  '+val(r'PinName="([^"]*)"')+(' OUT' if 'EGPD_Output' in l else ' IN')+' '+val(r'Default(?:Value|Object)="([^"]*)"')+' -> '+val(r'LinkedTo=\(([^)]+)\)'))
 nodes[str(m.start())]='\n'.join(rows)
if len(sys.argv)>2:
 for k,v in nodes.items():
  if any(x in v for x in sys.argv[2:]): print(v)
else: print('\n'.join(nodes.values()))



