#include "NarrativeBlipLibrary.h"
#include "NarrativeJawComponent.h"
#include "Dialogue.h"
#include "Components/AudioComponent.h"
#include "UObject/UObjectIterator.h"
#include "Sound/SoundBase.h"
#include "HAL/IConsoleManager.h"

static const FName PriorityTag(TEXT("Narrative.BlipPriority"));
float UNarrativeBlipAudio::GetPlaybackPitch(const FNarrativeBlipChoice& Choice, ENarrativeBlipVoicePitch VoicePitch)
{
    if (Choice.Route != ENarrativeBlipRoute::Blip) return Choice.Pitch;
    const float Semitones = VoicePitch == ENarrativeBlipVoicePitch::High ? 3.f :
        (VoicePitch == ENarrativeBlipVoicePitch::Low ? -3.f : 0.f);
    return Choice.Pitch * FMath::Pow(2.f, Semitones / 12.f);
}
FName UNarrativeBlipLibrary::ResolveTag(FName Tag) const
{
    return !Tag.IsNone() && BlipPools.Contains(Tag) ? Tag : FName(TEXT("BlipNPC"));
}
bool UNarrativeBlipAudio::HasSpokenText(const FText& Text)
{
    return UNarrativeJawComponent::CountVisibleCharacters(Text.ToString()) > 0;
}
void UNarrativeBlipAudio::RegisterPriorityAudio(UAudioComponent* Audio)
{
    if (!IsValid(Audio) || !Audio->GetWorld()) return;
    Audio->ComponentTags.AddUnique(PriorityTag);
    for (TObjectIterator<UDialogue> It; It; ++It)
        if (!It->IsTemplate() && It->GetWorld() == Audio->GetWorld()) It->StopBlip();
}
bool UNarrativeBlipAudio::HasPriorityAudio(const UWorld* World)
{
    if (!World) return false;
    for (TObjectIterator<UAudioComponent> It; It; ++It)
        if (!It->IsTemplate() && It->GetWorld() == World && It->ComponentHasTag(PriorityTag) && (It->IsPlaying() || It->bIsPaused)) return true;
    return false;
}
void UNarrativeBlipAudio::UnregisterPriorityAudio(UAudioComponent* Audio)
{
    if (IsValid(Audio)) Audio->ComponentTags.Remove(PriorityTag);
}

FNarrativeBlipChoice UNarrativeBlipLibrary::Choose(FName Tag, const FText& Text, USoundBase* VoiceOverride, bool bEnabled, bool bSuppressed, USoundBase* Previous) const
{
    FNarrativeBlipChoice Result;
    if (VoiceOverride) { Result.Route = ENarrativeBlipRoute::VoiceOverride; Result.Sound = VoiceOverride; return Result; }
    Result.ResolvedTag = ResolveTag(Tag);
    if (!bEnabled || bSuppressed || !UNarrativeBlipAudio::HasSpokenText(Text)) return Result;
    const auto* Pool = BlipPools.Find(Result.ResolvedTag);
    if (!Pool) return Result;
    TArray<USoundBase*> Valid;
    for (USoundBase* Source : Pool->Sources)
        if (IsValid(Source) && !Source->IsLooping()) Valid.AddUnique(Source);
    if (Valid.IsEmpty()) return Result;
    if (Pool->bAvoidImmediateRepeat && Valid.Num() > 1) Valid.Remove(Previous);
    Result.Route = ENarrativeBlipRoute::Blip;
    Result.Sound = Valid[FMath::RandRange(0, Valid.Num()-1)];
    auto Range = [](float A, float B, float Low, float High)
    {
        if (!FMath::IsFinite(A) || !FMath::IsFinite(B)) return 1.f;
        A = FMath::Clamp(A, Low, High); B = FMath::Clamp(B, Low, High);
        return FMath::FRandRange(FMath::Min(A,B), FMath::Max(A,B));
    };
    Result.Pitch = Range(Pool->PitchMin, Pool->PitchMax, .01f, 4.f);
    Result.Volume = Range(Pool->VolumeMin, Pool->VolumeMax, 0.f, 4.f);
    return Result;
}

// Console fast-forward changes cancel an existing blip immediately, without a playback tick.
static FAutoConsoleVariableSink GBlipFastForwardSink(FConsoleCommandDelegate::CreateLambda([]
{
    const auto* Fast = IConsoleManager::Get().FindConsoleVariable(TEXT("Narrative.FastForward"));
    if (Fast && Fast->GetFloat() > 0)
        for (TObjectIterator<UDialogue> It; It; ++It) if (!It->IsTemplate() && It->GetWorld()) It->StopBlip();
}));
