#pragma once
#include "Commandlets/Commandlet.h"
#include "Objects/Item.h"
#include "VTGStoryCombatLockCommandlet.generated.h"
UCLASS()
class UVTGStoryLockTestItem : public UItem { GENERATED_BODY() };
UCLASS()
class UVTGStoryCombatLockCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGStoryCombatLockCommandlet();
 virtual int32 Main(const FString& Params) override;
};
