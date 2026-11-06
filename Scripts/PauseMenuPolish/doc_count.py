from pathlib import Path
p=Path('Docs/CorvosSequenceRegression.md');s=p.read_text(encoding='utf-8').replace('原 Sequence 入口／Mail 20 项','原 Sequence 入口／Mail 21 项');p.write_text(s,encoding='utf-8')
