#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerAttackBalanceCommandlet.generated.h"
UCLASS()
class UVTGSewerAttackBalanceCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGSewerAttackBalanceCommandlet();
 virtual int32 Main(const FString& Params) override;
};
