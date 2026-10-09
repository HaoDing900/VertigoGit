#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGAlleyRepairCommandlet.generated.h"
UCLASS()
class UVTGAlleyRepairCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGAlleyRepairCommandlet();
 virtual int32 Main(const FString& Params) override;
};
