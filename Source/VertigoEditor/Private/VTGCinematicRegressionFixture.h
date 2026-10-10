#pragma once
#include "Dialogue.h"
#include "DialogueSM.h"
#include "NarrativeComponent.h"
#include "LevelSequenceDirector.h"
#include "LevelSequencePlayer.h"
#include "VTGCinematicRegressionFixture.generated.h"

UCLASS()
class UVTGReturnRegressionDialogue : public UDialogue
{
    GENERATED_BODY()
public:
    int32 VisibleChoices = 0;
    void WaitForChoice() { bWaitingForPlayerResponse = true; }
    void Prepare(UNarrativeComponent* Component, UDialogueNode_Player* Option)
    {
        OwningComp = Component;
        AvailableResponses = {Option};
        Component->OnDialogueOptionSelected.AddDynamic(this, &UVTGReturnRegressionDialogue::Selected);
        Component->OnDialogueRepliesAvailable.AddDynamic(this, &UVTGReturnRegressionDialogue::Replies);
    }
    UFUNCTION() void Selected(UDialogue* Dialogue, UDialogueNode_Player* Option) { VisibleChoices = 0; }
    UFUNCTION() void Replies(UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& Options) { VisibleChoices = Options.Num(); }
protected:
    // Keep the real silent player routing and chunk generation; omit camera/avatar playback.
    virtual void PlayNextNPCReply() override
    {
        OwningComp->OnDialogueRepliesAvailable.Broadcast(this, AvailableResponses);
    }
    virtual void FinishDialogueNode_Implementation(UDialogueNode*, const FDialogueLine&, const FSpeakerInfo&, AActor*, AActor*) override {}
};

UCLASS()
class UVTGSequenceRegressionDirector : public ULevelSequenceDirector
{
    GENERATED_BODY()
public:
    static int32 TriggerCount;
    static int32 RepeatCount;
    static int32 FinishCount;
    static bool bPauseAtEvent;
    static TWeakObjectPtr<UNarrativeComponent> DialogueOwner;
    UFUNCTION() void Trigger()
    {
        ++TriggerCount;
        if (bPauseAtEvent && Player) Player->Pause();
        if (DialogueOwner.IsValid())
        {
            if (TriggerCount == 1)
            {
                auto* Dialogue = NewObject<UVTGReturnRegressionDialogue>(DialogueOwner.Get());
                Dialogue->OwningComp = DialogueOwner.Get();
                DialogueOwner->CurrentDialogue = Dialogue;
            }
            else
                DialogueOwner->CurrentDialogue = nullptr; // Later story event must not overtake the choice.
        }
    }
    UFUNCTION() void Repeat() { ++RepeatCount; }
    UFUNCTION() void Finished() { ++FinishCount; }
};
