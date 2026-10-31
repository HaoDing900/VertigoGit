#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "VTGPlayerAudioComponent.generated.h"
class USoundBase;
class USoundAttenuation;
class UReverbEffect;
class UAudioComponent;
class USkeletalMesh;

/** Character footsteps and armor foley only. Level ambience is owned by level audio managers. */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGPlayerAudioComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    UVTGPlayerAudioComponent();
    /** Overrides surface variants when configured; evaluated again on every foot plant. */
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> DefaultSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> BarefootSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TObjectPtr<USkeletalMesh> BarefootMesh;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> ConcreteSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> MetalSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> WaterSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> WoodSteps;
    UPROPERTY(EditAnywhere, Category = "Footsteps") TObjectPtr<USoundAttenuation> StepAttenuation;
    UPROPERTY(EditAnywhere, Category = "Footsteps", meta = (ClampMin = "0", ClampMax = "2")) float FootstepVolume = .6f;
    // Legacy serialization/editor-tool compatibility only; hidden and never used by gameplay.
    UPROPERTY() FName SewerLevel = NAME_None;
    UPROPERTY() TObjectPtr<USoundBase> WaterBed;
    UPROPERTY() TObjectPtr<USoundBase> PipeBed;
    UPROPERTY() TObjectPtr<USoundBase> PipeHiss;
    UPROPERTY() float WaterVolume = .12f;
    UPROPERTY() float PipeVolume = .055f;
    UPROPERTY() TObjectPtr<USoundAttenuation> AmbientAttenuation;
    UPROPERTY() TObjectPtr<UReverbEffect> PipeReverb;
    UPROPERTY() TObjectPtr<UReverbEffect> CorridorReverb;
    UPROPERTY() TObjectPtr<UReverbEffect> ChamberReverb;
    UPROPERTY()
    float PipeWetLevel = .65f;
    UPROPERTY()
    float CorridorWetLevel = .75f;
    UPROPERTY()
    float ChamberWetLevel = .85f;
    UFUNCTION(BlueprintCallable, Category = "Audio") bool PlayFootstep(FName FootBone);
    /** Three short one-shots with authored fade-out tails. */
    UPROPERTY(EditAnywhere, Category = "Armor Foley") TArray<TObjectPtr<USoundBase>> ArmorRunStartSounds;
    UPROPERTY(EditAnywhere, Category = "Armor Foley", meta = (ClampMin = "0", ClampMax = "2")) float ArmorVolume = .45f;
    // Restricted sprint uses 340 cm/s; restricted walking uses 273 and does not set IsSprinting.
    UPROPERTY(EditAnywhere, Category = "Armor Foley") float RestrictedRunSpeedThreshold = 300.f;
    bool ConsumeArmorRunStart();
    UFUNCTION(BlueprintCallable, Category = "Audio") bool PlayArmorRunStart();
    bool CanPlayFootstep() const;
    const TArray<TObjectPtr<USoundBase>> &GetOutfitFootsteps() const;
    // 0=open, 1=pipe, 2=corridor, 3=chamber. Public for acoustic diagnostics/tests.
    int32 ProbeSpace(const FVector &Listener) const;

  protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *Fn) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;

  private:
    bool bWasRunning = false;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> ArmorAudio;
    float LastStepTime = -100.f;
    int32 LastVariant = -1;
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
