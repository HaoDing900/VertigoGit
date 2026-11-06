from pathlib import Path
p=Path('Scripts/PauseMenuPolish/edit_sequence.py')
s=p.read_text(encoding='utf-8')
s=s[s.index("p=Path('Source/VertigoEditor/Private/VTGCutsceneAudioDetails.cpp')"):]
s=s.replace('read_text()', "read_text(encoding='utf-8')").replace('write_text(s)', "write_text(s,encoding='utf-8')")
exec(s)
