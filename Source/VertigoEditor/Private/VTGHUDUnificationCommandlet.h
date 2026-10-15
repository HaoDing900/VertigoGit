#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGHUDUnificationCommandlet.generated.h"
UCLASS()
class UVTGHUDUnificationCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGHUDUnificationCommandlet();
 virtual int32 Main(const FString& Params) override;
};
