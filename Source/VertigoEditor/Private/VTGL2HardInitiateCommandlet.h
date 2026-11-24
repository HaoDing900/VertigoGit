#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGL2HardInitiateCommandlet.generated.h"

/** Move BPLM_L2StreetBarFight's level setup off OnStageBegin and onto its hard initiate event. */
UCLASS()
class UVTGL2HardInitiateCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGL2HardInitiateCommandlet();
    virtual int32 Main(const FString& Params) override;
};
