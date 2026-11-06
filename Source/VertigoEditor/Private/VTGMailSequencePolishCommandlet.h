#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGMailSequencePolishCommandlet.generated.h"
UCLASS()
class UVTGMailSequencePolishCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGMailSequencePolishCommandlet();
    virtual int32 Main(const FString& Params) override;
};
