from pathlib import Path
p=Path('Scripts/PoliceNotifyObservation/dodge_writers.py');s=p.read_text(encoding='utf-8-sig').replace("p=re.search(r'PinName=\"([^\"]+)\"',l).group(1);link=", "match=re.search(r'PinName=\"([^\"]+)\"',l)\n  if not match:continue\n  p=match.group(1);link=");p.write_text(s,encoding='utf-8')
