#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGJawMovementCommandlet.generated.h"
UCLASS()
class UVTGJawMovementCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGJawMovementCommandlet();
    virtual int32 Main(const FString& Params) override;
};
