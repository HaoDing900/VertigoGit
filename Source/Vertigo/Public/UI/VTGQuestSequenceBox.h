#pragma once
#include "CoreMinimal.h"
#include "Components/VerticalBox.h"
#include "VTGQuestSequenceBox.generated.h"
class ULevelSequence;
/** A presentation-only gate: normal widget visibility and quest animations remain independent. */
UCLASS(meta=(DisplayName="Quest Sequence Visibility Box"))
class VERTIGO_API UVTGQuestSequenceBox : public UVerticalBox
{
    GENERATED_BODY()
public:
    static bool GetSequenceDefault(ULevelSequence* Sequence);
    static void SetSequenceDefault(ULevelSequence* Sequence,bool Hide);
    static bool IsHiddenForWorld(const UWorld* World);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
