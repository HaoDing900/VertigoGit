#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGBPLMRatesCommandlet.generated.h"
UCLASS()
class UVTGBPLMRatesCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGBPLMRatesCommandlet();
    virtual int32 Main(const FString& Params) override;
};
