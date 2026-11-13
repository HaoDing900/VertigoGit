import array
import json
import math
import wave
from pathlib import Path
root = Path(__file__).resolve().parent
with wave.open(str(root / 'headache_source.wav'), 'rb') as f:
    channels, rate, width, count = f.getnchannels(), f.getframerate(), f.getsampwidth(), f.getnframes()
    assert width == 2
    samples = array.array('h', f.readframes(count))
step = rate * channels // 10
values = [math.sqrt(sum((v / 32768.0) ** 2 for v in samples[i:i+step]) / len(samples[i:i+step])) for i in range(0, len(samples), step)]
report = {'rate': rate, 'channels': channels, 'duration': count/rate, 'peak_rms_100ms': max(values),
          'rms_100ms': [round(v, 5) for v in values]}
(root / 'sound-envelope.json').write_text(json.dumps(report, indent=2))
print(json.dumps({k:v for k,v in report.items() if k != 'rms_100ms'}))
print('first 4 sec rms:', values[:40])
for name in ['PP_Sa_Headache', 'NE_PP_SaHeadache']:
    import re
    text = (root / (name + '.copy')).read_text()
    print(name, 'Functions', sorted(set(re.findall(r'FunctionReference=\([^\n]*?MemberName="([^"]+)"', text))))
    print('Timeline:', re.findall(r'(?:TimelineLength|bAutoPlay|bLoop|Keys)=[^\n]+', text)[:15])
