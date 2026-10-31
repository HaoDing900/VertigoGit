#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGQuestDisplayRepairCommandlet.generated.h"

UCLASS()
class UVTGQuestDisplayRepairCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;
};
