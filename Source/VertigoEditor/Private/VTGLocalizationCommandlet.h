#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGLocalizationCommandlet.generated.h"
UCLASS()
class UVTGLocalizationCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGLocalizationCommandlet();
    virtual int32 Main(const FString& Params) override;
};
