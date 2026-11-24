#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGKeyBackgroundCommandlet.generated.h"

/** Tie BottomBackgroundOfKey's visibility to the three Narrative main text boxes. */
UCLASS()
class UVTGKeyBackgroundCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGKeyBackgroundCommandlet();
    virtual int32 Main(const FString& Params) override;
};
