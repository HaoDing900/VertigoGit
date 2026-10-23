import pathlib,re,collections
base=pathlib.Path('Scripts/BlackMarketLookDiagnosis')
s=(base/'L4_BlackMarketInvestigation_Still_MainInvestigation.latest.copy').read_text(encoding='utf-16')
ns={}
for m in re.finditer(r'^      Begin Object Name="(DialogueNode_(?:NPC|Player)_\d+)"[^\n]*:DialogueTemplate\.',s,re.M):
 end=s.find('\n      End Object',m.end());b=s[m.start():end];name=m[1]
 ns[name]={'id':re.findall(r'^         ID="([^"]+)"',b,re.M),'func':re.findall(r'OnPlayNodeFuncName="([^"]+)"',b),'next':re.findall(r'(?:NPCReplies|PlayerReplies)\(\d+\)=.*?DialogueTemplate\.(DialogueNode_\w+)',b),'text':re.findall(r'Line=([^\n]+)',b)}
for n,d in ns.items():
 if any(x.endswith('_Cyan') for x in d['id']) or n in ['DialogueNode_NPC_227','DialogueNode_NPC_340']:
  print(n,d)
  q=collections.deque([(n,[n])]);seen=set()
  while q:
   cur,p=q.popleft()
   if cur in seen:continue
   seen.add(cur)
   if cur=='DialogueNode_NPC_227': print('PATH TO 227',p);break
   for nxt in ns.get(cur,{}).get('next',[]):q.append((nxt,p+[nxt]))

