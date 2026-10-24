#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGBlackMarketQuestScanCommandlet.generated.h"
UCLASS()
class UVTGBlackMarketQuestScanCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVTGBlackMarketQuestScanCommandlet();
    virtual int32 Main(const FString& Params) override;
};
