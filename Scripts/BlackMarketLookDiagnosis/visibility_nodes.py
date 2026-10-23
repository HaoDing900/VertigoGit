exec(open('Scripts/BlackMarketLookDiagnosis/trace_visibility.py',encoding='utf-8-sig').read().split('for f in [')[0])
for f,names in [('BPLM_BlackMarket',['K2Node_CustomEvent_20','K2Node_CallFunction_92','K2Node_MakeStruct_4']),('BP_Player_Sa',['K2Node_CallFunction_209','K2Node_Knot_12','K2Node_Knot_15','K2Node_Knot_91'])]:
 ns=nodes(f); print('\n',f)
 for n in names:
  print(ns.get(n,'MISSING'))
