from pathlib import Path
import re
for fn in ['BP_Player_Sa','ABP_Sa','BPLM_SewerUnderApt','L3Sewer_Freemove','L3Sewer_Still','BP_Boss_SeaMonster_L3Sewer','BP_SeaMonster_AcidProjectile']:
 raw=Path('Scripts/CombatSewerPolish/'+fn+'.copy').read_bytes();text=raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8-sig')
 out=[];graph=''
 for line in text.splitlines():
  if line.startswith('   Begin Object Name='):graph=line.split('"')[1]
  if line.startswith('      Begin Object Name='):out.append('\nGRAPH '+graph+' NODE '+line.split('"')[1])
  if line.startswith('         FunctionReference=') or line.startswith('         VariableReference=') or line.startswith('         CustomFunctionName='):out.append(line.strip())
  if line.startswith('         CustomProperties Pin '):
   name=re.search('PinName="([^"]+)"',line)
   if not name:continue
   if 'PinCategory="exec"' in line or name[1] in ['BlendTime','NewViewTarget','BlendExp','Target','self','HasNewMail','ReturnValue','LevelSequence','SequencePlayer']:
    link=re.search('LinkedTo=\((.*?)\),',line)
    default=re.search('Default(?:Value|Object)="([^"]+)"',line)
    out.append('  '+name[1]+' '+(' -> '+link[1] if link else '')+(' default='+default[1] if default else ''))
 Path('Scripts/CombatSewerPolish/'+fn+'.summary').write_text('\n'.join(out),encoding='utf-8')



