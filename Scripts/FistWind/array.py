exec(open('Scripts/FistWind/healthflow.py').read().split("queue=['K2Node_CustomEvent_20']")[0])
for name in ['K2Node_GetArrayItem_0','K2Node_VariableGet_122','K2Node_VariableGet_8','K2Node_IfThenElse_31','K2Node_PromotableOperator_40']:
 print(name)
 for l in nodes.get(name,'').splitlines():
  if 'Reference=' in l: print(l.strip())
  if 'CustomProperties Pin' in l:
   pin=re.search('PinName="([^"]+)"',l); link=re.search('LinkedTo=\((.*?)\)',l)
   if pin and link:print(pin[1],link[1])
