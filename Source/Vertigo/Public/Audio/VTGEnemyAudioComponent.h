#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/VTGHealthSource.h"
#include "VTGEnemyAudioComponent.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Sounds an enemy makes that no animation drives: its fight music and its death.
 * (Attacks and hit reactions are Play Sound notifies on their montages.)
 *
 * Reads health / combat state from a sibling component, by default the Boss AI Toolkit's
 * BPC_Boss_Behavior (Health / MaxHealth / InCombat), so the enemy needs no extra nodes:
 *  - Combat Music (optional, 2D) fades in when the fight starts, loops, and fades out a moment
 *    after the kill - or right away if the fight ends without one (the toolkit resets the boss).
 *  - Death Sound (optional) plays once at the enemy when its health reaches zero.
 */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGEnemyAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVTGEnemyAudioComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Audio|Music")
	TObjectPtr<USoundBase> CombatMusic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Audio|Music", meta = (ClampMin = "0"))
	float MusicVolume = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Audio|Music", meta = (ClampMin = "0"))
	float MusicFadeInTime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Audio|Music", meta = (ClampMin = "0"))
	float MusicFadeOutTime = 3.f;

	/** Seconds the music keeps playing after the kill before it fades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Audio|Music", meta = (ClampMin = "0"))
	float MusicHoldAfterDeath = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Audio|Death")
	TObjectPtr<USoundBase> DeathSound;

	/** Property names on the health source component (numeric, numeric, bool). */
	UPROPERTY(EditAnywhere, Category = "Enemy Audio|Source")
	FName HealthProperty = TEXT("Health");

	UPROPERTY(EditAnywhere, Category = "Enemy Audio|Source")
	FName MaxHealthProperty = TEXT("MaxHealth");

	UPROPERTY(EditAnywhere, Category = "Enemy Audio|Source")
	FName InCombatProperty = TEXT("InCombat");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetMusicPlaying(bool bPlay);

	/** Music cues don't loop on their own; restart while the fight is still on. */
	UFUNCTION()
	void HandleMusicFinished();

	FVTGHealthSource Source;
	FBoolProperty* InCombatProp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Music;

	bool bMusicWanted = false;
	bool bDied = false;
	float DeadFor = 0.f;
};
