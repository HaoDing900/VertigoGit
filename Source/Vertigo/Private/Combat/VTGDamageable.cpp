#include "Combat/VTGDamageable.h"

#include "Combat/VTGCombatComponent.h"
#include "GameFramework/Actor.h"

void IVTGDamageable::ReceiveCombatHit_Implementation(const FVTGHitEvent& Hit)
{
	// Native fallback: forward to the combat component if there is one. Blueprint classes that add
	// this interface must implement the event themselves (a BP-added interface never reaches this
	// native default), which is why the setup instructions say to wire Handle Incoming Hit.
	if (const AActor* Self = Cast<AActor>(_getUObject()))
	{
		if (UVTGCombatComponent* Combat = Self->FindComponentByClass<UVTGCombatComponent>())
		{
			Combat->HandleIncomingHit(Hit);
		}
	}
}
