#include "Quests/VTGEnsureQuestStateEvent.h"
#include "NarrativeComponent.h"
#include "Quest.h"
#include "QuestSM.h"
#include "UObject/UnrealType.h"

void UVTGEnsureQuestStateEvent::ExecuteEvent_Implementation(APawn*,APlayerController*,UNarrativeComponent* Component)
{
    if(!Component || !Quest || StateID.IsNone())return;
    UQuest* Instance=Component->GetQuestInstance(Quest);
    if(!Instance){Component->BeginQuest(Quest,StateID);return;}
    UQuestState* Target=Instance->GetState(StateID);
    if(!Target){UE_LOG(LogTemp,Warning,TEXT("Ensure Quest State: missing STATE '%s' in %s"),*StateID.ToString(),*Quest->GetName());return;}
    // Replaying this dialogue must not roll an already completed/later quest back.
    if(Instance->GetQuestCompletion()!=EQuestCompletion::QC_Started)return;
    if(auto* P=FindFProperty<FArrayProperty>(Instance->GetClass(),TEXT("ReachedStates"))){FScriptArrayHelper Values(P,P->ContainerPtrToValuePtr<void>(Instance));auto* Item=CastFieldChecked<FObjectPropertyBase>(P->Inner);for(int I=0;I<Values.Num();++I)if(Item->GetObjectPropertyValue(Values.GetRawPtr(I))==Target)return;}
    struct FArgs{UQuestState* NewState;};FArgs Args{Target};Instance->ProcessEvent(Instance->FindFunctionChecked(TEXT("EnterState")),&Args);
}
FString UVTGEnsureQuestStateEvent::GetGraphDisplayText_Implementation()
{
    return FString::Printf(TEXT("Ensure quest state: %s / %s"),*GetNameSafe(Quest.Get()),*StateID.ToString());
}

