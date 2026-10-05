#include "Combat/VTGCombatStatics.h"

#include "Combat/VTGDamageable.h"
#include "Combat/VTGCombatComponent.h"
#include "UI/VTGHealthSource.h"
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

bool UVTGCombatStatics::IsAlive(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	if (const UVTGCombatComponent* Combat = Actor->FindComponentByClass<UVTGCombatComponent>())
	{
		return Combat->IsAlive();
	}

	// AI Behavior System enemies (BP_AI_Base) flag their death on the actor.
	if (const FBoolProperty* Dead = FindFProperty<FBoolProperty>(Actor->GetClass(), TEXT("Dead?")))
	{
		if (Dead->GetPropertyValue_InContainer(Actor))
		{
			return false;
		}
	}

	// Boss AI Toolkit bosses and summons keep health on their behavior component (its "IsAlive?"
	// interface function is an empty stub, so it can't be asked).
	FVTGHealthSource Health;
	if (Health.Find(Actor, TEXT("Health"), TEXT("MaxHealth")))
	{
		return Health.GetHealth() > 0.f;
	}
	return true;
}

bool UVTGCombatStatics::CanReceiveDamage(const AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->CanBeDamaged() || !IsAlive(Actor))
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
