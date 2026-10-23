import re,pathlib,collections
s=pathlib.Path('Scripts/BlackMarketLookDiagnosis/L4_BlackMarketInvestigation_Still_MainInvestigation.hidden.copy').read_text(encoding='utf-16')
ns={}
for m in re.finditer(r'^      Begin Object Name="(DialogueNode_(?:NPC|Player)_\d+)"[^\n]*:DialogueTemplate\.',s,re.M):
 end=s.find('\n      End Object',m.end());b=s[m.start():end];ns[m[1]]=b
for n in ['DialogueNode_NPC_103','DialogueNode_NPC_110','DialogueNode_NPC_227']:
 print('\n',n);print(ns.get(n,'missing')[:6000])
q=collections.deque([('DialogueNode_NPC_227',['DialogueNode_NPC_227'])]);seen=set()
while q:
 cur,p=q.popleft()
 if cur in seen or len(p)>14:continue
 seen.add(cur)
 preds=[n for n,b in ns.items() if re.search(r'(?:NPCReplies|PlayerReplies)\(\d+\)=.*?DialogueTemplate\.'+cur+r"'",b)]
 if not preds or len(p)>9:print('PREDECESSORS',list(reversed(p)))
 for n in preds:q.append((n,p+[n]))
