#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGCombatStatics.generated.h"

/**
 * Target queries shared by every melee attacker, whichever AI system the victim comes from.
 *
 * The player's punch used to find victims with Sphere Overlap Actors filtered to BP_AI_Base, so only
 * AI Behavior System enemies could be hit; Boss AI Toolkit bosses (BP_Boss_Base) and their summons
 * were invisible to it. The two systems share no base class below ACharacter, and ACharacter also
 * covers friendlies like the rolling drone. What they DO share is that they react to damage - so
 * that is the filter.
 */
UCLASS()
class VERTIGO_API UVTGCombatStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Pawns inside the sphere that can take a hit: they implement VTGDamageable, or their Blueprint
	 * handles Point / Any / Radial Damage. Pawns that ignore damage (drones, ambient NPCs) are left out,
	 * so neither the hit VFX nor the soft-lock fires on them. The calling actor is never returned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore"))
	static TArray<AActor*> FindMeleeTargets(const UObject* WorldContextObject, FVector Center, float Radius, const TArray<AActor*>& ActorsToIgnore);

	/** True if the actor can currently be damaged and has something that reacts to damage. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	static bool CanReceiveDamage(const AActor* Actor);
};
