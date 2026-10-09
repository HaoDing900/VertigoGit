#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UI/VTGHealthSource.h"
#include "VTGCombatPresentation.generated.h"
class UNiagaraSystem;
class UTexture2D;

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

  protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *Fn) override;

  private:
    UFUNCTION() void OwnerDestroyed(AActor *Actor);
    void CheckHealth();
    FVTGHealthSource Source;
    float LastHealth = -1.f;
    bool bDeathPlayed = false;
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
    UPROPERTY(EditAnywhere, Category = "Effect") FName SocketName;
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
    UPROPERTY(EditAnywhere, Category = "HUD") TObjectPtr<UTexture2D> HeartFull;
    UPROPERTY(EditAnywhere, Category = "HUD") TObjectPtr<UTexture2D> HeartEmpty;
    UPROPERTY(EditAnywhere, Category = "HUD") TObjectPtr<UTexture2D> HeartHurt;

  protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *Fn) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;

  private:
    UPROPERTY(Transient) TObjectPtr<UVTGPlayerHearts> Hearts;
    float LastHealth = -1;
};
