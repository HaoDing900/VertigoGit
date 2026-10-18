#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGSmokeHazard.generated.h"
class UNiagaraComponent;
class UVTGAttackEffectNotify;
class UNiagaraSystem;

/** Installed only on eggs created by the boss ability; placed eggs retain their authored death. */
UCLASS()
class VERTIGO_API UVTGBossSummonedEggComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 void BindDeath();
 /** Explicit expiry / encounter cleanup; never plays a death animation. */
 UFUNCTION(BlueprintCallable,Category="L3 Sewer") void DespawnSummonedEgg();
protected:
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void HandleEggDeath();
 UFUNCTION() void HandleSummonerDeath();
 UFUNCTION() void HandleDamage(AActor* DamagedActor,float Damage,const UDamageType* DamageType,AController* InstigatedBy,AActor* DamageCauser);
 TWeakObjectPtr<UActorComponent> SummonerBehavior;
 bool bDeathBindingsInstalled=false;
 bool bEggDeathObserved=false;
 bool bKilledByPlayer=false;
};

/** A smoke cloud owns both the visible particles and the spatial damage window. */
UCLASS()
class VERTIGO_API AVTGSmokeHazard : public AActor
{
 GENERATED_BODY()
public:
 AVTGSmokeHazard();
 static AVTGSmokeHazard* Spawn(AActor* Source,AActor* Ability,const UVTGAttackEffectNotify* Settings,UNiagaraSystem* System,const FVector& Position,float VisualScale);
 virtual void Tick(float Dt) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> Cloud;
 UPROPERTY(BlueprintReadOnly) float Age=0;
 UPROPERTY(BlueprintReadOnly) float DamageStopsAt=0;
 UPROPERTY(BlueprintReadOnly) int32 DamagePulses=0;
 bool IsDamageWindowOpen() const;
private:
 friend struct FVTGSewerFollowupRegression;
 UPROPERTY() TObjectPtr<AActor> DamageProxy;
 UPROPERTY() TObjectPtr<AActor> OriginalAbility;
 UPROPERTY() TObjectPtr<AActor> DamageSource;
 float Lifetime=3,DamageStartsAt=.3f,Interval=.5f,NextPulse=.3f,Radius=250,InitialMultiplier=.2f,TickMultiplier=.2f;
 bool bFirstPulse=true;
 void Pulse();
};

UCLASS()
class VERTIGO_API UVTGSewerAbilityHelpers : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="L3 Sewer") static void InitializeEggSpawner(AActor* Ability);
 UFUNCTION(BlueprintCallable,Category="L3 Sewer") static void SpawnEggWithoutNavigation(AActor* Ability);
 UFUNCTION(BlueprintCallable,Category="L3 Sewer") static void SpawnEggNearTarget(AActor* Ability);
};
