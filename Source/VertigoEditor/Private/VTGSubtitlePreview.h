#pragma once
#include "CoreMinimal.h"
class ISequencer;
namespace VTGSubtitlePreview
{
    void Startup();
    void Shutdown();
    void Show(TSharedRef<ISequencer> Sequencer);
    void Update(TSharedPtr<ISequencer> Sequencer);
    void Release(TSharedPtr<ISequencer> Sequencer);
    void SetEnabled(bool Value);
    FText DisplayedText();
    void ShowGlobalStyle();
}
