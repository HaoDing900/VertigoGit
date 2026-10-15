#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGActionHUDCommandlet.generated.h"
UCLASS()
class UVTGActionHUDCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGActionHUDCommandlet();
 virtual int32 Main(const FString& Params) override;
};
