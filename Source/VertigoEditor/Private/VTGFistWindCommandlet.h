#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGFistWindCommandlet.generated.h"
UCLASS()
class UVTGFistWindCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGFistWindCommandlet();
    virtual int32 Main(const FString& Params) override;
};
