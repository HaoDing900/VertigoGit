#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGL2FightRepairCommandlet.generated.h"
UCLASS()
class UVTGL2FightRepairCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGL2FightRepairCommandlet();
 virtual int32 Main(const FString& Params) override;
};
