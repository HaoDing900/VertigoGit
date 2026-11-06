import json
n=json.load(open('Scripts/PoliceContractAudio/graph.json'))
for key in ['K2Node_VariableGet_40','K2Node_CustomEvent_20','K2Node_CallFunction_47','K2Node_CallFunction_29','K2Node_SwitchEnum_0','K2Node_CallFunction_17','K2Node_CustomEvent_25']:
 print(key,n.get(key))
visited=set()
def walk(key,depth=0):
 if key in visited or depth>12:return
 visited.add(key);x=n.get(key)
 if not x:return
 print(' '*depth,key,x['fn'],[(a,b) for a,b in x['pins'] if a not in ['execute','self','OutputDelegate']])
 for a,b in x['pins']:
  if a in ['then','else','Completed','NewEnumerator0','NewEnumerator1','NewEnumerator2','NewEnumerator3']:
   for link in b.rstrip(',').split(','):
    if link:walk(link.split()[0],depth+1)
for k in ['K2Node_CustomEvent_20','K2Node_CustomEvent_10','K2Node_CustomEvent_9']:walk(k)
