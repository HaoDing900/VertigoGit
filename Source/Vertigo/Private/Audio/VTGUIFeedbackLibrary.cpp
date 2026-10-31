#include "Audio/VTGUIFeedbackLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "NarrativeBlipLibrary.h"
#include "UObject/ConstructorHelpers.h"

UVTGUIFeedbackLibrary::UVTGUIFeedbackLibrary()
{
    for (int32 Index = 1; Index <= 5; ++Index)
    {
        const FString Path = FString::Printf(TEXT("/Game/SFX/Ovani/UI4/Mouse/Hover/Hover_Magic_%03d"), Index);
        ConstructorHelpers::FObjectFinder<USoundBase> Sound(*Path);
        if (Sound.Succeeded()) InteractionSounds.Add(Sound.Object);
    }
    for (const TCHAR* Name : {TEXT("Signals_Update_Glitch_002"), TEXT("Signals_Update_Glitch_004"), TEXT("Signals_Update_Magic_003")})
    {
        ConstructorHelpers::FObjectFinder<USoundBase> Sound(*(FString(TEXT("/Game/SFX/Ovani/UI4/Signals/Update/")) + Name));
        if (Sound.Succeeded()) QuestSounds.Add(Sound.Object);
    }
}

USoundBase* UVTGUIFeedbackLibrary::ResolveInteractionSound(bool bEnabled, USoundBase* SoundOverride)
{
    if (!bEnabled) return nullptr;
    if (SoundOverride) return SoundOverride;
    const auto& Sounds = GetDefault<UVTGUIFeedbackLibrary>()->InteractionSounds;
    return Sounds.IsEmpty() ? nullptr : Sounds[FMath::RandRange(0, Sounds.Num() - 1)].Get();
}

void UVTGUIFeedbackLibrary::PlayInteractionFeedback(UObject* WorldContextObject, bool bEnabled, USoundBase* SoundOverride)
{
    if (auto* Sound = ResolveInteractionSound(bEnabled, SoundOverride))
        UNarrativeBlipAudio::RegisterPriorityAudio(UGameplayStatics::SpawnSound2D(WorldContextObject, Sound));
}

void UVTGUIFeedbackLibrary::PlayQuestFeedback(UObject* WorldContextObject)
{
    const auto& Sounds = GetDefault<UVTGUIFeedbackLibrary>()->QuestSounds;
    if (!Sounds.IsEmpty())
        UNarrativeBlipAudio::RegisterPriorityAudio(UGameplayStatics::SpawnSound2D(WorldContextObject, Sounds[FMath::RandRange(0, Sounds.Num() - 1)]));
}
