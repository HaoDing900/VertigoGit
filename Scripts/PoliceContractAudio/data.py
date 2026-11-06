import json
n=json.load(open('Scripts/PoliceContractAudio/graph.json'))
for k in ['K2Node_Knot_5','K2Node_Knot_2','K2Node_VariableGet_15','K2Node_Knot_4','K2Node_VariableGet_14']:
 print(k,n.get(k))
