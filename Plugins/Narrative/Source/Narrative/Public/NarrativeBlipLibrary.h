#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NarrativeBlipLibrary.generated.h"
class USoundBase;
class UAudioComponent;

UENUM(BlueprintType)
enum class ENarrativeBlipVoicePitch : uint8
{
    Default UMETA(DisplayName="Default (Unchanged)"),
    High UMETA(DisplayName="High (+3 semitones)"),
    Low UMETA(DisplayName="Low (-3 semitones)")
};

UENUM(BlueprintType)
enum class ENarrativeBlipRoute : uint8 { Silence, VoiceOverride, Blip };

USTRUCT(BlueprintType)
struct NARRATIVE_API FNarrativeBlipChoice
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ENarrativeBlipRoute Route = ENarrativeBlipRoute::Silence;
    UPROPERTY(BlueprintReadOnly) FName ResolvedTag;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<USoundBase> Sound;
    UPROPERTY(BlueprintReadOnly) float Pitch = 1.f;
    UPROPERTY(BlueprintReadOnly) float Volume = 1.f;
};

USTRUCT(BlueprintType)
struct NARRATIVE_API FNarrativeBlipPool
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<USoundBase>> Sources;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float PitchMin = .96f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float PitchMax = 1.04f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float VolumeMin = .9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float VolumeMax = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAvoidImmediateRepeat = true;
};

UCLASS(BlueprintType)
class NARRATIVE_API UNarrativeBlipLibrary : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, FNarrativeBlipPool> BlipPools;
    /** Safety cap only; never affects dialogue timing. Sources must be non-looping. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05")) float MaxBlipSeconds = 2.f;
    UFUNCTION(BlueprintPure, Category="Dialogue|Blip") FName ResolveTag(FName Tag) const;
    /** Chooses once per execution; does not play, alter dialogue timing, or mutate shared library data. */
    UFUNCTION(BlueprintCallable, Category="Dialogue|Blip")
    FNarrativeBlipChoice Choose(FName Tag, const FText& Text, USoundBase* VoiceOverride, bool bEnabled, bool bSuppressed, USoundBase* Previous) const;
};

/** Stateless integration helpers. No additional manager or tick. */
UCLASS()
class NARRATIVE_API UNarrativeBlipAudio : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Speaker offset multiplies the pool's random pitch. Explicit voice audio is never shifted. */
    UFUNCTION(BlueprintPure, Category="Dialogue|Blip")
    static float GetPlaybackPitch(const FNarrativeBlipChoice& Choice, ENarrativeBlipVoicePitch VoicePitch);
    UFUNCTION(BlueprintPure, Category="Dialogue|Blip") static bool HasSpokenText(const FText& Text);
    /** Call after starting an event/story audio component. Stops existing blips and suppresses new ones while it plays. */
    UFUNCTION(BlueprintCallable, Category="Dialogue|Blip") static void RegisterPriorityAudio(UAudioComponent* Audio);
    static bool HasPriorityAudio(const UWorld* World);
};
