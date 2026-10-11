#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGCustomizeFadeCommandlet.generated.h"
UCLASS()
class UVTGCustomizeFadeCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGCustomizeFadeCommandlet();
    virtual int32 Main(const FString& Params) override;
};
