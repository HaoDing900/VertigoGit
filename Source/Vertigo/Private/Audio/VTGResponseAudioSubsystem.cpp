#include "Audio/VTGResponseAudioSubsystem.h"
#include "Audio/VTGUIFeedbackLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "NarrativeComponent.h"
#include "NarrativeBlipLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "CommonButtonBase.h"

namespace
{
void Stop(TWeakObjectPtr<UAudioComponent>& Audio)
{
    if(auto* Component=Audio.Get()) Component->Stop();
    Audio.Reset();
}
USoundBase* Choose(const TArray<TObjectPtr<USoundBase>>& Pool, TWeakObjectPtr<USoundBase>& Previous)
{
    TArray<USoundBase*> Valid;
    for(USoundBase* Sound:Pool) if(IsValid(Sound) && !Sound->IsLooping()) Valid.AddUnique(Sound);
    if(Valid.Num()>1) Valid.Remove(Previous.Get());
    if(Valid.IsEmpty()) return nullptr;
    auto* Sound=Valid[FMath::RandRange(0,Valid.Num()-1)];Previous=Sound;return Sound;
}
bool CanUse(UUserWidget* Button, UDialogue* Dialogue, UDialogueNode_Player* Option)
{
    if(auto* Common=Cast<UCommonButtonBase>(Button)) if(!Common->IsInteractionEnabled() || Common->GetLocked()) return false;
    return IsValid(Button) && Button->GetIsEnabled() && Button->IsVisible() &&
        IsValid(Dialogue) && Dialogue->IsWaitingForPlayerResponse() && Dialogue->CanSelectDialogueOption(Option);
}
}

void UVTGResponseAudioSubsystem::Hover(UUserWidget* Button)
{
    if(HoveredButton==Button) return;
    HoveredButton=Button;
    Stop(HoverAudio);
    // Hover is lower priority than confirmation, quest feedback and explicit voice.
    if(UNarrativeBlipAudio::HasPriorityAudio(GetWorld())) return;
    const double Now=GetWorld()->GetRealTimeSeconds();
    if(Now-LastHoverTime<0.06) return;
    if(auto* Sound=Choose(GetDefault<UVTGUIFeedbackLibrary>()->ResponseHoverSounds,LastHoverSound))
    {
        LastHoverTime=Now;
        HoverAudio=UGameplayStatics::SpawnSound2D(this,Sound);
    }
}
void UVTGResponseAudioSubsystem::Unhover(UUserWidget* Button)
{
    if(HoveredButton!=Button) return;
    HoveredButton.Reset();Stop(HoverAudio);
}
void UVTGResponseAudioSubsystem::Confirm()
{
    HoveredButton.Reset();Stop(HoverAudio);
    if(auto* Sound=Choose(GetDefault<UVTGUIFeedbackLibrary>()->ResponseSounds,LastConfirmSound))
    {
        ConfirmAudio=UGameplayStatics::SpawnSound2D(this,Sound,1.f,1.1f);
        UNarrativeBlipAudio::RegisterPriorityAudio(ConfirmAudio.Get());
    }
}
void UVTGResponseAudioSubsystem::Deinitialize()
{
    CancelTransition();Stop(HoverAudio);HoveredButton.Reset();Super::Deinitialize();
}
void UVTGUIFeedbackLibrary::HoverResponse(UUserWidget* Button,UDialogue* Dialogue,UDialogueNode_Player* Option)
{
    if(CanUse(Button,Dialogue,Option))
        if(auto* World=Button->GetWorld()) World->GetSubsystem<UVTGResponseAudioSubsystem>()->Hover(Button);
}
void UVTGUIFeedbackLibrary::UnhoverResponse(UUserWidget* Button)
{
    if(IsValid(Button)) if(auto* World=Button->GetWorld()) World->GetSubsystem<UVTGResponseAudioSubsystem>()->Unhover(Button);
}
void UVTGUIFeedbackLibrary::ConfirmResponse(UUserWidget* Button,UNarrativeComponent* NarrativeComponent,UDialogueNode_Player* Option)
{
    if(!IsValid(NarrativeComponent) || !CanUse(Button,NarrativeComponent->GetCurrentDialogue(),Option)) return;
    if(auto* World=Button->GetWorld()) World->GetSubsystem<UVTGResponseAudioSubsystem>()->BeginTransition(Button,NarrativeComponent,Option);
}
void UVTGUIFeedbackLibrary::ReleaseResponseWidget(UUserWidget* Button)
{
    UnhoverResponse(Button);
    if(IsValid(Button)) if(auto* World=Button->GetWorld()) World->GetSubsystem<UVTGResponseAudioSubsystem>()->CancelTransitionFor(Button);
}
bool UVTGUIFeedbackLibrary::CanUpdateResponseAppearance(UUserWidget* Button)
{
    if(IsValid(Button))if(auto* World=Button->GetWorld())
        return !World->GetSubsystem<UVTGResponseAudioSubsystem>()->IsTransitionPending();
    return true;
}
