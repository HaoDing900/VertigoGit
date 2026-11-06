from pathlib import Path
p=Path('Source/VertigoEditor/Private/VTGPauseMenuPolish.cpp');s=p.read_text(encoding='utf-8').replace('FLinearColor(1,1,1,.35f)','FLinearColor::White').replace('Art->GetColorAndOpacity().A,.35f','Art->GetColorAndOpacity().A,1.f').replace('artwork 35%%','artwork original alpha');p.write_text(s,encoding='utf-8')
