#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGRecoveryHUDCommandlet.generated.h"
UCLASS()
class UVTGRecoveryHUDCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGRecoveryHUDCommandlet();
 virtual int32 Main(const FString& Params) override;
};
