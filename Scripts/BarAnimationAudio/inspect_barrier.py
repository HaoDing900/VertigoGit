import pathlib,re
p=pathlib.Path('Scripts/BarAnimationAudio/BP_FightingAreaCollision.copy')
if p.exists():
 b=p.read_bytes();s=b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
 for l in s.splitlines():
  if any(x in l for x in ['VariableName=','StaticMesh=','OverrideMaterials','NiagaraSystem','SystemAsset=','CustomFunctionName=','ComponentTemplate=']):print(l[:300])
