#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGMailFlowCommandlet.generated.h"

UCLASS()
class UVTGMailFlowCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGMailFlowCommandlet();
    virtual int32 Main(const FString& Params) override;
};
