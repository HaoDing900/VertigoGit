from pathlib import Path
p=Path('Source/VertigoEditor/Private/VTGPauseMenuPolish.cpp');s=p.read_text(encoding='utf-8').replace('L->SetJustification(RefLabel->GetJustification());','L->SetJustification(ETextJustify::Left);');p.write_text(s,encoding='utf-8')
