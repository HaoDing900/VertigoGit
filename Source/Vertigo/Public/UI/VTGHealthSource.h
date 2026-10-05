#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

/**
 * Reads an actor's health from whichever of its components exposes a pair of numeric properties
 * (by default the Boss AI Toolkit's BPC_Boss_Behavior: Health / MaxHealth). Lets the C++ health
 * bars follow a Blueprint-owned health value without that Blueprint having to push updates.
 */
struct FVTGHealthSource
{
	/** Finds the first component of Owner with both properties. False if there is none. */
	bool Find(const AActor* Owner, FName HealthName, FName MaxHealthName)
	{
		Component.Reset();
		if (!Owner)
		{
			return false;
		}
		for (UActorComponent* Candidate : Owner->GetComponents())
		{
			FNumericProperty* H = FindFProperty<FNumericProperty>(Candidate->GetClass(), HealthName);
			FNumericProperty* M = FindFProperty<FNumericProperty>(Candidate->GetClass(), MaxHealthName);
			if (H && M)
			{
				Component = Candidate;
				HealthProp = H;
				MaxHealthProp = M;
				return true;
			}
		}
		return false;
	}

	bool IsValid() const { return Component.IsValid(); }
	UActorComponent* Get() const { return Component.Get(); }

	float GetHealth() const { return Read(HealthProp); }
	float GetMaxHealth() const { return Read(MaxHealthProp); }

private:
	float Read(const FNumericProperty* Prop) const
	{
		const UActorComponent* Source = Component.Get();
		return Source ? float(Prop->GetFloatingPointPropertyValue(Prop->ContainerPtrToValuePtr<void>(Source))) : 0.f;
	}

	TWeakObjectPtr<UActorComponent> Component;
	FNumericProperty* HealthProp = nullptr;
	FNumericProperty* MaxHealthProp = nullptr;
};
