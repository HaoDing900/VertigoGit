#pragma once
#include "Commandlets/Commandlet.h"
#include "GameFramework/Actor.h"
#include "VTGGoonDamageCommandlet.generated.h"

UCLASS()
class AVTGGoonDamageTarget : public AActor
{
    GENERATED_BODY()
public:
    AVTGGoonDamageTarget();
    float Health=100.f;
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer) override;
};

UCLASS()
class UVTGGoonDamageCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGGoonDamageCommandlet();
    virtual int32 Main(const FString& Params) override;
};
