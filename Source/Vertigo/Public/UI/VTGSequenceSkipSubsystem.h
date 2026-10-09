#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VTGSequenceSkipSubsystem.generated.h"
class ULevelSequencePlayer;

/** Holds Esc to advance cinematic players through normal Sequencer evaluation. */
UCLASS()
class VERTIGO_API UVTGSequenceSkipSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    bool HandleEscape(bool bDown, bool bRepeat = false);
    void TickInput(bool bFocused);
    virtual void Deinitialize() override;
    /** Evaluate one display frame at a time, preserving triggers, repeaters and finish delegates. */
    static bool AdvanceFrame(ULevelSequencePlayer* Player);
private:
    TArray<TWeakObjectPtr<ULevelSequencePlayer>> Targets;
    bool bHolding = false;
    bool bConsumeRelease = false;
    bool bSkipping = false;
    double HoldStart = 0;
    void Cancel();
    void ShowHint();
    void HideHint();
    TSharedPtr<class SWidget> Hint;
    TWeakObjectPtr<class UGameViewportClient> HintViewport;
};
