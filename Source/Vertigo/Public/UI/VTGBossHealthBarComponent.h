#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/VTGHealthSource.h"
#include "VTGBossHealthBarComponent.generated.h"

class UVTGBossHealthBar;

/**
 * Puts a UVTGBossHealthBar on the player's HUD while this boss is fighting.
 *
 * Add it to any boss Blueprint. It reads health and combat state from a sibling component that
 * exposes the properties named below - by default the Boss AI Toolkit's BPC_Boss_Behavior
 * (Health / MaxHealth / InCombat). The bar fades in when combat starts, follows health, stays on
 * screen empty for a moment after the kill, then fades out. If combat ends without a kill
 * (the toolkit resets the boss when the player leaves), it fades out right away.
 */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGBossHealthBarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVTGBossHealthBarComponent();

	/** Name shown above the bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Health Bar")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Health Bar")
	TSubclassOf<UVTGBossHealthBar> BarClass;

	/** Seconds the emptied bar stays up after the boss dies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Health Bar")
	float HideDelayAfterDeath = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Health Bar")
	int32 ViewportZOrder = 10;

	/** Property names on the health source component (numeric, numeric, bool). */
	UPROPERTY(EditAnywhere, Category = "Boss Health Bar|Source")
	FName HealthProperty = TEXT("Health");

	UPROPERTY(EditAnywhere, Category = "Boss Health Bar|Source")
	FName MaxHealthProperty = TEXT("MaxHealth");

	UPROPERTY(EditAnywhere, Category = "Boss Health Bar|Source")
	FName InCombatProperty = TEXT("InCombat");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool FindHealthSource();

	FVTGHealthSource Source;
	FBoolProperty* InCombatProp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UVTGBossHealthBar> Bar;

	float DeadFor = -1.f;
};
