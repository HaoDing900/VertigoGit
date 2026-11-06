#pragma once

#include "NarrativeCondition.h"
#include "VTGDialogueStartCondition.generated.h"

/** Tests the entry point of this dialogue instance, independent of persistent progression flags. */
UCLASS(EditInlineNew, meta=(DisplayName="Dialogue Started From ID"))
class VERTIGO_API UVTGDialogueStartCondition : public UNarrativeCondition
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Conditions")
    FName StartFromID;

    virtual bool CheckCondition_Implementation(APawn* Pawn, APlayerController* Controller, UNarrativeComponent* NarrativeComponent) override;
    virtual FString GetGraphDisplayText_Implementation() override;
};
