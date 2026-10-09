#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGLoadingScreenCommandlet.generated.h"

UCLASS()
class UVTGLoadingScreenCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UVTGLoadingScreenCommandlet();
    virtual int32 Main(const FString &Params) override;
};
