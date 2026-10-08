#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGAttackFacingCommandlet.generated.h"

/** Opt-in migration and regression checks for attack-only target facing. */
UCLASS()
class UVTGAttackFacingCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGAttackFacingCommandlet();
    virtual int32 Main(const FString& Params) override;
};
