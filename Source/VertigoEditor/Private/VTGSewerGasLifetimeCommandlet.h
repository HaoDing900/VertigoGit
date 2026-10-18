#pragma once
#include "Commandlets/Commandlet.h"
#include "GameFramework/Character.h"
#include "VTGSewerGasLifetimeCommandlet.generated.h"
UCLASS()
class UVTGSewerGasLifetimeCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGSewerGasLifetimeCommandlet();
 virtual int32 Main(const FString& Params) override;
};
UCLASS()
class AVTGSmokeDamageTestPawn : public ACharacter
{
 GENERATED_BODY()
public:
 float ReceivedDamage=0;
 virtual float TakeDamage(float Amount,const FDamageEvent& Event,AController* EventController,AActor* Causer) override {ReceivedDamage+=Amount;return Amount;}
};
