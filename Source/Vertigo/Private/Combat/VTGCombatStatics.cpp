#include "Combat/VTGCombatStatics.h"

#include "Combat/VTGDamageable.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"

TArray<AActor*> UVTGCombatStatics::FindMeleeTargets(const UObject* WorldContextObject, FVector Center, float Radius, const TArray<AActor*>& ActorsToIgnore)
{
	TArray<AActor*> Overlapping;
	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes{ UEngineTypes::ConvertToObjectType(ECC_Pawn) };
	UKismetSystemLibrary::SphereOverlapActors(WorldContextObject, Center, Radius, ObjectTypes, nullptr, ActorsToIgnore, Overlapping);

	// The asker is a damageable pawn too; never hand it back to itself (the Blueprint node's hidden
	// world-context pin is the calling actor).
	const AActor* Self = Cast<AActor>(WorldContextObject);
	if (!Self)
	{
		if (const UActorComponent* Component = Cast<UActorComponent>(WorldContextObject))
		{
			Self = Component->GetOwner();
		}
	}

	Overlapping.RemoveAll([Self](const AActor* Actor) { return Actor == Self || !CanReceiveDamage(Actor); });
	return Overlapping;
}

bool UVTGCombatStatics::CanReceiveDamage(const AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->CanBeDamaged())
	{
		return false;
	}

	const UClass* Class = Actor->GetClass();
	if (Class->ImplementsInterface(UVTGDamageable::StaticClass()))
	{
		return true;
	}

	// Enemy Blueprints take damage through these events: BP_AI_Base handles Point/Any, BP_Boss_Base
	// handles Point/Radial. Asking for the events instead of the classes keeps both AI systems - and
	// anything added later - hittable without this list growing.
	return Class->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(AActor, ReceivePointDamage))
		|| Class->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(AActor, ReceiveAnyDamage))
		|| Class->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(AActor, ReceiveRadialDamage));
}
