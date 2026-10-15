#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGHammerPostureCommandlet.generated.h"
UCLASS()
class UVTGHammerPostureCommandlet:public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGHammerPostureCommandlet();
    virtual int32 Main(const FString& Params)override;
};
