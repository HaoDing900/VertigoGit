#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGDroneFacingCommandlet.generated.h"
UCLASS()
class UVTGDroneFacingCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGDroneFacingCommandlet();
    virtual int32 Main(const FString& Params) override;
};
