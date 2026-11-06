#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGNarrativeAudio.generated.h"

class UAudioComponent;
class USoundBase;
class UDialogue;
class UNarrativeComponent;

USTRUCT()
struct FVTGNarrativeSound
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UAudioComponent> Audio;
    UPROPERTY() TObjectPtr<USoundBase> Source;
    UPROPERTY() TObjectPtr<USoundBase> PlaybackSound;
    TWeakObjectPtr<UObject> Owner;
    TWeakObjectPtr<UDialogue> Dialogue;
    FName ID;
    bool StopWithDialogue = true;
    bool Stopping = false;
    bool OwnsAudio = true;
    float FadeOut = 0.f;
};

/** Owns independent 2D components, never the level manager's shared audio tracks. */
UCLASS()
class VERTIGO_API UVTGNarrativeAudioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void Play(UObject* Owner, UNarrativeComponent* Narrative, USoundBase* Sound, FName ID,
              bool Loop, bool StopWithDialogue, float FadeIn, float Volume, float FadeOut);
    void Stop(UObject* Owner, USoundBase* Sound, FName ID, float FadeOut);
    bool RegisterExternal(UAudioComponent* Audio, UNarrativeComponent* Narrative, FName ID, bool StopWithDialogue);
    void UnregisterExternal(UAudioComponent* Audio);
    virtual void Deinitialize() override;
private:
    friend bool VTGVerifyIndependentAudio();
    UPROPERTY(Transient) TArray<FVTGNarrativeSound> Sounds;
    TArray<TWeakObjectPtr<UNarrativeComponent>> Bindings;
    static USoundBase* MakePlaybackSound(UObject* Outer, USoundBase* Source, bool Loop);
    void StopAt(int32 Index, float FadeOut);
    void Finished(UAudioComponent* Component);
    UFUNCTION() void DialogueFinished(UDialogue* Dialogue, bool StartingNewDialogue);
};

UCLASS()
class VERTIGO_API UVTGNarrativeAudioLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Narrative|Audio")
    static void PlayIndependentSound(APawn* Pawn, UNarrativeComponent* NarrativeComponent,
        USoundBase* NewSound, FName PlaybackID, bool Loop, bool StopWhenDialogueFinished,
        float FadeInTime, float VolumeMultiplier, float StopFadeOutTime);
    UFUNCTION(BlueprintCallable, Category="Narrative|Audio")
    static void StopIndependentSound(APawn* Pawn, UNarrativeComponent* NarrativeComponent,
        USoundBase* SoundToStop, FName PlaybackID, float FadeOutTime);
    /** Registers an existing Spawn Sound / actor audio component without starting or changing it.
     * Use the same NarrativeComponent as the Stop NE. PlaybackID is a registry key, not an Actor/Component tag. */
    UFUNCTION(BlueprintCallable, Category="Narrative|Audio", meta=(AdvancedDisplay="StopWhenDialogueFinished"))
    static bool RegisterAudioForNarrativeStop(UAudioComponent* AudioComponent, UNarrativeComponent* NarrativeComponent,
        FName PlaybackID, bool StopWhenDialogueFinished = false);
    /** Release the registration without stopping or destroying the external component. */
    UFUNCTION(BlueprintCallable, Category="Narrative|Audio")
    static void UnregisterAudioForNarrativeStop(UAudioComponent* AudioComponent);
};
