import pathlib,re
p=pathlib.Path('Scripts/BlackMarketQuestScan/BP_Narrative3Overlay.nodes.txt')
for b in re.split(r'(?=^Begin Object )',p.read_text(encoding='utf8'),flags=re.M):
 if ':On Quest New State.' in b.split('\n')[0] and any(s in b for s in ['VariableReference','FunctionEntry','GetQuests','GetCurrentState']):
  print('\n'.join(b.splitlines()[:3]))
