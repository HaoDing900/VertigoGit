#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "VTGSewerEncounterComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGEggKillCountChanged,int32,KillCount);

/** Installed only on BPLM_SewerUnderApt. References and saved progress live on that Blueprint. */
UCLASS(ClassGroup=(Vertigo),meta=(BlueprintSpawnableComponent))
class VERTIGO_API UVTGSewerEncounterComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVTGSewerEncounterComponent();
 UFUNCTION(BlueprintCallable,Category="L3 Sewer|Encounter") bool StartBossEncounter(bool bWaitForIntroDialogue=false);
 UFUNCTION(BlueprintCallable,Category="L3 Sewer|Encounter") void WaitForBossIntroDialogue(class UNarrativeComponent* Narrative,bool bDialogueStarted);
 /** Rebuild physical encounter state after save restoration, without replaying story events. */
 UFUNCTION(BlueprintCallable,Category="L3 Sewer|Encounter") void RestoreEncounterCheckpointScene();
 UFUNCTION(BlueprintPure,Category="L3 Sewer|Eggs") int32 GetMonsterEggKillCount() const;
 UPROPERTY(BlueprintAssignable,Category="L3 Sewer|Eggs") FVTGEggKillCountChanged OnMonsterEggKillCountChanged;
 /** Seconds the defeated boss stays on screen before it is hidden, which is the state a save reload rebuilds. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="L3 Sewer|Encounter") float BossBodyHideDelay=5.f;
 void RecordPlayerEggKill(AActor* Egg);
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick) override;
private:
 TWeakObjectPtr<class UNarrativeComponent> IntroNarrative;
 TWeakObjectPtr<class UDialogue> IntroDialogue;
 UFUNCTION() void OnBossIntroFinished(class UDialogue* Dialogue,bool bStartingNewDialogue);
 void ReleaseBossHealthBar();
 class UNarrativeComponent* OwnerNarrative() const;
 /** Replay the authored encounter event once the dialogue that blocked the intro line has ended. */
 void RetryBossIntro();
 void HideBossBody();
 UPROPERTY(Transient) TObjectPtr<AActor> Boss;
 UPROPERTY(Transient) TObjectPtr<UActorComponent> Behavior;
 FTimerHandle BossBodyTimer;
 bool bCombatActivated=false;
 bool bEncounterRequested=false;
 bool bIntroRetryPending=false;
 bool bIntroRetryUsed=false;
 UFUNCTION() void HandleBossDefeated();
 void ResolveBoss();
};

/** Observe damage after the toolkit processes point/radial damage, without editing the player. */
UCLASS(ClassGroup=(Vertigo),meta=(BlueprintSpawnableComponent))
class VERTIGO_API UVTGEggKillReporterComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 virtual void BeginPlay() override;
private:
 bool bDeathObserved=false;
 UFUNCTION() void HandleDamage(AActor* DamagedActor,float Damage,const UDamageType* DamageType,AController* InstigatedBy,AActor* DamageCauser);
};
