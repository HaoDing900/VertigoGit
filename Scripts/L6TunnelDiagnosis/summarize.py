from pathlib import Path
import re
root=Path(__file__).parent
def load(p):
 b=p.read_bytes();return b.decode('utf-16' if b[:2]==b'\xff\xfe' else 'utf-8-sig')
s=load(root/'L6Tunnel_ClinicToFactory.t3d')
nodes={m[1]:m[2] for m in re.finditer(r'^      Begin Object Name="(DialogueNode_NPC_\d+)"[^\n]*\n(.*?)^      End Object',s,re.M|re.S)}
lines=[]
for n,b in nodes.items():
 lines.append(n+'\n'+'\n'.join(l for l in b.splitlines() if not ('CustomProperties' in l or 'Editor' in l)))
(root/'dialogue.nodes.txt').write_text('\n'.join(lines),encoding='utf-8')
for n in ['DialogueNode_NPC_116','DialogueNode_NPC_41','DialogueNode_NPC_9']:
 print(n+'\n'+nodes.get(n,'missing'))
