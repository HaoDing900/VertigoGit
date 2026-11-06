from pathlib import Path
s=Path('Scripts/PoliceNotifyObservation/dodge_flow.py').read_text(encoding='utf-8');s=s[:s.index("for k in ['K2Node_VariableSet_69'")]+'''
for k in ['K2Node_VariableGet_14','K2Node_CallFunction_238','K2Node_MacroInstance_0','K2Node_CallFunction_239']:
 print(k,nodes.get(k))
''';Path('Scripts/PoliceNotifyObservation/dodge_condition.py').write_text(s,encoding='utf-8')
