#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/AssetUserData.h"
#include "VTGCutsceneAudio.generated.h"
class UAudioComponent;
class ULevelSequence;

UENUM(BlueprintType)
enum class EVTGCutsceneAudioPreset : uint8
{
    KeepWorld UMETA(DisplayName="Keep world audio"),
    DialogueFocus UMETA(DisplayName="Dialogue focus"),
    MuteWorldBeds UMETA(DisplayName="Mute ambience and music")
};

/** Saved on the Sequence asset, so authored cutscene behaviour is independent of placed players. */
UCLASS(BlueprintType,EditInlineNew)
class VERTIGO_API UVTGCutsceneAudioSettings : public UAssetUserData
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Category="Cutscene Audio") EVTGCutsceneAudioPreset Preset=EVTGCutsceneAudioPreset::KeepWorld;
    UPROPERTY(EditAnywhere,Category="Cutscene Audio",meta=(ClampMin="0")) float FadeSeconds=.35f;
    /** Multiplies Sequence audio tracks, including nested sequences, without editing their curves. */
    UPROPERTY(EditAnywhere,Category="Cutscene Audio",meta=(ClampMin="0",UIMin="0",UIMax="4",DisplayName="Sequence Audio Volume Multiplier"))
    float SequenceAudioVolumeMultiplier=1.f;
};

/** A temporary gain layer. Does not replace sounds, stop components, or own their playback. */
UCLASS()
class VERTIGO_API UVTGCutsceneAudioSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    static const FName FollowAmbientTag;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
    virtual void Deinitialize() override;
private:
    friend bool VTGVerifyCutsceneAudio();
    struct FGainState
    {
        float Base=1,LastApplied=1,Gain=1,Start=1,Target=1,Elapsed=0,Duration=.35f;
    };
    TMap<TWeakObjectPtr<UAudioComponent>,FGainState> Controlled;
    float ScanElapsed=0;
    float LastFadeSeconds=.35f;
    void ApplyTargets(const TMap<UAudioComponent*,float>& Targets,float Dt,float FadeSeconds);
    void RestoreAll();
};

UCLASS()
class VERTIGO_API UVTGCutsceneAudioLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Audio|Cutscene")
    static UVTGCutsceneAudioSettings* GetSequenceAudioSettings(ULevelSequence* Sequence,bool CreateIfMissing=false);
    /** Same per-component switch exposed on placed scene sound instances in Details. */
    UFUNCTION(BlueprintCallable,Category="Audio|Cutscene")
    static void SetFollowAmbientSequenceMute(UAudioComponent* Audio,bool Enabled);
    UFUNCTION(BlueprintPure,Category="Audio|Cutscene")
    static bool GetFollowAmbientSequenceMute(const UAudioComponent* Audio);
};
