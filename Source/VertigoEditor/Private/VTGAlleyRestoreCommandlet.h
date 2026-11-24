#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGAlleyRestoreCommandlet.generated.h"

/**
 * Finish the alley sub-stage restore chains in BPLM_BlackMarket: put the barrier back, and close
 * manual saving again. Both are things the forward playthrough does and the restore forgot.
 */
UCLASS()
class UVTGAlleyRestoreCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGAlleyRestoreCommandlet();
    virtual int32 Main(const FString& Params) override;
};
