from pathlib import Path
import re
p=Path('Scripts/PoliceNotifyObservation/live_snapshot.py');s=p.read_text(encoding='utf-8-sig');s=re.sub(r"sample\(([^\n]+?),([a-zA-Z_][a-zA-Z0-9_]*\.[a-zA-Z_][a-zA-Z0-9_]*)\)",r'sample(\1,lambda:\2())',s);s=s.replace("sa.get_components_by_class if False else lambda:sa.get_components_by_class(unreal.ActorComponent)","lambda:sa.get_components_by_class(unreal.ActorComponent)");p.write_text(s,encoding='utf-8')
