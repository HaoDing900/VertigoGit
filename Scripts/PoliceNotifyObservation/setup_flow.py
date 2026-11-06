from pathlib import Path
p=Path('Scripts/PoliceNotifyObservation/parent_collision.py')
s=p.read_text(encoding='utf-8');s=s[:s.index(" print('\\n',filename)")]+'''
 if filename!='Parent':continue
 seen=set()
 def forward(k,d=0):
  if k in seen or d>15 or k not in n:return
  seen.add(k);v=n[k];print(' '*d,k,v['fn'])
  for p,l in v['pins']:
   if p in ['then','else','OutputPin','Completed','Is Valid','Is Not Valid'] or p.startswith('then_'):
    for x in l.rstrip(',').split(','):
     if x:forward(x.split()[0],d+1)
 for k,v in n.items():
  if v['fn'] in ['LockPlayerInput','UnlockPlayerInput','FinishSequence','FinishSequenceForceLookReturnToPlayer'] or k=='K2Node_CustomEvent_5':
   print('ROOT');forward(k)
'''
p=Path('Scripts/PoliceNotifyObservation/parent_flow.py');p.write_text(s,encoding='utf-8')
