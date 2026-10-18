#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGDisplayLocalizationCommandlet.generated.h"
UCLASS()
class UVTGDisplayLocalizationCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGDisplayLocalizationCommandlet();
    virtual int32 Main(const FString& Params) override;
};
