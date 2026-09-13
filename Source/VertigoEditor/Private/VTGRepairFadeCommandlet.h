#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGRepairFadeCommandlet.generated.h"
UCLASS()
class UVTGRepairFadeCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGRepairFadeCommandlet();
 virtual int32 Main(const FString& Params) override;
};
