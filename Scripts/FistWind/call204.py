exec(open('Scripts/FistWind/healthflow.py').read().split("queue=['K2Node_CustomEvent_20']")[0])
t=nodes['K2Node_CallFunction_204']
for l in t.splitlines():
 if l.strip().startswith('FunctionReference='):print(l.strip())
