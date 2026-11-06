from pathlib import Path
p=Path('Scripts/PoliceNotifyObservation/collision_audit.py');s=p.read_text(encoding='utf-8').replace("s=Path(filename).read_text(encoding='utf-8-sig')","raw=Path(filename).read_bytes();s=raw.decode('utf-16' if raw[:2] in [b'\\xff\\xfe',b'\\xfe\\xff'] else 'utf-8-sig')");p.write_text(s,encoding='utf-8')
