#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGBlackMarketFollowupCommandlet.generated.h"
UCLASS()
class UVTGBlackMarketFollowupCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGBlackMarketFollowupCommandlet();
 int32 Main(const FString& Params) override;
};
