#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSequenceSkipCheckCommandlet.generated.h"
UCLASS()
class UVTGSequenceSkipCheckCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGSequenceSkipCheckCommandlet();
    virtual int32 Main(const FString& Params) override;
};
