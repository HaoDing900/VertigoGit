#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Combat/VTGCombatTypes.h"
#include "VTGDamageable.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UVTGDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything that can be punched implements this: the player, every enemy, breakable scenery, a
 * destructible barricade. It replaces the two parallel pipelines we had (player -> native
 * ApplyDamage(float), enemy -> BI_DamageType.ProcessDamage), so an attack no longer has to know
 * WHAT it hit in order to hurt it correctly.
 *
 * Setup on an actor:
 *   1. Add UVTGCombatComponent.
 *   2. Add this interface.
 *   3. Implement the Receive Combat Hit event -> CombatComponent -> Handle Incoming Hit.
 * That's the whole wiring. The component does health, block checks, reaction montage, launch,
 * hitstun and death. A breakable that doesn't want a combat component can implement the event
 * directly and do its own thing.
 *
 * Call it with IVTGDamageable::Execute_ReceiveCombatHit(TargetActor, Hit) - never call it raw, or
 * Blueprint implementations won't run.
 *
 * TWO NAMING/DESIGN RULES THIS INTERFACE LEARNED THE HARD WAY:
 *
 *  - It's ReceiveCombatHit, not ReceiveHit, because AActor::ReceiveHit already exists (the "Event
 *    Hit" node). A Blueprint cannot implement an interface function that collides with a parent
 *    function of a different signature - the Blueprint fails to compile outright.
 *
 *  - There is exactly ONE function here, and it returns void. Interface functions WITH return
 *    values get an auto-created, EMPTY graph in every Blueprint that adds the interface, and an
 *    empty graph silently returns false/zero - overriding whatever the C++ default said. An
 *    "IsAlive" that quietly returns false makes an actor unhittable with no error anywhere. So
 *    queries live on UVTGCombatComponent (IsAlive, TeamId), not here.
 */
class VERTIGO_API IVTGDamageable
{
	GENERATED_BODY()

public:

	/** Take a hit. The FVTGHitEvent already carries damage, reaction tier, launch and hitstop. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	void ReceiveCombatHit(const FVTGHitEvent& Hit);
	virtual void ReceiveCombatHit_Implementation(const FVTGHitEvent& Hit);
};
