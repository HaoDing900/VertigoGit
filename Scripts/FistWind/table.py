exec(open('Scripts/FistWind/healthflow.py').read().split("queue=['K2Node_CustomEvent_20']")[0])
for n,t in nodes.items():
 if 'K2Node_GetDataTableRow' in n or n=='K2Node_BreakStruct_1':
  print(n)
  for l in t.splitlines():
   if 'CustomProperties Pin' in l:
    pin=re.search('PinName="([^"]+)"',l);link=re.search('LinkedTo=\((.*?)\)',l);default=re.search('DefaultObject="([^"]+)"',l)
    if pin:print(pin[1],link[1] if link else '',default[1] if default else '')
