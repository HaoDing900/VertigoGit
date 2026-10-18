#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGSewerAcidPolishCommandlet.generated.h"
UCLASS()
class UVTGSewerAcidPolishCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGSewerAcidPolishCommandlet();
 virtual int32 Main(const FString& Params) override;
};
