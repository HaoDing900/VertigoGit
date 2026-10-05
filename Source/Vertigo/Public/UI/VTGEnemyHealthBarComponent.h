#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "UI/VTGHealthSource.h"
#include "VTGEnemyHealthBarComponent.generated.h"

/**
 * Small health bar floating over a regular enemy (the boss uses UVTGBossHealthBarComponent instead).
 *
 * Add it to an enemy Blueprint and set Widget Class to WBP_EnemyHealthBar - the same bar the human
 * enemies use. It reads health from a sibling component (by default the Boss AI Toolkit's
 * BPC_Boss_Behavior: Health / MaxHealth), so the enemy needs no extra nodes:
 *  - hidden at full health, shown from the first hit;
 *  - every change calls the widget's UpdateHealthUI(CurrentHealth, MaxHealth) event;
 *  - hidden again shortly after the enemy dies.
 * It places itself just above the owner's bounds when play starts.
 */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGEnemyHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UVTGEnemyHealthBarComponent();

	/** Widget event called with (CurrentHealth, MaxHealth) whenever health changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar")
	FName UpdateEventName = TEXT("UpdateHealthUI");

	/** Seconds the emptied bar stays up after the kill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar")
	float HideDelayAfterDeath = 0.6f;

	/** Move the bar to just above the owner's bounds at BeginPlay. Off = keep the placed location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar")
	bool bPlaceAboveOwner = true;

	/** Gap between the top of the owner and the bar, when Place Above Owner is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar", meta = (EditCondition = "bPlaceAboveOwner"))
	float HeightAboveOwner = 25.f;

	/** Property names on the health source component (numeric, numeric). */
	UPROPERTY(EditAnywhere, Category = "Enemy Health Bar|Source")
	FName HealthProperty = TEXT("Health");

	UPROPERTY(EditAnywhere, Category = "Enemy Health Bar|Source")
	FName MaxHealthProperty = TEXT("MaxHealth");

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void PlaceAboveOwner();
	void PushHealth(float Health, float MaxHealth);

	FVTGHealthSource Source;

	float LastHealth = -1.f;
	float DeadFor = -1.f;
};
