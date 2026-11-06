#include "Audio/VTGUIFeedbackLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "NarrativeBlipLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"

FText UVTGUIFeedbackLibrary::FilterSpeakerLabel(FName SpeakerID,FText SpeakerName,FText Label)
{
    FString ID=SpeakerID.ToString().ToLower();ID.ReplaceInline(TEXT("_"),TEXT(""));
    const bool Mail=ID==TEXT("mail") || ID==TEXT("idmail") || SpeakerName.ToString().TrimStartAndEnd().Equals(TEXT("Mail"),ESearchCase::IgnoreCase);
    return Mail?FText::GetEmpty():Label;
}

UVTGUIFeedbackLibrary::UVTGUIFeedbackLibrary()
{
    ConstructorHelpers::FObjectFinder<UTexture2D> Locked(TEXT("/Game/2DArt/UI/Response/ResponseChoiceLocked.ResponseChoiceLocked"));
    ResponseLockedTexture=Locked.Object;
    for (int32 Index = 1; Index <= 3; ++Index)
    {
        ConstructorHelpers::FObjectFinder<USoundBase> Confirm(*FString::Printf(TEXT("/Game/SFX/N/NSFX_Response%d"), Index));
        ConstructorHelpers::FObjectFinder<USoundBase> Hover(*FString::Printf(TEXT("/Game/SFX/N/NSFX_ResponseHover%d"), Index));
        if(Confirm.Succeeded()) ResponseSounds.Add(Confirm.Object);
        if(Hover.Succeeded()) ResponseHoverSounds.Add(Hover.Object);
    }
    for (int32 Index = 1; Index <= 5; ++Index)
    {
        const FString Path = FString::Printf(TEXT("/Game/SFX/Ovani/UI4/Mouse/Hover/Hover_Magic_%03d"), Index);
        ConstructorHelpers::FObjectFinder<USoundBase> Sound(*Path);
        if (Sound.Succeeded()) InteractionSounds.Add(Sound.Object);
    }
    for (const TCHAR* Name : {TEXT("UsedInQuest/Signals_Update_Magic_003_quest"), TEXT("UsedInQuest/Signals_Update_Glitch_004_quest"), TEXT("UsedInQuest/Signals_Update_Magic_005")})
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
