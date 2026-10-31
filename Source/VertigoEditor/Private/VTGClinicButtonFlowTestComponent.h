#pragma once
#include "NarrativeComponent.h"
#include "VTGClinicButtonFlowTestComponent.generated.h"

/** Records dialogue requests from the real BPLM without playing unrelated cinematic node events. */
UCLASS()
class UVTGClinicButtonFlowTestComponent : public UNarrativeComponent
{
 GENERATED_BODY()
public:
 TArray<FName> Starts;
 bool AcceptDialogue = true;
 virtual bool BeginDialogue(TSubclassOf<UDialogue>,const FDialoguePlayParams Params=FDialoguePlayParams()) override
 {Starts.Add(Params.StartFromID);return AcceptDialogue;}
};
