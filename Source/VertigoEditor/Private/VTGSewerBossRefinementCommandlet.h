#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerBossRefinementCommandlet.generated.h"
UCLASS()
class UVTGSewerBossRefinementCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGSewerBossRefinementCommandlet();
 virtual int32 Main(const FString& Params) override;
};
