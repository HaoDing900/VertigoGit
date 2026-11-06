#pragma once

#include "Commandlets/Commandlet.h"
#include "VTGSaBlinkCommandlet.generated.h"

UCLASS()
class UVTGSaBlinkCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGSaBlinkCommandlet();
    virtual int32 Main(const FString& Params) override;
};
