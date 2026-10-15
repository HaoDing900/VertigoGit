#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGHintIntegrationCommandlet.generated.h"
UCLASS()
class UVTGHintIntegrationCommandlet:public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGHintIntegrationCommandlet();
    virtual int32 Main(const FString& Params)override;
};
