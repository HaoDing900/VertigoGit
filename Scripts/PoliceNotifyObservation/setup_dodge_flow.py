from pathlib import Path
s=Path('Scripts/PoliceNotifyObservation/dodge_writers.py').read_text(encoding='utf-8');s=s[:s.index('for k,n in nodes.items():')]+'''
for k in ['K2Node_VariableSet_69','K2Node_IfThenElse_74','K2Node_Knot_82','K2Node_CallFunction_196','K2Node_MacroInstance_12']:
 print(k,nodes.get(k))
seen=set()
def forward(k,d=0):
 if k in seen or d>9 or k not in nodes:return
 seen.add(k);v=nodes[k];print(' '*d,k,v['fn'])
 for p,l in v['pins']:
  if p in ['then','else','Completed','OutputPin','Is Valid','Is Not Valid','On Completed','On Interrupted','On Blend Out']:
   for x in l.rstrip(',').split(','):
    if x:forward(x.split()[0],d+1)
forward('K2Node_VariableSet_69')
'''
Path('Scripts/PoliceNotifyObservation/dodge_flow.py').write_text(s,encoding='utf-8')
