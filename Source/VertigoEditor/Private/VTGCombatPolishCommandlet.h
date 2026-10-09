#pragma once
#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "VTGCombatPolishCommandlet.generated.h"
UCLASS()
class UVTGCombatPolishCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGCombatPolishCommandlet();
    virtual int32 Main(const FString &Params) override;
};
