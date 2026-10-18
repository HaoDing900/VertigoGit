#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VTGSewerEncounterComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGEggKillCountChanged,int32,KillCount);

/** Installed only on BPLM_SewerUnderApt. References and saved progress live on that Blueprint. */
UCLASS(ClassGroup=(Vertigo),meta=(BlueprintSpawnableComponent))
class VERTIGO_API UVTGSewerEncounterComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVTGSewerEncounterComponent();
 UFUNCTION(BlueprintCallable,Category="L3 Sewer|Encounter") bool StartBossEncounter();
 UFUNCTION(BlueprintPure,Category="L3 Sewer|Eggs") int32 GetMonsterEggKillCount() const;
 UPROPERTY(BlueprintAssignable,Category="L3 Sewer|Eggs") FVTGEggKillCountChanged OnMonsterEggKillCountChanged;
 void RecordPlayerEggKill(AActor* Egg);
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick) override;
private:
 UPROPERTY(Transient) TObjectPtr<AActor> Boss;
 UPROPERTY(Transient) TObjectPtr<UActorComponent> Behavior;
 bool bCombatActivated=false;
 bool bEncounterRequested=false;
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
