exec(open('Scripts/FistWind/healthflow.py').read().split("queue=['K2Node_CustomEvent_20']")[0])
t=nodes['K2Node_CallFunction_204']
for l in t.splitlines():
 if 'CustomProperties Pin' in l:
  p=re.search('PinName="([^"]+)"',l);q=re.search('LinkedTo=\((.*?)\)',l)
  if p and q:
   print(p[1],q[1])
   for n in re.findall(r'(K2Node_\w+) ',q[1]):
    for z in nodes.get(n,'').splitlines():
     if z.strip().startswith('VariableReference='):print(z.strip())
