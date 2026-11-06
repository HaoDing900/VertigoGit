from pathlib import Path
import re
s=Path('Scripts/PoliceContractAudio/Parent.copy').read_text(encoding='utf-16')
for m in re.finditer(r'Begin Object Name="([^"]+)" ExportPath="[^"]+:EventGraph\.[^"]+"(.*?)End Object',s,re.S):
 k,b=m.groups()
 if any(t in b for t in ['SelectedFunctionName="LockPlayerInput"','SelectedFunctionName="UnlockPlayerInput"','DelegateReference=(MemberParent=','DelegatePropertyName="OnPlay"','DelegatePropertyName="OnFinished"','DelegatePropertyName="OnStop"']):
  a=re.findall(r'(?:SelectedFunctionName|DelegatePropertyName)="([^"]+)"',b)
  if a:
   print(k,a)
   for l in b.splitlines():
    if 'CustomProperties Pin (' in l and any('PinName="'+x+'"' in l for x in ['Delegate','OutputDelegate','execute','then']):
     pn=re.search(r'PinName="([^"]+)"',l).group(1);links=re.search(r'LinkedTo=\((.*?)\)',l)
     if links:print(' ',pn,links.group(1))
