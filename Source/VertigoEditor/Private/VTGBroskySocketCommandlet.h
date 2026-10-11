#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGBroskySocketCommandlet.generated.h"

UCLASS()
class UVTGBroskySocketCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGBroskySocketCommandlet();
    virtual int32 Main(const FString& Params) override;
};
