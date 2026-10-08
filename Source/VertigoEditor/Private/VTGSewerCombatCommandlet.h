#pragma once
#include "Commandlets/Commandlet.h"
#include "GameFramework/Character.h"
#include "Combat/VTGDamageable.h"
#include "VTGSewerCombatCommandlet.generated.h"

UCLASS()
class UVTGSewerCombatCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGSewerCombatCommandlet();
    virtual int32 Main(const FString& Params) override;
};

/** Transient editor-only receiver for testing the real Blueprint damage event. */
UCLASS(Transient, NotBlueprintable)
class AVTGAttackRangeTestTarget : public ACharacter, public IVTGDamageable
{
    GENERATED_BODY()
public:
    virtual float TakeDamage(float Damage, const FDamageEvent&, AController*, AActor*) override
    {
        TotalDamage += Damage;
        return Damage;
    }
    float TotalDamage = 0.f;
};
