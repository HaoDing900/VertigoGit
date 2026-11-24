#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGBPLMInitiateCommandlet.generated.h"

/**
 * Survey and repair the "hard initiate" wiring across every in-level BPLM.
 *
 * -Dump   read-only: what each BPLM's event graph actually looks like today
 * -Apply  write the hard initiate event into the ones that are missing it
 */
UCLASS()
class UVTGBPLMInitiateCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGBPLMInitiateCommandlet();
    virtual int32 Main(const FString& Params) override;
};
