exec(open('Scripts/BelloSequenceDiagnosis/trace_idle_flow.py').read().split("for name in")[0])
for file,wanted in [('BPLM_PoliceStation',['K2Node_VariableGet_24','K2Node_VariableGet_30','K2Node_CustomEvent_4','K2Node_VariableSet_1']),('L3PoliceStation_Still',['K2Node_CustomEvent_3','K2Node_CustomEvent_16'])]:
 d=Path('Scripts/BelloSequenceDiagnosis/'+file+'.copy').read_bytes();s=d.decode('utf-16' if d.startswith(b'\xff\xfe') else 'utf-8-sig')
 for m in re.finditer(r'(?ms)^      Begin Object Name="([^"]+)"[^\n]*:EventGraph\.[^\n]*\n(.*?)^      End Object',s):
  if m[1] not in wanted:continue
  print(m[1])
  for l in m[2].splitlines():
   if any(x in l for x in ['VariableReference=','CustomFunctionName=']):print(l.strip())
   if 'PinName=' in l and ('bool' in l or 'then' in l):
    print(re.sub(r'.*PinName="([^"]+)".*',r'pin \1',l), re.search(r'DefaultValue="([^"]*)"',l).group(1) if 'DefaultValue=' in l else '', re.search(r'LinkedTo=\(([^)]+)',l).group(1) if 'LinkedTo=' in l else '')
