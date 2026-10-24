#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGBlackMarketCombatCommandlet.generated.h"
UCLASS()
class UVTGBlackMarketCombatCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGBlackMarketCombatCommandlet();
    virtual int32 Main(const FString& Params) override;

};

