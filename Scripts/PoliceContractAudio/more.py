import json
n=json.load(open('Scripts/PoliceContractAudio/graph.json'))
for k,v in n.items():
 if v['fn'] in ['ForceReadAllPages','L3_HadReadContract','Client_OpenFullScreenWidget','K2Node_AddDelegate_0','K2Node_ClearDelegate_0'] or k in ['K2Node_VariableGet_5','K2Node_VariableSet_5']:print(k,v)
