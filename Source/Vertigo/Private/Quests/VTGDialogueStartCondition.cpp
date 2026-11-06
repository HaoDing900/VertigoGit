#include "Quests/VTGDialogueStartCondition.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "UObject/UnrealType.h"

bool UVTGDialogueStartCondition::CheckCondition_Implementation(APawn*, APlayerController*, UNarrativeComponent*)
{
    const UDialogueNode* Node = GetTypedOuter<UDialogueNode>();
    const UDialogue* Dialogue = Node ? Node->OwningDialogue : nullptr;
    // Narrative stores launch parameters as a reflected protected property.
    const FStructProperty* Property = FindFProperty<FStructProperty>(UDialogue::StaticClass(), TEXT("PlayParams"));
    return Dialogue && Property && Property->Struct == FDialoguePlayParams::StaticStruct()
        && Property->ContainerPtrToValuePtr<FDialoguePlayParams>(Dialogue)->StartFromID == StartFromID;
}

FString UVTGDialogueStartCondition::GetGraphDisplayText_Implementation()
{
    return FString::Printf(TEXT("Dialogue %s from %s"), bNot ? TEXT("did not start") : TEXT("started"), *StartFromID.ToString());
}
