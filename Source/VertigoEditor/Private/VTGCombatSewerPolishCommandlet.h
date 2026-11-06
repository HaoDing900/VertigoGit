#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGCombatSewerPolishCommandlet.generated.h"
UCLASS()
class UVTGCombatSewerPolishCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGCombatSewerPolishCommandlet();
    virtual int32 Main(const FString& Params) override;
};

