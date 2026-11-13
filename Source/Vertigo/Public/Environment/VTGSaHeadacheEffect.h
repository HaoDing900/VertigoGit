#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VTGSaHeadacheEffect.generated.h"

class UAudioComponent;
class UPostProcessComponent;
class UTimelineComponent;
class UCurveFloat;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundWave;

/** Playback support for the existing PP_Sa_Headache Blueprint, not a global manager. */
UCLASS(Blueprintable)
class VERTIGO_API AVTGSaHeadacheEffect : public AActor
{
    GENERATED_BODY()
public:
    AVTGSaHeadacheEffect();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Headache") TObjectPtr<UPostProcessComponent> HeadachePostProcess;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Headache") TObjectPtr<UAudioComponent> HeadacheAudio;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Headache") TObjectPtr<UTimelineComponent> HeadacheTimeline;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Headache") TObjectPtr<UMaterialInterface> HeadacheMaterial;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Headache") TObjectPtr<USoundWave> HeadacheSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Headache", meta=(ClampMin="0.1", Units="s")) float HeadacheDuration=5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Headache", meta=(ClampMin="0", ClampMax="1")) float HeadacheAudioVolume=0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Headache", meta=(ClampMin="0", ClampMax="0.25")) float HeadacheEnvelopeInfluence=0.12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Headache") bool bAutoPlayOnBeginPlay=true;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Headache") float CurrentHeadacheIntensity=0.f;

    UFUNCTION(BlueprintCallable, Category="Headache")
    void PlayHeadacheVFX(float Intensity=1.f, float Duration=5.f);
    UFUNCTION(BlueprintCallable, Category="Headache") void StopHeadacheVFX();
    /** Resolve the existing effect Actor or create it once; repeated events restart the same playback. */
    UFUNCTION(BlueprintCallable, Category="Headache", meta=(WorldContext="WorldContextObject"))
    static AVTGSaHeadacheEffect* PlaySaHeadache(UObject* WorldContextObject, TSubclassOf<AVTGSaHeadacheEffect> EffectClass,
        float Intensity=1.f, float Duration=5.f, float AudioVolume=0.65f, float EnvelopeInfluence=0.12f);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<UCurveFloat> IntensityCurve;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HeadacheMID;
    float PeakIntensity=1.f;
    float RestartIntensity=0.f;
    float AudioEnvelope=1.f;
    FTimerHandle AudioFadeTimer;
    FTimerHandle AudioEndFadeTimer;
    bool bTimelineReady=false;
    void InitializePlayback();
    void StartAudioFade();
    void FinishAudioFade();
    UFUNCTION() void UpdateHeadache(float Shape);
    UFUNCTION() void FinishHeadache();
    UFUNCTION() void ReceiveHeadacheEnvelope(const USoundWave* Wave, float EnvelopeValue);
};
