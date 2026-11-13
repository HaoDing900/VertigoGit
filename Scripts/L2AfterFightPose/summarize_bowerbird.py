import re
from pathlib import Path
out=Path(__file__).parent/'Bowerbird'
for name in ['BPLM_L2StreetBarFight','BP_StillNPC','BP_Bowerbird_Still']:
 lines=(out/(name+'.t3d')).read_text(encoding='utf-8-sig').splitlines()
 result=[]
 for line in lines:
  if re.match(r'^   Begin Object Name=',line):result.append('\nGRAPH '+line.split('Name="')[1].split('"')[0])
  elif re.match(r'^      Begin Object Name=',line):result.append('\nNODE '+line.split('Name="')[1].split('"')[0])
  elif any(p in line for p in ['FunctionReference=','VariableReference=','CustomFunctionName=','EventReference=','NodeComment=','MacroGraphReference=']):result.append(line.strip())
  elif 'CustomProperties Pin ' in line:
   def get(p):
    m=re.search(p,line);return m.group(1) if m else ''
   result.append('  '+get(r'PinName="([^"]+)"')+' '+('OUT' if 'Direction="EGPD_Output"' in line else 'IN')+' default='+get(r'DefaultValue="([^"]*)"')+' links='+get(r'LinkedTo=\(([^)]*)'))
 (out/(name+'.nodes.txt')).write_text('\n'.join(result),encoding='utf-8')
