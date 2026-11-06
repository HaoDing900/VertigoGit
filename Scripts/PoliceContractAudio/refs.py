import json
n=json.load(open('Scripts/PoliceContractAudio/graph.json'))
for k in ['K2Node_VariableSet_8','K2Node_CallFunction_52','K2Node_VariableGet_34','K2Node_VariableSet_6','K2Node_VariableSet_0']:
 print(k,n.get(k))
