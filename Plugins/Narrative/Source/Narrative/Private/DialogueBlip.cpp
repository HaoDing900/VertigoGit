#include "Dialogue.h"
#include "NarrativeBlipLibrary.h"
#include "NarrativeComponent.h"
#include "NarrativeDialogueSettings.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

void UDialogue::StopBlip()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BlipSafetyTimer);
    if (IsValid(BlipAudio)) { BlipAudio->Stop(); BlipAudio->DestroyComponent(); }
    BlipAudio = nullptr;
}
void UDialogue::SetBlipFastForward(bool bEnabled)
{
    bBlipFastForward = bEnabled;
    if (bEnabled) StopBlip();
}
void UDialogue::StartBlip(UDialogueNode* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker)
{
    StopBlip();
    // Never put a blip into Line.DialogueSound or bind it to EndCurrentLine.
    if (Line.DialogueSound || !Node || !Node->bEnableBlip || !bEnableBlips || bBlipFastForward || bBlipPaused || !GetWorld()) return;
    const auto* Fast = IConsoleManager::Get().FindConsoleVariable(TEXT("Narrative.FastForward"));
    if (Fast && Fast->GetFloat() > 0) return;
    if (!UNarrativeBlipAudio::HasSpokenText(Line.Text) || UNarrativeBlipAudio::HasPriorityAudio(GetWorld())) return;
    if (!BlipLibrary) BlipLibrary = GetDefault<UNarrativeDialogueSettings>()->DefaultBlipLibrary.LoadSynchronous();
    if (!BlipLibrary) return;
    const FName Tag = BlipLibrary->ResolveTag(Speaker.BlipTag);
    const auto* Previous = LastBlipSources.Find(Tag);
    const auto Choice = BlipLibrary->Choose(Speaker.BlipTag, Line.Text, Line.DialogueSound, true, false, Previous ? Previous->Get() : nullptr);
    if (Choice.Route != ENarrativeBlipRoute::Blip) return;
    const float Pitch = UNarrativeBlipAudio::GetPlaybackPitch(Choice, Speaker.BlipVoicePitch);
    BlipAudio = UGameplayStatics::SpawnSound2D(this, Choice.Sound, Choice.Volume * 0.4f, Pitch);
    if (!BlipAudio) return;
    LastBlipSources.Add(Tag, Choice.Sound.Get());
    const float Limit = FMath::IsFinite(BlipLibrary->MaxBlipSeconds) ? FMath::Clamp(BlipLibrary->MaxBlipSeconds,.05f,30.f) : 2.f;
    GetWorld()->GetTimerManager().SetTimer(BlipSafetyTimer, this, &UDialogue::StopBlip, Limit, false);
}
