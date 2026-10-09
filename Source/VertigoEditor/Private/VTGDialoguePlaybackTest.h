#pragma once
#include "Dialogue.h"
#include "DialogueSM.h"
#include "NarrativeComponent.h"
#include "VTGDialoguePlaybackTest.generated.h"

/** Exercises real line completion while isolating avatar/camera and story side effects. */
UCLASS()
class UVTGDialoguePlaybackTest : public UDialogue
{
    GENERATED_BODY()
public:
    int32 FinishedLines = 0;
    void Prepare(UNarrativeComponent* Component, bool bSkippable, bool bRouting = false)
    {
        OwningComp = Component;
        bWaitingForPlayerResponse = false;
        auto* Node = NewObject<UDialogueNode_NPC>(this);
        Node->Line.Text = bRouting ? FText::GetEmpty() : FText::FromString(TEXT("Playback test"));
        Node->bIsSkippable = bSkippable;
        CurrentNode = Node;
        CurrentLine = Node->Line;
    }
    void WaitForChoice() { bWaitingForPlayerResponse = true; }
    void NaturalFinish() { FinishNPCDialogue(); }
    void ManualFinish() { EndCurrentLine(); }
protected:
    virtual void PlayNextNPCReply() override { ++FinishedLines; }
    virtual void FinishDialogueNode_Implementation(UDialogueNode*, const FDialogueLine&, const FSpeakerInfo&, AActor*, AActor*) override {}
};
