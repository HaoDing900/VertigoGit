#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerBossSyncCommandlet.generated.h"
UCLASS()
class UVTGSewerBossSyncCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGSewerBossSyncCommandlet();
 virtual int32 Main(const FString& Params) override;
};
