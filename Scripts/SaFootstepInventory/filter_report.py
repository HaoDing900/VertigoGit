from pathlib import Path
import json
p=Path('Scripts/SaFootstepInventory/scan.py');s=p.read_text(encoding='utf-8-sig').replace("any(t in st.lower() for t in ['footstep','foot_step'])","any(t in sound.get_name().lower() for t in ['footstep','foot_step'])");p.write_text(s,encoding='utf-8')
p=Path('Scripts/SaFootstepInventory/inventory.json');data=json.loads(p.read_text(encoding='utf-8'));rows=[]
for a in data['animations']:
 a['notifies']=[n for n in a['notifies'] if n['kind']!='PlaySound' or any(t in n['sound'].split('.')[-1].lower() for t in ['footstep','foot_step'])]
 if a['notifies']:rows.append(a)
data['animations']=rows;p.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
lines=['# Sa 脚步 Notify 动画清单','',f'只读扫描 {data["scanned"]} 个匹配 Sa 骨骼的 AnimSequence / AnimMontage。共 {len(rows)} 个资源带脚步通知或直接播放脚步音效。未修改或保存动画资产。','','VTGFootstep 会调用 PlayerAudio 的状态检查和地面检测；PlaySound 直接播放固定脚步音效。动画中的衣物和挥拳 whoosh 音效不计为脚步。','','| 动画完整资源路径 | 类型 | 脚步 Notify |','|---|---|---|']
for a in rows:
 detail='；'.join((n['kind']+' / '+n.get('bone','')+('（落地）' if n.get('landing') else '')) if n['kind']=='VTGFootstep' else 'PlaySound / '+n.get('sound','').split('.')[-1] for n in a['notifies'])
 lines.append('| `'+a['path']+'` | '+a['type']+' | '+detail+' |')
Path('Scripts/SaFootstepInventory/inventory.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('assets',len(rows),'VTG',sum(any(n['kind']=='VTGFootstep' for n in a['notifies']) for a in rows),'PlaySound',sum(any(n['kind']=='PlaySound' for n in a['notifies']) for a in rows))
