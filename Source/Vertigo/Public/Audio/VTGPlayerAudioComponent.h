#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "VTGPlayerAudioComponent.generated.h"
class USoundBase;
class USoundAttenuation;
class UReverbEffect;
class UAudioComponent;

/** Animation-timed footsteps and geometry-adaptive sewer acoustics. No movement/combat state is written. */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGPlayerAudioComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    UVTGPlayerAudioComponent();
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> ConcreteSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> MetalSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> WaterSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> WoodSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TObjectPtr<USoundAttenuation> StepAttenuation;
    UPROPERTY(EditAnywhere, Category = "Footsteps", meta = (ClampMin = "0", ClampMax = "2")) float FootstepVolume = .6f;
    UPROPERTY(EditAnywhere, Category = "Sewer") FName SewerLevel = TEXT("L_SewerUnderApartment");
    UPROPERTY(EditAnywhere, Category = "Sewer") TObjectPtr<USoundBase> WaterBed;
    UPROPERTY(EditAnywhere, Category = "Sewer") TObjectPtr<USoundBase> PipeBed;
    UPROPERTY(EditAnywhere, Category = "Sewer") TObjectPtr<USoundBase> PipeHiss;
    UPROPERTY(EditAnywhere, Category = "Sewer", meta = (ClampMin = "0", ClampMax = "1")) float WaterVolume = .12f;
    UPROPERTY(EditAnywhere, Category = "Sewer", meta = (ClampMin = "0", ClampMax = "1")) float PipeVolume = .055f;
    UPROPERTY(EditAnywhere, Category = "Sewer") TObjectPtr<USoundAttenuation> AmbientAttenuation;
    UPROPERTY(EditAnywhere, Category = "Sewer") TObjectPtr<UReverbEffect> PipeReverb;
    UPROPERTY(EditAnywhere, Category = "Sewer") TObjectPtr<UReverbEffect> CorridorReverb;
    UPROPERTY(EditAnywhere, Category = "Sewer") TObjectPtr<UReverbEffect> ChamberReverb;
    UPROPERTY(EditAnywhere, Category = "Sewer|Reverb", meta = (ClampMin = "0", ClampMax = "1"))
    float PipeWetLevel = .65f;
    UPROPERTY(EditAnywhere, Category = "Sewer|Reverb", meta = (ClampMin = "0", ClampMax = "1"))
    float CorridorWetLevel = .75f;
    UPROPERTY(EditAnywhere, Category = "Sewer|Reverb", meta = (ClampMin = "0", ClampMax = "1"))
    float ChamberWetLevel = .85f;
    UFUNCTION(BlueprintCallable, Category = "Audio") bool PlayFootstep(FName FootBone);
    bool CanPlayFootstep() const;
    // 0=open, 1=pipe, 2=corridor, 3=chamber. Public for acoustic diagnostics/tests.
    int32 ProbeSpace(const FVector &Listener) const;

  protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *Fn) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;

  private:
    void StopSewerAudio();
    bool bStarted = false, bOwnReverb = false;
    int32 ActiveSpace = -1, PendingSpace = -1, StableSamples = 0;
    float LastStepTime = -100.f, NextHissTime = 0;
    int32 LastVariant = -1;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Beds;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Hiss;
};

UCLASS()
class VERTIGO_API UVTGFootstepNotify : public UAnimNotify
{
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Footstep") FName FootBone = TEXT("foot_l");
    virtual void Notify(USkeletalMeshComponent *Mesh, UAnimSequenceBase *Animation,
                        const FAnimNotifyEventReference &Ref) override;
};
