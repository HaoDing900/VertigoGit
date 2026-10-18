#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UI/VTGHealthSource.h"
#include "VTGCombatPresentation.generated.h"
class UNiagaraSystem;
class UTexture2D;
class UAnimMontage;

/** Presentation only: damage and attack ownership remain in the existing Blueprints. */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGEnemyFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    UVTGEnemyFeedbackComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback") FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback") TObjectPtr<UNiagaraSystem> HitEffect;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback") TObjectPtr<UNiagaraSystem> DeathEffect;
    UPROPERTY(EditAnywhere, Category="Combat") bool bEndsSewerCombatOnDeath = false;
    UPROPERTY(EditAnywhere,Category="Feedback") TObjectPtr<UAnimMontage> HitReactionMontage;
    UPROPERTY(EditAnywhere,Category="Feedback") float HitReactionMinInterval=.2f;
    UPROPERTY(EditAnywhere,Category="Feedback") FName HitEffectSocketName;

  protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *Fn) override;

  private:
    friend struct FVTGHUDUnificationRegression;
    UFUNCTION() void OwnerDestroyed(AActor *Actor);
    void CheckHealth();
    FVTGHealthSource Source;
    float LastHealth = -1.f;
    bool bDeathPlayed = false;
    float LastHitReactionTime=-1000.f;
};

/** Timed visual burst. Explicit deactivation also handles looping Niagara systems. */
UCLASS()
class VERTIGO_API UVTGAttackEffectNotify : public UAnimNotify
{
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Effect") TObjectPtr<UNiagaraSystem> Effect;
    UPROPERTY(EditAnywhere, Category = "Effect") TObjectPtr<UNiagaraSystem> MediumEffect;
    UPROPERTY(EditAnywhere, Category = "Effect") TObjectPtr<UNiagaraSystem> LargeEffect;
    UPROPERTY(EditAnywhere, Category = "Effect") float ActiveSeconds = 1.5f;
    /** Per-attack Niagara speed; does not change animation or gameplay time. */
    UPROPERTY(EditAnywhere, Category = "Effect", meta=(ClampMin="0.01")) float EffectPlayRate = 1.f;
    UPROPERTY(EditAnywhere, Category = "Effect", meta=(ClampMin="0.01")) float EffectScale = 1.f;
    /** If positive, scale this effect to the current BossAI area ability Radius. */
    UPROPERTY(EditAnywhere, Category = "Effect", meta=(ClampMin="0")) float EffectReferenceRadius = 0.f;
    UPROPERTY(EditAnywhere, Category = "Effect") FName SocketName;
    /** Use a smoke hazard with a shared, authored particle/damage lifetime. */
    UPROPERTY(EditAnywhere, Category = "Smoke Damage") bool bPersistentSmokeDamage = false;
    UPROPERTY(EditAnywhere, Category = "Smoke Damage") float SmokeLifetime = 3.f;
    UPROPERTY(EditAnywhere, Category = "Smoke Damage") float SmokeDamageDelay = .3f;
    UPROPERTY(EditAnywhere, Category = "Smoke Damage") float SmokeSafeFadeSeconds = .6f;
    UPROPERTY(EditAnywhere, Category = "Smoke Damage") float SmokeTickInterval = .5f;
    UPROPERTY(EditAnywhere, Category = "Smoke Damage") float SmokeTickMultiplier = .2f;
    virtual void Notify(USkeletalMeshComponent *Mesh, UAnimSequenceBase *Animation,
                        const FAnimNotifyEventReference &Ref) override;
};

UCLASS()
class VERTIGO_API UVTGPlayerHearts : public UUserWidget
{
    GENERATED_BODY()
  public:
    UPROPERTY() TObjectPtr<UTexture2D> Full;
    UPROPERTY() TObjectPtr<UTexture2D> Empty;
    UPROPERTY() TObjectPtr<UTexture2D> Hurt;
    float Health = 100, MaxHealth = 100, HitFlash = 0;
    /** Each heart represents exactly one eighth of maximum health, including partial hearts. */
    float GetHeartFill(int32 Index) const;

  protected:
    virtual int32 NativePaint(const FPaintArgs &, const FGeometry &, const FSlateRect &, FSlateWindowElementList &,
                              int32, const FWidgetStyle &, bool) const override;
};

UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGPlayerPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    UVTGPlayerPresentationComponent();
    UFUNCTION(BlueprintCallable, Category = "Combat") void ConsumeBufferedAttack();
    UFUNCTION(BlueprintPure, Category="Combat") bool ResolveCombatState(bool EnemyNearby, bool SewerLevel) const;
    UFUNCTION(BlueprintCallable, Category="Combat") void FinishSewerCombat();
    /** Hold empty hands and suppress combat until explicitly released. Does not own cinematic input/camera. */
    UFUNCTION(BlueprintCallable, Category="Story|Combat") void SetStoryCombatLock(bool Enabled);
    UFUNCTION(BlueprintPure, Category="Story|Combat") bool IsStoryCombatLocked() const { return bStoryCombatLocked; }
    void RefreshBrassKnuckles();
    UPROPERTY(EditAnywhere, Category = "HUD") TObjectPtr<UTexture2D> HeartFull;
    UPROPERTY(EditAnywhere, Category = "HUD") TObjectPtr<UTexture2D> HeartEmpty;
    UPROPERTY(EditAnywhere, Category = "HUD") TObjectPtr<UTexture2D> HeartHurt;
    UPROPERTY(EditAnywhere, Category = "HUD") TSubclassOf<UVTGPlayerHearts> ActionHUDClass;
    /** Below the default viewport layer used by Narrative HUD. */
    UPROPERTY(EditAnywhere, Category = "HUD") int32 ActionHUDZOrder = -10;

  protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *Fn) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;

  private:
    friend struct FVTGActionHUDRegression;
    friend struct FVTGHUDUnificationRegression;
    UPROPERTY(Transient) TObjectPtr<UVTGPlayerHearts> Hearts;
    float LastHealth = -1;
    bool bStoryCombatLocked = false;
    TMap<FName, bool> StoryLockPreviousFlags;
    void EnforceStoryCombatLock();
};
